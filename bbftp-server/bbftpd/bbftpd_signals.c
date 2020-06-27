/*
 * bbftpd/bbftpd_signals.c
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

  
  
 bbftpd_signals.c   v 2.0.0 2000/12/14  - Replace files set_signals.c and
                                          set_father_signals.c
                    v 2.0.1 2001/04/23  - Correct indentation
                    v 2.0.2 2001/05/04  - Correct include for RFIO
                                        - Changes for CASTOR
                    v 2.1.0 2001/06/01  - Change routines name
                                        - Correct syslog level

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
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>


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
extern int  castfd ;
extern struct  timeval  tstart;
extern my64_t  filesize ;
extern  char            currentusername[MAXLEN] ;

int bbftpd_checkendchild(int status)
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

void bbftpd_sigchld(int sig) 
{
    int *pidfree ;
    int    pid ;
    int    status ;
    int    totpid ;
    int i ;
    char logmessage[1024] ;
    int    retcode ;
    struct utimbuf ftime ;
    int goon ;
    /*
    ** We are going to check if the process generating this
    ** signal was not already detected as dead. This was causing 
    ** a send of a MSG_OK if all process have been dectected dead
    ** througn the waitpid routine and the signal was pending.
    ** If all processes have been detected dead before childendinerror 
    ** to 0, totpid also so we send a reply MSS_OK
    */
    totpid = 0 ;
    pidfree = mychildren ;
    for ( i=0 ; i< nbpidchild ; i++) {
        totpid = totpid + *pidfree ;
        pidfree++ ;
    }
    if ( totpid == 0 ) {
        /*
        ** All Ended
        */
        free_all_var() ;
        return ;
    }
/*
** Check the status
*/
    pidfree = mychildren ;
    for ( i=0 ; i< nbpidchild; i++) {
        if ( *pidfree != 0 ) {
            pid = waitpid(*pidfree,&status,WNOHANG) ;
            if ( pid == *pidfree ) {
                /*
                ** the process has ended
                */
                if ( (retcode = bbftpd_checkendchild(status)) != 0 ) {
                    if ( childendinerror == 0 ) {
                        childendinerror = 1 ;
                        syslog(BBFTPD_ERR,"Child pid %d ends in error status %d",pid,retcode) ;
                        if ( (unlinkfile == 1) || (unlinkfile == 2) ) {
                            bbftpd_storeunlink(realfilename) ;
                        }
                        if (retcode == 255) {
                            sprintf(logmessage,"Disk quota excedeed or No Space left on device") ;
                            reply(MSG_BAD_NO_RETRY,logmessage) ;
                        } else {
                            if (retcode ==  ENOEXEC ) {
                                sprintf(logmessage,"Interrupted by signal") ;
                            } else {
                                sprintf(logmessage,"Error on server: %s",strerror(retcode)) ;
                            }
                            reply(MSG_BAD,logmessage) ;
                        }
                    } else {
                        syslog(BBFTPD_ERR,"Child pid %d ends in error",pid) ;
                    }
                }
                *pidfree = 0 ;
            }
        }
        pidfree++ ;
    }
    totpid = 0 ;
    pidfree = mychildren ;
    for ( i=0 ; i< nbpidchild ; i++) {
        totpid = totpid + *pidfree ;
        pidfree++ ;
    }
    if ( totpid == 0 && childendinerror == 0 ) {
        if ((unlinkfile == 1 || unlinkfile == 2 || unlinkfile == 4)) {
            goon = 0 ;
            if ( bbftpd_storeclosecastfile(realfilename,logmessage) < 0 ) {
                syslog(BBFTPD_ERR,logmessage) ;
                bbftpd_storeunlink(realfilename) ;
                reply(MSG_BAD,logmessage) ;
                goon = 1 ;
            }
            if ((goon == 0 ) &&  ((transferoption & TROPT_ACC ) == TROPT_ACC) ) {
                sscanf(lastaccess,"%08x",&ftime.actime) ;
                sscanf(lastmodif,"%08x",&ftime.modtime) ;
                if ( bbftpd_storeutime(realfilename,&ftime,logmessage) < 0 ) {
                    syslog(BBFTPD_ERR,logmessage) ;
                    bbftpd_storeunlink(realfilename) ;
                    reply(MSG_BAD,logmessage) ;
                    goon = 1 ;
                }
           }
           if ( (goon == 0 ) && ((transferoption & TROPT_MODE ) == TROPT_MODE) ) {
                if ( bbftpd_storechmod(realfilename,filemode,logmessage) < 0 ) {
                    syslog(BBFTPD_ERR,logmessage) ;
                    bbftpd_storeunlink(realfilename) ;
                    reply(MSG_BAD,logmessage) ;
                    goon = 1 ;
                }
            }
            if ( (goon == 0 ) && ((transferoption & TROPT_TMP ) == TROPT_TMP ) ) {
                if ( bbftpd_storerename(realfilename,curfilename,logmessage) < 0 ) {
                    syslog(BBFTPD_ERR,logmessage) ;
                    bbftpd_storeunlink(realfilename) ;
                    reply(MSG_BAD,logmessage) ;
                    goon = 1 ;
                }
            }
            state = S_LOGGED ;
            if ( goon == 0 ) reply(MSG_OK,"OK") ;
            if (unlinkfile == 4) {
                bbftpd_storeunlink(curfilename) ;
            } else if  (&tstart) {
	        /* Stats PUT user file bytes_transfered seconds kbytes/s Mbits/s*/
       	        float s, bs;
       	        struct  timeval tend ,tdiff;
       	        (void) gettimeofday(&tend, (struct timezone *)0);
       	        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec ;
       	        tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
       	        if (tdiff.tv_usec < 0) tdiff.tv_sec--, tdiff.tv_usec += 1000000;
       	        s = tdiff.tv_sec + (tdiff.tv_usec / 1000000.);
 #define nz(x)   ((x) == 0 ? 1 : (x))
                bs = filesize / nz(s);
                sprintf(logmessage,"PUT %s %s %" LONG_LONG_FORMAT" %.3g %.3g %.3g", currentusername, curfilename, filesize, s, bs / 1024.0,(8.0*bs) / (1024.0 * 1024.0));
                syslog(BBFTPD_NOTICE,logmessage);
            }
            free_all_var() ;
        } else {
            state = S_LOGGED ;
            reply(MSG_OK,"OK") ;
            if  (&tstart) {
	        /* Stats GET user file bytes_transfered seconds kbytes/s Mbits/s*/
       	        float s, bs;
       	        struct  timeval tend ,tdiff;
       	        (void) gettimeofday(&tend, (struct timezone *)0);
       	        tdiff.tv_sec = tend.tv_sec - tstart.tv_sec ;
       	        tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
       	        if (tdiff.tv_usec < 0) tdiff.tv_sec--, tdiff.tv_usec += 1000000;
       	        s = tdiff.tv_sec + (tdiff.tv_usec / 1000000.);
                bs = filesize / nz(s);
                sprintf(logmessage,"GET %s %s %" LONG_LONG_FORMAT" %.3g %.3g %.3g", currentusername, curfilename, filesize, s, bs / 1024.0,(8.0*bs) / (1024.0 * 1024.0));
                syslog(BBFTPD_NOTICE,logmessage);
            }
            free_all_var() ;
        }
    } 
    if ( childendinerror == 1 ) {
        clean_child() ;
    }
    if (totpid == 0 ) {
        state = S_LOGGED ;
        unlinkfile = 0 ;
        killdone = 0 ;
        childendinerror = 0 ;
        free_all_var() ;
    }
}

void bbftpd_sighup( int sig) 
{
    flagsighup = 1 ;
}

void bbftpd_sigterm(int sig) 
{
    if ( fatherpid == getpid() ) {
        /*
        ** We are in father
        */
        /*
        ** Unlink the file if we are getting a file
        */ 
        if ( unlinkfile == 1 ) unlink(realfilename) ;
        clean_child() ;
        exit(0) ; /* BUG porting */
    } else {
        exit(EINTR) ;
    }
}

int bbftpd_setsignals() 
{
    struct    sigaction    sga ;
    
    sga.sa_handler = SIG_IGN ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0   ;
    if ( sigaction(SIGPIPE,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPIPE : %s",strerror(errno)) ;
        return(-1) ;
    }
    sga.sa_handler = bbftpd_sigchld ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0  ;
    if ( sigaction(SIGCHLD,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGCHLD : %s",strerror(errno)) ;
        return(-1) ;
    }
    sga.sa_handler = bbftpd_sighup ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0  ;
    if ( sigaction(SIGHUP,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGHUP : %s",strerror(errno)) ;
        return(-1) ;
    }
    sga.sa_handler = bbftpd_sigterm ;
    sigemptyset(&(sga.sa_mask));
    sga.sa_flags = 0  ;
    if ( sigaction(SIGTERM,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGTERM : %s",strerror(errno)) ;
        return(-1) ;
    }
    return 0 ;
}
int bbftpd_blockallsignals() {
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
/*
**    if ( sigaction(SIGTERM,&sga,0) < 0 ) {
**        syslog(BBFTPD_ERR,"Error sigaction SIGTERM : %s",strerror(errno)) ;
**        return(-1) ;
**    }
*/
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
#ifndef DARWIN
    if ( sigaction(SIGPOLL,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPOLL : %s",strerror(errno)) ;
        return(-1) ;
    }
#endif
#ifdef SIGPROF		/* BUG porting */
    if ( sigaction(SIGPROF,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGPROF : %s",strerror(errno)) ;
        return(-1) ;
    }
#endif
    if ( sigaction(SIGURG,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGURG : %s",strerror(errno)) ;
        return(-1) ;
    }
#ifdef SIGVTALRM	/* BUG porting */
    if ( sigaction(SIGVTALRM,&sga,0) < 0 ) {
        syslog(BBFTPD_ERR,"Error sigaction SIGVTALRM : %s",strerror(errno)) ;
        return(-1) ;
    }
#endif
    return 0 ;
}
