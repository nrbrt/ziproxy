/* ziproxylogtool.c
 * Ziproxy log analyser.
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2006-2014 Daniel Mealha Cabrita
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * ---------------------------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../globaldefs.h"

#define LINE_BUFFER 4096
#define HOSTNAME_BUFFER 256
#define STATS_MATRIX_BULK_MALLOC 64
#define HOSTNAME_CACHE_ENTRIES 256	/* not too low, neither too high */

typedef struct {
	enum	{GLOBAL, PER_HOST, FILTER, NOT_DEFINED} logtool_mode;
	long long int	epoch_min, epoch_max;		/* unix epoch, -1 if unused */
	long long int	bytes_in_min, bytes_in_max;
	long long int	bytes_out_min, bytes_out_max;
	long long int	delay_min, delay_max;
	const char	*in_filename, *out_filename;	/* if NULL, uses stdin/stdout */
	FILE		*in_file, *out_file;		/* 'real' file or stdin/stdout */
} t_goptions;

typedef struct {
	long long int bytes_in;
	long long int bytes_out;
	long long int epoch;
	long long int delay;
	char    host_name [HOSTNAME_BUFFER];
} t_loglinedata;

typedef struct {
	int	host_name_fingerprint [HOSTNAME_CACHE_ENTRIES];
	int	entry_position_in_table [HOSTNAME_CACHE_ENTRIES];
	int	next_avail;
} t_hostname_cache;

typedef struct {
	char	host_name [HOSTNAME_BUFFER];
	long long int	total_bytes_orig;
	long long int	total_bytes_comp;
	long long int	total_accesses;
} t_stats_line;

typedef struct {
	int	used_entries;
	int	alloc_entries;
	t_stats_line    *table;
	int	*host_name_fingerprint;	// speeds up string search
	t_hostname_cache	hostname_cache; // speeds up string search even more
} t_stats_matrix;

int string_fingerprint (const char *given_string)
{
	int	fingerprint = 0;
	int	this_char;

	this_char = (int) *given_string;
	while (this_char != 0) {
		fingerprint += this_char;
		fingerprint ^= (this_char) << 15;
		fingerprint ^= fingerprint << 7;
		fingerprint ^= fingerprint >> 24;

		this_char = (int) *given_string;
		given_string++;
	}
	return (fingerprint);
}

/* only checks whether equal or different, otherwise similar to strcmp */
int fast_strcmp (const char *str1, const char *str2)
{
	while (*str1 == *(str2++)) {
		if (*(str1++) == '\0')
			return (0);
	}
	return (1);
}

t_stats_matrix *stats_matrix_create (void)
{
	t_stats_matrix *new_stats_matrix;
	int	cache_step;
	
	if ((new_stats_matrix = (t_stats_matrix *) malloc (sizeof (t_stats_matrix))) != NULL) {
		new_stats_matrix->used_entries = 0;
		new_stats_matrix->alloc_entries = 0;
		new_stats_matrix->table = NULL;
		new_stats_matrix->host_name_fingerprint = NULL;

		/* initializes cache */
		new_stats_matrix->hostname_cache.next_avail = 0;
		cache_step = HOSTNAME_CACHE_ENTRIES;
		while (cache_step--) {
			new_stats_matrix->hostname_cache.host_name_fingerprint [cache_step] = -1;
			new_stats_matrix->hostname_cache.entry_position_in_table [cache_step] = -1;
		}
		
		return (new_stats_matrix);
	}
	return (NULL);
}

void stats_matrix_destroy (t_stats_matrix *stats_matrix)
{
	if (stats_matrix->table != NULL)
		free (stats_matrix->table);
	if (stats_matrix->host_name_fingerprint != NULL)
		free (stats_matrix->host_name_fingerprint);
	free (stats_matrix);
}

/* reserves and returns the available entry (0=first), or -1 if failed */
int stats_matrix_alloc_entry (t_stats_matrix *stats_matrix)
{
	t_stats_line	*new_table;
	int		*new_host_name_fingerprint;

	if (stats_matrix->used_entries == stats_matrix->alloc_entries) {
		if ((new_table = (t_stats_line *) realloc (stats_matrix->table, (stats_matrix->alloc_entries + STATS_MATRIX_BULK_MALLOC) * sizeof (t_stats_line))) == NULL) {
			return (-1);
		}
		if ((new_host_name_fingerprint = (int *) realloc (stats_matrix->host_name_fingerprint, (stats_matrix->alloc_entries + STATS_MATRIX_BULK_MALLOC) * sizeof (int))) == NULL) {
			return (-1);
		}

		stats_matrix->alloc_entries += STATS_MATRIX_BULK_MALLOC;
		stats_matrix->table = new_table;
		stats_matrix->host_name_fingerprint = new_host_name_fingerprint;
	}

	stats_matrix->used_entries++;
	return (stats_matrix->used_entries - 1);
}

/* returns: >= 0 (position in matrix), ok -- -1, error */
int stats_matrix_add_into (t_stats_matrix *stats_matrix, long long int bytes_in, long long int bytes_out, const char *host_name)
{
	int	pos_in_matrix = -1;
	int	counter;
	int	host_name_fingerprint;

	int	*fingerprints_table;
	int	used_entries;
	t_stats_line	*table;
	int	found_in_cache = 0;

	fingerprints_table = stats_matrix->host_name_fingerprint;
	used_entries = stats_matrix->used_entries;
	table = stats_matrix->table;
	
	host_name_fingerprint = string_fingerprint (host_name);
	
	/* check whether host entry exists */

	/* checks in cache, first */
	counter = 0;
	while (LIKELY(counter < HOSTNAME_CACHE_ENTRIES)) {
		if (UNLIKELY(stats_matrix->hostname_cache.host_name_fingerprint [counter] == host_name_fingerprint)) {
			if (LIKELY(stats_matrix->hostname_cache.entry_position_in_table [counter] >= 0)) {
				if (LIKELY(! fast_strcmp (table[stats_matrix->hostname_cache.entry_position_in_table [counter]].host_name, host_name))) {
					pos_in_matrix = stats_matrix->hostname_cache.entry_position_in_table [counter];
					found_in_cache = 1;
					counter = HOSTNAME_CACHE_ENTRIES; /* ends loop */				
				}
			}
		}
		counter++;
	}

	/* if not found in cache, search the whole table */
	if (UNLIKELY(pos_in_matrix < 0)) {
		counter = 0;
		while (LIKELY(counter < used_entries)) {
			if (UNLIKELY(fingerprints_table[counter] == host_name_fingerprint)) {
				if (LIKELY(! fast_strcmp (table[counter].host_name, host_name))) {
					pos_in_matrix = counter;
					counter = used_entries; /* ends loop */
				}
			}
			counter++;
		}
	}
	
	/* create host entry, if it does not exist */
	if (UNLIKELY(pos_in_matrix < 0)) {
		pos_in_matrix = stats_matrix_alloc_entry (stats_matrix);
		if (pos_in_matrix < 0) // realloc error
			return (-1);
		
		stats_matrix->table[pos_in_matrix].total_bytes_orig = 0;
		stats_matrix->table[pos_in_matrix].total_bytes_comp = 0;
		stats_matrix->table[pos_in_matrix].total_accesses = 0;

		strncpy (stats_matrix->table[pos_in_matrix].host_name, host_name, HOSTNAME_BUFFER - 1);
		stats_matrix->table[pos_in_matrix].host_name[strlen (host_name)] = '\0';
		stats_matrix->host_name_fingerprint[pos_in_matrix] = string_fingerprint (stats_matrix->table[pos_in_matrix].host_name);
	}

	/* if not found in cache, roll the cache and insert that entry */
	if (found_in_cache == 0) {
		stats_matrix->hostname_cache.entry_position_in_table[stats_matrix->hostname_cache.next_avail] = pos_in_matrix;
		stats_matrix->hostname_cache.host_name_fingerprint [stats_matrix->hostname_cache.next_avail] = host_name_fingerprint;
		stats_matrix->hostname_cache.next_avail++;
		if (stats_matrix->hostname_cache.next_avail == HOSTNAME_CACHE_ENTRIES)
			stats_matrix->hostname_cache.next_avail = 0;
	}
	
	/* update host data */
	stats_matrix->table[pos_in_matrix].total_bytes_orig += bytes_in;
	stats_matrix->table[pos_in_matrix].total_bytes_comp += bytes_out;
	stats_matrix->table[pos_in_matrix].total_accesses++;

	return (pos_in_matrix);
}

/* dump all matrix contents into out_file as text */
void stats_matrix_dump_data (t_stats_matrix *stats_matrix, FILE *out_file)
{
	int	counter = 0;
	long long int	compression;
	
	while (counter < stats_matrix->used_entries) {
		if (stats_matrix->table[counter].total_bytes_orig == 0)
			compression = 100;
		else
			compression = (stats_matrix->table[counter].total_bytes_comp * 100) / stats_matrix->table[counter].total_bytes_orig;

		if (*(stats_matrix->table[counter].host_name) != '\0') {
			fprintf (out_file, "%lld %lld %lld %lld %s\n", \
					stats_matrix->table[counter].total_accesses, \
					stats_matrix->table[counter].total_bytes_orig, \
					stats_matrix->table[counter].total_bytes_comp, \
					compression, \
					stats_matrix->table[counter].host_name);
		}
		counter++;
	}
}

long long int convert_str_to_llint (const char *in_buf)
{
	int	is_negative = 0;
	int	in_buf_len = 0;
	long long int abs_num_value = 0;
	long long int mult_digit = 1;

	if (*in_buf == '-') {
		is_negative = 1;
		in_buf++;
	}

	while ((*(in_buf + in_buf_len) != '\0') && (*(in_buf + in_buf_len) != '.'))
		in_buf_len++;

	while (in_buf_len) {
		in_buf_len--;
		abs_num_value += (*(in_buf + in_buf_len) - 0x30) * mult_digit;
		mult_digit *= 10;
	}

	if (is_negative == 0)
		return (abs_num_value);
	return (0 - abs_num_value);
}

int pick_element (const char *in_buf, char *out_buf, int element)
{
	const char	*count = in_buf;
	int		element_len = 0;
	
	while (element) {
		/* skip characters */
		/* != '\0' && != ' ' */
		while (LIKELY((unsigned) *count > (unsigned) ' '))
			count++;
		
		element--;
		
		/* skip spaces */
		while (LIKELY(*count == ' '))
			count++;
	}
	
	/* determine element size */
	/* != '\0' && != ' ' */
	while (LIKELY((unsigned) *(count + element_len) > (unsigned) ' '))
		element_len++;

	// strncpy (out_buf, count, element_len);
	memcpy (out_buf, count, element_len * sizeof(char));
	*(out_buf + element_len) = '\0';

	return (element_len);
}

void get_hostname_only (const char *given_url, char *output_string)
{
	if (! strncmp (given_url, "http://", 7))
		given_url += 7;

	while ((*given_url > ' ') && (*given_url != '/') && (*given_url != ':'))
		*(output_string++) = *(given_url++);

	*output_string = '\0';
}

int get_data_from_line (const char *log_line, t_loglinedata *loglinedata)
{
	char	tmp_line [LINE_BUFFER];

	/* in bytes */
	pick_element (log_line, tmp_line, 4);
	loglinedata->bytes_in = convert_str_to_llint (tmp_line);

	/* out bytes */	
	pick_element (log_line, tmp_line, 5);
	loglinedata->bytes_out = convert_str_to_llint (tmp_line);
	
	/* hostname */
	pick_element (log_line, tmp_line, 7);
	get_hostname_only (tmp_line, loglinedata->host_name);

	/* epoch */
	pick_element (log_line, tmp_line, 0);
	loglinedata->epoch = convert_str_to_llint (tmp_line);

	/* delay */
	pick_element (log_line, tmp_line, 1);
	loglinedata->delay = convert_str_to_llint (tmp_line);
	
	return (0);
}

/* returns: ==0, unacceptable -- !=0, ok */
int acceptable_entry (const t_goptions *goptions, const t_loglinedata *loglinedata)
{
	/* check whether line is within the specified time range */
	if (goptions->epoch_min >= 0) {
		if (loglinedata->epoch < goptions->epoch_min)
			return (0);
	}
	if (goptions->epoch_max >= 0) {
		if (loglinedata->epoch >= goptions->epoch_max)
			return (0);
	}

	/* check whether line is within in_bytes size range */
	if (goptions->bytes_in_min >= 0) {
		if (loglinedata->bytes_in < goptions->bytes_in_min)
			return (0);
	}
	if (goptions->bytes_in_max >= 0) {
		if (loglinedata->bytes_in >= goptions->bytes_in_max)
			return (0);
	}

	/* check whether line is within out_bytes size range */
	if (goptions->bytes_out_min >= 0) {
		if (loglinedata->bytes_out < goptions->bytes_out_min)
			return (0);
	}
	if (goptions->bytes_out_max >= 0) {
		if (loglinedata->bytes_out >= goptions->bytes_out_max)
			return (0);
	}

	/* check whether line is within delay msec range */
	if (goptions->delay_min >= 0) {
		if (loglinedata->delay < goptions->delay_min)
			return (0);
	}
	if (goptions->delay_max >= 0) {
		if (loglinedata->delay >= goptions->delay_max)
			return (0);
	}
	
	return (1);
}

void stats_general_loop (const t_goptions *goptions, long long int *total_accesses, long long int *total_bytes_in, long long int *total_bytes_out)
{
	char	log_line [LINE_BUFFER];
	t_loglinedata	loglinedata;

	*total_accesses = -1;	/* kludgy fix -- only after trying to read past EOF if returns so */
	*total_bytes_in = 0;
	*total_bytes_out = 0;
	
	while (! feof (goptions->in_file)) {
		log_line [0] = '\0';
		fgets (log_line, LINE_BUFFER - 1, goptions->in_file);
	
		get_data_from_line (log_line, &loglinedata);

		if (acceptable_entry (goptions, &loglinedata) != 0) {
			*total_accesses += 1;
			*total_bytes_in += loglinedata.bytes_in;
			if (loglinedata.bytes_out == -1)
				loglinedata.bytes_out = loglinedata.bytes_in;
			*total_bytes_out += loglinedata.bytes_out;
		}
	}
}

void stats_general (const t_goptions *goptions)
{
	long long int	total_accesses;
	long long int	total_bytes_in, total_bytes_out;
	
	stats_general_loop (goptions, &total_accesses, &total_bytes_in, &total_bytes_out);
	fprintf (goptions->out_file, "Total accesses                     : %lld\n", total_accesses);
	fprintf (goptions->out_file, "Total incoming bytes (uncompressed): %lld\n", total_bytes_in);
	fprintf (goptions->out_file, "Total outgoing bytes (compressed)  : %lld\n", total_bytes_out);
}

void stats_max_hosts (const t_goptions *goptions)
{
	char	log_line [LINE_BUFFER];
	t_loglinedata	loglinedata;
	t_stats_matrix *stats_matrix;

	stats_matrix = stats_matrix_create ();
	
	while (! feof (goptions->in_file)) {
		log_line [0] = '\0';
		fgets (log_line, LINE_BUFFER - 1, goptions->in_file);

		get_data_from_line (log_line, &loglinedata);

		if (loglinedata.bytes_out == -1)
			loglinedata.bytes_out = loglinedata.bytes_in;

		if (acceptable_entry (goptions, &loglinedata) != 0) {
			stats_matrix_add_into (stats_matrix, loglinedata.bytes_in, loglinedata.bytes_out, loglinedata.host_name);
		}
	}

	stats_matrix_dump_data (stats_matrix, goptions->out_file);
	stats_matrix_destroy (stats_matrix);
}

void stats_filter_mode (t_goptions *goptions)
{
	char	log_line [LINE_BUFFER];
	t_loglinedata	loglinedata;
	
	while (! feof (goptions->in_file)) {
		log_line [0] = '\0';
		fgets (log_line, LINE_BUFFER - 1, goptions->in_file);
	
		get_data_from_line (log_line, &loglinedata);

		if (acceptable_entry (goptions, &loglinedata) != 0) {
			fputs (log_line, goptions->out_file);
		}		
	}
}

void option_error (int exitcode, char *message, ...)
{
	va_list ap;

	va_start (ap, message);
	vfprintf (stderr, message, ap);
	va_end (ap);

	fprintf (stderr, "\nCall `ziproxylogtool --help` for a list of available parameters with their syntax.\n\n");
	exit (exitcode);
}

void process_command_line_arguments (t_goptions *goptions, int argc, char **argv)
{
	int option_index = 0;
	int option;
	struct option long_options[] =
	{
		{"mode", 1, 0, 'm'},
		{"in-file", 1, 0, 'i'},
		{"out-file", 1, 0, 'o'},
		{"help", 0, 0, 'h'},
		{"epoch-min", 1, 0, '1'},
		{"epoch-max", 1, 0, '2'},
		{"bytes-in-min", 1, 0, '3'},
		{"bytes-in-max", 1, 0, '4'},
		{"bytes-out-min", 1, 0, '5'},
		{"bytes-out-max", 1, 0, '6'},
		{"delay-min", 1, 0, '7'},
		{"delay-max", 1, 0, '8'},
		{0, 0, 0, 0}
	};

	goptions->logtool_mode = NOT_DEFINED;
	goptions->in_filename = NULL;
	goptions->out_filename = NULL;
	goptions->epoch_min = -1;
	goptions->epoch_max = -1;
	goptions->bytes_in_min = -1;
	goptions->bytes_in_max = -1;
	goptions->bytes_out_min = -1;
	goptions->bytes_out_max = -1;
	goptions->delay_min = -1;
	goptions->delay_max = -1;
	
	while ((option = getopt_long (argc, argv, "m:i:o:h1:2:3:4:5:6:7:8:", long_options, &option_index)) != EOF){
		switch (option) {
			case 'm':
				switch (*optarg) {
					case 'g':
						goptions->logtool_mode = GLOBAL;
						break;
					case 'h':
						goptions->logtool_mode = PER_HOST;
						break;
					case 'f':
						goptions->logtool_mode = FILTER;
						break;
					default:
						option_error (4, "Invalid operation mode defined.\n");
				}
				break;
			case 'i':
				goptions->in_filename = optarg;
				break;
			case 'o':
				goptions->out_filename = optarg;
				break;
			case 'h':
				fprintf (stderr, "Ziproxy Log Tool 1.1\n"
						"Copyright (c)2006-2014 Daniel Mealha Cabrita\n"
						"Licensed under GNU GPL v2 or later version. See documentation for details.\n\n"
						"Usage: ziproxylogtool <-m <mode>> [-i <filename>] [-o <filename>] [-h] ... [other options]\n\n"
						"GENERAL OPTIONS\n\n"
						"-m <g|h>, --mode\n\tOutput mode:\n\t\"g\" - Global stats\n\t\"h\" - Per host stats (accesses, in_bytes, out_bytes, compression %%, hostname)\n\t\"f\" - Filter mode (filter log entries according to filtering options)\n\n"
						"-i <filename>, --in-file\n\tInput file (Ziproxy log file). If unspecified, uses stdin.\n\n"
						"-o <filename>, --out-file\n\tOutput file (stats output). If unspecified, uses stdout.\n\n"
						"-h, --help\n\tDisplay summarized help (this text).\n\n"
						"FILTERING OPTIONS (if ommited, won't apply)\n\n"
						"-1 <unix epoch>, --epoch-min\n\tFilter entries starting from that epoch.\n\n"
						"-2 <unix epoch>, --epoch-max\n\tFilter entries older than that epoch.\n\n"
						"-3 <bytes>, --bytes-in-min\n\tFilter entries which incoming_bytes >= <bytes>\n\n"
						"-4 <bytes>, --bytes-in-max\n\tFilter entries which incoming_bytes < <bytes>\n\n"
						"-5 <bytes>, --bytes-out-min\n\tFilter entries which outgoing_bytes >= <bytes>\n\n"
						"-6 <bytes>, --bytes-out-max\n\tFilter entries which outgoing_bytes < <bytes>\n\n"
						"-7 <mili_seconds>, --delay-min\n\tFilter entries which delay >= <mili_seconds>\n\n"
						"-8 <mili_seconds>, --delay-max\n\tFilter entries which delay < <mili_seconds>\n\n"
						"\n");
				exit (0);
				break;
			case '1':
				goptions->epoch_min = convert_str_to_llint (optarg);
				break;
			case '2':
				goptions->epoch_max = convert_str_to_llint (optarg);
				break;
			case '3':
				goptions->bytes_in_min = convert_str_to_llint (optarg);
				break;
			case '4':
				goptions->bytes_in_max = convert_str_to_llint (optarg);
				break;
			case '5':
				goptions->bytes_out_min = convert_str_to_llint (optarg);
				break;
			case '6':
				goptions->bytes_out_max = convert_str_to_llint (optarg);
				break;
			case '7':
				goptions->delay_min = convert_str_to_llint (optarg);
				break;
			case '8':
				goptions->delay_max = convert_str_to_llint (optarg);
				break;
			default:
				option_error (4, "Unrecognized option.\n");
		}
	}

	if ((goptions->epoch_min >= 0) && (goptions->epoch_max >= 0)) {
		if (goptions->epoch_max <= goptions->epoch_min)
			option_error (12, "Epoch-max must be higher than Epoch-min.\n");
	}
	if ((goptions->bytes_in_min >= 0) && (goptions->bytes_in_max >= 0)) {
		if (goptions->bytes_in_max <= goptions->bytes_in_min)
			option_error (12, "Bytes-in-max must be higher than Bytes-in-min.\n");
	}
	if ((goptions->bytes_out_min >= 0) && (goptions->bytes_out_max >= 0)) {
		if (goptions->bytes_out_max <= goptions->bytes_out_min)
			option_error (12, "Bytes-out-max must be higher than Bytes-out-min.\n");
	}
	if ((goptions->delay_min >= 0) && (goptions->delay_max >= 0)) {
		if (goptions->delay_max <= goptions->delay_min)
			option_error (12, "Delay-max must be higher than Delay-min.\n");
	}

	if (goptions->logtool_mode == NOT_DEFINED)
		option_error (3, "It is required to define an operation mode (-m <mode>)\n");
}

int main (int argc, char **argv, char *env[])
{
	t_goptions	goptions;

	process_command_line_arguments (&goptions, argc, argv);

	/* sets input file or stdin */
	if (goptions.in_filename == NULL ) {
		goptions.in_file = stdin;
	} else {
		if ((goptions.in_file = fopen (goptions.in_filename, "r")) == NULL)
			option_error (10, "Unable to open input file.\n");
	}
	/* sets output file or stdout */
	if (goptions.out_filename == NULL ) {
		goptions.out_file = stdout;
	} else {
		if ((goptions.out_file = fopen (goptions.out_filename, "w")) == NULL) {
			if (goptions.in_filename != NULL)
				fclose (goptions.in_file);
			option_error (11, "Unable to open output file.\n");
		}
	}
	
	switch (goptions.logtool_mode) {
		case GLOBAL:
			stats_general (&goptions);
			break;
		case PER_HOST:
			stats_max_hosts (&goptions);
			break;
		case FILTER:
			stats_filter_mode (&goptions);
			break;
		case NOT_DEFINED:
			// it will never reach this point (only to avoid warnings from compiler)
			break;
	}

	if (goptions.in_filename != NULL)	
		fclose (goptions.in_file);
	if (goptions.out_filename != NULL)
		fclose (goptions.out_file);

	exit (0);
}

