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

#include <stdlib.h>

/*#ifndef SX*/
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include <errno.h>

#include <pwd.h>
/* #include <syslog.h> */
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <netinet/in.h>
#ifdef SHADOW_PASSWORD
#include <shadow.h>
#endif
#ifdef USE_PAM
#include <security/pam_appl.h>
#endif
#ifdef HAVE_SECURITY_PASS
#include <userpw.h>
#endif

#include <sys/types.h>
#include <grp.h>

#include <config.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>
#include <version.h>

#include "_bbftpd.h"

#ifdef USE_PAM
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

static int check_pass (char *calcpass, char *pass, char *username)
{
   if ((pass != NULL) && (calcpass != NULL)
       && (0 == strcmp (calcpass, pass)))
     return 0;

   bbftpd_log (BBFTPD_ERR, "Incorrect password for user %s", username);
   bbftpd_msg_reply (MSG_BAD_NO_RETRY, "%s", "Incorrect password");
   return -1;
}

static int handle_shadow_pass (char *username, char *password, char *bad_reply_msg)
{
#ifdef SHADOW_PASSWORD
   struct spwd *sunpass ;
   char *calcpass;
   errno = 0;

   if (NULL == (sunpass = getspnam(username)))
     {
	bbftpd_log (BBFTPD_ERR, "getspnam failed for usename %s: %s", username, strerror(errno));
	bbftpd_msg_reply (MSG_BAD_NO_RETRY, "bbftpd version %s : %s", VERSION, bad_reply_msg);
	return -1 ;
     }

   calcpass = (char *) crypt(password,sunpass->sp_pwdp) ;
   return check_pass (calcpass, sunpass->sp_pwdp, username);

#elif defined(HAVE_SECURITY_PASS)

   struct userpw *secpass ;
   char *calcpass;
   if (NULL == (secpass = getuserpw(username)))
     {
	bbftpd_log(BBFTPD_ERR,"Unknown user %s",username) ;
	bbftpd_msg_reply (MSG_BAD_NO_RETRY, "bbftpd version %s : %s", VERSION, bad_reply_msg);
	return -1 ;
     }
   calcpass = (char *) crypt(password,secpass->upw_passwd) ;
   return check_pass (calcpass, secpass->upw_passwd, username);
#else

   bbftpd_log(BBFTPD_ERR,"No Password user %s",username) ;
   bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : No pasword user %s", VERSION, username);
   return -1;

#endif
}

static int check_local_user_pass (char *username, char *password, struct passwd *uspass, char *bad_reply_msg)
{
   char *calcpass;

   if ((0 == strcmp(uspass->pw_passwd,"x")
	|| (0 == strcmp(uspass->pw_passwd,"X"))))
     return handle_shadow_pass (username, password, bad_reply_msg);

   calcpass = (char *) crypt(password,uspass->pw_passwd);
   return check_pass (calcpass, uspass->pw_passwd, username);
}

#ifdef USE_PAM
static int do_pam_auth (char *username, char *password)
{
   pam_handle_t *pamh=NULL;

   PAM_password = password ;

   if ((PAM_SUCCESS != (status = pam_start("bbftp", username, &PAM_conversation, &pamh)))
       || (PAM_SUCCESS != (status = pam_authenticate(pamh, PAM_SILENT)))
       || (PAM_SUCCESS != (status = pam_acct_mgmt(pamh, 0)))
# ifdef PAM_ESTABLISH_CRED
       || (PAM_SUCCESS != (status = pam_setcred(pamh, PAM_ESTABLISH_CRED)))
# else
       || (PAM_SUCCESS != (status = pam_setcred(pamh, PAM_CRED_ESTABLISH)))
# endif
      )
     {
	pam_end(pamh, 0);
	bbftpd_log(BBFTPD_ERR,"PAM error (%d) user %s", satus, username);
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : PAM Error", VERSION);
	return -1;
     }
   pam_end(pamh, PAM_SUCCESS);
    /*
    ** Reopen the lag as the pam functions close it
    */
   bbftpd_log_reopen ();
   return 0;
}
#endif				       /* USE_PAM */

#ifdef AFS
static int do_afs_auth (char *username, char *password, struct passwd *uspass)
{
   char *inst = "";
   char *calcpass;

   setpag();

   if (0 != ka_UserAuthenticate (username,inst,0,password,0,&calcpass))
     {
	bbftpd_log(BBFTPD_ERR,"ka_UserAuthenticate message : %s ",calcpass) ;

	if (-1 == check_local_user_pass (username, password, uspass, calcpass))
	  return -1;
     }

   return 0;
}
#endif				       /* AFS */

int bbftpd_loginsequence (struct message *msg)
{
    struct mess_sec msg_sec ;
    struct mess_rsa msg_rsa ;
    char username[MAXLEN] ;
    char password[MAXLEN] ;
    struct passwd    *uspass ;

    /*
    ** Read the crypt type.  The msglen should be CRYPTMESSLEN+RSAMESSLEN
    */
   if (msg->msglen != CRYPTMESSLEN + RSAMESSLEN)
     {
	bbftpd_log (BBFTPD_ERR, "MSG_LOG: Incorrect length.  Expected %lu, got %d",
		    CRYPTMESSLEN+RSAMESSLEN, msg->msglen);
	bbftpd_msg_reply (MSG_BAD, "%s", "Incorrect length for MSG_LOG command");
	return -1;
     }

   if ((-1 == bbftpd_msgread_bytes ((char *)&msg_sec, CRYPTMESSLEN))
       || (-1 == bbftpd_msgread_bytes ((char *)&msg_rsa, RSAMESSLEN)))
     return -1;

   switch (msg_sec.crtype)
     {
      case CRYPT_RSA_PKCS1_OAEP_PADDING :
	/*
	 ** RSA
	 */
	if (-1 == decodersapass (&msg_rsa, username ,password))
	  {
	     bbftpd_log(BBFTPD_ERR,"Decode user/password error") ;
	     bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : Decode user/password error", VERSION);
	     return -1 ;
	  }
	break;

      case NO_CRYPT :
	strncpy(username, msg_rsa.cryptuser, sizeof(username));
	username[sizeof(username)-1]='\0';
	strncpy(password, msg_rsa.cryptpass, sizeof(password));
	password[sizeof(password)-1]='\0';
	break ;

      default :
	bbftpd_log (BBFTPD_ERR,"Unkwown encryption %d",msg_sec.crtype) ;
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : Unknown encryption", VERSION);
	return -1 ;
    }

    /*
    ** Here we check the username and pass and set the default dir
    */
    if (NULL == (uspass = getpwnam(username)))
     {
        bbftpd_log(BBFTPD_ERR,"Unknown user %s",username) ;
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : Unknown user (%s) ", VERSION, username);
        return -1 ;
     }

   /*
    ** If the AFS flag is set it take precedence on all other mechanism
    */
#ifdef AFS
   if (-1 == do_afs_auth (username, password, uspass))
     return -1;
#elif defined(USE_PAM)
   if (-1 == do_pam_auth (username, password))
     return -1;
#else
   if (-1 == check_local_user_pass (username, password, uspass, "Incorrect password"))
     return -1;
#endif

   if (-1 == setgid (uspass->pw_gid))
     {
        bbftpd_log(BBFTPD_ERR,"Error setgid user %s : %s",username,strerror(errno)) ;
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : Unable to set gid: %s", VERSION, strerror(errno));
        return -1 ;
     }

    /* Initialize the group list. */
    if (0 == getuid())
     {
	if (-1 == initgroups(uspass->pw_name, uspass->pw_gid))
	  {
	     bbftpd_log(BBFTPD_WARNING,"Error Initialize the group list %s : %s",username,strerror(errno));
	     return -1 ;
	  }
	endgrent();
    }

    if (-1 == setuid(uspass->pw_uid))
     {
        bbftpd_log(BBFTPD_ERR,"Error setgid user %s : %s",username,strerror(errno)) ;
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : Unable to set uid: %s", VERSION, strerror(errno));
        return -1 ;
    }

    if ( uspass->pw_dir == NULL )
     {
        bbftpd_log(BBFTPD_ERR,"No home directory user %s : %s",username,strerror(errno)) ;
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : No home directory: %s", VERSION, strerror(errno));
        return -1 ;
    }
   if ( chdir(uspass->pw_dir) < 0)
     {
        bbftpd_log(BBFTPD_ERR,"Error chdir user %s : %s",username,strerror(errno)) ;
	bbftpd_msg_reply (MSG_BAD, "bbftpd version %s : Unable to change directory: %s", VERSION, strerror(errno));
        return -1 ;
     }

    bbftpd_log(BBFTPD_DEBUG,"User %s connected",username) ;
    strcpy(currentusername,username) ;
    return 0 ;
}
