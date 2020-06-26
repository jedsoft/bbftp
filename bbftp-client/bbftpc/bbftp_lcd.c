/*
 * bbftpc/bbftp_lcd.c
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

  
 RETURN :
    BB_RET_OK                   = everything OK 
    BB_RET_FT_NR                = Fatal error no retry needed on this command but 
                                  connection has been kept
    BB_RET_ERROR                = Error but retry possible
    BB_RET_FT_NR_CONN_BROKEN    = Fatal error no retry needed connection broken

 bbftp_lcd.c v 2.0.0 2001/03/13    - Creation of the routine 
             v 2.0.1 2001/04/19    - Correct indentation 
 
*****************************************************************************/
#include <bbftp.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <structures.h>

extern  int        timestamp;
extern  int        transferoption ;
extern  int        verbose ;

int bbftp_lcd(char *dirpath,int  *errcode) 
{

    int     savederrno ;

    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : lcd %s\n",dirpath) ;

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
    if ( (transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n","lcd not supported under RFIO") ;
        *errcode = 26 ;
        return BB_RET_FT_NR ;
    }
#endif
    /*
    ** We change the directory
    */
    if ( chdir(dirpath) < 0 ) {
        savederrno = errno ;
        if ( savederrno == EACCES ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ||
                savederrno == ENOENT ) {
            printmessage(stderr,CASE_ERROR,71,timestamp,"Error changing directory %s (%s)\n",dirpath,strerror(savederrno)) ;
            *errcode = 71 ;
            return BB_RET_FT_NR ;
        } else {
            printmessage(stderr,CASE_ERROR,71,timestamp,"Error changing directory %s (%s)\n",dirpath,strerror(savederrno)) ;
            *errcode = 71 ;
            return BB_RET_ERROR ;
        }
    } 
    if ( verbose) printf("<< OK : Current local directory is %s\n",dirpath) ;
    return BB_RET_OK ;
}
