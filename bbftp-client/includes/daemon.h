/*
 * includes/daemon.h
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

  
  
 daemon.h v 0.0.0 1999/11/24
 		  v 1.6.0 2000/03/25	- Add COMPRESSION type
		  v 1.6.1 2000/03/30	- Set the TCPWINSIZE for OSF1 to 128000
		  						- Add STARCHILDTO time out for the child
								  when waiting for the HUP signal (set up
								  for machines where the HUP signal was
								  sent by the father before the child waits
								  on the select
		  v 1.8.0 2000/04/14	- Add NBITSINKEY for the Crypto
		  v 1.8.1 2000/04/17	- Increase CONTROLSOCKTO because 
								  the time to get the RSA key is not
								  neglectable
		  v 1.8.7 2000/05/25	- Rewrite header
          v 2.1.0 2001/06/28    - Add log level
 
*****************************************************************************/

/* RETRSTARTTO :
**		Define the time out to wait a retreive start message on the control socket.
**		Expressed in second, usual value 100.
*/
#define RETRSTARTTO 100

/* STARTCHILDTO :
**		Define the time a child has to wait before starting
**		Expressed in second, usual value 20.
*/
#define STARTCHILDTO 20

/* BBFTP LOG Level:
**      In order to be able to debug bbftpd is writing in syslog. As often
**      the syslog management is not accessible to user bbftpd has its 
**      own log level that are mapped on syslog facilities and priorities
*/
/*
** Facility
*/
#define BBFTPD_FACILITY LOG_DAEMON
/*
** Priorities they have to be ordered
** But if your syslog on log LOG_ERR then you can set
** BBFTPD_WARNING, BBFTPD_NOTICE, BBFTPD_INFO and BBFTPD_DEBUG to
** LOG_ERR.
*/
#define BBFTPD_EMERG    LOG_EMERG
#define BBFTPD_ALERT    LOG_ALERT
#define BBFTPD_CRIT     LOG_CRIT 
#define BBFTPD_ERR      LOG_ERR
#define BBFTPD_WARNING  LOG_WARNING
#define BBFTPD_NOTICE   LOG_NOTICE
#define BBFTPD_INFO     LOG_INFO
#define BBFTPD_DEBUG    LOG_DEBUG

/*
** define the default priority 
*/
#ifndef BBFTPD_DEFAULT
#define BBFTPD_DEFAULT  BBFTPD_EMERG
#endif

/* CONFIGURATION FILE
 */
#define BBFTPD_CONF "/etc/bbftpd.conf"
