/*
 * bbftpc/writemessage.c
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

  
  
 writemessage.c v 1.6.0  2000/03/24     - Creation of the routine
                v 1.7.1  2000/04/04     - Portage to IRIX (IRIX64)
                v 1.8.7  2000/05/24     - Set the socket in non block mode
                v 1.8.9  2000/08/08     - Add timestamp
                v 1.8.10 2000/08/10     - Portage to Linux
                v 1.9.0  2000/08/24     - Use configure to help port
                v 2.0.0  2001/03/08     - Change send for write and add debug
                v 2.0.1  2001/04/17     - Correct indentation 

*****************************************************************************/
#include <bbftp.h>

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

#include <client.h>
#include <client_proto.h>

extern int  timestamp ;
extern int  debug ;

int writemessage(int sock,char *buffer,int msglen,int to,int fromchild) 
{
    int     retcode ;
    char    ent[40] ;
    int     nfds ;
    fd_set  selectmask ; /* Select mask */
    struct timeval    wait_timer;

    int     msgsize ;
    int     msgsend ;
    int     nbsent ;

    if ( fromchild == 1 ) {
        sprintf(ent,"Child %06d : ",getpid()) ;
    } else {
        strcpy(ent,"") ;
    }
    nbsent = 0 ;
    msgsize = msglen ;
/*
** start the writing loop
*/
    while ( nbsent < msgsize ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(FD_SETSIZE,0,&selectmask,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            if (debug ) 
                printmessage(stderr,CASE_NORMAL,0,timestamp,"%sWrite message : Select error : MSG (%d,%d)\n",ent,msglen,nbsent) ;
            return -1 ;
        } else if ( retcode == 0 ) {
            if (debug ) 
                printmessage(stderr,CASE_NORMAL,0,timestamp,"%sWrite message : Time Out : MSG (%d,%d)\n",ent,msglen,nbsent) ;
            return -1 ;
        } else {
            msgsend = write(sock,&buffer[nbsent],msgsize-nbsent) ;
            if ( msgsend < 0 ) {
                if (debug ) 
                    printmessage(stderr,CASE_NORMAL,0,timestamp,"%sWrite message : Send error %d(%s) : MSG (%d,%d)\n",ent,errno,strerror(errno),msglen,nbsent) ;
                return -1 ;
            } else if (msgsend  == 0 ) {
                if (debug ) 
                    printmessage(stderr,CASE_NORMAL,0,timestamp,"%sWrite message : Connection breaks : MSG (%d,%d)\n",ent,msglen,nbsent) ;
                return -1 ;
            } else {
                nbsent = nbsent + msgsend ;
            }
        }
    }
    return(0) ;
}
