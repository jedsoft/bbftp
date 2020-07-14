/*
 * bbftpc/bbftp_mget.c
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

  
  
 bbftp_mget.c  v 2.0.0  2001/03/26  - Complete rewriting for version 2.0.0
               v 2.0.1  2001/04/19  - Correct indentation 


*****************************************************************************/
#include <bbftp.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

int bbftp_mget(char *remotefile,char *localdir, int  *errcode)
{
    char    logmessage[1024] ;
    int     retcode ;
    char    *filelist ;
    int     filelistlen ;
    char    *tmpfile, *tmpchar, *slash ;
    int     nbtry ;
    
    if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : mget %s %s\n",remotefile,localdir) ;
    /*
    ** First look at the local directory
    */
    if ( !strcmp(localdir,"./")) {
        /*
        ** Test if it is a local RFIO 
        */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
            printmessage(stderr,CASE_ERROR,26, "Incorrect command : local rfio and localdir equal ./ are imcompatible in mget\n") ;
            *errcode = 26 ;
            return  BB_RET_FT_NR ;
        }
#endif
    } else {
        /*
        ** Check if local dir is a directory
        */
        if ( (retcode = bbftp_retrcheckdir(localdir,logmessage,errcode)) < 0 ) {
            printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage) ;
            return BB_RET_FT_NR ;
        } else if ( retcode > 0 ) {
            printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage) ;
            return BB_RET_ERROR ;
        }
    }
    filelist = NULL ;
    filelistlen = 0 ;
    if ( (retcode = bbftp_list(remotefile,&filelist,&filelistlen,errcode) ) != BB_RET_OK) {
        if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< FAILED\n") ;
        return retcode ;
    }
    if ( filelistlen == 0 ) {
        if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
        return BB_RET_OK ;
    }
    tmpfile = filelist ;
    tmpchar = NULL ;
    while (filelistlen > 0 ) {
        /*
        ** The file list is under the form : 
        **      full remote file name \0
        **      two char \0
        **          first char is 'l' if file is a link blank if not
        **          second char is  'u' if link is on a unexisting file
        **                          'd' if it is a directory
        **                          'f' if it is a regular file
        */
        tmpchar = tmpfile + strlen(tmpfile) + 1 ;
        filelistlen = filelistlen - strlen(tmpfile) - 1 - strlen(tmpchar) - 1;
        if ( tmpchar[1] == 'u' ) {
            tmpfile = tmpchar + strlen(tmpchar) + 1 ;
        } else if ( tmpchar[1] == 'd') {
            tmpfile = tmpchar + strlen(tmpchar) + 1 ;
        } else {
            /*
            ** Find the last slash in remote file
            */
            slash = tmpfile + strlen(tmpfile) ;
            while ( *slash != '/' && slash != tmpfile ) slash-- ;
            if ( *slash == '/' ) slash++ ;
            for ( nbtry = 1 ; nbtry <= BBftp_Globaltrymax ; nbtry++ ) {
                /*
                ** malloc space for BBftp_Curfilename and BBftp_Realfilename
                */
                if ( (BBftp_Curfilename = (char *) malloc (strlen(localdir)+strlen(slash)+2) ) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(filelist) ;
                    return BB_RET_ERROR ;
                }
                if ( (BBftp_Realfilename = (char *) malloc (strlen(localdir)+strlen(slash)+30)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(BBftp_Curfilename) ;
                    free(filelist) ;
                    BBftp_Curfilename=NULL ;
                    return BB_RET_ERROR ;
                }
                sprintf(BBftp_Curfilename,"%s/%s",localdir,slash) ;
                if ( (BBftp_Transferoption & TROPT_TMP) == TROPT_TMP ) {
			char lhostname[10 + 1];
			if (gethostname(lhostname, sizeof(lhostname)) < 0) {
				lhostname[0] = '\0';
			} else {
				lhostname[sizeof(lhostname) - 1] = '\0';
			}
#ifdef CASTOR
                    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                        sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                    } else {
                        sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
                    }
#else                         
                    sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
#endif
                } else {
                    sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                }
                /*
                ** check if the last try was not disconnected
                */
                if ( BBftp_Connectionisbroken == 1 ) {
                    reconnecttoserver() ;
                    BBftp_Connectionisbroken = 0 ;
                }
                retcode = bbftp_get(tmpfile,errcode) ;
                BBftp_State = 0 ;
                if ( retcode == BB_RET_FT_NR ) {
                    /*
                    ** Fatal error no retry and connection is not broken
                    */
                    break ;
                } else if ( retcode == BB_RET_FT_NR_CONN_BROKEN ) {
                    /*
                    ** Fatal error no retry and connection is broken
                    */
                    BBftp_Connectionisbroken = 1 ;
                    break ;
                } else if ( retcode == BB_RET_OK ) {
                    /*
                    ** Success , no retry
                    */                    
                    break ;
                } else if ( retcode == BB_RET_CONN_BROKEN ) {
                    /*
                    ** Retry needed with a new connection
                    */
                    if ( nbtry != BBftp_Globaltrymax ) {
                        sleep(WAITRETRYTIME) ;
                        reconnecttoserver() ;
                    } else {
                        /*
                        ** In case of broken connection and max retry reached
                        ** indicate to the next transfer that the connection
                        ** is broken
                        */
                        BBftp_Connectionisbroken = 1 ;
                    }
                } else {
                    /*
                    ** retcode > 0 means retry on this transfer
                    */
                    if ( nbtry != BBftp_Globaltrymax ) {
                        sleep(WAITRETRYTIME) ;
                    }
                }
            }
            if ( retcode != BB_RET_OK && nbtry == BBftp_Globaltrymax+1) {
                BBftp_Myexitcode = *errcode ;
                if ( BBftp_Resfd < 0 ) {
                    if (!BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "get %s %s/%s FAILED\n",tmpfile,localdir,slash)        ;
                } else {
                    write(BBftp_Resfd,"get ",4) ;
                    write(BBftp_Resfd,tmpfile,strlen(tmpfile)) ;
                    write(BBftp_Resfd," ",1) ;
                    write(BBftp_Resfd,localdir,strlen(localdir)) ;                
                    write(BBftp_Resfd,"/",1) ;
                    write(BBftp_Resfd,slash,strlen(slash)) ;                
                    write(BBftp_Resfd," FAILED\n",8) ;
                }
            } else if ( retcode == BB_RET_OK) {        
                if ( BBftp_Resfd < 0 ) {
                    if (!BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "get %s %s/%s OK\n",tmpfile,localdir,slash)        ;
                } else {
                    write(BBftp_Resfd,"get ",4) ;
                    write(BBftp_Resfd,tmpfile,strlen(tmpfile)) ;
                    write(BBftp_Resfd," ",1) ;
                    write(BBftp_Resfd,localdir,strlen(localdir)) ;                
                    write(BBftp_Resfd,"/",1) ;
                    write(BBftp_Resfd,slash,strlen(slash)) ;                
                    write(BBftp_Resfd," OK\n",4) ;
                }    
            } else {
                BBftp_Myexitcode = *errcode ;
                if ( BBftp_Resfd < 0 ) {
                    if (!BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "get %s %s/%s FAILED\n",tmpfile,localdir,slash)        ;
                } else {
                    write(BBftp_Resfd,"get ",4) ;
                    write(BBftp_Resfd,tmpfile,strlen(tmpfile)) ;
                    write(BBftp_Resfd," ",1) ;
                    write(BBftp_Resfd,localdir,strlen(localdir)) ;                
                    write(BBftp_Resfd,"/",1) ;
                    write(BBftp_Resfd,slash,strlen(slash)) ;                
                    write(BBftp_Resfd," FAILED\n",8) ;
                }
            } 
            tmpfile = tmpchar + strlen(tmpchar) + 1 ;
        }
    }
    if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
    free(filelist) ;
    return BB_RET_OK ;
    
}
