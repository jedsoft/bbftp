/*
 * bbftpc/bbftp_mput.c
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

  
  
 bbftp_mput.c  v 2.0.0  2001/03/26  - Complete rewriting for version 2.0.0
               v 2.0.1  2001/04/19  - Correct indentation 


*****************************************************************************/
#include <bbftp.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

extern  int     verbose ;
extern  int     timestamp ;
extern  char    *curfilename ;
extern  char    *realfilename;
extern  int     globaltrymax;
extern  int     myexitcode;
extern  int     connectionisbroken ;
extern  int     resfd ;

int bbftp_mput(char *localfile,char *remotedir, int  *errcode)
{
    char    logmessage[1024] ;
    char    *filelist ;
    int     filelistlen ;
    int     retcode ;
    int     nbtry ;
    char    *remotefilename = NULL ;
    char    *tmpfile, *tmpchar, *slash ;
    
    
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : mput %s %s\n",localfile,remotedir) ;

    if ( (retcode = bbftp_retrlistdir(localfile,&filelist,&filelistlen,logmessage,errcode) ) < 0 ) {
        printmessage(stderr,CASE_ERROR,*errcode,timestamp,"%s\n",logmessage) ;
        return BB_RET_FT_NR ;
    }
    if ( filelistlen == 0 ) {
        if (verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
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
            ** Find the last slash in local file
            */
            slash = tmpfile + strlen(tmpfile) ;
            while ( *slash != '/' && slash != tmpfile ) slash-- ;
            if ( *slash == '/' ) slash++ ;
            for ( nbtry = 1 ; nbtry <= globaltrymax ; nbtry++ ) {
                /*
                ** malloc space for remote file name
                */
                if ( (remotefilename = (char *) malloc (strlen(remotedir)+strlen(slash)+2)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(filelist) ;
                    return BB_RET_ERROR ;
                }
                sprintf(remotefilename,"%s/%s",remotedir,slash) ;
                
                /*
                ** malloc space for curfilename and realfilename
                */
                if ( (curfilename = (char *) malloc (strlen(tmpfile)+1)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(remotefilename) ;
                    free(filelist) ;
                    return BB_RET_ERROR ;
                }
                if ( (realfilename = (char *) malloc (strlen(tmpfile)+1)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(remotefilename) ;
                    free(curfilename) ;
                    free(filelist) ;
                    curfilename=NULL ;
                    return BB_RET_ERROR ;
                }
                sprintf(curfilename,"%s",tmpfile) ;
                sprintf(realfilename,"%s",tmpfile) ;
                /*
                ** check if the last try was not disconnected
                */
                if ( connectionisbroken == 1 ) {
                    reconnecttoserver() ;
                    connectionisbroken = 0 ;
                }
                retcode = bbftp_put(remotefilename,errcode) ;
                free(remotefilename) ;
                if ( retcode == BB_RET_FT_NR ) {
                    /*
                    ** Fatal error no retry and connection is not broken
                    */
                    break ;
                } else if ( retcode == BB_RET_FT_NR_CONN_BROKEN ) {
                    /*
                    ** Fatal error no retry and connection is broken
                    */
                    connectionisbroken = 1 ;
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
                    if ( nbtry != globaltrymax ) {
                        sleep(WAITRETRYTIME) ;
                        reconnecttoserver() ;
                    } else {
                        /*
                        ** In case of broken connection and max retry reached
                        ** indicate to the next transfer that the connection
                        ** is broken
                        */
                        connectionisbroken = 1 ;
                    }
                } else {
                    /*
                    ** retcode > 0 means retry on this transfer
                    */
                    if ( nbtry != globaltrymax ) {
                        sleep(WAITRETRYTIME) ;
                    }
                }
            }
            if ( retcode != BB_RET_OK && nbtry == globaltrymax+1) {
                myexitcode = *errcode ;
                if ( resfd < 0 ) {
                    if (!verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"put %s %s/%s FAILED\n",tmpfile,remotedir,slash)        ;
                } else {
                    write(resfd,"put ",4) ;
                    write(resfd,tmpfile,strlen(tmpfile)) ;
                    write(resfd," ",1) ;
                    write(resfd,remotedir,strlen(remotedir)) ;                
                    write(resfd,"/",1) ;
                    write(resfd,slash,strlen(slash)) ;                
                    write(resfd," FAILED\n",8) ;
                }
            } else if ( retcode == BB_RET_OK) {        
                if ( resfd < 0 ) {
                    if (!verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"put %s %s/%s OK\n",tmpfile,remotedir,slash)        ;
                } else {
                    write(resfd,"put ",4) ;
                    write(resfd,tmpfile,strlen(tmpfile)) ;
                    write(resfd," ",1) ;
                    write(resfd,remotedir,strlen(remotedir)) ;                
                    write(resfd,"/",1) ;
                    write(resfd,slash,strlen(slash)) ;                
                    write(resfd," OK\n",4) ;
                }    
            } else {
                myexitcode = *errcode ;
                if ( resfd < 0 ) {
                    if (!verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"put %s %s/%s FAILED\n",tmpfile,remotedir,slash)        ;
                } else {
                    write(resfd,"put ",4) ;
                    write(resfd,tmpfile,strlen(tmpfile)) ;
                    write(resfd," ",1) ;
                    write(resfd,remotedir,strlen(remotedir)) ;                
                    write(resfd,"/",1) ;
                    write(resfd,slash,strlen(slash)) ;                
                    write(resfd," FAILED\n",8) ;
                }
            } 
            tmpfile = tmpchar + strlen(tmpchar) + 1 ;
        }
    }
    if (verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
    free(filelist) ;
    return BB_RET_OK ;
}
