/*
 * bbftpd/bbftpd_socket.c
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

 
 bbftpd_socket.c    v 2.0.0 2000/12/19  - Routines creation
                    v 2.0.1 2001/04/23  - Correct indentation
                                        - Port to IRIX
               
*****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>

#include <netinet/tcp.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>

#include "_bbftpd.h"

/*******************************************************************************
** bbftpd_createreceivesocket :                                                       *
**                                                                             *
**      Routine to create a receive socket                                     *
**      This routine may send MSG_INFO                                         *
**                                                                             *
**      INPUT variable :                                                       *
**           portnumber   :  portnumber to connect    NOT MODIFIED             *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      RETURN:                                                                *
**          -1   Creation failed unrecoverable error                           *
**           0   OK, but retry                                                 *
**           >0  socket descriptor                              *
**                                                                             *
*******************************************************************************/
int bbftpd_createreceivesocket (int portnumber,char *msgbuf, size_t msgbuf_size)
{
   struct sockaddr_in data_source ;
   int    sock ;
   int    on = 1 ;
   int tcpwinsize ;
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
   int        addrlen ;
#else
   socklen_t        addrlen ;
#endif

   /*bbftpd_log(BBFTPD_INFO,"New socket to: %s:%d\n",inet_ntoa(his_addr.sin_addr), portnumber);*/

   if (-1 == (sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )))
     {
        bbftpd_log(BBFTPD_ERR, "Cannot create receive socket on port %d : %s",portnumber,strerror(errno));
        (void) snprintf(msgbuf, msgbuf_size, "Cannot create receive socket on port %d : %s",portnumber,strerror(errno)) ;
        return -1;
    }

   if (-1 == setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)))
     {
        close(sock) ;
        bbftpd_log(BBFTPD_ERR,"Cannot set SO_REUSEADDR on receive socket port %d : %s",portnumber,strerror(errno)) ;
        (void) snprintf (msgbuf, msgbuf_size, "Cannot set SO_REUSEADDR on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return -1;
    }

   tcpwinsize = 1024 * recvwinsize ;
   if (-1 == setsockopt(sock,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)))
     {
	/*
	 ** In this case we are going to log a message
	 */
	bbftpd_log(BBFTPD_ERR,"Cannot set SO_RCVBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
	/* fall through */
   }

   addrlen = sizeof(tcpwinsize) ;
   if (-1 == getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&tcpwinsize,&addrlen))
     {
	bbftpd_log(BBFTPD_ERR,"Cannot get SO_RCVBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        close(sock) ;
        (void) snprintf (msgbuf, msgbuf_size, "Cannot get SO_RCVBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return -1;
     }

   if ( tcpwinsize < 1024 * recvwinsize )
     {
        (void) snprintf (msgbuf, msgbuf_size, "Receive buffer on port %d cannot be set to %d Bytes, Value is %d Bytes",portnumber,1024*recvwinsize,tcpwinsize) ;
        reply(MSG_INFO,msgbuf) ;
     }

   tcpwinsize = 1024 * sendwinsize ;
   if (-1 == setsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)))
     {
        /*
        ** In this case we just log an error
        */
        bbftpd_log(BBFTPD_ERR,"Cannot set SO_SNDBUF  on receive socket port %d : %s",portnumber,strerror(errno)) ;
	/* fall through */
    }

   addrlen = sizeof(tcpwinsize) ;
   if (-1 == getsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,&addrlen))
     {
        bbftpd_log(BBFTPD_ERR,"Cannot get SO_SNDBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        close(sock) ;
        (void) snprintf (msgbuf, msgbuf_size, "Cannot get SO_SNDBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return -1;
    }
   if ( tcpwinsize < 1024 * sendwinsize )
     {
        (void) snprintf (msgbuf, msgbuf_size, "Send buffer on port %d cannot be set to %d Bytes, Value is %d Bytes",portnumber,1024*sendwinsize,tcpwinsize) ;
        reply(MSG_INFO,msgbuf) ;
	/* fall through */
    }

   if (-1 == setsockopt(sock,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)))
     {
        close(sock) ;
        bbftpd_log(BBFTPD_ERR,"Cannot set TCP_NODELAY on receive socket port %d : %s",portnumber,strerror(errno)) ;
        (void) snprintf (msgbuf, msgbuf_size, "Cannot set TCP_NODELAY on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return -1;
     }

   data_source.sin_family = AF_INET;
#ifdef SUNOS
   data_source.sin_addr = ctrl_addr.sin_addr;
#else
   data_source.sin_addr.s_addr = INADDR_ANY;
#endif

    if (fixeddataport == 1)
     data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);
   else
     data_source.sin_port = 0;

   if (-1 == bind(sock, (struct sockaddr *) &data_source,sizeof(data_source)))
     {
        close(sock) ;
        bbftpd_log(BBFTPD_ERR,"Cannot bind on receive socket port %d : %s",portnumber,strerror(errno)) ;
        (void) snprintf (msgbuf, msgbuf_size, "Cannot bind on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return -1;
     }

   data_source.sin_addr = his_addr.sin_addr;
   data_source.sin_port = htons(portnumber);
   addrlen = sizeof(data_source) ;

   while (-1 == connect(sock,(struct sockaddr *) &data_source,addrlen))
     {
	if (errno == EINTR)
	  continue;		       /* The original code returned 0 with a message for the client to retry */

	(void) snprintf (msgbuf, msgbuf_size, "Cannot connect receive socket port %s:%d : %s",inet_ntoa(his_addr.sin_addr),portnumber,strerror(errno)) ;
	bbftpd_log(BBFTPD_ERR,"Cannot connect receive socket port %s:%d : %s",inet_ntoa(his_addr.sin_addr),portnumber,strerror(errno)) ;
	close(sock) ;
	return -1;
     }

   bbftpd_log(BBFTPD_DEBUG,"Connected on port: %s:%d\n",inet_ntoa(his_addr.sin_addr),portnumber);
   return sock ;
}

static int init_data_socket (int sock, struct sockaddr_in *sck)
{
   struct  linger li ;
   int     tcpwinsize ;
   int     on = 1 ;
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
   int     addrlen ;
#else
   socklen_t        addrlen ;
#endif
   int     port ;
   int qbss_value = 0x20;

   if (-1 == setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)))
     {
	bbftpd_log(BBFTPD_ERR, "Cannot set SO_REUSEADDR on data socket : %s",strerror(errno));
	/*            (void) snprintf (msgbuf, msgbuf_size, "Cannot set SO_REUSEADDR on data socket : %s",strerror(errno)) ;*/
	return -1 ;
     }

   if (-1 == setsockopt(sock,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)))
     {
	bbftpd_log(BBFTPD_ERR, "Cannot set TCP_NODELAY on data socket : %s",strerror(errno));
	/*            (void) snprintf (msgbuf, msgbuf_size, "Cannot set TCP_NODELAY on data socket : %s",strerror(errno)) ;*/
	return -1 ;
     }

   if ((transferoption & TROPT_QBSS ) == TROPT_QBSS)
     {
	/* Setting the value for IP_TOS to be 0x20 */
	if (-1 == setsockopt(sock,IPPROTO_IP, IP_TOS, (char *)&qbss_value, sizeof(qbss_value)))
	  bbftpd_log(BBFTPD_WARNING, "Cannot set IP_TOS on data socket : %s",strerror(errno));
     }

   /*
    ** Send and receive are inversed because they are supposed
    ** to be the daemon char
    */
   tcpwinsize = 1024 * recvwinsize ;
   if (-1 == setsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)))
     {
	addrlen = sizeof(tcpwinsize) ;
	if (-1 == getsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char *)&tcpwinsize,&addrlen))
	  bbftpd_log(BBFTPD_WARNING, "Unable to get send buffer size %d: %s\n",tcpwinsize,strerror(errno));
	else
	  bbftpd_log(BBFTPD_WARNING, "Send buffer cannot be set to %d Bytes, Value is %d Bytes\n",1024*recvwinsize,tcpwinsize);
     }

   tcpwinsize = 1024 * sendwinsize ;
   if (-1 == setsockopt(sock,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)))
     {
	addrlen = sizeof(tcpwinsize) ;
	if (-1 == getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&tcpwinsize,&addrlen))
	  bbftpd_log(BBFTPD_WARNING, "Unable to get receive buffer size %d: %s\n",tcpwinsize,strerror(errno));
	else
	  bbftpd_log(BBFTPD_WARNING, "Receive buffer cannot be set to %d Bytes, Value is %d Bytes\n",1024*recvwinsize,tcpwinsize);
     }

   li.l_onoff = 1 ;
   li.l_linger = 1 ;
   if (-1 == setsockopt(sock,SOL_SOCKET,SO_LINGER,(char *)&li,sizeof(li)))
     {
	bbftpd_log(BBFTPD_ERR, "Cannot set SO_LINGER on data socket : %s",strerror(errno));
	/*            (void) snprintf (msgbuf, msgbuf_size, "Cannot set SO_LINGER on data socket : %s",strerror(errno)) ;*/
	return -1 ;
     }

   addrlen = sizeof(struct sockaddr_in);
   if (pasvport_min)
     {
	/* Try to bind within a range */
	for (port=pasvport_min; port<=pasvport_max; port++)
	  {
	     sck->sin_port = htons(port);
	     if (-1 != bind(sock, (struct sockaddr *) sck, addrlen))
	       break;
	  }
	if (port > pasvport_max)
	  {
	     bbftpd_log(BBFTPD_ERR, "Cannot bind on data socket in port range %d-%d: %s",
			pasvport_min, pasvport_max, strerror(errno));
	     return -1 ;
	  }
     }
   else
     {
	sck->sin_port = 0;
	if (-1 == bind(sock, (struct sockaddr *) sck, addrlen))
	  {
	     bbftpd_log(BBFTPD_ERR, "Cannot bind on data socket, port=%d: %s", sck->sin_port, strerror(errno));
	     return -1 ;
	  }
     }

   if (-1 == getsockname(sock,(struct sockaddr *)sck, &addrlen))
     {
	bbftpd_log(BBFTPD_ERR, "Cannot getsockname on data socket : %s",strerror(errno));
	/*(void) snprintf (msgbuf, msgbuf_size, "Cannot getsockname on data socket : %s",strerror(errno)) ;*/
	return -1 ;
     }

   if (-1 == listen(sock, 1))
     {
	bbftpd_log(BBFTPD_ERR, "Cannot listen on data socket : %s",strerror(errno));
	/*(void) snprintf (msgbuf, msgbuf_size, "Cannot listen on data socket : %s",strerror(errno)) ;*/
	return -1 ;
     }

   return sock;
}

/*******************************************************************************
** bbftpd_getdatasockk :                                                       *
**                                                                             *
**      Routine to create all the needed sockets to send data                  *
**                                                                             *
**      INPUT variable :                                                       *
**           nbsock   :  number of sockets to create    NOT MODIFIED           *
**                                                                             *
**      RETURN:                                                                *
**          -1   Creation failed unrecoverable error                           *
**           0   OK                                                            *
**                                                                             *
*******************************************************************************/
int bbftpd_getdatasock (int nbsock)
{
   struct  sockaddr_in sck ;
   int     i ;

   sck = ctrl_addr ;

   for (i=0 ; i<nbsock ; i++)
     {
	int sock;

	if (-1 == (sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP)))
	  bbftpd_log(BBFTPD_ERR, "Cannot create data socket : %s",strerror(errno));
	else if (-1 == init_data_socket (sock, &sck))
	  close (sock);
	else
	  {
	     mysockets[i] = sock;
	     myports[i] = ntohs (sck.sin_port);
	     bbftpd_log(BBFTPD_DEBUG,"Listen on port %d\n", myports[i]);
	     continue;
	  }

	/* Error */
	while (i != 0)
	  {
	     i--;
	     (void) close (mysockets[i]);
	  }
	return -1;
     }

   return 0 ;
}
