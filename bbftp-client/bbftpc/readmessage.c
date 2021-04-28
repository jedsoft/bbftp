/*
 * bbftpc/readmessage.c
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
 *
 readmessage.c v 0.0.0  1999/11/24
 v 1.6.0  2000/03/24  - Cosmetic...
 v 1.7.1  2000/04/04  - Portage to IRIX (IRIX64)
 v 1.8.7  2000/05/24  - Set the socket in non block mode
 v 1.8.9  2000/08/08  - Add timestamp
 v 1.8.10 2000/08/10  - Portage to Linux
 v 1.9.0  2000/08/24  - Use configure to help port
 v 2.0.0  2001/03/08  - Change recv for read and add debug
 v 2.0.1  2001/04/19  - Correct indentation
 *
 *****************************************************************************/
#include <bbftp.h>

#include <syslog.h>
#include <errno.h>
#include <stdio.h>
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
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>

static int try_select (int fd, int is_read, int to)
{
   struct timeval wait_timer, *wait_timerp;
   fd_set *read_mask = NULL, *write_mask = NULL;
   fd_set mask;

   FD_ZERO(&mask) ;
   FD_SET(fd, &mask);

   if (is_read)
     read_mask = &mask;
   else
     write_mask = &mask;

   wait_timerp = &wait_timer;
   if (to < 0) wait_timerp = NULL;

   while (1)
     {
	int status;

	if (to > 0)
	  {
	     wait_timer.tv_sec  = to ;
	     wait_timer.tv_usec = 0 ;
	  }

	if ((-1 == (status = select(fd+1, read_mask, write_mask, NULL, wait_timerp)))
	    && ((errno == EINTR) || (errno == EAGAIN)))
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
	     if (BBftp_Debug )
	       printmessage(stderr,CASE_NORMAL,0, "Read message : Select error : MSG (%d,%d)\n",msglen,nbget) ;
	     return -1;
	  }
	if ( retcode == 0 )
	  {
	     if (BBftp_Debug )
	       printmessage(stderr,CASE_NORMAL,0, "Read message : Time out : MSG (%d,%d)\n", msglen,nbget) ;
	     return -1 ;
	  }

	while (-1 == (retcode = read(sock, &buffer[nbget], msglen-nbget)))
	  {
	     if (errno == EINTR)
	       continue;
	     if (BBftp_Debug )
	       printmessage(stderr,CASE_NORMAL,0, "Read message : Receive error : MSG (%d,%d) %s\n",msglen,nbget,strerror(errno)) ;
	     return -1 ;
	  }

	if ( retcode == 0 )
	  {
	     if (BBftp_Debug )
	       printmessage(stderr,CASE_NORMAL,0, "Read message : Connection breaks : MSG (%d,%d)\n",msglen,nbget) ;
	     return -1 ;
	  }

	nbget = nbget + retcode ;
     }

   return 0;
}


int discardmessage (int sock, int msglen, int timeout)
{
   int nread;
   char buffer[256];

   nread = 0;
   while (nread < msglen)
     {
	int dn = msglen - nread;

	if (dn > 256) dn = 256;
	if (-1 == readmessage (sock, buffer, dn, timeout))
	  {
	     if (BBftp_Debug)
	       printmessage(stderr,CASE_NORMAL,0, "Discard message :Discard message : Receive error : %d/%d bytes read\n", nread, msglen);
	     return -1;
	  }
	nread += dn;
     }
   return 0;
}

/* This is a debugging function that is called prior to closing the sock.
 * It just writes out everything it reads regardless of the content.
 * It always returns -1.
 */
int discardandprintmessage(int sock,int timeout)
{
   while (1)
     {
	char buffer[32] ;
	int retcode;

	if (-1 == try_select (sock, 1, timeout))
	  return -1;

	while (-1 == (retcode = read(sock, buffer,sizeof(buffer)-1)))
	  {
	     if (errno == EINTR) continue;
	     return -1 ;
	  }

	if ( retcode == 0 ) return -1;

	buffer[retcode] = '\0' ;
	printmessage(stdout,CASE_NORMAL,0, "%s",buffer) ;
     }
}

int bbftp_input_pending (int fd, int timeout)
{
   int status = try_select (fd, 1, timeout);

   if (status == -1)
     {
	if (BBftp_Debug)
	  printmessage(stderr,CASE_NORMAL,0, "bbftp_input_pending: select failure: %s\n", strerror(errno));
	return -1;
     }
   return (status != 0);
}

/* Returns 1 if input available, 0 if not, or -1 upon error */
int bbftp_msg_pending (int timeout)
{
   return bbftp_input_pending (BBftp_Incontrolsock, timeout);
}

int bbftp_fd_msgread_msg (int fd, struct message *msg)
{
   if (-1 == readmessage (fd, (char *)msg, MINMESSLEN, BBftp_Recvcontrolto))
     return -1;

   msg->msglen = ntohl (msg->msglen);
   return 0;
}

/* Returns:
 *   -1 if read failed
 *    0 upon success
 */
int bbftp_msgread_msg (struct message *msg)
{
   return bbftp_fd_msgread_msg (BBftp_Incontrolsock, msg);
}

int bbftp_msgread_int32 (int32_t *val)
{
   if (-1 == readmessage (BBftp_Incontrolsock, (char *)val, 4, BBftp_Recvcontrolto))
     return -1;

   *val = ntohl (*val);
   return 0;
}

int bbftp_msgread_bytes (char *bytes, int num)
{
   return readmessage (BBftp_Incontrolsock, bytes, num, BBftp_Recvcontrolto);
}

int bbftp_msg_discard (struct message *msg)
{
   char buf[256];
   int len = 0;

   while (len < msg->msglen)
     {
	int dlen = msg->msglen - len;
	if (dlen > 255)		       /* 1 less than sizeof(buf) */
	  dlen = 255;

	if (-1 == readmessage (BBftp_Recvcontrolto, buf, dlen, BBftp_Recvcontrolto))
	  return -1;
	if (BBftp_Debug )
	  {
	     buf[dlen] = 0;
	     printmessage(stderr,CASE_NORMAL,0, "%s", buf);
	  }
	len += dlen;
     }
   printmessage(stderr,CASE_NORMAL,0, "%s", "\n");
   return 0;
}
