/*
 * includes/status.h
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

  
  
 status.h v 0.0.0 1999/11/24
          v 1.6.0 2000/03/25    - Add status for sending
          v 2.0.0 2000/12/19    - Add status for protocol v2

*****************************************************************************/


#define S_CONN		                0	/* Just connected */
#define S_LOGGED	                1	/* Logged IN */
#define S_RECEIVING	                2	/* Receiving a file */
#define S_SENDING	                3	/* Sending a file */
#define S_PROTWAIT	                4	/* Waiting for the prot message */
#define S_WAITING_FILENAME_STORE    5	/* Waiting for the filename in store */
#define S_WAITING_FILENAME_RETR     6	/* Waiting for the filename in retreive */
#define S_WAITING_STORE_START       7	/* Waiting for the start (store) */
#define S_WAITING_RETR_START        8	/* Waiting for the start (retr) */
#define S_WAITING_CREATE_ZERO       9	/* Waiting for the start (retr) */
