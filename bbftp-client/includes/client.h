/*
 * includes/client.h
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

  
  
 client.h v 1.5.0 2000/03/23	- Header creation
          v 1.6.0 2000/03/24	- Add timer for getting answer in get
		  v 1.8.7 2000/05/25	- Rewrite header
		  v 1.8.9 2000/08/08	- Add CHECKCHILDTIME
		  v 1.9.0 2000/08/24	- Use configure to help port
          v 2.0.0 2001/03/21    - Suppress MAXLINE

*****************************************************************************/
/* CASE_XX
**      Define for print message level
*/
#define CASE_NORMAL         0
#define CASE_ERROR          1
#define CASE_WARNING        2
#define CASE_FATAL_ERROR    3

/* CONFIGURATION FILE
 */
#define BBFTP_CONF "/etc/bbftp.conf"

/* NBTRYMAX :
**		Define the number of time a command in the control file will be 
**		retried. (in the case of a MSG_BAD answer, no retry will be done
**		if MSG_BAD_NO_RETRY is sent). Must be greater than 0.
**		Usual value 5. 
*/
#define NBTRYMAX  5

/* WAITRETRYTIME :
**		Define the time in second a client will wait before retrying.
**		Usual value is 60 
*/
#define WAITRETRYTIME  30

/* CHILDWAITTIME :
**		Define the time in second a child of the client will wait a
**		connexion from a child of the server. 
**		Usual value is 200 
*/
#define CHILDWAITTIME 200

/* GETANSWAITTIME :
**		Define the time in second the client will wait the answer giving the
**		file length from the server in case of a get command
**		Usual value is 100 
*/
#define GETANSWAITTIME 100

/* CHECKCHILDTIME :
**		Define the time in second the client will survey his children. This
**		will only be done while waiting on the control connection during
**		transfer 
**		Usual value is 180 
*/
#define CHECKCHILDTIME 180

/* BB_RET_XX :
**		Define the return code for the routine called by the main program
*/
#define BB_RET_OK					0	/* Everything OK */
#define BB_RET_CONN_BROKEN			-1	/* Connexion with server broken */
#define BB_RET_FT_NR_CONN_BROKEN	-2	/* Fatal error, no retry needed, connexion broken */
#define BB_RET_FT_NR				-3	/* Fatal error, no retry needed but connexion kept */
#define BB_RET_ERROR				1	/* Error, retry possible, connexion kept */

/* BB_SIG_XX :
**      Define the signal to be used to communicate between bbftpc and bbftpcd
*/
#ifdef HAVE_SIGINFO
#ifdef USE_STANDART_SIGNALS
#define STARTNEW	SIGUSR1
#define TRANSOK		SIGHUP
#define TRANSFAIL	SIGUSR2
#else
#define STARTNEW	SIGRTMIN
#define TRANSOK		( SIGRTMIN + 1 )
#define TRANSFAIL	( SIGRTMIN + 2 )
#endif
#endif
