/*
 * bbftpc/setsignals.c
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

  
  
 setsignals.c   v 2.0.0 2001/03/01  - Complete rewriting for version 2.0.0
                v 2.0.1 2001/04/19  - Correct indentation 


*****************************************************************************/
#include <bbftp.h>

#include <errno.h>

#include <signal.h>
#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

extern  int     timestamp ;
extern  int     usessh ;
extern  int     sshchildpid ;
extern  int     state ;
extern  char    *realfilename;

void bbftp_sigchld(int sig)
{
   (void) sig;
}
void bbftp_sigint(int sig) 
{
   (void) sig;
    /*
    ** Check if a transfer is occuring
    ** if yes kill the child, destroy the children, and erase the file 
    */
    if ( state == 1 ) {
        if ( realfilename != NULL ) bbftp_storeunlink(realfilename) ;
    }
    bbftp_clean_child() ;
    /*
    ** Check if cennected through ssh
    ** if yes kill the child and exit
    */
    if (usessh) {
        if ( sshchildpid != 0) {
             kill(sshchildpid,SIGKILL) ;
        }
    }
    printmessage(stderr,CASE_FATAL_ERROR,33,timestamp,"Killed by SIGINT\n") ;    
}
void bbftp_sigterm(int sig) 
{
   (void) sig;
    /*
    ** Check if a transfer is occuring
    ** if yes kill the child, destroy the children, and erase the file 
    */
    if ( state == 1 ) {
        if ( realfilename != NULL ) bbftp_storeunlink(realfilename) ;
    }
    bbftp_clean_child() ;
    /*
    ** Check if cennected through ssh
    ** if yes kill the child and exit
    */
    if (usessh) {
        if ( sshchildpid != 0) {
             kill(sshchildpid,SIGKILL) ;
        }
    }
    printmessage(stderr,CASE_FATAL_ERROR,34,timestamp,"Killed by SIGTERM\n") ;    
}


void blockallsignals() 
{
    struct    sigaction    sga ;

    sga.sa_handler= SIG_IGN ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGABRT,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGABRT : %s\n",strerror(errno)) ;
    }
    if ( sigaction(SIGALRM,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGALRM : s\n",strerror(errno)) ;
    }
    if ( sigaction(SIGHUP,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGHUP : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGINT,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGINT : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGPIPE,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGPIPE : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGQUIT,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGQUIT : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGTERM,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGTERM : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGUSR1,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGUSR1 : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGUSR2,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGUSR2 : %s \n",strerror(errno)) ;
    }
    if ( sigaction(SIGTSTP,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGTSTP : %s \n",strerror(errno)) ;
    }
#ifndef DARWIN
    if ( sigaction(SIGPOLL,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGPOLL : %s \n",strerror(errno)) ;
    }
#endif
#ifdef SIGPROF
    if ( sigaction(SIGPROF,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGPROF : %s \n",strerror(errno)) ;
    }
#endif
    if ( sigaction(SIGURG,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGURG : %s \n",strerror(errno)) ;
    }
#ifdef SIGVTALRM
    if ( sigaction(SIGVTALRM,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGVTALRM : %s \n",strerror(errno)) ;
    }
#endif
    sga.sa_handler= SIG_IGN ;
    sigemptyset(&(sga.sa_mask));
#ifdef HAVE_SA_NOCLDWAIT
    sga.sa_flags = SA_NOCLDSTOP|SA_NOCLDWAIT   ;
#else
    sga.sa_flags = SA_NOCLDSTOP   ;
#endif
    if ( sigaction(SIGCHLD,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGCHLD : %s \n",strerror(errno)) ;
    }
}

void bbftp_setsignals() 
{
    struct    sigaction    sga ;

    sga.sa_handler= bbftp_sigint  ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGINT,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGINT : %s \n",strerror(errno)) ;
    }
    sga.sa_handler= bbftp_sigterm ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGTERM,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGTERM : %s \n",strerror(errno)) ;
    }
}
void bbftp_setsignal_sigchld() 
{
    struct    sigaction    sga ;

    sga.sa_handler= bbftp_sigchld ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGCHLD,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGCHLD : %s \n",strerror(errno)) ;
    }
}
void bbftp_unsetsignal_sigchld() 
{
    struct    sigaction    sga ;

    sga.sa_handler= SIG_IGN ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGCHLD,&sga,0) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,32,timestamp,"Error setting signal SIGCHLD : %s \n",strerror(errno)) ;
    }
}
