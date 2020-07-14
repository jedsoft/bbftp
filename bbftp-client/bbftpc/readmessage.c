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

  
  
 readmessage.c v 0.0.0  1999/11/24
               v 1.6.0  2000/03/24  - Cosmetic...
               v 1.7.1  2000/04/04  - Portage to IRIX (IRIX64)
               v 1.8.7  2000/05/24  - Set the socket in non block mode
               v 1.8.9  2000/08/08  - Add timestamp
               v 1.8.10 2000/08/10  - Portage to Linux
               v 1.9.0  2000/08/24  - Use configure to help port
               v 2.0.0  2001/03/08  - Change recv for read and add debug
               v 2.0.1  2001/04/19  - Correct indentation 

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

int readmessage(int sock,char *buffer,int msglen,int to,int fromchild) 
{
    int     retcode ;
    char    ent[40] ;
    int     nfds ;
    fd_set  selectmask ; /* Select mask */
    struct timeval    wait_timer;
    int     msgsize ;
    int     msgget ;
    int     nbget ;


    if ( fromchild == 1 ) {
        sprintf(ent,"Child %06d : ",getpid()) ;
    } else {
        strcpy(ent,"") ;
    }
    nbget = 0 ;
    msgsize = msglen ;
/*
** start the reading loop
*/
    while ( nbget < msgsize ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
       (void) nfds;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            if (BBftp_Debug ) 
                printmessage(stderr,CASE_NORMAL,0, "%sRead message : Select error : MSG (%d,%d)\n",ent,msglen,nbget) ;
            return -1 ;
        } else if ( retcode == 0 ) {
            if (BBftp_Debug ) 
                printmessage(stderr,CASE_NORMAL,0, "%sRead message : Time out : MSG (%d,%d)",ent,msglen,nbget) ;
            return -1 ;
        } else {
            msgget = read(sock,&buffer[nbget],msgsize-nbget) ;
            if ( msgget < 0 ) {
                if (BBftp_Debug ) 
                    printmessage(stderr,CASE_NORMAL,0, "%sRead message : Receive error : MSG (%d,%d) %s",ent,msglen,nbget,strerror(errno)) ;
                return -1 ;
            } else if ( msgget == 0 ) {
                if (BBftp_Debug ) 
                    printmessage(stderr,CASE_NORMAL,0, "%sRead message : Connection breaks : MSG (%d,%d)\n",ent,msglen,nbget) ;
                return -1 ;
            } else {
                nbget = nbget + msgget ;
            }
        }
    }
    return(0) ;
}
int discardmessage(int sock,int msglen,int to,int fromchild) 
{
    int    nbget ;
    int    retcode ;
    int    nfds ;
    fd_set selectmask ; /* Select mask */
    struct timeval    wait_timer;
    char    buffer[256] ;
    char    ent[40] ;

    if ( fromchild == 1 ) {
        sprintf(ent,"Child %06d : ",getpid()) ;
    } else {
        strcpy(ent,"") ;
    }
    
    /*
    ** We are going to read buflen by buflen till the message 
    ** is exhausted
    */
    
    nbget = 0 ;
    while ( nbget < msglen ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
       (void) nfds;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            if (BBftp_Debug ) 
                printmessage(stderr,CASE_NORMAL,0, "%sDiscard message :Discard message : Select error : MSG (%d,%d)",ent,msglen,nbget) ;
            return -1 ;
        } else if ( retcode == 0 ) {
            if (BBftp_Debug ) 
                printmessage(stderr,CASE_NORMAL,0, "%sDiscard message :Discard message : Time out : MSG (%d,%d)",ent,msglen,nbget) ;
            return -1 ;
        } else {
            retcode = read(sock,buffer,sizeof(buffer)) ;
            if ( retcode < 0 ) {
                if (BBftp_Debug ) 
                    printmessage(stderr,CASE_NORMAL,0, "%sDiscard message :Discard message : Receive error : MSG (%d,%d) : %s",ent,msglen,nbget,strerror(errno)) ;
                return -1 ;
            } else if ( retcode == 0 ) {
                if (BBftp_Debug ) 
                    printmessage(stderr,CASE_NORMAL,0, "%sDiscard message :Discard message : Connexion breaks",ent) ;
                return -1 ;
            } else {
                nbget = nbget + retcode ;
            }
        }
    }
    return(0) ;
}
int discardandprintmessage(int sock,int to,int fromchild) 
{
    int    retcode ;
    int    nfds ;
    fd_set selectmask ; /* Select mask */
    struct timeval    wait_timer;
    char    buffer[2] ;
    char    ent[40] ;

    if ( fromchild == 1 ) {
        sprintf(ent,"Child %06d : ",getpid()) ;
    } else {
        strcpy(ent,"") ;
    }
    
    /*
    ** We are going to read buflen by buflen till the message 
    ** is exhausted
    */
    
    while ( 1 == 1 ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
       (void) nfds;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            return -1 ;
        } else if ( retcode == 0 ) {
            return -1 ;
        } else {
            retcode = read(sock,buffer,sizeof(buffer)) ;
            if ( retcode < 0 ) {
                return -1 ;
            } else if ( retcode == 0 ) {
                return -1 ;
            } else {
                buffer[retcode] = '\0' ;
                printmessage(stdout,CASE_NORMAL,0, "%s",buffer) ;
            }
        }
    }
}
