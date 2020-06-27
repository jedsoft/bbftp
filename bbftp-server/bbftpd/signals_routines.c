/*
 * bbftpd/signals_routines.c
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

  
  
 signals_routines.c v 2.0.0 2000/12/14  - Replace files set_signals.c and
                                          set_father_signals.c
                    v 2.0.1 2001/04/23  - Correct indentation
                    v 2.0.2 2001/05/04  - Correct include for RFIO
 
*****************************************************************************/
#include <bbftpd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>

/*#ifdef RFIO
**#include <shift.h>
**#endif
*/
/*
** For V1 Protocol
*/
extern int  pid_child[MAXPORT] ;
extern char currentfilename[MAXLENFILE];
/*
** For V1 and V2 Protocol
*/
extern int  flagsighup;
extern int  childendinerror ;
extern int  state ;
extern int  killdone ;
extern int  unlinkfile ;
extern pid_t    fatherpid ;
/*
** For V2 protocol
*/
extern char *realfilename ;
extern char *curfilename ;
extern int  transferoption ;
extern int  *mychildren ;
extern int  nbpidchild ;
extern char lastaccess[9] ;
extern char lastmodif[9] ;
extern int  filemode ;

int checkendchild(status)
    int status ;
{
    
    if (WEXITSTATUS(status) == 0 ) {
        if ( WIFSIGNALED(status) == 0 ) {
            return(0) ;
        } else {
            /*
            ** In order to differenciate signal and ernno we choose
            ** to return ENOEXEC in case of signal
            */
            return(ENOEXEC) ;
        }
    } else {
        return(WEXITSTATUS(status)) ;
    }             
}

void in_sigchld_v1(sig) 
int        sig ;
{
    int    pid ;
    int    status ;
    int    totpid ;
    int i ;
    char logmessage[256] ;
    int    retcode ;
    
    /*
    ** We are going to check if the process generating this
    ** signal was not already detected as dead. This was causing 
    ** a send of a MSG_OK if all process have been dectected dead
    ** througn the waitpid routine and the signal was pending.
    ** If all processes have been detected dead before childendinerror 
    ** to 0, totpid also so we send a reply MSS_OK
    */
    totpid = 0 ;
    for ( i=0 ; i< MAXPORT; i++) {
        totpid = totpid + pid_child[i] ;
    }
    if ( totpid == 0 ) {
        /*
        ** All Ended
        */
        return ;
    }
    totpid = 0 ;
/*
** Check the status
*/
    for ( i=0 ; i< MAXPORT; i++) {
        if ( pid_child[i] != 0 ) {
            pid = waitpid(pid_child[i],&status,WNOHANG) ;
            if ( pid == pid_child[i] ) {
                /*
                ** the process has ended
                */
                if ( (retcode = checkendchild(status)) != 0 ) {
                    if ( childendinerror == 0 ) {
                        childendinerror = 1 ;
                        syslog(BBFTPD_ERR,"Child pid %d ends in error status %d",pid,retcode) ;
                        if ( unlinkfile == 1 ) unlink(currentfilename) ;
                        if (retcode == 255) {
                            sprintf(logmessage,"Disk quota excedeed or No Space left on device") ;
                            reply(MSG_BAD_NO_RETRY,logmessage) ;
                        } else {
                            if (retcode ==  ENOEXEC ) {
                                sprintf(logmessage,"Interrupted by signal") ;
                            } else {
                                sprintf(logmessage,"%s",strerror(retcode)) ;
                            }
                            reply(MSG_BAD,logmessage) ;
                        }
                    } else {
                        syslog(BBFTPD_ERR,"Child pid %d ends in error",pid) ;
                    }
                }
                pid_child[i] = 0 ;
            }    
        }
    }
    for ( i=0 ; i< MAXPORT; i++) {
        totpid = totpid + pid_child[i] ;
    }
    if ( totpid == 0 && childendinerror == 0 ) {
        state = S_LOGGED ;
        reply(MSG_OK,"OK") ;
    } 
    if ( childendinerror == 1 ) {
        clean_child() ;
    }
    if (totpid == 0 ) {
        state = S_LOGGED ;
        unlinkfile = 0 ;
        killdone = 0 ;
        childendinerror = 0 ;
    }
}

void in_sighup_v1(sig) 
int        sig ;
{
    flagsighup = 1 ;
}


void in_sigterm_v1(sig) 
int        sig ;
{
    if ( fatherpid == getpid() ) {
        /*
        ** We are in father
        */
        /*
        ** Unlink the file if we are getting a file
        */ 
        if ( unlinkfile == 1 ) unlink(currentfilename) ;
        clean_child() ;
        exit(0) ; 	/* BUG porting */
    } else {
        exit(EINTR) ;
    }
}


int set_signals_v1() {
    struct    sigaction    sga ;
    
    sga.sa_handler = SIG_IGN ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGPIPE,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPIPE : %s",strerror(errno)) ;
        return(-1) ;
    }
    sga.sa_handler = in_sigchld_v1 ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0  ;
    if ( sigaction(SIGCHLD,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGCHLD : %s",strerror(errno)) ;
        return(-1) ;
    }
    sga.sa_handler = in_sighup_v1 ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0  ;
    if ( sigaction(SIGHUP,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGHUP : %s",strerror(errno)) ;
        return(-1) ;
    }
    sga.sa_handler = in_sigterm_v1 ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0  ;
    if ( sigaction(SIGTERM,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGTERM : %s",strerror(errno)) ;
        return(-1) ;
    }
    return 0 ;
}

