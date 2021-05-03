/*
 * bbftpd/bbftpd_check.c
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

  
 Routines : void checkfromwhere()
            int checkprotocol()
  
 bbftpd_check.c v 2.0.0  2000/12/18 - Creation of the routine.
                v 2.0.1  2001/04/23 - Correct indentation
                v 2.1.0  2001/06/01 - Change file name
                                    - Reorganise routines as in bbftp_
                                     
 *****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

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

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>
#include <version.h>

#include "_bbftpd.h"

static int get_ip_address (const char *host, in_addr_t *ip)
{
   char hostbuf[512];
   struct in_addr addr;
   struct hostent *h;
   char **h_addr_list;

   if (host == NULL)
     {
	if (-1 == gethostname (hostbuf, sizeof(hostbuf)))
	  {
	     bbftpd_log (BBFTPD_ERR, "gethostname failed: %s\n", strerror (errno));
	     return -1;
	  }
	host = hostbuf;
     }

   if (NULL == (h = gethostbyname (host)))
     {
	int save_herrno = h_errno;
	/* In case of dotted form */
	if (0 != inet_aton (host, &addr))   /* returns non-0 upon success */
	  {
	     *ip = addr.s_addr;
	     return 0;
	  }
	bbftpd_log (BBFTPD_ERR, "gethostbyname(%s) failed: h_errno=%d\n", host, save_herrno);
	return -1;
     }

   /* Take the first address */
   if ((h->h_addrtype != AF_INET) || (h->h_length != 4))
     {
	bbftpd_log (BBFTPD_ERR, "%s\n", "AF_INET6 addresses are unsupported");
	return -1;
     }
   /* Default to the first one */
   h_addr_list = h->h_addr_list;
   addr = *((struct in_addr *)h_addr_list[0]);
   /* Avoid the loopback address */
   while (*h_addr_list != NULL)
     {
	unsigned char *bytes = (unsigned char *)h_addr_list[0];
	/* The loopback address is "0x7F 0x?? 0x?? 0x??" in network byte order */
	if (bytes[0] == 0x7F)
	  {
	     h_addr_list++;
	     continue;
	  }
	addr = *(struct in_addr *)bytes;
	break;
     }

   *ip = addr.s_addr;
   return 0;
}

/* This function assumes network byte order for ip */
static char *ip_to_dotted (in_addr_t ip, char *buf, size_t buflen)
{
   unsigned char *s;

   s = (unsigned char *)&ip;
   snprintf (buf, buflen, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
   return buf;
}

static int exchange_addessses_with_client (void)
{
   struct message msg;
   in_addr_t local, remote;
   char local_dotted[16], remote_dotted[16];

   if (-1 == get_ip_address (NULL, &local))
     return -1;

   if (-1 == bbftpd_msgwrite_bytes (MSG_IPADDR, (char *)&local, 4))
     {
	bbftpd_log(BBFTPD_ERR,"%s", "Error writing MSG_IPADDR to client\n");
	reply (MSG_BAD, "Error writing MSG_IPADDR");
	return -1;
     }

   if (-1 == bbftpd_msgread_msg (&msg))
     {
	bbftpd_log(BBFTPD_ERR, "Time out or error waiting for MSG_IPADDR");
	reply(MSG_BAD, "Failed to get MSG_IPADDR message");
	return -1;
     }

   if (msg.code != MSG_IPADDR_OK)
     {
	bbftpd_log (BBFTPD_ERR, "Expected MSG_IPADDR_OK message.  Got %d", msg.code);
	reply(MSG_BAD, "Expected to see MSG_IPADDR_OK with ipaddress");
	return -1;
     }
   if (msg.msglen != 4)
     {
	bbftpd_log (BBFTPD_ERR, "Expected a 32bit IP address to follow MSG_IPADDR_OK");
	reply(MSG_BAD, "Expected a 32bit IP address to follow MSG_IPADDR_OK");
	return -1;
     }

   if (-1 == bbftpd_msgread_bytes ((char *)&remote, 4))
     {
	bbftpd_log (BBFTPD_ERR, "Failed to read 32 bit IP address\n");
	reply (MSG_BAD, "Failed to read 32 bit IP address");
	return -1;
     }

   his_addr.sin_addr.s_addr = remote;
   (void) ip_to_dotted (remote, remote_dotted, sizeof(remote_dotted));
   (void) ip_to_dotted (local, local_dotted, sizeof(local_dotted));
   bbftpd_log (BBFTPD_INFO, "Server IP: %s, Client IP: %s\n", local_dotted, remote_dotted);

   return 0;
}

/*******************************************************************************
** checkfromwhere :                                                            *
**                                                                             *
**      This routine get a socket on a port, send a MSG_LOGGED_STDIN and       *
**      wait for a connection on that port.                                    *
**                                                                             *
**      RETURN:                                                                *
**          No return but the routine exit in case of error.                   *
**
** This function gets used when bbftpd gets started via an ssh connection, and *
** not running as a daemon.
*******************************************************************************/

static int try_bind (int port_min, int port_max,
		     int sockfd, struct sockaddr_in *serv)
{
   int port;

   for (port = port_min; port <= port_max; port++)
     {
	int ret;

        serv->sin_port = htons (port);
	while (-1 == (ret = bind (sockfd, (struct sockaddr *) serv, sizeof(*serv))))
	  {
	     if (errno == EINTR)
	       continue;

	     break;
	  }
	if (ret == 0) return 0;
     }

   bbftpd_log(BBFTPD_ERR, "Cannot bind on data socket : %s", strerror(errno));
   reply(MSG_BAD,"Cannot bind checkreceive socket") ;
   bbftpd_log(BBFTPD_INFO,"User %s disconnected", currentusername) ;
   return -1;
}


int checkfromwhere (int ask_remote, int ctrlport)
{
   socklen_t addrlen;
   struct sockaddr_in server ;
   struct message msg ;
   struct linger li ;
   int sock, ns, retcode;
   int on = 1 ;
   int port_min, port_max;

   if (ask_remote)
     {
	if (-1 == exchange_addessses_with_client ())
	  return -1;

	ctrl_addr.sin_port = htons(ctrlport) ;
	return 0;
     }

   if (1 == (sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )))
     {
        bbftpd_log(BBFTPD_ERR, "Cannot create checkreceive socket : %s",strerror(errno));
        reply(MSG_BAD,"Cannot create checkreceive socket") ;
	return -1;
    }

   if (-1 == setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)))
     {
        bbftpd_log(BBFTPD_ERR,"Cannot set SO_REUSEADDR on checkreceive socket : %s\n",strerror(errno)) ;
        reply(MSG_BAD,"Cannot set SO_REUSEADDR on checkreceive socket") ;
        close(sock) ;
        return -1;
     }

    li.l_onoff = 1 ;
    li.l_linger = 1 ;
    if (-1 == setsockopt(sock,SOL_SOCKET,SO_LINGER,(char *)&li,sizeof(li)))
     {
        bbftpd_log(BBFTPD_ERR,"Cannot set SO_LINGER on checkreceive socket : %s\n",strerror(errno)) ;
        reply(MSG_BAD,"Cannot set SO_LINGER on checkreceive socket") ;
	exit(1) ;
     }

   server.sin_family = AF_INET;
   server.sin_addr.s_addr = INADDR_ANY;
   port_min = port_max = ctrlport + 1;
   if (fixeddataport == 0 && pasvport_min)
     {
	port_min = pasvport_min;
	port_max = pasvport_max;
     }

   if (-1 == try_bind (port_min, port_max, sock, &server))
     {
	close(sock) ;
	return -1;
     }

   addrlen = sizeof(server);
   if (-1 == getsockname(sock,(struct sockaddr *)&server, &addrlen))
     {
        bbftpd_log(BBFTPD_ERR,"Error getsockname checkreceive socket : %s",strerror(errno)) ;
        reply(MSG_BAD,"Cannot getsockname checkreceive socket") ;
        close(sock) ;
        return -1;
    }

   if (-1 == listen(sock,1))
     {
        bbftpd_log (BBFTPD_ERR,"Error listening checkreceive socket : %s",strerror(errno)) ;
        reply (MSG_BAD,"Error listening checkreceive socket") ;
        close(sock) ;
	return -1;
    }
    bbftpd_log(BBFTPD_INFO, "listen port : %d",ntohs(server.sin_port));

    /*
    ** Send the MSG_LOGGED_STDIN message
    */
   if (-1 == bbftpd_msgwrite_int32 (MSG_LOGGED_STDIN, ntohs(server.sin_port)))
     {
        bbftpd_log(BBFTPD_ERR,"Error writing checkreceive socket port");
        reply(MSG_BAD,"Error writing checkreceive socket port") ;
        close(sock) ;
    }

   retcode = bbftpd_input_pending (sock, checkstdinto);
   if (retcode <= 0)
     {
	close (sock);
	if (retcode == -1)
	  {
	     bbftpd_log(BBFTPD_ERR,"Error select checkreceive socket : %s ",strerror(errno)) ;
	     reply(MSG_BAD,"Error select checkreceive socket") ;
	  }
	else
	  {
	     bbftpd_log(BBFTPD_ERR,"Time out select checkreceive socket ") ;
	     reply(MSG_BAD,"Time Out select checkreceive socket") ;
	  }
        return -1;
    }

   if (-1 == (ns = accept(sock,0,0)))
     {
        bbftpd_log(BBFTPD_ERR,"Error accept checkreceive socket ") ;
        reply(MSG_BAD,"Error accept checkreceive socket") ;
        close(sock) ;
        return -1;
     }
    close(sock) ;

    addrlen = sizeof(his_addr);
    if (-1 == getpeername(ns, (struct sockaddr *) &his_addr, &addrlen))
     {
        bbftpd_log(BBFTPD_ERR, "getpeername : %s",strerror(errno));
        reply(MSG_BAD,"getpeername error") ;
        close(ns);
	return -1;
    }

    addrlen = sizeof(ctrl_addr);
    if (-1 == getsockname(ns, (struct sockaddr *) &ctrl_addr, &addrlen))
     {
        bbftpd_log(BBFTPD_ERR, "getsockname : %s",strerror(errno));
        reply(MSG_BAD,"getsockname error") ;
        close(ns) ;
	return -1;
    }

    /*
    ** Now read the message
    */
   if (-1 == bbftpd_fd_msgread_msg (ns, &msg))
     {
        bbftpd_log(BBFTPD_ERR,"Error reading checkreceive socket") ;
        reply(MSG_BAD,"Error reading checkreceive socket") ;
        close(ns) ;
	return -1;
     }
    if (msg.code != MSG_IPADDR)
     {
        bbftpd_log(BBFTPD_ERR,"Receive unkown message on checkreceive socket") ;
        reply(MSG_BAD,"Receive unkown message on checkreceive socket") ;
        close(ns) ;
	return -1;
     }

   /*
    ** Everything seems OK so send a MSG_IPADDR_OK on the
    ** control socket and close the connection
    */
   if (-1 == bbftpd_fd_msgwrite_len (ns, MSG_IPADDR_OK, 0))
     {
        bbftpd_log(BBFTPD_ERR,"Error writing checkreceive socket OK message") ;
        reply(MSG_BAD,"Error writing checkreceive socket OK message") ;
        close(ns) ;
	return -1;
     }
   /*
    ** set the port of ctrl_addr structure to the control port
    */
   ctrl_addr.sin_port = htons(ctrlport) ;
   /*
    ** Wait a while before closing
    */
   close(ns) ;
   return 0;
}


/*******************************************************************************
** checkprotocol :                                                             *
**                                                                             *
**      This routine send it protocol version and wait for the client prorocol *
**      version.                                                               *
**                                                                             *
**      RETURN:                                                                *
**           0 OK                                                              *
**          -1 calling program exit                                            *
*******************************************************************************/

int checkprotocol (int *versionp)
{
   struct  message msg ;

   if (-1 == bbftpd_msgwrite_int32_2 (MSG_PROT_ANS, protocolmin, protocolmax))
     {
        bbftpd_log(BBFTPD_ERR,"Error writing MSG_PROT_ANS") ;
        return -1 ;
     }

   if (-1 == bbftpd_msgread_msg (&msg))
     {
        bbftpd_log(BBFTPD_ERR,"Error waiting MSG_PROT_ANS") ;
        return -1 ;
    }

   if (msg.code == MSG_PROT_ANS)
     {
        if (msg.msglen == 4)
	  {
	     int32_t p;
	     if (-1 == bbftpd_msgread_int32 (&p))
	       {
		  bbftpd_log(BBFTPD_ERR,"Error waiting MSG_PROT_ANS (protocol version)") ;
		  return -1 ;
	       }
	     *versionp = p;
	     return 0 ;
	  }
	bbftpd_log(BBFTPD_ERR,"Unexpected length while MSG_PROT_ANS %d", msg.msglen) ;
	return -1 ;
     }

   if (msg.code == MSG_BAD_NO_RETRY )
     {
	bbftpd_log(BBFTPD_ERR,"Incompatible server and client") ;
	*versionp = -1;
	return -1;
     }

   bbftpd_log(BBFTPD_ERR,"Unexpected message while MSG_PROT_ANS %d", msg.code) ;
   return -1 ;
}
