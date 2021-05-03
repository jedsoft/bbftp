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
/* #include <syslog.h> */
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

#include "_bbftpd.h"

int do_daemon (int argc, char **argv, int ctrlport)
{
   int pid;
    int        prpg ;
    int i;
    int     controlsock ;
    struct sockaddr_in server;
    int     on = 1;
    int     nfds ;


   (void) argc;
   (void) argv;

   controlsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
   if (controlsock == -1)
     {
        bbftpd_log_stderr (BBFTPD_ERR, "Cannot create socket to listen on: %s",strerror(errno));
        return -1;
    }
   if (-1 == setsockopt (controlsock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)))
     {
        bbftpd_log_stderr (BBFTPD_ERR,"Cannot set SO_REUSEADDR on control socket : %s",strerror(errno)) ;
	close (controlsock);
	return -1;
    }

   server.sin_family = AF_INET ;
   server.sin_addr.s_addr = INADDR_ANY ;
   server.sin_port = htons(ctrlport);

   while (-1 == bind (controlsock,(struct sockaddr *) &server, sizeof(server)))
     {
	if (errno == EINTR) continue;

        bbftpd_log_stderr (BBFTPD_ERR,"Error binding control socket : %s",strerror(errno)) ;
	close (controlsock);
        return -1;
     }

   if (-1 == listen (controlsock, 100))
     {
        bbftpd_log_stderr (BBFTPD_ERR,"Error listening control socket : %s",strerror(errno)) ;
	close (controlsock);
	return -1;
     }

   /* Fork - so I'm not the owner of the process group any more */
   if (-1 == (pid = fork()))
     {
        bbftpd_log_stderr (BBFTPD_ERR, "Cannot fork %s",strerror(errno));
        return -1;
     }

   /* No need for the parent any more */
   if (pid > 0) _exit(0);

   if (-1 == (prpg = setsid ()))      /* disassoiciate from control terminal */
     {
        bbftpd_log (BBFTPD_ERR,"Cannot daemonise: %s",strerror(errno)) ;
        return -1;
     }

   /* Close off all file descriptors and reopen bbftpd_log */
   nfds = sysconf(_SC_OPEN_MAX) ;

   bbftpd_log_close();

   for (i = 0; i < nfds; i++)
     {
	if ( i != controlsock) close(i);
     }
   bbftpd_log_reopen ();

   /* log PID in /var/run/bbftpd.pid */
     {
	FILE *f;
	if (NULL != (f = fopen("/var/run/bbftpd.pid", "w")))
	  {
	     (void) fprintf(f, "%d", getpid());
	     fclose(f);
	  }
     }

   /* junk stderr */
   (void) freopen("/dev/null", "w", stderr);

   bbftpd_log (BBFTPD_DEBUG,"Starting bbftpd in background mode") ;

   if (-1 == bbftpd_blockallsignals())
     return -1;

   if (-1 == bbftpd_crypt_init_random ())
     return -1;

   while (1)
     {
	int s;

	if (-1 == (s = accept(controlsock, 0, 0)))
	  {
	     bbftpd_log(BBFTPD_ERR, "Accept failed: %s",strerror(errno));
	     sleep(1);
	     continue;
	  }

	/* Fork off a handler */
	if (-1 == (pid = fork()))
	  {
	     bbftpd_log(BBFTPD_ERR, "failed to fork: %s",strerror(errno));
	     close (s);
             sleep(1);
	     continue;
	  }

	if (pid > 0)
	  {
	     /* parent */
	     close (s);
	     continue;
	  }

	/* I am that forked off child */

	bbftpd_log_close();
	/* Make sure that stdin/stdout are the new socket */
	/* Only parent needs controlsock */
	if (controlsock != 0 && controlsock != 1)
	  close(controlsock);
	bbftpd_log_reopen ();
	return s;
     }
}

