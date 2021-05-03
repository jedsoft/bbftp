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
/* #include <syslog.h> */
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

#include "_bbftpd.h"

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
# define ST_BLKSIZE_FORMAT LONG_LONG_FORMAT
#else
# define ST_BLKSIZE_FORMAT "s"
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
# define ST_BLOCKS_FORMAT LONG_LONG_FORMAT
#else
# define ST_BLOCKS_FORMAT "s"
#endif

static void copy_ctime (time_t t, char *buf, size_t bufsize)
{
   char *s = ctime (&t);

   if (s == NULL)
     {
	*buf = 0;
	return;
     }

   strncpy(buf, s, bufsize);
   buf[bufsize-1] = 0;
   if (NULL != (s = strchr (buf, '\n')))
     *s = 0;
}

static void send_stat_info (time_t atime, time_t mtime, time_t _ctime,
			    long long st_ino, int st_mode, long long st_size,
			    long long st_blksize, long long st_blocks,
			    int st_uid, int st_gid)
{
   char at[96], mt[96], ct[96];

   copy_ctime (atime, at, sizeof(at));
   copy_ctime (mtime, mt, sizeof(mt));
   copy_ctime (_ctime, ct, sizeof(ct));

   bbftpd_msg_reply (MSG_OK, "%" LONG_LONG_FORMAT " %#08o %" LONG_LONG_FORMAT " %" ST_BLKSIZE_FORMAT " %" ST_BLOCKS_FORMAT "  %d %d (%s) (%s) (%s)",
		     st_ino, st_mode, st_size,
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
		     st_blksize,
#else
		     "N/A",
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
		     st_blocks,
#else
		     "N/A",
#endif
		     st_uid,
		     st_gid,
		     at,
		     mt,
		     ct);
}

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
static int do_rfio_stat (char *filename)
{
# ifdef STANDART_FILE_CALL
   struct stat st;
# else
#  ifdef WITH_RFIO64
   struct stat64 st;
#  else
   struct stat st;
#  endif
# endif

# ifdef WITH_RFIO64
   if (-1 == rfio_stat64(filename,&rfstatbuf)) return -1;
# else
   if (-1 == rfio_stat(filename,&rfstatbuf)) return -1;
# endif

   send_stat_info (st.st_atime, st.st_mtime, st.st_ctime,
		   st.st_ino, st.st_mode, st.st_size,
		   st.st_blksize, st.st_blocks,
		   st.st_uid, st.st_gid)
   return 0 ;

}
#endif 				       /* RFIO */

int bbftpd_stat (struct mess_dir *msg_file)
{
    char    *filename ;

#ifdef STANDART_FILE_CALL
    struct stat st;
#else
    struct stat64 st;
#endif
   int status;

   transferoption  = msg_file->transferoption ;
   filename = msg_file->dirname ;

    if ((transferoption & TROPT_RFIO) == TROPT_RFIO)
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
	return do_rfio_stat (filename);
#else
        bbftpd_msg_reply (MSG_BAD_NO_RETRY, "%s", "RFIO is not supported by the server");
	bbftpd_log (BBFTPD_ERR, "%s", "MSG_STAT: RFIO is not supported");
	return -1;
#endif
     }
#ifdef STANDART_FILE_CALL
   status = stat (filename,&st);
#else
   status = stat64(filename,&st);
#endif
   if (status == 0)
     {
	send_stat_info (st.st_atime, st.st_mtime, st.st_ctime,
			st.st_ino, st.st_mode, st.st_size,
			st.st_blksize, st.st_blocks,
			st.st_uid, st.st_gid);
	return 0;
     }

   bbftpd_msg_reply (MSG_BAD_NO_RETRY, "stat %s failed", filename);
   return 0;
}
