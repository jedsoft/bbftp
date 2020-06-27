/*
 * bbftpd/bbftpd_daemon.c
 * Copyright (C) 1999, 2000, 2001, 2002 IN2P3, CNRS
 * bbftp@in2p3.fr
 * http://doc.in2p3.fr/bbftp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */ 

/****************************************************************************

  
  
 bbftpd_daemon.c    v 0.0.0  1999/11/24
                    v 1.3.0  2000/03/16
                    v 1.6.1  2000/03/30 - Use sysconf instead of OPEN_MAX
                    v 1.8.4  2000/04/21 - Seed the randoma generator
                    v 1.8.7  2000/05/24 - Include version.h
                    v 1.8.10 2000/08/11 - Portage to Linux
                    v 1.9.3  2000/10/12 - Define the socket first to be able
                                          to give information to the user 
                    v 1.9.4  2000/10/16 - Supress %m
                    v 2.0.0  2000/12/13 - Supress be_daemon and controlsock
                                          as global parameters
                                        - Add parameters incontrolsock and
                                          outcontrolsock for bbftp v2
                    v 2.0.1  2001/04/23 - Correct indentation
                    v 2.0.2  2001/05/04 - Correct include for RFIO
                    v 2.1.0  2001/06/11 - Change file name

*****************************************************************************/
#include <bbftpd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/socket.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <version.h>
#include <openssl/err.h>
#include <openssl/rand.h>
/*
** Common variables for BBFTP protocole version 1 and 2
*/
extern int  newcontrolport ;
extern char daemonchar[50] ;
/*
** Variables for BBFTP protocole version 1 
*/
extern int msgsock ;
/*
** Variables for BBFTP protocole version 2
*/
extern int incontrolsock ;
extern int outcontrolsock ;

void do_daemon(int argc,char **argv,char **envp)
{

    int        prpg ;
    int        i ;
    int     retcode ;
    int     controlsock ;
    struct sockaddr_in server;
    int     on = 1;
    int     nfds ;
    int     pid;
    
    char    buffrand[NBITSINKEY] ;
    struct timeval tp ;
    unsigned int seed ;
    
    controlsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
    if ( controlsock < 0 ) {
        syslog(BBFTPD_ERR, "Cannot create socket to listen on: %s",strerror(errno));
        fprintf(stderr,"Cannot create socket to listen on: %s\n",strerror(errno)) ;
        exit(1) ;
    }
    if ( setsockopt(controlsock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
        syslog(BBFTPD_ERR,"Cannot set SO_REUSEADDR on control socket : %s",strerror(errno)) ;
        fprintf(stderr,"Cannot set SO_REUSEADDR on control socket : %s\n ",strerror(errno)) ;
        exit(1) ;
    }
    server.sin_family = AF_INET ;
    server.sin_addr.s_addr = INADDR_ANY ;
    server.sin_port = htons(newcontrolport);
    if ( bind (controlsock,(struct sockaddr *) &server, sizeof(server) ) < 0 ) {
        syslog(BBFTPD_ERR,"Error binding control socket : %s",strerror(errno)) ;
        fprintf(stderr,"Error binding control socket : %s\n ",strerror(errno)) ;
        exit(1) ;
    }
    if ( listen(controlsock,100) < 0 ) {
        syslog(BBFTPD_ERR,"Error listening control socket : %s",strerror(errno)) ;
        fprintf(stderr,"Error listening control socket : %s\n ",strerror(errno)) ;
        exit(1) ;
    }
    /* Fork - so I'm not the owner of the process group any more */
    retcode = fork();
    if (retcode < 0) {
        syslog(BBFTPD_ERR, "Cannot fork %s",strerror(errno));
        exit(1);
    }
    /* No need for the parent any more */
    if (retcode > 0) _exit(0);

    prpg = 0 ;
    prpg = setsid () ;     /* disassoiciate from control terminal */
    if ( prpg < 0 ) {
        syslog(BBFTPD_ERR,"Cannot daemonise: %s",strerror(errno)) ;
        exit(1) ;
    }
    /* Close off all file descriptors and reopen syslog */

    nfds = sysconf(_SC_OPEN_MAX) ;

    closelog();
    for (i = 0; i <= nfds; i++) {
        if ( i != controlsock) close(i);
    }
    openlog(daemonchar, LOG_PID | LOG_NDELAY, BBFTPD_FACILITY);

    /* log PID in /var/run/bbftpd.pid */
    {
	FILE *f;
	if ((f = fopen("/var/run/bbftpd.pid", "w")) != NULL) {
	    fprintf(f, "%d", getpid());
	    fclose(f);
	}
    }

    /* junk stderr */
    (void) freopen("/dev/null", "w", stderr);
    
    syslog(BBFTPD_DEBUG,"Starting bbftpd in background mode") ;
    if ( bbftpd_blockallsignals() < 0 ) {
        exit(1) ;
    }
    /*
    ** Load the error message from the crypto lib
    */
    ERR_load_crypto_strings() ;
    /*
    ** Initialize the buffrand buffer which is giong to be used to initialize the 
    ** random generator
    */
    /*
    ** Take the usec to initialize the random session
    */
    gettimeofday(&tp,NULL) ;
    seed = tp.tv_usec ;
    srandom(seed) ;
    for (i=0; i < sizeof(buffrand) ; i++) {
        buffrand[i] = random() ;
    }
    /*
    ** Initialize the random generator
    */
    RAND_seed(buffrand,NBITSINKEY) ;
    while (1) {

        msgsock = accept(controlsock, 0, 0);
        if (msgsock < 0) {
             syslog(BBFTPD_ERR, "Accept failed: %s",strerror(errno));
            sleep(1);
            continue;
        }
            /* Fork off a handler */
        pid = fork();
        if (pid < 0) {
            syslog(BBFTPD_ERR, "failed to fork: %s",strerror(errno));
             sleep(1);
            continue;
        }
        if (pid == 0) {
            /* I am that forked off child */
            closelog();
            /* Make sure that stdin/stdout are the new socket */
            /* Only parent needs controlsock */
            if (controlsock != 0 && controlsock != 1)
                close(controlsock);
                openlog(daemonchar, LOG_PID | LOG_NDELAY, BBFTPD_FACILITY);
                incontrolsock = msgsock ;
                outcontrolsock = msgsock ;
                   return;
                }

            /* I am the parent */
        close(msgsock);
    
    }
}

