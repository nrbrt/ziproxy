/* ziproxy.c
 * Code for processing an individual request.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <syslog.h>

#define SRC_ZIPROXY_C
#include "image.h"
#include "cfgfile.h"
#include "http.h"
#include "log.h"
#include "tosmarking.h"
#include "session.h"

static void sigcatch (int sig);

static int open_client_socket (char* hostname, unsigned short int Port, struct sockaddr_in *socket_host);
void proxy_ssl (http_headers *hdr, FILE* sockrfp, FILE* sockwfp);

// define socket_host=NULL if binding to a specific IP is not required
int ziproxy (const char *client_addr, struct sockaddr_in *socket_host, SOCKET sock_child_out) {
	int sockfd;
	FILE* sockrfp;
	FILE* sockwfp;
	http_headers* hdrs;
	int i, portmatch;

	/* reset debug_log timer */
	debug_log_reset_difftime ();
	/* use current PID (process for a specific request) for debug_log */
	debug_log_set_pid_current ();

	/* init TOS marking TOSMarking */
	if (TOSMarking)
		debug_log_puts ("TOS marking is enabled.");
	if (tosmarking_init (TOSMarking, sock_child_out, TOSFlagsDefault, TOSFlagsDiff, tos_markasdiff_url, tos_maskasdiff_ct, TOSMarkAsDiffSizeBT))
		debug_log_printf ("TOS: default traffic set to 0x%x\n", TOSFlagsDefault);
	
	if(ConnTimeout){
		/* catch timeouts */
		(void) signal (SIGALRM, sigcatch);
		(void) alarm (ConnTimeout);
	}

	/* catch broken pipes */
	(void) signal (SIGPIPE, sigcatch);

	/* catch SIGTERM */
	signal (SIGTERM, sigcatch);

	/* new HTTP request, reset access_log (if active) */
	access_log_reset ();
	access_log_define_client_adrr (client_addr);

	hdrs = parse_initial_request();

	access_log_define_method (hdrs->method);

	if (((hdrs->flags & H_TRANSP_PROXY_REQUEST) == 0) && (! ConventionalProxy))
		send_error (400, "Bad Request", NULL, "HTTP proxy requests not honoured by server.");
	
	if (hdrs->flags & H_USE_SSL) {
		access_log_set_flags (LOG_AC_FLAG_CONV_PROXY);	/* CONNECT only works in conventional proxy mode */
		access_log_set_flags (LOG_AC_FLAG_CONN_METHOD);
		NextProxy = NULL;
	} else {	/* not SSL, fill in the rest of client request */
		get_client_headers (hdrs);
		debug_log_difftime ("getting, parsing headers");
		debug_log_printf ("Method = %s Protocol = %s\n", hdrs->method, hdrs->proto);

		/* if a request received as transparent proxy... */
		if (hdrs->flags & H_TRANSP_PROXY_REQUEST) {
			access_log_set_flags (LOG_AC_FLAG_TRANSP_PROXY);
			fix_request_url (hdrs);
		} else {
			access_log_set_flags (LOG_AC_FLAG_CONV_PROXY);
		}
	}

	/* at this point, we've got both HTTP method and URL */
	access_log_define_method (hdrs->method);
	access_log_define_url (hdrs->url);

	/* catch signals indicating crash,
	   but only after this point since:
	   - client headers were received
	   - accesslog was initialized */
	if (InterceptCrashes) {
		signal (SIGSEGV, sigcatch);
		signal (SIGFPE, sigcatch);
		signal (SIGILL, sigcatch);
		signal (SIGBUS, sigcatch);
		signal (SIGSYS, sigcatch);
	}

	/* is BindOutgoing enabled?
	   if so, is this host in the BindOutgoing exception list?
	   if so, bind to a fixed (source) IP instead */
	if ((BindOutgoing != NULL) && (BindOutgoingExList != NULL)) {
		if (slist_check_if_matches (BindOutgoingExList, hdrs->host)) {
			socket_host->sin_addr.s_addr = *BindOutgoingExAddr;
		}
	}

	/* check whether we don't have local restrictions on method/port */
	if (hdrs->flags & H_USE_SSL) {
		/* method CONNECT */
		if (! AllowMethodCONNECT)
			send_error (403, "Forbidden", NULL, "CONNECT method not allowed.");

		if (RestrictOutPortCONNECT_len > 0) {
			portmatch = 0;
			for (i = 0; i < RestrictOutPortCONNECT_len; i++) {
				if (hdrs->port == RestrictOutPortCONNECT [i])
					portmatch = 1;
			}
			if (portmatch == 0)
				send_error (403, "Forbidden", NULL, "Requested CONNECT to forbidden port.");
		}
	} else {
		/* generic HTTP */
		if (RestrictOutPortHTTP_len > 0) {
			portmatch = 0;
			for (i = 0; i < RestrictOutPortHTTP_len; i++) {
				if (hdrs->port == RestrictOutPortHTTP [i])
					portmatch = 1;
			}
			if (portmatch == 0)
				send_error (403, "Forbidden", NULL, "Requested HTTP connection to forbidden port.");
		}
	}

	/* Open the client socket to the real web server. */
	sockfd = open_client_socket (hdrs->host, hdrs->port, socket_host);

	/* Open separate streams for read and write, r+ doesn't always work. 
	 * What about "a+" ? */
	sockrfp = fdopen( sockfd, "r" );
	sockwfp = fdopen( sockfd, "w" );

	if (hdrs->flags & H_USE_SSL) {
		/* HTTP CONNECT method */
		proxy_ssl (hdrs, sockrfp, sockwfp);
	} else {
		proxy_http (hdrs, sockrfp, sockwfp);
	}

	/* Done. */
	(void) close( sockfd );
	exit( 0 );
}


#if defined(AF_INET6) && defined(IN6_IS_ADDR_V4MAPPED)
#define USE_IPV6
#endif

/* FIXME: This is a hardcoded limit, it should be dynamic instead.
          It poses a theoretically possible problem. */
/* unlikely a hostname will have more than that many IPs */
#define MAX_SA_ENTRIES 16

// define socket_host=NULL if binding to a specific IP is not required
static int open_client_socket (char* hostname, unsigned short int Port, struct sockaddr_in *socket_host) {
#ifdef USE_IPV6
    struct addrinfo hints;
    char Portstr[10];
    int gaierr;
    struct addrinfo* ai;
    struct addrinfo* ai2;
    struct addrinfo* aiv4;
    struct addrinfo* aiv6;
    struct sockaddr_in6 sa[MAX_SA_ENTRIES];
#else /* USE_IPV6 */
    struct hostent *he;
    struct sockaddr_in sa[MAX_SA_ENTRIES];
#endif /* USE_IPV6 */
    int sa_len, sock_family, sock_type, sock_protocol;
    int sockfd;
    int sa_entries = 0;
    
    memset( (void*) &sa, 0, sizeof(sa) );

#ifdef USE_IPV6
#define SIZEOF_SA sizeof(struct sockaddr_in6)
#else
#define SIZEOF_SA sizeof(struct sockaddr_in)
#endif
    
if (NextProxy != NULL) {
	hostname = NextProxy;
	Port = NextPort;
}


#ifdef USE_IPV6
    (void) memset( &hints, 0, sizeof(hints) );
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    (void) snprintf( Portstr, sizeof(Portstr), "%hu", Port );
    if ( (gaierr = getaddrinfo( hostname, Portstr, &hints, &ai )) != 0 )
	send_error( 404, "Not Found", NULL, "Unknown host." );

    /* Find the first IPv4 and IPv6 entries. */
    aiv4 = NULL;
    aiv6 = NULL;
    for ( ai2 = ai; ai2 != NULL; ai2 = ai2->ai_next )
	{
	switch ( ai2->ai_family )
	    {
	    case AF_INET: 
	    if ( aiv4 == NULL )
		aiv4 = ai2;
	    break;
	    case AF_INET6:
	    if ( aiv6 == NULL )
		aiv6 = ai2;
	    break;
	    }
	}

    /* If there's an IPv4 address, use that, otherwise try IPv6. */
    if (aiv4 != NULL) {
	if (SIZEOF_SA < aiv4->ai_addrlen) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_UNSPECIFIED,
			"%s - sockaddr too small (%llu < %llu)\n",
			hostname, (unsigned long long) SIZEOF_SA,
			(unsigned long long) aiv4->ai_addrlen);
		exit (1);
	}
	sock_family = aiv4->ai_family;
	sock_type = aiv4->ai_socktype;
	sock_protocol = aiv4->ai_protocol;
	sa_len = aiv4->ai_addrlen;

	/* loops each returned IP address and fills the array */
	{	
		struct addrinfo* current_aiv4 = aiv4;
		
		(void) memmove (&sa[sa_entries++], current_aiv4->ai_addr, sa_len);
		while ((sa_entries < MAX_SA_ENTRIES) && (current_aiv4->ai_next != NULL)) {
			current_aiv4 = current_aiv4->ai_next;
			(void) memmove (&sa[sa_entries++], current_aiv4->ai_addr, sa_len);
		}
	}
	
	goto ok;
	}
    if ( aiv6 != NULL )
	{
	if ( SIZEOF_SA < aiv6->ai_addrlen )
	    {
	    (void) fprintf(
		stderr, "%s - sockaddr too small (%lu < %lu)\n",
		hostname, (unsigned long) SIZEOF_SA,
		(unsigned long) aiv6->ai_addrlen );
	    exit( 1 );
	    }
	sock_family = aiv6->ai_family;
	sock_type = aiv6->ai_socktype;
	sock_protocol = aiv6->ai_protocol;
	sa_len = aiv6->ai_addrlen;

	/* loops each returned IP address and fills the array */
	{
		struct addrinfo* current_aiv6 = aiv6;

		(void) memmove (&sa[sa_entries++], current_aiv6->ai_addr, sa_len);
		while ((sa_entries < MAX_SA_ENTRIES) && (current_aiv6->ai_next != NULL)) {
			current_aiv6 = current_aiv6->ai_next;
			(void) memmove (&sa[sa_entries++], current_aiv6->ai_addr, sa_len);
		}
	}

	goto ok;
	}

    send_error( 404, "Not Found", NULL, "Unknown host." );

    ok:
    freeaddrinfo( ai );

#else /* USE_IPV6 */

    he = gethostbyname( hostname );
    if ( he == NULL )
	send_error( 404, "Not Found", NULL, "Unknown host." );
    sock_family = he->h_addrtype;
    sock_type = SOCK_STREAM;
    sock_protocol = 0;
    sa_len = SIZEOF_SA;

    /* loops each returned IP address and fills the array */
    while ((sa_entries < MAX_SA_ENTRIES) && (he->h_addr_list[sa_entries] != NULL)) {
	(void) memmove (&sa[sa_entries].sin_addr, he->h_addr_list[sa_entries], sizeof (sa[sa_entries].sin_addr));
	sa[sa_entries].sin_family = he->h_addrtype;
	sa[sa_entries].sin_port = htons (Port);
	sa_entries++;
    }
    
#endif /* USE_IPV6 */

    sockfd = socket( sock_family, sock_type, sock_protocol );
    if ( sockfd < 0 )
	send_error( 500, "Internal Error", NULL, "Couldn't create socket." );

    /* bind (outgoing connection) to a specific IP */
    if (socket_host != NULL)
	bind (sockfd, (struct sockaddr *) socket_host, sizeof (*socket_host));

    /* try each returned IP of that hostname */
    while (sa_entries--){
        if ( connect( sockfd, (struct sockaddr*) &sa[sa_entries], sa_len ) >= 0 )
            return sockfd;
    }
    send_error( 503, "Service Unavailable", NULL, "Connection refused." );

    /* it won't reach this point (it will either return sockfd or it will call send_error()
     * which will finish the process soon later) */
    return (-1);
}


void proxy_ssl (http_headers *hdr, FILE* sockrfp, FILE* sockwfp)
{ 
	/* Return SSL-proxy greeting header. */
	fputs ("HTTP/1.0 200 Connection established\r\n\r\n", sess_wclient);
	fflush (sess_wclient);

	blind_tunnel (hdr, sockrfp, sockwfp);
	access_log_dump_entry ();
}

static void sigcatch (int sig)
{
	static int must_abort = 0;
	ZP_FLAGS sigflag;

	/* if signal while treating SIGSEGV... */
	if (must_abort != 0)
		exit (100);

	switch (sig) {
	case SIGALRM:
		access_log_set_flags (LOG_AC_FLAG_XFER_TIMEOUT);
		access_log_dump_entry ();
		send_error (408, "Request Timeout", NULL, "Request timed out.");
		break;
	case SIGPIPE:
		/* usually we can interrupt immediately when the connection is broken at the remote server's side,
		 * the remaining few bytes in the buffer can be discarded since the file is incomplete anyway.
		 * one problem (may be others aswell), though, is when we're downloading a redirection page,
		 * from a bad-behaved http server which breaks the connection instead of properly closing it.
		 * the chances are the client will receive 0 bytes and the redirection won't happen.
		 * so this signal is used only for flagging broken pipe in the access log and the program will
		 * continue its processing and behave accordingly. */
		access_log_set_flags (LOG_AC_FLAG_BROKEN_PIPE);
		debug_log_puts ("ERROR: Pipe broken. Transfer interrupted.");
		return;
		// access_log_flags (accesslog_data->rep_strm_inlen_decompressed ? &(accesslog_data->inlen_decompressed) : NULL, NULL, "B");
		// exit (100);
		break;
	case SIGSEGV:
	case SIGFPE:
	case SIGILL:
	case SIGBUS:
	case SIGSYS:
	case SIGTERM:
		must_abort = -1;

		switch (sig) {
		case SIGSEGV:	sigflag = LOG_AC_FLAG_SIGSEGV; break;
		case SIGFPE:	sigflag = LOG_AC_FLAG_SIGFPE; break;
		case SIGILL:	sigflag = LOG_AC_FLAG_SIGILL; break;
		case SIGBUS:	sigflag = LOG_AC_FLAG_SIGBUS; break;
		case SIGSYS:	sigflag = LOG_AC_FLAG_SIGSYS; break;
		case SIGTERM:	sigflag = LOG_AC_FLAG_SIGTERM; break;
		}

		access_log_set_flags (sigflag);
		access_log_dump_entry ();

		exit (100);
		break;
	default:
		// this shouldn't happen since we're not expecting other signals
		access_log_set_flags (LOG_AC_SOFTWARE_BUG); /* help! */
		access_log_dump_entry ();

		exit (100);
		break;
	}
}

