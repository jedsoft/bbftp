/*
 * bbftpc/bbftp_dir.c
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

int bbftp_dir(char *remotefile, int  *errcode)
{
    /* char    logmessage[1024] ; */
    int     retcode ;
    char    *filelist ;
    int     filelistlen ;
    char    *tmpfile, *tmpchar, *tmpremotefile ;
    
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : dir %s \n",remotefile) ;
    filelist = NULL ;
    filelistlen = 0 ;
    if ( (tmpremotefile = (char *) malloc (strlen(remotefile)+3)) == NULL ) {
	return -1;
    }
    strcpy(tmpremotefile, remotefile);
    strcat(tmpremotefile, "/*");
    if ( (retcode = bbftp_list(tmpremotefile,&filelist,&filelistlen,errcode) ) != BB_RET_OK) {
        if (verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< FAILED\n") ;
        return retcode ;
    }
    if ( statoutput ) {
        printmessage(stdout,CASE_NORMAL,0,0,"dir %s\n", remotefile) ;
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
        if ( statoutput ) {
            printmessage(stdout,CASE_NORMAL,0,0,"%s %s\n", tmpchar, tmpfile) ;
        }
        tmpfile = tmpchar + strlen(tmpchar) + 1 ;
    }
    if (verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
    free(filelist) ;
    return BB_RET_OK ;
    
}
