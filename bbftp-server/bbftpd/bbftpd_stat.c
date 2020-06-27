/*
 * bbftpd/bbftpd_stat.c
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

  
 RETURN:
        0  Keep the connection open (does not mean that the file has been
           successfully created)
        -1 Tell the calling program to close the connection
  
 *****************************************************************************/


#include <bbftpd.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <utime.h>

#if HAVE_STRING_H
# include <string.h>
#endif

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
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>

extern int transferoption ;
extern	int	recvcontrolto ;

int bbftpd_stat(int sock,int msglen) 
{
    char    *buffer ;
    char    *logmessage ;
    struct  mess_dir *msg_file ;
    char    *filename ;
    int     recursif ;
    int     retcode ;

#ifdef STANDART_FILE_CALL
    struct stat statbuf;
    struct stat rfstatbuf;
#else
    struct stat64 statbuf ;
# ifdef WITH_RFIO64
    struct stat64 rfstatbuf ;
# else
    struct stat rfstatbuf;
# endif
#endif

    if ( (buffer = (char *) malloc (msglen+1) ) == NULL ) {
        syslog(BBFTPD_ERR,"Unable to malloc space for file name (%d)",msglen) ;
        reply(MSG_BAD,"Unable to malloc space for file name") ;
        return 0 ;
    }
    if ( (logmessage = (char *) malloc (msglen+1024) ) == NULL ) {
        syslog(BBFTPD_ERR,"Unable to malloc space for logmessage ") ;
        reply(MSG_BAD,"Unable to malloc space for logmessage") ;
        FREE(buffer) ;
        return 0 ;
    }
    /*
    ** Read the characteristics of the file
    */
    if ( readmessage(sock,buffer,msglen,recvcontrolto) < 0 ) {
        /*
        ** Error ...
        */
        syslog(BBFTPD_ERR,"Error reading file name") ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return -1 ;
    }
    buffer[msglen] = '\0' ;
    msg_file = (struct mess_dir *) buffer ;
    transferoption  = msg_file->transferoption ;
    filename = msg_file->dirname ;
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        sprintf(logmessage,"Unable to stat %s",filename) ;
# ifdef WITH_RFIO64
	retcode = rfio_stat64(filename,&rfstatbuf);
# else
	retcode = rfio_stat(filename,&rfstatbuf);
# endif
#else
        retcode = -1;
        sprintf(logmessage,"RFIO is not supported by the server") ;
#endif
    } else {
        sprintf(logmessage,"Unable to stat %s",filename) ;
#ifdef STANDART_FILE_CALL
        retcode = stat(filename,&statbuf);
#else
        retcode = stat64(filename,&statbuf);
#endif
    }	   
    if (retcode < 0) {
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    } else {
      char *at, *mt, *ct;
      if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
	at = strdup(ctime(&rfstatbuf.st_atime));
	at[strlen(at)-1]='\0';
	mt = strdup(ctime(&rfstatbuf.st_mtime));
	mt[strlen(mt)-1]='\0';
	ct = strdup(ctime(&rfstatbuf.st_ctime));
	ct[strlen(ct)-1]='\0';

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
# define RF_ST_BLKSIZE_FORMAT "d"
#else
# define RF_ST_BLKSIZE_FORMAT "s"
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
# define RF_ST_BLOCKS_FORMAT LONG_LONG_FORMAT
#else
# define RF_ST_BLOCKS_FORMAT "s"
#endif

        sprintf(logmessage,"%" LONG_LONG_FORMAT " %#08o %" LONG_LONG_FORMAT " %" RF_ST_BLKSIZE_FORMAT " %" RF_ST_BLOCKS_FORMAT "  %d %d (%s) (%s) (%s)",
			(long long)rfstatbuf.st_ino, 
			rfstatbuf.st_mode,
			(long long)rfstatbuf.st_size, 
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
			rfstatbuf.st_blksize, 
#else
			"N/A",
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
			(long long)rfstatbuf.st_blocks, 
#else
			"N/A",
#endif
			rfstatbuf.st_uid, 
			rfstatbuf.st_gid, 
			at, 
			mt, 
			ct) ;
      } else {
	at = strdup(ctime(&statbuf.st_atime));
	at[strlen(at)-1]='\0';
	mt = strdup(ctime(&statbuf.st_mtime));
	mt[strlen(mt)-1]='\0';
	ct = strdup(ctime(&statbuf.st_ctime));
	ct[strlen(ct)-1]='\0';

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
# define ST_BLKSIZE_FORMAT "d"
#else
# define ST_BLKSIZE_FORMAT "s"
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
# define ST_BLOCKS_FORMAT LONG_LONG_FORMAT
#else
# define ST_BLOCKS_FORMAT "s"
#endif

        sprintf(logmessage,"%" LONG_LONG_FORMAT " %#08o %" LONG_LONG_FORMAT " %" ST_BLKSIZE_FORMAT " %" ST_BLOCKS_FORMAT "  %d %d (%s) (%s) (%s)",
			(long long)statbuf.st_ino, 
			statbuf.st_mode, 
			(long long)statbuf.st_size, 
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
			statbuf.st_blksize, 
#else
			"N/A",
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
			(long long)statbuf.st_blocks, 
#else
			"N/A",
#endif
			statbuf.st_uid, 
			statbuf.st_gid, 
			at, 
			mt, 
			ct) ;
      }
      reply(MSG_OK,logmessage) ;
      FREE(buffer) ;
      FREE(logmessage) ;
      FREE(at) ;
      FREE(mt) ;
      FREE(ct) ;
      return 0 ;
    }
}
