/*
 * includes/bbftp.h.in
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

  
 
 This header contains all parameters that are configured
 by configure
 
 bbftp.h v 1.9.0  2000/08/17    - Header creation
         v 2.0.0  2000/03/20    - Add flag for readir call
         v 2.0.2  2000/05/07    - Add flag for RFIO and CASTOR
         v 2.2.0  2001/10/04    - Add flag HAVE_SSH for command ssh or ssf (cc)

*****************************************************************************/
/*<<<<<<<<<<<<<<<<<<< Authentication Mechanism >>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/* USE_PAM :
**      use PAM as authentication
*/
#undef USE_PAM

/* SHADOW_PASSWORD :
**      Define if we have everything to get the shadow pass
*/
#undef SHADOW_PASSWORD

/* HAVE_SECURITY_PASS :
**      Define if we have using /etc/security/ files
*/
#undef HAVE_SECURITY_PASS

/* USE_GETPASSPHRASE :
**      Use getpassphrase to enter the password (instead of getpass)
*/
#undef USE_GETPASSPHRASE

/* AFS : Standard authentication thru AFS
**      Define if AFS is used for the standard authentication
*/
#undef AFS

/* CERTIFICATE_AUTH
**      Globus certificates authentication mecanism
*/
#undef CERTIFICATE_AUTH

/* PRIVATE_AUTH
**      User-defined authentication mecanism
*/
#undef PRIVATE_AUTH

/*<<<<<<<<<<<<<<<<<<<<<<<< ARCHITECTURE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/*
** STANDART_FILE_CALL :
**      To know if we are going to use the standart calls to open, lseek, stat
**      or use open64, lseek64 and stat64
**
*/
#undef STANDART_FILE_CALL
/*
** STANDART_READDIR_CALL :
**      To know if we are going to use the standart calls to readdir
**      or readdir64 (tihs is done only if not using standart file
**      call
**
*/
#undef STANDART_READDIR_CALL

/* my64_t :
**      That a 64 bits type defined for me
**
*/
#undef my64_t

/* LONG_LONG_FORMAT :
**      How to print a long long
**
*/
#undef LONG_LONG_FORMAT

/* HAVE_NTOHLL
**      Have ntohll function
**
*/
#undef HAVE_NTOHLL

/*<<<<<<<<<<<<<<<<<<<<<<< SIGNALS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/

/* HAVE_SIGINFO :
**      Have siginfo structure
**
*/
#undef HAVE_SIGINFO

/* HAVE_SA_NOCLDWAIT :
**      Have siginfo structure
**
*/
#undef HAVE_SA_NOCLDWAIT

/* USE_STANDART_SIGNALS :
**      Use SIGHUP SIGUSR1 and SIGUSR2, if not defined use Real time signals
**
*/
#undef USE_STANDART_SIGNALS

/*<<<<<<<<<<<<<<<<<<<<<<< EXTERNAL SOFTWARE >>>>>>>>>>>>>>>>>>>>>>>>>>*/
/* WITH_RFIO :
**      Define if RFIO has been included and defines which version
*/
#undef WITH_RFIO
#undef WITH_RFIO64

/* CASTOR :
**      Define if RFIO castor library is present
*/
#undef CASTOR

/* WITH_GZIP
**      Define if compression has been included in building
*/
#undef WITH_GZIP

/* WITH_SSL
**      Define if encoding has been included in building
*/
#undef WITH_SSL

#undef PORT_RANGE
