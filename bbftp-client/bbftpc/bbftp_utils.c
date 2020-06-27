/*
 * bbftpc/bbftp_utils.c
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

  
  
 bbftp_utils.c v 2.0.0 2001/03/01   - Creation of the routines
               v 2.0.1 2001/04/19   - Correct indentation
               v 2.1.0 2001/05/29   - Add private authentication
                                    - Add -m option
			   v 2.2.0  2001/10/03  - Add certificate authentication

*****************************************************************************/

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bbftp.h>
#include <common.h>
#include <client.h>
#include <client_proto.h>
#include <netinet/in.h>
#include <structures.h>
#include <config.h>
#include <time.h>
#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

extern  int     incontrolsock ;
extern  int     outcontrolsock ;
extern  int     sshchildpid  ;
extern  char    *curfilename ;
extern  char    *realfilename ;
extern  int     *myports ;
extern  int     *mysockets ;
extern  char    *readbuffer ;
extern  char    *compbuffer ;
extern  int     *mychildren ;
extern  int     debug ;
extern  int     requestedstreamnumber ;

#if 0
static my64_t convertlong(my64_t v) {
    struct bb {
        int    fb ;
        int sb ;
    } ;
    struct bb *bbpt ;
    int     tmp ;
    my64_t    tmp64 ;

    tmp64 = v ;
    bbpt = (struct bb *) &tmp64 ;
    tmp = bbpt->fb ;
    bbpt->fb = ntohl(bbpt->sb) ;
    bbpt->sb = ntohl(tmp) ;
    return tmp64 ;
}
#endif

#ifndef HAVE_NTOHLL
my64_t ntohll(my64_t v) {
#ifdef HAVE_BYTESWAP_H
    return bswap_64(v);
#else
    long lo = v & 0xffffffff;
    long hi = v >> 32U;
    lo = ntohl(lo);
    hi = ntohl(hi);
    return ((my64_t) lo) << 32U | hi;
#endif
}
#define htonll ntohll
#endif

void printmessage(FILE *strm , int flag, int errcode, int tok, char *fmt, ...) 
{
    va_list ap;
    time_t  logtime ;
    char    clogtime[50] ;
    
    va_start(ap,fmt);
    
    /*
    ** If timestamp start to print the time
    */
    if (tok) {
        /*
        ** Get time
        */
        if ( time(&logtime) == -1 ) {
            strcpy(clogtime,"") ;
        } else {
            if ( strftime(clogtime,sizeof(clogtime),"%a %b %d %H:%M:%S (%Z)",localtime(&logtime)) == 0 )    {
                strcpy(clogtime,"") ;
            }
        }
        /*
        ** And print it
        */
        if ( strlen(clogtime) > 0 ) {
            (void) fprintf(strm,"%s : ",clogtime) ;
        }
    }
    /*
    ** Check if it is an error
    */
    if ( flag == CASE_ERROR || flag == CASE_FATAL_ERROR ) {
        /*
        ** It is an error
        */
        (void) fprintf(strm,"BBFTP-ERROR-%05d : ",errcode) ;
    } else if ( flag == CASE_WARNING ) {
        /*
        ** It is a warning
        */
        (void) fprintf(strm,"BBFTP-WARNING-%05d : ",errcode) ;
    }
    /*
    ** And print the requested string 
    */
    (void) vfprintf(strm, fmt, ap);
    fflush(strm) ;
    if ( flag == CASE_FATAL_ERROR ) {
        /*
        ** exit in case of error
        */
        exit(errcode) ;
    }
    va_end(ap);
}

void Usage() 
{
    printf("Usage bbftp -i inputfile | -e \"command line\" \n") ;
#ifdef CERTIFICATE_AUTH
    printf("           [-g service name]\n") ;
    printf("           [-u username]\n") ;
#else
    printf("            -u username\n") ;
#endif
    printf("           [-b (background)]\n") ;
#ifdef WITH_GZIP    
    printf("           [-c (gzip compress)]\n") ;
#endif    
    printf("           [-D[min:max] (Domain of Ephemeral Ports) ]\n") ;
    printf("           [-f errorfile]\n") ;
#ifndef PRIVATE_AUTH
    printf("           [-E server command for ssh]\n") ;
    printf("           [-I ssh identity file]\n") ;
    printf("           [-L ssh command]\n") ;
    printf("           [-s (use ssh)]\n") ;
    printf("           [-S (use ssh in batch mode)]\n") ;
#else
    printf("           [-P Private authentication string]\n") ;
#endif
/*#endif*/
    printf("           [-m (special output for statistics)]\n") ;
    printf("           [-n (simulation mode: no data written)]\n") ;
    printf("           [-o outputfile]\n") ;
    printf("           [-p number of // streams]\n") ;
    printf("           [-q (for QBSS on)]\n") ;
    printf("           [-r number of tries ]\n") ;
    printf("           [-R .bbftprc filename]\n") ;
    printf("           [-t (timestamp)]\n") ;
    printf("           [-V (verbose)]\n") ;
    printf("           [-w controlport]\n") ;
    printf("           [-W (print warning to stderr) ]\n") ;
    printf("           host\n") ;
    printf("      or \n") ;
    printf("      bbftp -v\n") ;
}

/*
** Taken from Free Software Foundation
*/
void strip_trailing_slashes (char *path)
{
  int last;

  last = strlen (path) - 1;
  while (last > 0 && path[last] == '/')
    path[last--] = '\0';
}

void bbftp_close_control() 
{

    close(incontrolsock) ;
    if ( incontrolsock != outcontrolsock) close(outcontrolsock) ;
    if ( sshchildpid != 0 ) {
        kill(sshchildpid,SIGKILL) ;
    }
}
void bbftp_free_all_var() 
{

    if ( myports != NULL ) {
        free(myports) ;
        myports = NULL ;
    }
     if ( mychildren != NULL ) {
        free(mychildren) ;
        mychildren = NULL ;
    }
   if ( realfilename != NULL ) {
        free(realfilename) ;
        realfilename = NULL ;
    }
    if ( curfilename != NULL ) {
        free(curfilename) ;
        curfilename = NULL ;
    }
    if ( mysockets != NULL ) {
        free(mysockets) ;
        mysockets = NULL ;
    }
    if ( readbuffer != NULL ) {
        free(readbuffer) ;
        readbuffer = NULL ;
    }
    if ( compbuffer != NULL ) {
        free(compbuffer) ;
        compbuffer = NULL ;
    }
}
void bbftp_clean_child()
{
    int     *pidfree ;
    int     i ;
    
    if (mychildren != NULL ) {
        pidfree = mychildren ;
        for ( i=0 ; i<requestedstreamnumber ; i++) {
            if ( *pidfree != 0 ) {
                if (debug) printf("Killing child %d\n",*pidfree) ; 
                kill(*pidfree,SIGKILL) ;
            }
            *pidfree++ = 0 ;
        }
    }
}
