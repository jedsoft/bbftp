/*
 * bbftpc/bbftp_private_user.c
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

  
  
 bbftp_private_user.c   v 2.1.0 2001/05/21  - Routines creation

*****************************************************************************/
#include <stdio.h>

#include <bbftp_private_user.h>

/*
** username contain a pointer to the string given with the -u flag
*/
/* extern char *username ; */
/*
** privatestr contain a pointer to the string given with the -P flag
*/
/* extern char *privatestr ; */

/*******************************************************************************
** bbftp_private_getargs :                                                     *
**                                                                             *
**      Routine to get arguments for private authentication. This routine is   *
**      called after several tests have been made. The variable to be set are  *
**      to be define in bbftp_private_user.h to be used by other routines.     *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_private_getargs(char *logmessage)
{
    return 0 ;
}

/*******************************************************************************
** bbftp_private_auth :                                                        *
**                                                                             *
**      Routine to do the private authentication. This routine just have to    *
**      send or to receive private data to the bbftpd daemon. For that it will *
**      use the two routines :                                                 *
**          bbftp_private_send                                                 *
**          bbftp_private_recv                                                 *
**      The reception of the OK or BAD message will be done by the calling     *
**      routine, so if this routine has no check to do (for example just       *
**      sending username and password) it will just call bbftp_private_send    *
**      twice and let the calling program check if it is OK or BAD.            *
**                                                                             *
**      The bbftp_private_send routine has to be called with the following     *
**      parameters:                                                            *
**                                                                             *
**      int bbftp_private_send(char *buffertosend,int buffertosendlength,      *
**                             char *logmessage)                               *
**          char    *buffertosend = string to be send to the daemon            *
**          int     buffertosendlength = length of the string                  *
**          char    *logmessage                                                *
**                                                                             *
**      and return 0 in case of success, -1 in case of error with logmessage   *
**      filled                                                                 *
**                                                                             *
**      The bbftp_private_recv routine has to be called with the following     *
**      parameters:                                                            *
**                                                                             *
**      int bbftp_private_recv(char *buffertorecv,int lengthtoreceive,         *
**                             char *logmessage)                               *
**          char    *buffertorecv = buffer to put data received from the       *
**                                  daemon                                     *
**          int     lengthtorecv = length of the buffer                        *
**          char    *logmessage                                                *
**                                                                             *
**      and return number of byte received in case of success, -1 in case of   *
**      error with logmessage filled                                           *
**      It is the duty of the programmer to be sure that the buffer is large   *
**      enought                                                                * 
**                                                                             *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_private_auth(char *logmessage) 
{
    
    return  0 ;
}
