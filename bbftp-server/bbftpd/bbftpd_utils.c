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

#if HAVE_STRING_H
# include <string.h>
#endif

extern char *curfilename ;
extern char *realfilename ;
extern int  curfilenamelen ;
extern int  *myports ;
extern int  *mychildren ;
extern int  *mysockets ;
extern int  *readbuffer;
extern int  *compbuffer;
extern int  nbpidchild ;



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

void free_all_var() 
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
        
