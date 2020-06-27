/*
 * bbftpd/bbftpd_cert.c
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

  
  
 bbftpd_cert.c   v 2.2.0 2001/10/03  - Routines creation

*****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/socket.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <config.h>
#include <common.h>
#include <daemon_proto.h>
#include <daemon.h>
#include <structures.h>
#include <version.h>
#include <gssapi.h>
#include <gfw.h>

extern  int             incontrolsock ;
extern  int             outcontrolsock ;
extern  int	            recvcontrolto ;
extern  char            currentusername[MAXLEN] ;
extern  gss_cred_id_t   server_creds;

/*******************************************************************************
** bbftpd_cert_receive_connection :                        	                   *
**                                                                             *
**      This routine is called when a connection occurs in certificate         *
**      authentication mode. It receives the plublic key of the client and     *
**      calls the GSS Framework (GFW) api to validate the connection           * 
**      routine returns 0 to set up correctly the global parameters            *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          hisrsa              MODIFIED                                       *
**                                                                             *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/
int bbftpd_cert_receive_connection(int msglen) 
{

    char    logmessage[1024] ;
    char    *username ;
    struct    passwd    *uspass ;
    gss_buffer_desc client_name;
    OM_uint32 min_stat, maj_stat;

    sprintf(logmessage,"bbftpd version %s",VERSION) ;

	maj_stat = gfw_accept_sec_context(&min_stat, incontrolsock, outcontrolsock, server_creds, &client_name);
    if (maj_stat != GSS_S_COMPLETE) {
        gfw_msgs_list *messages = NULL;
        gfw_status_to_strings(maj_stat, min_stat, &messages) ;
        strcat(logmessage, " : ");
        strcat(logmessage, messages->msg);
        while (messages != NULL) {
            syslog(BBFTPD_ERR,"gfw_accept_sec_context failed: %s", messages->msg) ;
            messages = messages->next;
        }
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        return -1;
	}
	
    syslog(BBFTPD_INFO,"Checked certificate : \"%s\"",(char *)client_name.value) ;
	/*
	** Map cert with local user
	*/
	if (globus_gss_assist_gridmap((char *)client_name.value, &username) != 0) {
        syslog(BBFTPD_ERR,"mapping failed for: %s",(char *)client_name.value) ;
		strcat(logmessage, " : grid mapping failed");
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        return -1 ;
	}
	syslog(BBFTPD_INFO, "Mapfile user is:%s", username);
    /*
    ** Here we check the username and pass and set the default dir
    */
    if ( (uspass = getpwnam(username)) == NULL ) {
        syslog(BBFTPD_ERR,"%s is not a local user",username) ;
        strcat(logmessage," : You need an account on the server") ;
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        return -1 ;
    }
    /*
    ** Set the uid and gid of the process
    */
    if ( setgid(uspass->pw_gid) < 0 ) {
        syslog(BBFTPD_ERR,"Error setgid user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : Cannot set gid: ") ;
	strcat(logmessage,strerror(errno));
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    if ( setuid(uspass->pw_uid) < 0 ) {
        syslog(BBFTPD_ERR,"Error setuid user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : Cannot set uid: ") ;
	strcat(logmessage,strerror(errno));
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    if ( uspass->pw_dir == NULL ) {
        syslog(BBFTPD_ERR,"No home directory for user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : You need a home directory on the server") ;
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    /*
    ** Try to cd into home directory. If permission denied (ie no AFS token)
    ** try to cd into "/tmp"
    */
    if ( chdir(uspass->pw_dir) < 0) {
        if ( errno == EACCES) {
            syslog(BBFTPD_WARNING,"Permission denied on user %s home directory: using /tmp",username) ;
            if ( chdir("/tmp") < 0) {
                syslog(BBFTPD_ERR,"Cannot cd into /tmp: %s",strerror(errno)) ;
                strcat(logmessage," : Cannot access home directory nor /tmp") ;
                reply(MSG_BAD,logmessage) ;
                return -1 ;
            }
            strcat(logmessage," : Home directory not accessible, /tmp used instead") ;
            syslog(BBFTPD_INFO,"User %s connected",username) ;
            strcpy(currentusername,username) ;
            reply(MSG_WARN,logmessage) ;
            return 1 ;
        } else {
            syslog(BBFTPD_ERR,"Cannot cd into user %s home directory: %s",username,strerror(errno)) ;
            strcat(logmessage," : Cannot access home directory: ") ;
	    strcat(logmessage,strerror(errno));
            reply(MSG_BAD,logmessage) ;
            return -1 ;
        }
    }
                
    syslog(BBFTPD_INFO,"User %s connected",username) ;
    strcpy(currentusername,username) ;
    return 0 ;
}

