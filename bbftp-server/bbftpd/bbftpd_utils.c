/*
 * bbftpd/bbftpd_utils.c
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


 Routines : void strip_trailing_slashes (char *path)
            void free_all_var() 
 
 bbftpd_utils.c v 2.0.0 2000/12/19  - Routine creation
                v 2.0.1 2001/04/23  - Correct indentation

            
*****************************************************************************/
#include <bbftpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include <daemon.h>
#include "_bbftpd.h"

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

/*******************************************************************************
** free_all_var :                                                              *
**                                                                             *
**      Routine to free all global variables                                   *
**                                                                             *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                 all                                                         *
**                                                                             *
**                                                                             *
*******************************************************************************/

void free_all_var(void)
{
    if ( curfilename != NULL ) {
        free(curfilename) ;
        curfilename = NULL ;
        curfilenamelen = 0 ;
    }
    if ( realfilename != NULL ) {
        free(realfilename) ;
        realfilename = NULL ;
    }
    if ( myports != NULL ) {
        free(myports) ;
        myports = NULL ;
    }
    if ( mychildren != NULL ) {
        free(mychildren) ;
        mychildren = NULL ;
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
    nbpidchild = 0 ;
}

char *bbftpd_strdup (const char *in)
{
   size_t len;
   char *out;

   len = strlen (in);
   if (NULL != (out = (char *) malloc (len+1)))
     strcpy (out, in);

   return out;
}

char *bbftpd_strcat (const char *a, const char *b)
{
   char *c;
   size_t lena, lenb;

   lena = strlen(a);
   lenb = strlen(b);
   if (NULL == (c = (char *)malloc (lena+lenb+1)))
     {
	bbftpd_syslog(BBFTPD_ERR, "Unable to allocate space for strcat\n");
	return NULL;
     }
   memcpy (c, a, lena);
   memcpy (c+lena, b, lenb+1);
   return c;
}


/* required to return a \0 terminated string */
char *bbftpd_read_file (const char *file, size_t *sizep)
{
   char *buf;
   struct stat st;
   size_t nread;
   int fd;

   while (-1 == (fd = open (file, O_RDONLY)))
     {
	if (errno == EINTR) continue;

	bbftpd_syslog (BBFTPD_ERR, "Error opening file (%s) : %s \n", file, strerror(errno));
	return NULL;
     }

   if (-1 == fstat (fd, &st))
     {
	bbftpd_syslog (BBFTPD_ERR, "Unable to stat (%s) : %s \n", file, strerror(errno));
	(void) close (fd);
	return NULL;
     }

   if (NULL == (buf = (char *) malloc(1+st.st_size)))
     {
	bbftpd_syslog (BBFTPD_ERR, "Unable to allocate space to read file (%s)\n", file);
	(void) close (fd);
	return NULL;
     }

   nread = 0;
   while (nread < (size_t)st.st_size)
     {
	ssize_t dn;

	if (-1 == (dn = read (fd, buf+nread, st.st_size - nread)))
	  {
	     if (errno == EINTR)
	       continue;

	     free (buf);
	     bbftpd_syslog(BBFTPD_ERR, "Error reading file (%s): %s\n", file, strerror(errno));
	  }
	nread += dn;
	if (dn == 0)
	  {
	     /* End of file. */
	     break;
	  }
     }

   buf[nread] = 0;
   (void) close (fd);
   *sizep = nread;
   return buf;
}
