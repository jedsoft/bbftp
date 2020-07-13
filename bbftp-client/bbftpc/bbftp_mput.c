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

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

int bbftp_mput(char *localfile, char *lremotedir, int  *errcode)
{
    char    logmessage[1024] ;
    char    *filelist ;
    int     filelistlen ;
    int     retcode ;
    int     nbtry ;
    char    *remotefilename = NULL ;
    char    *tmpfile, *tmpchar, *slash ;

    if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,">> COMMAND : mput %s %s\n",localfile,lremotedir) ;

    if ( (retcode = bbftp_retrlistdir(localfile,&filelist,&filelistlen,logmessage,errcode) ) < 0 ) {
        printmessage(stderr,CASE_ERROR,*errcode,BBftp_Timestamp,"%s\n",logmessage) ;
        return BB_RET_FT_NR ;
    }
    if ( filelistlen == 0 ) {
        if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"<< OK\n") ;
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
            for ( nbtry = 1 ; nbtry <= BBftp_Globaltrymax ; nbtry++ ) {
                /*
                ** malloc space for remote file name
                */
                if ( (remotefilename = (char *) malloc (strlen(lremotedir)+strlen(slash)+2)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,BBftp_Timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(filelist) ;
                    return BB_RET_ERROR ;
                }
                sprintf(remotefilename,"%s/%s",lremotedir,slash) ;
                
                /*
                ** malloc space for BBftp_Curfilename and BBftp_Realfilename
                */
                if ( (BBftp_Curfilename = (char *) malloc (strlen(tmpfile)+1)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,BBftp_Timestamp,"Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(remotefilename) ;
                    free(filelist) ;
                    return BB_RET_ERROR ;
                }
                if ( (BBftp_Realfilename = (char *) malloc (strlen(tmpfile)+1)) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,BBftp_Timestamp,"Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                    *errcode = 35 ;
                    free(remotefilename) ;
                    free(BBftp_Curfilename) ;
                    free(filelist) ;
                    BBftp_Curfilename=NULL ;
                    return BB_RET_ERROR ;
                }
                sprintf(BBftp_Curfilename,"%s",tmpfile) ;
                sprintf(BBftp_Realfilename,"%s",tmpfile) ;
                /*
                ** check if the last try was not disconnected
                */
                if ( BBftp_Connectionisbroken == 1 ) {
                    reconnecttoserver() ;
                    BBftp_Connectionisbroken = 0 ;
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
                Bbftp_Myexitcode = *errcode ;
                if ( BBftp_Resfd < 0 ) {
                    if (!BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"put %s %s/%s FAILED\n",tmpfile,lremotedir,slash)        ;
                } else {
                    write(BBftp_Resfd,"put ",4) ;
                    write(BBftp_Resfd,tmpfile,strlen(tmpfile)) ;
                    write(BBftp_Resfd," ",1) ;
                    write(BBftp_Resfd,lremotedir,strlen(lremotedir)) ;                
                    write(BBftp_Resfd,"/",1) ;
                    write(BBftp_Resfd,slash,strlen(slash)) ;                
                    write(BBftp_Resfd," FAILED\n",8) ;
                }
            } else if ( retcode == BB_RET_OK) {        
                if ( BBftp_Resfd < 0 ) {
                    if (!BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"put %s %s/%s OK\n",tmpfile,lremotedir,slash)        ;
                } else {
                    write(BBftp_Resfd,"put ",4) ;
                    write(BBftp_Resfd,tmpfile,strlen(tmpfile)) ;
                    write(BBftp_Resfd," ",1) ;
                    write(BBftp_Resfd,lremotedir,strlen(lremotedir)) ;                
                    write(BBftp_Resfd,"/",1) ;
                    write(BBftp_Resfd,slash,strlen(slash)) ;                
                    write(BBftp_Resfd," OK\n",4) ;
                }    
            } else {
                Bbftp_Myexitcode = *errcode ;
                if ( BBftp_Resfd < 0 ) {
                    if (!BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"put %s %s/%s FAILED\n",tmpfile,lremotedir,slash)        ;
                } else {
                    write(BBftp_Resfd,"put ",4) ;
                    write(BBftp_Resfd,tmpfile,strlen(tmpfile)) ;
                    write(BBftp_Resfd," ",1) ;
                    write(BBftp_Resfd,lremotedir,strlen(lremotedir)) ;                
                    write(BBftp_Resfd,"/",1) ;
                    write(BBftp_Resfd,slash,strlen(slash)) ;                
                    write(BBftp_Resfd," FAILED\n",8) ;
                }
            } 
            tmpfile = tmpchar + strlen(tmpchar) + 1 ;
        }
    }
    if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"<< OK\n") ;
    free(filelist) ;
    return BB_RET_OK ;
}
