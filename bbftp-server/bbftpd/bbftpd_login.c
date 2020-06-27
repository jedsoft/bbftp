/*
 * bbftpd/bbftpd_login.c
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

  
 RETURN:
         0  login OK
        -1 login failed
 
 bbftpd_login.c  v 1.6.0  2000/03/24    - Creation of the routine. This part of
                                          code was contained in readcontrol.c
                 v 1.8.0  2000/04/14    - Introduce RSA Cryptage
                 v 1.8.3  2000/04/18    - Change the reply message in order
                                          to stop retry in case of unkown user
                                          or incorrect password
                 v 1.8.7  2000/05/24    - Modify headers
                 v 1.8.11 2000/05/24    - Add Pam for Linux (and only pam...)
                 v 1.9.0  2000/08/18    - Use configure to help portage
                                        - Change the reply message (for future
                                          use)
                 v 1.9.0  2000/09/19    - Correct bug for pam
                 v 1.9.4  2000/10/16    - Supress %m
                 v 2.0.0  2000/12/18    - Use incontrolsock instead of msgsock
                 v 2.0.1  2001/04/23    - Correct indentation
                 v 2.0.2  2001/05/04    - Correct return code treatment
                                        - Change use of tmpchar
                 v 2.1.0  2001/06/11    - Change file name
                                     
 *****************************************************************************/
#include <bbftpd.h>

/*#ifndef SX*/
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include <errno.h>

#include <pwd.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <netinet/in.h>
#include <config.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>
#include <version.h>

#ifdef SHADOW_PASSWORD
#include <shadow.h>
#endif
#ifdef USE_PAM
#include <security/pam_appl.h>
#endif
#ifdef HAVE_SECURITY_PASS
#include <userpw.h>
#endif

extern int incontrolsock ;
extern	int	recvcontrolto ;
extern char currentusername[MAXLEN] ;

#ifdef USE_PAM
extern char daemonchar[50] ;

static char *PAM_password;
/* PAM conversation function
 * We assume that echo on means login name, and echo off means password.
 */
static int PAM_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
    int replies = 0;
    struct pam_response *reply = NULL;

#define COPY_STRING(s) (s) ? strdup(s) : NULL

    reply = (struct pam_response *) malloc(sizeof(struct pam_response) * num_msg);
    if (!reply) return PAM_CONV_ERR;

    for (replies = 0; replies < num_msg; replies++) {
        switch (msg[replies]->msg_style) {
        case PAM_PROMPT_ECHO_ON:
            return PAM_CONV_ERR;
            break;
        case PAM_PROMPT_ECHO_OFF:
            reply[replies].resp_retcode = PAM_SUCCESS;
            reply[replies].resp = COPY_STRING(PAM_password);
            /* PAM frees resp */
            break;
        case PAM_TEXT_INFO:
            /* ignore it... */
            reply[replies].resp_retcode = PAM_SUCCESS;
            reply[replies].resp = NULL;
            break;
        case PAM_ERROR_MSG:
            /* ignore it... */
            reply[replies].resp_retcode = PAM_SUCCESS;
            reply[replies].resp = NULL;
            break;
        default:
           /* Must be an error of some sort... */
            return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}

static struct pam_conv PAM_conversation =
{
    &PAM_conv,
    NULL
};
#endif

int loginsequence() {
    
    int        retcode ;
    char    receive_buffer[MAXMESSLEN] ;
    struct mess_sec *msg_sec ;
    struct mess_rsa *msg_rsa ;
    
    char    username[MAXLEN] ;
    char    password[MAXLEN] ;
    struct    passwd    *uspass ;
    char    logmessage[1024] ;
#ifdef SHADOW_PASSWORD
    struct    spwd    *sunpass ;
#endif
#ifdef HAVE_SECURITY_PASS
    struct    userpw    *secpass ;
#endif
    char    *calcpass ;
#ifdef AFS
    char    *inst = "";
#endif
#ifdef USE_PAM
    pam_handle_t *pamh=NULL;
#endif
    sprintf(logmessage,"bbftpd version %s",VERSION) ;
    /*
    ** Read the crypt type
    */
    if ( (retcode = readmessage(incontrolsock,receive_buffer,CRYPTMESSLEN,recvcontrolto)) < 0 ) {
        /*
        ** Error ...
        */
        return(retcode) ;
    }
    msg_sec = (struct mess_sec *)receive_buffer ;
    switch (msg_sec->crtype) {
        case CRYPT_RSA_PKCS1_OAEP_PADDING :
            /*
            ** RSA
            */
            if ( (retcode = readmessage(incontrolsock,receive_buffer,RSAMESSLEN,recvcontrolto)) < 0) {
                return(retcode) ;
            }
            if ( decodersapass(receive_buffer,username,password) < 0 ) {
                syslog(BBFTPD_ERR,"Decode user/password error") ;
                strcat(logmessage," : Decode user/password error") ;
                reply(MSG_BAD,logmessage) ;
                return -1 ;
            }
            break ;
        case NO_CRYPT :
            if ( (retcode = readmessage(incontrolsock,receive_buffer,RSAMESSLEN,recvcontrolto)) < 0) {
                return(retcode) ;
            }
            msg_rsa = (struct mess_rsa *)receive_buffer ;
            strncpy(username, msg_rsa->cryptuser, strlen(msg_rsa->cryptuser));
            username[strlen(msg_rsa->cryptuser)]='\0';
            strncpy(password, msg_rsa->cryptpass, strlen(msg_rsa->cryptpass));
            password[strlen(msg_rsa->cryptpass)]='\0';
            break ;
        default :
            syslog(BBFTPD_ERR,"Unkwown encryption %d",msg_sec->crtype) ;
            strcat(logmessage," : Unknwon encryption") ;
            reply(MSG_BAD,logmessage) ;
            return -1 ;
    }
    /*
    ** Here we check the username and pass and set the default dir
    */
    if ( (uspass = getpwnam(username)) == NULL ) {
        syslog(BBFTPD_ERR,"Unknown user %s",username) ;
        strcat(logmessage," : Unknown user (") ;
        strcat(logmessage, username);
        strcat(logmessage, ")");
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        return -1 ;
    }
/*
** If the AFS flag is set it take precedence on all other mechanism
*/
#ifdef AFS
    setpag() ;
    retcode = ka_UserAuthenticate(username,inst,0,password,0,&calcpass) ;
    if ( retcode != 0 ) {
        syslog(BBFTPD_ERR,"ka_UserAuthenticate message : %s ",calcpass) ;
        /*
        ** Check local user 
        */
        if ( (strcmp(uspass->pw_passwd,"x") == 0) || 
             (strcmp(uspass->pw_passwd,"X") == 0) ) {
#ifdef SHADOW_PASSWORD
            if ( (sunpass = getspnam(username)) == NULL ) {
                syslog(BBFTPD_ERR,"Unknown user %s",username) ;
                /*
                ** We send ka_UserAuthenticate error msg
                */
                strcat(logmessage, " : ") ;
                strcat(logmessage, calcpass) ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
            calcpass = (char *) crypt(password,sunpass->sp_pwdp) ;
            if ( strcmp(calcpass,sunpass->sp_pwdp) != 0 ) {
                syslog(BBFTPD_ERR,"Incorrect password user %s",username) ;
                strcat(logmessage," : Incorrect password") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
#elif defined(HAVE_SECURITY_PASS)
            if ( (secpass = getuserpw(username)) == NULL ) {
                syslog(BBFTPD_ERR,"Unknown user %s",username) ;
                /*
                ** We send ka_UserAuthenticate error msg
                */
                strcat(logmessage, " : ") ;
                strcat(logmessage, calcpass) ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
            calcpass = (char *) crypt(password,secpass->upw_passwd) ;
            if ( strcmp(calcpass,secpass->upw_passwd) != 0 ) {
                syslog(BBFTPD_ERR,"Incorrect password user %s",username) ;
                strcat(logmessage," : Incorrect password") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
#else

            syslog(BBFTPD_ERR,"No Password user %s",username) ;
            strcat(logmessage," : No password user") ;
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return -1 ;
#endif
        } else {
            calcpass = (char *) crypt(password,uspass->pw_passwd) ;
            if ( strcmp(calcpass,uspass->pw_passwd) != 0 ) {
                syslog(BBFTPD_ERR,"Incorrect password user %s",username) ;
                strcat(logmessage," : Incorrect password") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
        }
    }
#elif defined(USE_PAM)
    /*
    ** For PAM
    */
    strcat(logmessage," : PAM Error") ;
    PAM_password = password ;
    retcode = pam_start("bbftp", username, &PAM_conversation, &pamh);
#define PAM_BAIL if (retcode != PAM_SUCCESS) { pam_end(pamh, 0); syslog(BBFTPD_ERR,"PAM error (%d) user %s",retcode,username) ;reply(MSG_BAD_NO_RETRY,logmessage) ;return -1; }
    PAM_BAIL;
    retcode = pam_authenticate(pamh, PAM_SILENT);
    PAM_BAIL;
    retcode = pam_acct_mgmt(pamh, 0);
    PAM_BAIL;
#ifdef PAM_ESTABLISH_CRED
    retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED);
#else
    retcode = pam_setcred(pamh, PAM_CRED_ESTABLISH);
#endif
    PAM_BAIL;
    pam_end(pamh, PAM_SUCCESS);
    /*
    ** Reopen the lag as the pam functions close it
    */
    openlog(daemonchar, LOG_PID | LOG_NDELAY, BBFTPD_FACILITY);
#else
    /*
    ** For the others
    */
    if ( (strcmp(uspass->pw_passwd,"x") == 0) || 
         (strcmp(uspass->pw_passwd,"X") == 0)) {
#ifdef SHADOW_PASSWORD
            if ( (sunpass = getspnam(username)) == NULL ) {
                syslog(BBFTPD_ERR,"Unknown user %s",username) ;
                strcat(logmessage," : Unknown user") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
            calcpass = (char *) crypt(password,sunpass->sp_pwdp) ;
            if ( strcmp(calcpass,sunpass->sp_pwdp) != 0 ) {
                syslog(BBFTPD_ERR,"Incorrect password user %s",username) ;
                strcat(logmessage," : Incorrect password") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
#elif defined(HAVE_SECURITY_PASS)
            if ( (secpass = getuserpw(username)) == NULL ) {
                syslog(BBFTPD_ERR,"Unknown user %s",username) ;
                strcat(logmessage," : Unknown user") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
            calcpass = (char *) crypt(password,secpass->upw_passwd) ;
            if ( strcmp(calcpass,secpass->upw_passwd) != 0 ) {
                syslog(BBFTPD_ERR,"Incorrect password user %s",username) ;
                strcat(logmessage," : Incorrect password") ;
                reply(MSG_BAD_NO_RETRY,logmessage) ;
                return -1 ;
            }
#else
            syslog(BBFTPD_ERR,"No Password user %s",username) ;
            strcat(logmessage," : No password") ;
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return -1 ;
#endif
    } else {
        calcpass = (char *) crypt(password,uspass->pw_passwd) ;
        if ( strcmp(calcpass,uspass->pw_passwd) != 0 ) {
            syslog(BBFTPD_ERR,"Incorrect password user %s",username) ;
            strcat(logmessage," : Incorrect password") ;
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return -1 ;
        }
    }
#endif
    /*
    ** Set the uid and gid of the process
    */
    if ( setgid(uspass->pw_gid) < 0 ) {
        syslog(BBFTPD_ERR,"Error setgid user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : Unable to set gid: ") ;
	strcat(logmessage,strerror(errno));
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    /* Initialize the group list. */
    if (getuid() == 0) {
      if (initgroups(uspass->pw_name, uspass->pw_gid) < 0) {
        syslog(BBFTPD_WARNING,"Error Initialize the group list %s : %s",username,strerror(errno)) ;
        return -1 ;
      }
      endgrent();
    }

    if ( setuid(uspass->pw_uid) < 0 ) {
        syslog(BBFTPD_ERR,"Error setuid user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : Unable to set uid: ") ;
	strcat(logmessage,strerror(errno));
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    if ( uspass->pw_dir == NULL ) {
        syslog(BBFTPD_ERR,"No home directory user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : No home directory: ") ;
	strcat(logmessage,strerror(errno));
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    if ( chdir(uspass->pw_dir) < 0) {
        syslog(BBFTPD_ERR,"Error chdir user %s : %s",username,strerror(errno)) ;
        strcat(logmessage," : Unable to change directory: ") ;
	strcat(logmessage,strerror(errno));
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
                
    syslog(BBFTPD_DEBUG,"User %s connected",username) ;
    strcpy(currentusername,username) ;
    return 0 ;
}
