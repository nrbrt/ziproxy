/* preemptdns.c
 * Preemptive name resolution feature.
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2005-2014 Daniel Mealha Cabrita
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

/* FIXME: this code will work only if the html page
 * was not previously compressed */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include "log.h"
#include "cfgfile.h"

typedef struct {
	const char *hostname;
	pthread_t tid;
	struct hostent *h;
} nameresolve_thread;

enum {
	INSERT_OK = 1,
	INSERT_FAIL_LISTFULL = 0,
	INSERT_FAIL_IP = 2,
	INSERT_FAIL_INVALID = 3,
	INSERT_FAIL_REPEATED = 4
	};

	
char * collect_hostname (char *prev_pos)
{
	char *start_pos, *curr_pos;
	int hostname_size = 0, stop_loop = 0;
	const char *srch_patt="://";
	
	if (*prev_pos == '\0')
		return (NULL);
	curr_pos = prev_pos;
	
	if ((start_pos = strstr (curr_pos, srch_patt)) != NULL){
		start_pos = start_pos + strlen(srch_patt);
		curr_pos = start_pos;

		while (stop_loop == 0){
			switch (*curr_pos){
			case '\0':
				stop_loop = 1;
				break;
			case '@':
				start_pos = ++curr_pos;
				hostname_size = 0;
				break;
			/* FIXME: oops-may-be-ipv6
			 * usually a portnumber follows, though
			 * probably not a problem, see code below */
			case ':':
			case '?':
			case ';':
			case '\\':
			case '/':
			case '\'':
			case '\"':
			case '>':
			case '\n':
			case '\r':
			case ')':
			case '&':
				*curr_pos = '\0';
				stop_loop = 1;
				break;
			default:
				curr_pos++;
				hostname_size++;
			}
		}
	}

	if (hostname_size == 0)
		return (NULL);
	else
		return (start_pos);	
}

int insert_hostname (nameresolve_thread *threads_array, int maxnames, const char *hostname)
{
	int step = 0, is_ipv4 = 1, hostname_len;
	const char *charstep = hostname;
	char last_char;

	hostname_len = strlen (hostname);

	/* full array, do not waste processing here */
	if (threads_array [maxnames - 1].hostname != NULL)
		return (INSERT_FAIL_LISTFULL);
	
	/* FIXME: half-assed way to check
	 * whether hostname is an IPv6 number instead
	 * see: oops-may-be-ipv6 */
	if (hostname_len <= 2)
		return (INSERT_FAIL_IP);

	/* try to avoid bogus hostnames (accepts only .xxxx, .xxx or .xx) */
	/* FIXME: won't accept .museum and long domains like that */
	if ((PreemptNameResBC) && (hostname_len >= 5)){
		if (strchr (hostname + (hostname_len - 5), '.') == NULL)
			return (INSERT_FAIL_INVALID);
	}
	
	/* check whether hostname is an IPv4 number instead */
	last_char = *charstep;
	while (last_char != '\0'){
		if (! (((last_char >= '0') && (last_char <= '9')) || \
			(last_char == '.'))){
			is_ipv4 = 0;
			last_char = '\0';
		} else {
			last_char = *(++charstep);
		}
	}
	if (is_ipv4 == 1)
		return (INSERT_FAIL_IP);

	/* check whether hostname is already in the table */
	while (step < maxnames){
		if (threads_array [step].hostname == NULL){
			threads_array [step].hostname = hostname;
			debug_log_printf ("Hostname (preemptdns): %s\n", hostname);		
			step = maxnames;
		} else if (strcmp (threads_array [step].hostname, hostname) == 0){
			return (INSERT_FAIL_REPEATED);
		}
		step++;
	}	

	return (INSERT_OK);
}

void init_threads_array (nameresolve_thread *threads_array, int maxnames)
{
	int step = 0;

	while (step < maxnames)
		threads_array [step++].hostname = NULL;
	return;
}

void *nameresolv_thread (void *given_thread)
{
	nameresolve_thread *workthread = (nameresolve_thread *) given_thread;

	workthread->h = gethostbyname (workthread->hostname);
	pthread_exit(0);
}

void preempt_dns_from_html (char* inbuf, int inlen)
{
	char *workbuf, *hostname, *search_from;
	int stop_loop = 0, step;
	int maxnames = PreemptNameResMax;
	nameresolve_thread threads_array [maxnames], *workthread;

	if (maxnames < 1)
		return;

	if ((workbuf = malloc (inlen + 2)) != NULL){
		init_threads_array (threads_array, maxnames);
		memcpy (workbuf, inbuf, inlen);
		workbuf [inlen] = '\0';
		workbuf [inlen + 1] = '\0';
		
		/* start a new process for preemptdns */
		switch (fork ()){
		case 0: //child		
                        // setsid();
                        daemon (0, 0);
			break;
		case -1:
			debug_log_printf ("Error. Unable to fork (preemptdns)\n");
		default://parent
			free (workbuf);
			return;
		}
		
		/* populate threads_array */
		search_from = workbuf;
		while (stop_loop == 0){
			if ((hostname = collect_hostname (search_from)) != NULL){	
				if (insert_hostname (threads_array, maxnames, hostname) == INSERT_FAIL_LISTFULL){
					stop_loop = 1;
					debug_log_printf ("Note (preemptdns): List full\n");
				} else {
					search_from = hostname + strlen (hostname) + 1;
				}
			} else {
				stop_loop = 1;
			}
		}

		/* launch threads */
		step = 0;
		while (step < maxnames){
			if (threads_array [step].hostname != NULL){
				workthread = &threads_array [step];
				pthread_create(&workthread->tid, NULL, nameresolv_thread, workthread);
			} else {
				step = maxnames;				
			}
			step++;
		}	

		/* wait for threads to finish */
		step = 0;
		while (step < maxnames){
			if (threads_array [step].hostname != NULL){
				workthread = &threads_array [step];
				pthread_join (workthread->tid, NULL);
			} else {
				step = maxnames;				
			}
			step++;
		}	

		free (workbuf);
	} else {
		debug_log_printf ("Unable to alloc work buffer (preemptdns)\n");
	}

	/* if it reached this point, it's the child process for sure
	 * end of game for it */
	exit (0);
}

