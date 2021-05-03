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
/* #include <syslog.h> */
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

#include "_bbftpd.h"

#if defined(HAVE_STATVFS64) && !defined(HAVE_STRUCT_STATVFS64)
/* Handle inconsistency.  All systems that I know use statvfs64 struct for statvfs64 system call */
# undef HAVE_STATVFS64
# undef HAVE_STATVF
# define HAVE_STATVFS 1
#endif

#ifdef HAVE_STRUCT_STATVFS64
# define STATFS_T statvfs64
# define STATFS_FUNC statvfs64
#elif defined(HAVE_STRUCT_STATVFS)
# define STATFS_T statvfs
# define STATFS_FUNC statvfs
#elif defined(HAVE_STRUCT_STATFS)
# define STATFS_FUNC statfs
# define STATFS_T statfs
#endif

int bbftpd_statfs (struct mess_dir *msg_file)
{
#if !defined(STATFS_FUNC) || !defined(STATFS_T)
   bbftpd_msg_reply (MSG_BAD_NO_RETRY, "%s", "df operation not supported by the server");
   return 0 ;
#else
   struct STATFS_T stfs;
   char    *dirname ;

   transferoption  = msg_file->transferoption ;
   dirname = msg_file->dirname ;

   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO )
     {
# if defined(WITH_RFIO) || defined(WITH_RFIO64)
	bbftpd_msg_reply (MSG_BAD_NO_RETRY, "%s", "df is not supported by RFIO");
# else
        bbftpd_msg_reply (MSG_BAD_NO_RETRY, "%s", "RFIO is not supported by the server") ;
# endif
        return 0 ;
     }

   if (-1 == STATFS_FUNC(dirname, &stfs))
     {
	bbftpd_msg_reply (MSG_BAD_NO_RETRY, "Unable to df %s: %s", dirname, strerror(errno));
        return 0 ;
     }

   /* print the following information:
    * total_blocks blocks_available block_size total_inode inode_available filename_max_length
    */
   bbftpd_msg_reply (MSG_OK,
		     "%" LONG_LONG_FORMAT " %" LONG_LONG_FORMAT " %lu %" LONG_LONG_FORMAT " %" LONG_LONG_FORMAT " %lu",
		     (long long) stfs.f_blocks,
		     (long long) stfs.f_bavail,
# ifdef HAVE_STATVFS64
		     stfs.f_frsize,
# else
		     stfs.f_bsize,
# endif
		     (long long) stfs.f_files,
		     (long long) stfs.f_ffree,
# if defined(HAVE_STATVFS64) || defined(HAVE_STATVFS) || !defined(DARWIN)
		     stfs.f_namemax
# else
		     255       /* DARWIN?? */
# endif
		    );
   return 0 ;
#endif
}
