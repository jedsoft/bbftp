/*
 * includes/bbftp_turl.h
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

 bbftp_turl.h	v 2.2.1	2013/02/01	- Header creation to avoid implicit declaration issue 
  
*****************************************************************************/

#include <structures.h>

struct decodedTURL {
        char hostname[50];
        char port[MAXPORT];
        char filename[MAXLENFILE];
        int rfio;
} ;

/*
** Prototype for TURL routines
*/
int translatecommand(char *buffercmd, char **translatedcmd, char **host, int *port, int *remoterfio, int *localrfio);
int decodeTURL(char *oTURL, struct decodedTURL **dTURL);

