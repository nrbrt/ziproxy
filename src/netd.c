/* netd.c
 * Core daemon routines.
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
 *
 * This code also contains portions under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c) Juraj Variny, 2004
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY JURAJ VARINY
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ---------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "globaldefs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#define _GNU_SOURCE
#include <getopt.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <assert.h>

#include "cfgfile.h"
#include "log.h"
#include "ziproxy.h"
#include "txtfiletools.h"
#include "session.h"

int	proxy_server ();
int	proxy_handlereq (SOCKET sock_client, const char *client_addr, struct sockaddr_in *socket_host);
void	process_request (const char *client_addr, struct sockaddr_in *socket_host, SOCKET sock_child_out);
void 	daemon_sigcatch (int);
void	process_command_line_arguments (int argc, char **argv);
void	option_error (int exitcode, char *message, ...);
int	daemonize (void);

static int dpid_issue (const char *dpid_file, pid_t dpid);
static pid_t dpid_retrieve (const char *dpid_file);
static void daemon_error_cleanup_privileged (void);

char *cfg_file = DefaultCfgLocation;

struct struct_command_options{
	int daemon_mode;	/* != 0, daemon mode; == 0, [x]inetd mode)	*/
	int stop_daemon;	/* != 0, stops running daemon; == 0, proceed normally */
//	int cfg_specified;	/* != 0, used overrided default config file; == 0, default config file path	*/
	struct in_addr addr_low, addr_high;
} command_options;

/* user connections being attended at this moment. If (MaxActiveUserConnections > 0), should be <= MaxActiveUserConnections. */
static int curr_active_user_conn = 0;

static int daemon_process_greenlight = 0;
static pid_t daemon_sid = 0;	/* set to 0 'just in case' */
static pid_t daemon_pid;

/* writes dpid to pid file
   returns: ==0 OK, ==1 PID file already exists, ==2 unable to write PID file */
static int dpid_issue (const char *dpid_file, pid_t dpid)
{
	char outbuf [128];
	long long int my_pid;

	if (file_exists (dpid_file))
		return (1);

	my_pid = dpid;
	snprintf (outbuf, 128, "%lld\n", my_pid);
	if (dump_string_to_textfile (dpid_file, outbuf) != 0)
		return (2);

	return (0);
}

/* retrieves the PID in dpid_file
   returns: <0 (error), >0 (PID), ==0 (file exists, but no PID) */
static pid_t dpid_retrieve (const char *dpid_file)
{
	char * dpid_data;
	long long int retrieved_dpid = 0;

	if ((dpid_data = load_textfile_to_memory (dpid_file)) == NULL) {
		if (file_exists (dpid_file)) {
			return (0);
		} else {
			return (-1);
		}
	}
	sscanf (dpid_data, "%lld", &retrieved_dpid);

	free (dpid_data);
	return (retrieved_dpid);
}

// Parse the input string into an address range defined by addr_low and
// addr_high. The input can be a single address or a range separated by '-'.
// If the input is a single address, addr_low would be the same as addr_high.
// Return 0 if fails, 1 if succeeds.
static int parse_address_range(const char *str, struct in_addr *addr_low,
			       struct in_addr *addr_high)
{
  char *str_copy, *tmp_ptr;
  struct hostent * addr2 = NULL;

  // try a single address
  if (inet_aton(str, addr_low)) {
    addr_high->s_addr = addr_low->s_addr;
    return 1;
  }
  if ((addr2 = gethostbyname(str)) != NULL) {
    addr_low->s_addr = ((struct in_addr *)addr2->h_addr)->s_addr;
    addr_high->s_addr = addr_low->s_addr;
    return 1;
  }

  // try address range
  str_copy = strdup(str);
  if ((tmp_ptr = index(str_copy, '-')) == NULL) {
    free(str_copy);
    return 0;
  }

  *tmp_ptr = '\0'; // break the string into 2
  if (!inet_aton(str_copy,addr_low) && ((addr2 = gethostbyname(str_copy)) == NULL)) {
    free(str_copy);
    return 0;
  }
  if (addr2 != NULL) {
    addr_low->s_addr = ((struct in_addr *)addr2->h_addr)->s_addr;
    addr2 = NULL;
  }
  if (!inet_aton(tmp_ptr+1,addr_high) && ((addr2 = gethostbyname(tmp_ptr+1)) == NULL)) {
    free(str_copy);
    return 0;
  }
  free(str_copy);

  if (addr2 != NULL)
    addr_high->s_addr = ((struct in_addr *)addr2->h_addr)->s_addr;
  
  if (ntohl(addr_low->s_addr) > ntohl(addr_high->s_addr))
    return 0;

  return 1;
}

int main(int argc, char **argv, char *env[])
{
	int debuglogdes, i;

	/* We don't have the configuration data to do
	   a complete error_log initialization, so we dump
	   to stderr for now. */
	error_log_pre_init ();
	
	command_options.addr_low.s_addr=0;

	/* set defaults */
	command_options.daemon_mode = 0;

	process_command_line_arguments (argc, argv);

	i = ReadCfgFile(cfg_file);
	if (i != 0)
		return (5);

	/* is that a 'stop daemon' request? */
	if (command_options.stop_daemon != 0) {
		pid_t dpid;

		if (PIDFile != NULL) {
			dpid = dpid_retrieve (PIDFile);
			if (dpid == 0) {
				error_log_printf (LOGMT_FATALERROR, LOGSS_DAEMON,
					"PID file exists but its either unreadable or contains invalid data. PID file: %s\n",
					PIDFile);
			} else if (dpid < 0) {
				error_log_printf (LOGMT_FATALERROR, LOGSS_DAEMON,
					"The specified PID file does not exist! Nothing to do. PID file: %s\n",
					PIDFile);
			} else {
				/* ok, let's stop the daemon... */
				kill (dpid, SIGTERM);
				if (unlink (PIDFile) != 0) {
					error_log_printf (LOGMT_FATALERROR, LOGSS_DAEMON,
						"Daemon likely stopped, yet unable to remove PID file! PID file: %s\n",
						PIDFile);
					return (5);
				}

				/* FIXME: wishful thinking here:
				   we sleep a bit, then we _assume_ the daemon is gone */
				sleep (1);
				return (0);
			}
		} else {
			error_log_puts (LOGMT_FATALERROR, LOGSS_DAEMON,
				"No PID file specified! Nothing to do.");
		}
		return (5);
	}

	if (command_options.daemon_mode == 0) {	// [x]inetd mode
		/* start logging ([x]inetd mode) */
		debug_log_init (DebugLog);
		access_log_init (AccessLog);

		TOSMarking = QP_FALSE;			// we cannot set TOS while in [x]inetd mode
		sess_rclient = stdin;
		sess_wclient = stdout;
		process_request (NULL, NULL, 0);	// client address is unknown in this mode
	}

	if(!command_options.addr_low.s_addr && OnlyFrom){
		if (!parse_address_range (OnlyFrom, &(command_options.addr_low), &(command_options.addr_high))) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG, "Invalid address or host name '%s'\n", OnlyFrom);
			return 3;
		}
	}

	/* start logging (daemon mode) */
	debug_log_init (DebugLog);
	access_log_init (AccessLog);

	/* turn it into a daemon */
	{
		pid_t dpid;	/* daemonize() pid, not necessarily the daemon PID */
		long long int dpid_llint;
		int daemon_retcode = 0;

		signal (SIGUSR1, daemon_sigcatch); /* 'wake up, daemon' signal */

		dpid = daemonize ();
		dpid_llint = dpid;

		if (dpid < 0)
			return (9);	/* daemonize() failed */

		if (dpid > 0) {
			/* this is the parent process, the child is stablished as daemon at this point */
			if (PIDFile != NULL) {
				switch (dpid_issue (PIDFile, dpid)) {
				case 0:
					/* no problems here */
					break;
				case 1:
					/* file exists */
					error_log_printf (LOGMT_FATALERROR, LOGSS_DAEMON, "The PID file already exists! Daemon is either running already, or a previous process crashed. The PID file is: %s\n", PIDFile);
					daemon_retcode = 9;
					break;
				case 2:
					/* error while writting file */
					error_log_puts (LOGMT_FATALERROR, LOGSS_DAEMON, "Error while writting PID file.");
					daemon_retcode = 9;
					break;
				default:
					/* another error */
					error_log_puts (LOGMT_FATALERROR, LOGSS_DAEMON, "Error while creating/writting PID file.");
					daemon_retcode = 9;
					break;
				}
			} else {
				/* no PID file, let's just dump the daemon PID to stdout then */
				printf ("%lld\n", dpid_llint);
			}

			if (daemon_retcode != 0) {
				kill (dpid, SIGTERM);
				waitpid (dpid, NULL, 0);
				return (daemon_retcode);
			}

			kill (dpid, SIGUSR1); /* OK, the daemon process may continue */
			return (0);
		}
	}

	/* the daemon process suspends itself and waits for an 'OK' from its parent.
	   at this point, the daemon process may receive a SIGTERM from its parent
	   if something else goes wrong. */
	while (daemon_process_greenlight == 0) {
		sleep (1);	/* wait for signal or timeout, loop */
	}

	/* phew! we are clear to go */

	signal (SIGUSR1, SIG_DFL);	/* we no longer need this */
	daemon_pid = getpid ();
	signal (SIGTERM, daemon_sigcatch);

	return proxy_server(&(command_options.addr_low), &(command_options.addr_high));
}

/* daemon core */
int proxy_server(struct in_addr *addr_low, struct in_addr *addr_high)
{
	SOCKET sock_listen, sock_client;
	int sin_size, Status;
	int so_val = 1;
	// These are addresses in host byte order for comparison purpose.
	uint32_t addr_low_host, addr_high_host, connection_host;
	struct timeval tv;
	fd_set readfds;
	struct sockaddr_in sockAddr, gotConn;
	int which_BindOutgoing = 0;
	struct sockaddr_in pre_socket_host;
	struct sockaddr_in *socket_host = NULL;

	if (BindOutgoing_entries != 0)
		socket_host = &pre_socket_host;
	
	sockAddr.sin_family=AF_INET;
	sockAddr.sin_port=htons(Port);
	sockAddr.sin_addr.s_addr=INADDR_ANY;
	if (Address != NULL) {
		if (*Address != '\0')
			sockAddr.sin_addr.s_addr = inet_addr(Address);
	}
	
	sock_listen = socket(AF_INET, SOCK_STREAM, 0);

        /* FIXME: netd does not close the socket while exiting, this is just a workaround */
        Status = setsockopt (sock_listen, SOL_SOCKET, SO_REUSEADDR, &so_val, sizeof (so_val));
	Status = bind(sock_listen, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
	if(Status < 0)
	{
		error_log_printf (LOGMT_FATALERROR, LOGSS_DAEMON,
			"Failed to connect socket for receiving connections (port: %d).\n", Port);
		daemon_error_cleanup_privileged ();
		return 20;
	}
	if (listen(sock_listen, SOMAXCONN) != 0)
	{
		error_log_printf (LOGMT_FATALERROR, LOGSS_DAEMON,
			"Failed to listening mode on socket (port: %d).\n", Port);
		daemon_error_cleanup_privileged ();
		return 21;
	}
	if (setsockopt (sock_listen, SOL_SOCKET, SO_REUSEADDR, &so_val, sizeof (so_val)) < 0)
		error_log_printf (LOGMT_ERROR, LOGSS_DAEMON, "Failed to set REUSEADDR flag on socket (port: %d).\n", Port);
	
	addr_low_host = ntohl(addr_low->s_addr);
	addr_high_host = ntohl(addr_high->s_addr);

	/* At this point the daemon is stablished.
	   We should close stderr now. */
	error_log_no_stderr ();
	close (STDERR_FILENO);	

	/* do we need to change user/group? */
	if (switch_to_user_group (RunAsUser, RunAsGroup, 0) != 0)
		return (22);

	error_log_puts (LOGMT_INFO, LOGSS_DAEMON, "Daemon started.");

	/* daemon main loop */
	while (1)
	{
		/* create data structures for BindOutgoing rotation (if appliable) */
		if (socket_host != NULL) {
			if (which_BindOutgoing == BindOutgoing_entries)
				which_BindOutgoing = 0;

			socket_host->sin_family = AF_INET;
			socket_host->sin_port = 0; // the OS chooses the port
			socket_host->sin_addr.s_addr = BindOutgoing [which_BindOutgoing];
		
			which_BindOutgoing++;
		}

		/* watch listen socket for readability */
		FD_ZERO(&readfds);
		FD_SET(sock_listen, &readfds);

		/* timeout after one second */
		tv.tv_sec  = 1;
		tv.tv_usec = 0;

		Status = select(sock_listen + 1, &readfds, NULL, NULL, &tv);
		if (Status < 0) {
			error_log_puts (LOGMT_ERROR, LOGSS_DAEMON, "select() failed.");
		}
		else if (Status != 0) {
			/* data ready */
			sin_size=sizeof(gotConn);

			if ((sock_client = accept(sock_listen, (struct sockaddr *) &gotConn, &sin_size)) < 0) {
				error_log_puts (LOGMT_ERROR, LOGSS_DAEMON, "accept() failed.");
			}

			if(addr_low->s_addr) {
				connection_host = ntohl(gotConn.sin_addr.s_addr);
				if ((connection_host < addr_low_host) ||
				    (connection_host > addr_high_host)) {
					error_log_printf (LOGMT_WARN, LOGSS_DAEMON,
							"Connection from %s refused.\n",
							inet_ntoa (gotConn.sin_addr));
					close(sock_client);
					continue;
				}
			}

fork_retry:
			switch(fork())
			{
			case 0:
				/* CHILD */
				signal (SIGTERM, SIG_DFL); /* we don't want children using daemon's SIGTERM handler */
				close(sock_listen);

				/* TODO: implement general log, this shall not go to ErrorLog */
				/*
				if (! addr_low->s_addr) {
					error_log_printf (LOGMT_INFO, LOGSS_DAEMON, "[%d] Incoming connection from %s\n", \
						getpid(), inet_ntoa (gotConn.sin_addr));
				}
				*/

				return proxy_handlereq (sock_client, inet_ntoa(gotConn.sin_addr), socket_host);
			case -1:
				/* ERROR */
				error_log_puts (LOGMT_ERROR, LOGSS_DAEMON, "Fork() failed, waiting then retrying...\n");

				/* collect terminated child procs */
				while (waitpid (-1, NULL, WNOHANG) > 0) {
					curr_active_user_conn--;
				}

				/* sleep a bit, to avoid busy-looping the machine,
				   just in case this fork() failure is due
				   to something more serious */
				sleep (1);

				goto fork_retry;
				break;				
			default:
				/* PARENT */
				close(sock_client);
				curr_active_user_conn++;	/* one more active connection */
			}
		}

		/* collect terminated child procs */
		while (waitpid (-1, NULL, WNOHANG) > 0) {
			curr_active_user_conn--;
		}

		/* limit (still) reached? wait until another process is over */
		if ((MaxActiveUserConnections > 0) && (curr_active_user_conn == MaxActiveUserConnections)) {
			error_log_printf (LOGMT_WARN, LOGSS_DAEMON, "MaxActiveUserConnections limit reached (%d). Waiting for a connection to finish.\n", MaxActiveUserConnections);
			waitpid (-1, NULL, 0);

			curr_active_user_conn--;
		}
	}

	/* it should not reach this point */
	return 0;
}

/* handle HTTP session request */
int proxy_handlereq (SOCKET sock_client, const char *client_addr, struct sockaddr_in *socket_host)
{
	sess_rclient = fdopen (sock_client, "r");
	sess_wclient = fdopen (sock_client, "w");
	process_request (client_addr, socket_host, sock_client);

	close (sock_client);

	return(0);
}

void daemon_sigcatch (int signo)
{
	static pid_t current_pgrp;
	static int rcvd_sigterm = 0;

	switch (signo) {
	case SIGTERM:
		/* this is a handler exclusive to daemon process,
		   but since detaching this handler in children procs
		   is not atomic, we take care of this
		   (extremely) unlikely situation */
		if (daemon_pid != getpid()) {
			exit (0);
		}

		if (rcvd_sigterm > 1) {
			error_log_puts (LOGMT_WARN, LOGSS_DAEMON, "Too many SIGTERM received. Aborting the daemon process at once. Good luck, children!");
			exit (0);
		}

		if (rcvd_sigterm == 1) {
			error_log_puts (LOGMT_WARN, LOGSS_DAEMON, "Received SIGTERM for the 2nd time!");

			/* wait for children while the daemon itself
			   does not receive SIGTERM */
			while (waitpid (-1, NULL, 0) > 0);	/* 1-line loop */

			exit (0);
		}

		error_log_puts (LOGMT_INFO, LOGSS_DAEMON, "Received SIGTERM.");
		rcvd_sigterm++;

		current_pgrp = getpgrp ();
		killpg ((int) current_pgrp, SIGTERM);

		/* wait for children while the daemon itself
		   does not receive SIGTERM */
		while (waitpid (-1, NULL, 0) > 0); /* 1-line loop */

		exit (0);
		break;
	case SIGUSR1:
		daemon_process_greenlight = 1;
		break;
	default:
		break;
	}
}

void process_request (const char *client_addr, struct sockaddr_in *socket_host, SOCKET sock_child_out)
{
	ziproxy (client_addr, socket_host, sock_child_out);
	exit (14);
}

void process_command_line_arguments (int argc, char **argv)
{
	int option_index = 0;
	int defined_mode = 0;
	int defined_user = 0;
	int defined_group = 0;
	int defined_pidfile = 0;
	int option;
	struct option long_options[] =
	{
		{"config-file", 1, 0, 'c'},
		{"daemon-mode", 0, 0, 'd'},
		{"only-from", 1, 0, 'f'},
		{"help", 0, 0, 'h'},
		{"inetd-mode", 0, 0, 'i'},
		{"stop-daemon", 0, 0, 'k'},
		{"user", 1, 0, 'u'},
		{"group", 1, 0, 'g'},
		{"pid-file", 1, 0, 'p'},
		{0, 0, 0, 0}
	};

	command_options.daemon_mode = 0;
	command_options.stop_daemon = 0;
	cli_RunAsUser = NULL;
	cli_RunAsGroup = NULL;
	cli_PIDFile = NULL;

	while ((option = getopt_long (argc, argv, "c:df:g:hikp:u:", long_options, &option_index)) != EOF){
		switch(option){
			case 'c':
				cfg_file = optarg;
				break;
			case 'd':
				if (defined_mode != 0)
					option_error (3, "Invalid parameters: daemon-mode, stop-daemon and inet-mode are mutually exclusive.\n");
				command_options.daemon_mode = 1;
				defined_mode = 1;
				break;
			case 'f':
				option_error (3, "Obsolete option. Use \"OnlyFrom\" config option instead.\n", optarg);
				break;
			case 'g':
				/* will check if group exists later */
				cli_RunAsGroup = optarg;
				defined_group = 1;
				break;
			case 'h':
				printf ("Ziproxy " VERSION "\n"
						"Copyright (c)2005-2014 Daniel Mealha Cabrita\n"
						"\n"

						"This program is free software; you can redistribute it and/or modify\n"
						"it under the terms of the GNU General Public License as published by\n"
						"the Free Software Foundation; either version 2 of the License, or\n"
						"(at your option) any later version.\n"
						"\n"
						"This program is distributed in the hope that it will be useful,\n"
						"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
						"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
						"GNU General Public License for more details.\n"
						"\n"
						"You should have received a copy of the GNU General Public License\n"
						"along with this program; if not, write to the Free Software\n"
						"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA\n"
						"\n\n"
						
						"Usage: ziproxy <-d|-i|-k> [-c config_file] [-u user_name] [-g group_name] [-p pid_filename] [-h]\n\n"
						"-d, --daemon-mode\n\tUsed when running in standalone mode.\n\n"
						"-k, --stop-daemon\n\tStops daemon.\n\n"
						"-i, --inetd-mode\n\tUsed when running from inetd or xinetd.\n\n"
						"-c <config_file>, --config-file=<config_file>\n\tFull path to ziproxy.conf file (instead of default one).\n\n"
						"-u <user_name>, --user=<user_name>\n\tRun daemon as the specified user.\n\n"
						"-g <group_name>, --group=<group_name>\n\tRun daemon as the specified group.\n\tIf unspecified and user_name is specified, uses user_name's group.\n\n"
						"-p <pid_filename>, --pid-file=<pid_filename>\n\tUse the specified PID file for daemon control.\n\n"
						"-h, --help\n\tDisplay summarized help (this text).\n\n"
						"\n");
				exit (0);
				break;
			case 'i':
				if (defined_mode != 0)
					option_error (3, "Invalid parameters: daemon-mode, stop-daemon and inet-mode are mutually exclusive.\n");
				defined_mode = 1;
				break;
			case 'k':
				if (defined_mode != 0)
					option_error (3, "Invalid parameters: daemon-mode, stop-daemon and inet-mode are mutually exclusive.\n");
				command_options.stop_daemon = 1;
				defined_mode = 1;
				break;
			case 'p':
				/* at this point we cannot check properly whether
				   this PID file is usable, so we just set it.
				   if anything wrong, ziproxy will
				   complain in a later stage */
				cli_PIDFile = optarg;
				defined_pidfile = 1;
				break;
			case 'u':
				/* will check if user exists later */
				cli_RunAsUser = optarg;
				defined_user = 1;
				break;
			case ':':
				option_error (4, "Missing mandatory parameter.\n");
				break;
			case '?':
				option_error (4, "Unknown parameter provided.\n");
				break;
			default:
				option_error (4, "Unrecognized option.\n");
		}
	}

	if (defined_mode == 0)
		option_error (3, "It is required to define either 'daemon-mode', 'stop-daemon' or 'inetd-mode'.\n");

	/* options appliable to daemon-related operation only */
	if ((command_options.daemon_mode == 0) && (command_options.stop_daemon == 0)) {
		if ((defined_user != 0) || (defined_group != 0))
			option_error (3, "Setting 'user' or 'group' requires either 'daemon-mode' or 'stop-daemon' mode.\n");

		if (defined_pidfile != 0)
			option_error (3, "Setting PID file requires either 'daemon-mode' or 'stop-daemon' mode.\n");
	}
}

void option_error (int exitcode, char *message, ...) {
	va_list ap;

	va_start (ap, message);
	error_log_vprintf (LOGMT_FATALERROR, LOGSS_PARAMETER, message, ap);
	va_end (ap);

	error_log_printf (LOGMT_INFO, LOGSS_PARAMETER, "Run `ziproxy --help` for a list of available parameters with their syntax.\n");
	exit (exitcode);
}

/* returns:
 * 0: child, daemon created successfully..
 * >0: parent, (daemon created successfully from parent's side) returns child's PID. must exit afterwards.
 * -1: error (parent process)
 * -2: error (child process)
 */
pid_t daemonize (void) {
	pid_t pid;

	pid = fork ();

	if (pid < 0) {
		error_log_puts (LOGMT_FATALERROR, LOGSS_DAEMON, "Unable to fork() to create daemon.");
		return (-1);
	}

	if (pid > 0)
		return (pid);

	umask (0);

	daemon_sid = setsid ();
	if (daemon_sid < 0) {
		error_log_puts (LOGMT_FATALERROR, LOGSS_DAEMON, "Unable to set daemon SID.");
		return (-2);
	}

	if ((chdir ("/")) < 0) {
		error_log_puts (LOGMT_FATALERROR, LOGSS_DAEMON, "Unable to change to root directory.");
		return (-2);
	}

	close (STDIN_FILENO);
	close (STDOUT_FILENO);

	/* stderr will be closed at a later stage */
	//close (STDERR_FILENO);
										        
	return (0);
}

/* do the necessary cleanup after a fatal error
   happens while starting the daemon, but after the
   initial checking */
void daemon_error_cleanup_privileged (void)
{
	if (PIDFile != NULL) {
		unlink (PIDFile);
	}
}

