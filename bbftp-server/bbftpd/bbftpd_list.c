/*
 * bbftpd/bbftpd_list.c
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
        0  Keep the connection open (does not mean that the directory has been
           successfully created)
        -1 Tell the calling program to close the connection
  
 bbftpd_list.c  v 2.0.0  2001/02/27 - Creation of the routine.
                v 2.0.1  2001/04/23 - Correct indentation
                v 2.0.2  2001/05/04 - Change the philosophy in order
                                      to bypass CASTOR rfio_rewinddir bug 
                v 2.1.0  2001/06/01 - Reorganise routines as in bbftp_

 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>

#include "_bbftpd.h"

int bbftpd_list (char *pattern)
{
   char msgbuf[1024];
   char    *filelist ;
   int     filelistlen ;
   int status;

   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO )
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
	status = bbftpd_retrlistdir_rfio (pattern,filelist,filelistlen,msgbuf) ;
#else
	reply (MSG_BAD_NO_RETRY, "Fail to LIST : RFIO not supported");
        return -1 ;
#endif
    }
   else
     status = bbftpd_retrlistdir (pattern, &filelist, &filelistlen, msgbuf, sizeof(msgbuf));

   if (status != 0)
     {
        reply (((status < 0) ? MSG_BAD_NO_RETRY : MSG_BAD), "bbftpd_retrlistdir failed");
	return 0;
     }

   if (-1 == (status = bbftpd_msgwrite_bytes (MSG_LIST_REPL_V2, filelist, filelistlen)))
     bbftpd_log (BBFTPD_ERR,"Error sending filelist");

   FREE(filelist);
   return status;
}
