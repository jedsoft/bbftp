/*
 * bbftpd/bbftpd_statfs.c
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
#ifdef DARWIN
#define HAVE_STRUCT_STATFS 1
#endif

#include <stdio.h>
#include <syslog.h>
#include <malloc.h>
#include <utime.h>
#include <errno.h>
#ifdef HAVE_SYS_STATVFS_H
# include <sys/types.h>
# include <sys/statvfs.h>
#elif defined(HAVE_SYS_VFS_H)
# include <sys/vfs.h>
#endif
#ifdef DARWIN
#include <sys/mount.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif
#include <netinet/in.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>

extern int transferoption ;
extern	int	recvcontrolto ;

int bbftpd_statfs(int sock,int msglen) 
{
#if defined(HAVE_STATVFS64) || defined(HAVE_STATVFS) || defined(HAVE_STATFS)
    char    *buffer ;
    char    *logmessage ;
    struct  mess_dir *msg_file ;
    char    *dirname ;
    int     recursif ;
    int     retcode ;

# ifdef HAVE_STRUCT_STATVFS64
    struct statvfs64 statfsbuf;
# elif defined(HAVE_STRUCT_STATVFS)
    struct statvfs statfsbuf;
# elif defined(HAVE_STRUCT_STATFS)
    struct statfs statfsbuf;
# endif
#ifdef _SX
    int statlen, fstyp=0;
# endif

    if ( (buffer = (char *) malloc (msglen+1) ) == NULL ) {
        syslog(BBFTPD_ERR,"Unable to malloc space for dir name (%d)",msglen) ;
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
        syslog(BBFTPD_ERR,"Error reading dir name") ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return -1 ;
    }
    buffer[msglen] = '\0' ;
    msg_file = (struct mess_dir *) buffer ;
    transferoption  = msg_file->transferoption ;
    dirname = msg_file->dirname ;
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        sprintf(logmessage,"df is not supported by RFIO") ;
#else
        sprintf(logmessage,"RFIO is not supported by the server") ;
#endif
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    }

#ifdef HAVE_STATVFS64
    if (statvfs64(dirname,&statfsbuf) < 0) {
#elif defined(HAVE_STATVFS)
    if (statvfs(dirname,&statfsbuf) < 0) {
#elif _SX
    if (statfs(dirname,&statfsbuf, statlen, fstyp) < 0) {
#else
    if (statfs(dirname,&statfsbuf) < 0) {
#endif
        sprintf(logmessage,"Unable to df %s: %s",dirname,strerror(errno)) ;
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    } else {
#ifdef HAVE_STATVFS64
        sprintf(logmessage,"%" LONG_LONG_FORMAT " %" LONG_LONG_FORMAT " %lu %" LONG_LONG_FORMAT " %" LONG_LONG_FORMAT " %lu", statfsbuf.f_blocks, statfsbuf.f_bavail, statfsbuf.f_frsize, statfsbuf.f_files, statfsbuf.f_ffree, statfsbuf.f_namemax) ;
#elif defined(HAVE_STATVFS)
        sprintf(logmessage,"%ld %ld %lu %ld %ld %lu", statfsbuf.f_blocks, statfsbuf.f_bavail, statfsbuf.f_bsize, statfsbuf.f_files, statfsbuf.f_ffree, statfsbuf.f_namemax) ;
#elif _SX
        sprintf(logmessage,"%ld %ld %ld %ld %ld %ld", statfsbuf.f_blocks, statfsbuf.f_bfree, statfsbuf.f_bsize, statfsbuf.f_files, statfsbuf.f_ffree, 0) ;
#else
#ifdef DARWIN
		sprintf(logmessage,"%ld %ld %ld %ld", statfsbuf.f_blocks, statfsbuf.f_bsize, statfsbuf.f_files, statfsbuf.f_ffree) ;
#else
        sprintf(logmessage,"%ld %ld %ld %ld %ld %ld", statfsbuf.f_blocks, statfsbuf.f_b_avail, statfsbuf.f_bsize, statfsbuf.f_files, statfsbuf.f_ffree, statfsbuf.f_namelen) ;
#endif
#endif
        reply(MSG_OK,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    }
#else
    reply(MSG_BAD_NO_RETRY,"operation not supported by the server") ;
    return 0 ;
#endif
}
