/*
 * bbftpd/set_father_signals.c
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

  
  
 set_father_signals.c v 0.0.0  1999/11/24
                      v 1.9.4  2000/10/16   - Supress %m
                      v 2.0.1  2001/04/23   - Correct indentation
 
*****************************************************************************/
#include <errno.h>
#include <signal.h>
#include <syslog.h>

int set_father_signals() {
    struct    sigaction    sga ;

    sga.sa_handler= SIG_IGN ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGABRT,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGABRT : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGALRM,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGALRM : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGHUP,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGHUP : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGINT,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGINT : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGPIPE,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPIPE : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGQUIT,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGQUIT : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGTERM,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGTERM : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGUSR1,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGUSR1 : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGUSR2,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGUSR2 : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGCHLD,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGCHLD : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGTSTP,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGTSTP : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGPOLL,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPOLL : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGPROF,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPROF : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGURG,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGURG : %s",strerror(errno)) ;
        return(-1) ;
    }
    if ( sigaction(SIGVTALRM,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGVTALRM : %s",strerror(errno)) ;
        return(-1) ;
    }
    return 0 ;
}
