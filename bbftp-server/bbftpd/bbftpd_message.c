/*
 * bbftpd/bbftpd_message.c
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

  
  
 bbftpd_message.c   v 0.0.0  1999/11/24
                    v 1.6.0  2000/03/24 - Take the readmessage routine from 
                                          bbftpc which is more flexible the
                                          only difference is that this one is
                                          logging into syslog
                    v 1.6.1  2000/03/24 - Portage to OSF1
                                        - Set the socket in non block mode
                    v 1.8.7  2000/05/30 - Portage to IRIX64
                    v 1.8.10 2000/08/11 - Portage to Linux
                    v 1.9.0  2000/08/24 - Use configure to help port
                    v 1.9.4  2000/10/16 - Supress %m
                    v 2.0.0  2000/12/07 - Introduce discardmessage routine
                                        - Replace recv by read for ssh mode
                                          (Tim Adye)
                    v 2.0.1 2001/04/23  - Correct indentation
                    v 2.1.0 2001/06/01  - Set all messages routine in one 
                                          file

*****************************************************************************/
#include <bbftpd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* #include <syslog.h> */
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

#include <netinet/in.h>
#include <stdarg.h>

#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>
#include <version.h>

#include "_bbftpd.h"

static int try_select (int fd, int is_read, int to)
{
   fd_set *read_mask = NULL, *write_mask = NULL;
   fd_set mask;

   FD_ZERO(&mask) ;
   FD_SET(fd, &mask);

   if (is_read)
     read_mask = &mask;
   else
     write_mask = &mask;

   while (1)
     {
	struct timeval wait_timer;
	int status;

	wait_timer.tv_sec  = to ;
	wait_timer.tv_usec = 0 ;

	if ((-1 == (status = select(fd+1, read_mask, write_mask, NULL, &wait_timer)))
	    && (errno == EINTR))
	  continue;

	return status;
     }
}

int readmessage (int sock, char *buffer, int msglen, int to)
{
   int    nbget ;
   int    retcode ;

   nbget = 0 ;
   /*
    ** start the reading loop
    */
   while ( nbget < msglen )
     {
	if (-1 == (retcode = try_select (sock, 1, to)))
	  {
	     bbftpd_log(BBFTPD_ERR,"Read message : Select error : MSG (%d,%d)", msglen, nbget);
	     return -1;
	  }
	if ( retcode == 0 )
	  {
	     bbftpd_log(BBFTPD_ERR,"Read message : Time out : MSG (%d,%d)", msglen, nbget) ;
	     return -1 ;
	  }

	while (-1 == (retcode = read(sock, &buffer[nbget], msglen-nbget)))
	  {
	     if (errno == EINTR)
	       continue;

	     bbftpd_log(BBFTPD_ERR,"Read message : Receive error : MSG (%d,%d) : %s", msglen, nbget, strerror(errno)) ;
	     return -1 ;
	  }

	if ( retcode == 0 )
	  {
	     bbftpd_log(BBFTPD_ERR,"Read message : Connexion breaks") ;
	     return -1 ;
	  }

	nbget = nbget + retcode ;
     }

   return 0;
}

int discardmessage (int sock, int msglen, int to)
{
    int    nbget ;
    int    retcode ;
    char buffer[256] ;

   /*
    ** We are going to read buflen by buflen till the message 
    ** is exhausted
    */

   nbget = 0 ;
   while ( nbget < msglen )
     {
	if (-1 == (retcode = try_select (sock, 1, to)))
	  {
	     bbftpd_log(BBFTPD_ERR,"Discard message : Select error : MSG (%d,%d)",msglen,nbget) ;
	     return -1 ;
	  }

	if ( retcode == 0 )
	  {
	     bbftpd_log(BBFTPD_ERR,"Discard message : Time out : MSG (%d,%d)",msglen,nbget) ;
	     return -1 ;
	  }

	while (-1 == (retcode = recv(sock, buffer, sizeof(buffer), 0)))
	  {
	     if (errno == EINTR)
	       continue;

	     bbftpd_log(BBFTPD_ERR,"Discard message : Receive error : MSG (%d,%d) : %s",msglen,nbget,strerror(errno)) ;
	     return -1 ;
	  }

	if ( retcode == 0 )
	  {
	     bbftpd_log(BBFTPD_ERR,"Discard message : Connexion breaks") ;
	     return -1 ;
	  }

	nbget = nbget + retcode ;
     }

    return 0;
}

int writemessage (int sock, const char *buffer, int msglen, int to)
{
   int    nbsent ;
   int    retcode ;

   nbsent = 0 ;
   /*
    ** start the writing loop
    */
   while ( nbsent < msglen )
     {
	if (-1 == (retcode = try_select (sock, 0, to)))
	  {
	     bbftpd_log(BBFTPD_ERR,"Write message : Select error : MSG (%d,%d)",msglen,nbsent) ;
	     return -1 ;
	  }
	if ( retcode == 0 )
	  {
	     bbftpd_log(BBFTPD_ERR,"Write message : Time out : MSG (%d,%d)",msglen,nbsent) ;
		  return -1 ;
	  }

	while (-1 == (retcode = write(sock, buffer + nbsent, msglen - nbsent)))
	  {
	     if (errno == EINTR)
	       continue;

	     bbftpd_log(BBFTPD_ERR,"Write message : write error : MSG (%d,%d) : %s",msglen,nbsent,strerror(errno)) ;
	     return -1 ;
	  }
	if (retcode == 0)
	  {
	     bbftpd_log(BBFTPD_ERR,"Write message : Connexion breaks") ;
	     return -1 ;
	  }

	nbsent = nbsent + retcode ;
     }

    return(0) ;
}

int bbftpd_fd_msgwrite_int32_2 (int fd, int code, int i, int j)
{
   int32_t buf[4];

   buf[0] = code;
   buf[1] = htonl (8);
   buf[2] = htonl (i);
   buf[3] = htonl (j);

   return writemessage (fd, (char *)buf, sizeof(buf), sendcontrolto);
}

int bbftpd_fd_msgwrite_int32 (int fd, int code, int i)
{
   int32_t buf[3];

   buf[0] = code;
   buf[1] = htonl (4);
   buf[2] = htonl (i);

   return writemessage (fd, (char *)buf, sizeof(buf), sendcontrolto);
}

int bbftpd_fd_msgwrite_len (int fd, int code, int len)
{
   int32_t buf[2];

   buf[0] = code;
   buf[1] = htonl(len);

   return writemessage (fd, (char *)buf, sizeof(buf), sendcontrolto);
}

int bbftpd_msgwrite_int32_2 (int code, int i, int j)
{
   int32_t buf[4];

   buf[0] = code;
   buf[1] = htonl(8);
   buf[2] = htonl(i);
   buf[3] = htonl(j);

   return writemessage (outcontrolsock, (char *)buf, sizeof(buf), sendcontrolto);
}

int bbftpd_msgwrite_int32 (int code, int i)
{
   return bbftpd_fd_msgwrite_int32 (outcontrolsock, code, i);
}

int bbftpd_msgwrite_len (int code, int len)
{
   return bbftpd_fd_msgwrite_len (outcontrolsock, code, len);
}

int bbftpd_msgwrite_bytes (int code, char *bytes, int len)
{
   if ((-1 == bbftpd_fd_msgwrite_len (outcontrolsock, code, len))
       || (-1 == writemessage (outcontrolsock, bytes, len, sendcontrolto)))
     return -1;

   return 0;
}

/* Byteswap in place */
int _bbftpd_write_int32_array (int32_t *a, int len)
{
   int i;

   for (i = 0; i < len; i++) a[i] = htonl (a[i]);
   return writemessage (outcontrolsock, (char *)a, len*sizeof(int32_t), sendcontrolto);
}

void reply (int code, const char *str)
{
   int len;

   len = strlen (str);
   if ((-1 == bbftpd_msgwrite_len (code, len))
       || (-1 == writemessage (outcontrolsock, str, len, sendcontrolto)))
     {
        bbftpd_log(BBFTPD_ERR,"Error on reply") ;
        clean_child() ;
        exit(1) ;
    }
}

int bbftpd_input_pending (int fd, int timeout)
{
   int status;

   while (1)
     {
	fd_set selectmask ;
	struct timeval wait_timer;

	FD_ZERO(&selectmask) ;
	FD_SET(fd,&selectmask) ;
	if (timeout >= 0)
	  {
	     wait_timer.tv_sec = timeout ;
	     wait_timer.tv_usec = 0 ;
	     status = select (fd + 1, &selectmask, NULL, NULL, &wait_timer);
	  }
	else
	  status = select (fd + 1, &selectmask, NULL, NULL, NULL);

	if (status > 0) return 1;;
	if (status == 0)
	  return 0;
	if ((errno == EINTR) || (errno == EAGAIN))
	  continue;

	bbftpd_log (BBFTPD_ERR, "Error in select: %s", strerror(errno));
	return -1;
     }
}

/* Returns 1 if input available, 0 if not, or -1 upon error */
int bbftpd_msg_pending (int timeout)
{
   return bbftpd_input_pending (incontrolsock, timeout);
}

int bbftpd_fd_msgread_msg (int fd, struct message *msg)
{
   if (-1 == readmessage (fd, (char *)msg, MINMESSLEN, recvcontrolto))
     return -1;

   msg->msglen = ntohl (msg->msglen);
   return 0;
}

/* Returns:
 *   -1 if read failed
 *    0 upon success
 */
int bbftpd_msgread_msg (struct message *msg)
{
   return bbftpd_fd_msgread_msg (incontrolsock, msg);
}

int bbftpd_msgread_int32 (int32_t *val)
{
   if (-1 == readmessage (incontrolsock, (char *)val, 4, recvcontrolto))
     return -1;

   *val = ntohl (*val);
   return 0;
}

int bbftpd_msgread_int32_array (int32_t *a, int num)
{
   int i;

   if (-1 == readmessage (incontrolsock, (char *)a, num*sizeof(int32_t), recvcontrolto))
     return -1;

   for (i = 0; i < num; i++)
     a[i] = ntohl (a[i]);

   return 0;
}


int bbftpd_msgread_bytes (char *bytes, int num)
{
   return readmessage (incontrolsock, bytes, num, recvcontrolto);
}

void bbftpd_msg_reply (int code, const char *format, ...)
{
   char msg[4096];
   va_list ap;

   va_start(ap, format);
   (void) vsnprintf (msg, sizeof(msg), format, ap);
   va_end(ap);

   reply (code, msg);
}
