/*
 * includes/bbftp_private.h
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

  
  
 bbftp_private.h    v 2.1.0 2001/05/25  - Header creation
			  
*****************************************************************************/

/*
** This include must contain all global variable to be used by the
** general purpose private authentication routines
**
** IT HAS NOT TO BE TOUCHED BY THE POEPLE IMPLEMENTING THE AUTHENTICATION
** MECHANISM
*/
/*
** prv_ctrlsock:
**      This is the temporary control socket
*/
int     prv_ctrlsock ;
/*
** hisrsa:
**      The place to store the remote public RSA key
*/
RSA     *hisrsa ;
/*
** myrsa
**      The place to store the local RSA key pair
*/
RSA     *myrsa ;
