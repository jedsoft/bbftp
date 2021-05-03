/*
 * bbftpd/bbftpd_readcontrol.c
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
         0  calling program return to main loop
        -1 calling program exit
 
 bbftpd_readcontrol.c   v 2.0.0 2000/12/19  - Routine creation
                        v 2.0.1 2001/04/23  - Correct indentation
                        v 2.0.2 2001/05/04  - Correct return code treatment
                                            - Correct case of file size 0
                        v 2.1.0 2001/05/30  - Correct syslog level
                                            - Change routines name
                        v 2.2.0 2001/11/21  - Remote COS received can be -1
               
*****************************************************************************/
#include <bbftpd.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
/* #include <syslog.h> */
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <netinet/in.h>

#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>
#include <version.h>

#include "_bbftpd.h"

#ifndef WORDS_BIGENDIAN
# ifndef HAVE_NTOHLL
my64_t    ntohll(my64_t v) ;
# endif
#endif

static int read_int (int msglen, const char *codestr, int *valp)
{
   int32_t v32;

   if (msglen != sizeof(int32_t))
     {
	bbftpd_log(BBFTPD_ERR, "%s: Expecting msglen=4, found %d\n", codestr, msglen);
	reply(MSG_BAD,"Expecting a 32bit integer");
	return -1;
     }
   if (-1 == bbftpd_msgread_int32 (&v32))
     {
	bbftpd_log(BBFTPD_ERR,"Error reading %s\n", codestr) ;
	reply(MSG_BAD,"Read failed") ;
	return -1 ;
     }
   *valp = v32;
   return 0;
}

/* Returns -2 if malloc failed, or -1 if the failure occured reading */
static int read_string (int msglen, const char *codestr, char **strp)
{
   char *str;

   if (msglen < 0)
     {
	bbftpd_log (BBFTPD_ERR, "%s: message string length is negative\n", codestr);
	reply(MSG_BAD, "invalid string length") ;
	return -1;
     }

   if (NULL == (str = (char *) malloc (msglen+1)))
     {
	bbftpd_log (BBFTPD_ERR, "%s: malloc failed\n", codestr);
	reply(MSG_BAD,"malloc failed") ;
	return -2;
     }

   if (-1 == bbftpd_msgread_bytes (str, msglen))
     {
	bbftpd_log (BBFTPD_ERR, "%s: Error reading string\n", codestr);
	reply(MSG_BAD,"read error") ;
	free (str);
     }
   str[msglen] = 0;
   *strp = str;
   return 0;
}

static int read_msg_store_v2 (int msglen, const char *codestr, struct mess_store_v2 *m)
{
   if (msglen != STORMESSLEN_V2)
     {
	bbftpd_log (BBFTPD_ERR, "%s: msg_store_v2 object has the wrong size, expected %d, got %d\n",
		    codestr, msglen, (int) STORMESSLEN_V2);
	reply(MSG_BAD, "Invalid size for MSG_STORE_V2") ;
	return -1;
     }

   if (-1 == bbftpd_msgread_bytes ((char *)m, msglen))
     {
	bbftpd_log (BBFTPD_ERR, "%s: Error reading msg_store_v2 object\n", codestr);
	reply(MSG_BAD,"read error") ;
	return -1;
     }

   m->filesize = ntohll(m->filesize);
   m->filemode = ntohl(m->filemode) ;
   /* m->lastaccess = m->lastaccess; */
   /* m->lastmodif = m->lastmodif; */
   m->sendwinsize = ntohl (m->sendwinsize);
   m->recvwinsize = ntohl (m->recvwinsize);
   m->buffersizeperstream = ntohl (m->buffersizeperstream);
   m->nbstream = ntohl (m->nbstream);

   return 0;
}

static int do_msg_chcos (struct message *msg)
{
   int val;

   bbftpd_log(BBFTPD_DEBUG,"Receiving MSG_CHCOS, msglen = %d", msg->msglen) ;

   if (-1 == read_int (msg->msglen, "MSG_CHCOS", &val))
     return -1;

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
   bbftpd_rfio_set_cos (val);

   if (val >= 0)
     {
	bbftpd_log(BBFTPD_DEBUG,"Cos set to %d", val) ;
	reply(MSG_OK,"COS set") ;
     }
   else
     {
	bbftpd_log(BBFTPD_DEBUG,"Cos received : %d", val);
	reply(MSG_OK,"COS not set") ;
     }
#else

   bbftpd_log(BBFTPD_DEBUG,"Cos received : %d, RFIO is not supported", val);
   reply(MSG_OK,"COS not set, RFIO is not supported") ;

#endif
   return 0 ;
}

static int do_msg_chumask (struct message *msg)
{
   int val;
   int oldumask, newumask ;
   char logmessage[1024] ;

   bbftpd_log(BBFTPD_DEBUG,"Receiving MSG_UMASK ") ;

   if (-1 == read_int (msg->msglen, "MSG_UMASK", &val))
     return -1;

   /*
    ** We reset the umask first then call twice the umask
    ** in order to see the real umask set
    */
   oldumask = umask(0) ;
   (void) umask(val);
   newumask = umask(val) ;
   sprintf(logmessage,"umask changed from %04o to %04o", oldumask, newumask) ;
   reply(MSG_OK, logmessage) ;
   return 0;
}

static int do_msg_close_conn (struct message *msg)
{
   (void) msg;
   return -1;
}

static int do_msg_list_v2 (struct message *msg)
{
   struct mess_dir *msg_dir;
   int status;

   bbftpd_log(BBFTPD_DEBUG, "Receiving MSG_LIST_V2") ;

   if (0 != read_string (msg->msglen, "MSG_LIST_V2", (char **)&msg_dir))
     return -1;

   /* The library transferoption variable is bigendian */
   transferoption  = msg_dir->transferoption ;
   bbftpd_log (BBFTPD_DEBUG, "Pattern = %s", msg_dir->dirname) ;

   status = bbftpd_list (msg_dir->dirname) ;
   free (msg_dir);
   return status;
}

static int do_msg_retr_v2 (struct message *msg)
{
   struct mess_store_v2 msg_store_v2 ;

   bbftpd_log(BBFTPD_DEBUG,"Receiving MSG_RETR_V2") ;
   if (-1 == read_msg_store_v2 (msg->msglen, "MSG_RETR_V2", &msg_store_v2))
     return -1;

   /*
    ** Set up the transfer parameter, we are going only to store 
    ** them
    */
   transferoption = msg_store_v2.transferoption;
   sendwinsize = msg_store_v2.sendwinsize;
   recvwinsize = msg_store_v2.recvwinsize;
   buffersizeperstream = msg_store_v2.buffersizeperstream;
   requestedstreamnumber = msg_store_v2.nbstream;

   bbftpd_log(BBFTPD_DEBUG,"transferoption         = %d ",transferoption) ;
   bbftpd_log(BBFTPD_DEBUG,"sendwinsize            = %d ",sendwinsize) ;
   bbftpd_log(BBFTPD_DEBUG,"recvwinsize            = %d ",recvwinsize) ;
   bbftpd_log(BBFTPD_DEBUG,"buffersizeperstream    = %d ",buffersizeperstream) ;
   bbftpd_log(BBFTPD_DEBUG,"requestedstreamnumber  = %d ",requestedstreamnumber) ;

   state = S_WAITING_FILENAME_RETR ;
   return 0;
}

static int do_msg_store_v2 (struct message *msg)
{
   struct mess_store_v2 msg_store_v2;

   bbftpd_log(BBFTPD_DEBUG,"Receiving MSG_STORE_V2") ;

   if (-1 == read_msg_store_v2 (msg->msglen, "MSG_STORE_V2", &msg_store_v2))
     return -1;

   /*
    ** Set up the transfer parameter, we are going only to store 
    ** them 
    */
   transferoption = msg_store_v2.transferoption ;
   filemode = msg_store_v2.filemode ;
   strncpy(lastaccess,msg_store_v2.lastaccess,8) ;
   lastaccess[8] = '\0' ;
   strncpy(lastmodif,msg_store_v2.lastmodif,8) ;
   lastmodif[8] = '\0' ;
   sendwinsize = msg_store_v2.sendwinsize ;
   recvwinsize = msg_store_v2.recvwinsize ;
   buffersizeperstream = msg_store_v2.buffersizeperstream ;
   requestedstreamnumber = msg_store_v2.nbstream ;
   filesize = msg_store_v2.filesize ;

   bbftpd_log(BBFTPD_DEBUG,"transferoption         = %d ",transferoption) ;
   bbftpd_log(BBFTPD_DEBUG,"filemode               = %d ",filemode) ;
   bbftpd_log(BBFTPD_DEBUG,"lastaccess             = %s ",lastaccess) ;
   bbftpd_log(BBFTPD_DEBUG,"lastmodif              = %s ",lastmodif) ;
   bbftpd_log(BBFTPD_DEBUG,"sendwinsize            = %d ",sendwinsize) ;
   bbftpd_log(BBFTPD_DEBUG,"recvwinsize            = %d ",recvwinsize) ;
   bbftpd_log(BBFTPD_DEBUG,"buffersizeperstream    = %d ",buffersizeperstream) ;
   bbftpd_log(BBFTPD_DEBUG,"requestedstreamnumber  = %d ",requestedstreamnumber) ;
   bbftpd_log(BBFTPD_DEBUG,"filesize               = %" LONG_LONG_FORMAT " ",filesize) ;

   state = S_WAITING_FILENAME_STORE ;
   return 0 ;
}

/* The caller must call free_all_var upon failure */
static int allocate_memory_for_transfer (void)
{
   size_t size = requestedstreamnumber * sizeof(int);

   if (NULL == (myports = (int *) malloc (size)))
     return -1;
   if (NULL == (mychildren = (int *) malloc (size)))
     return -1;
   if (NULL == (readbuffer = (char *) malloc (buffersizeperstream*1024)))
     return -1;

   if (TROPT_GZIP == (transferoption & TROPT_GZIP))
     {
	if (NULL == (compbuffer = (char *) malloc (buffersizeperstream*1024)))
	  return -1;
     }

   if (protocolversion == 3)
     {
	if (NULL == ((mysockets = (int *) malloc (requestedstreamnumber*sizeof(int)))))
	  return -1;
     }
   return 0;
}

static int init_and_send_msg_store_v2 (int send_ports)
{
   struct mess_store_v2 msg_store_v2;
   int code;

   msg_store_v2.filesize = ntohll(filesize) ;
   msg_store_v2.transferoption = transferoption ;
   msg_store_v2.filemode = ntohl(filemode) ;
   strncpy(msg_store_v2.lastaccess,lastaccess,8) ; msg_store_v2.lastaccess[8] = '0' ;
   strncpy(msg_store_v2.lastmodif,lastmodif,8) ; msg_store_v2.lastmodif[8] = '0' ;
   msg_store_v2.sendwinsize = ntohl(sendwinsize) ;
   msg_store_v2.recvwinsize = ntohl(recvwinsize) ;
   msg_store_v2.buffersizeperstream  = ntohl(buffersizeperstream) ;
   msg_store_v2.nbstream = ntohl(requestedstreamnumber) ;

   /* V2: active, V3: passive */
   code = (protocolversion == 2) ? MSG_TRANS_OK_V2 : MSG_TRANS_OK_V3;
   if (-1 == bbftpd_msgwrite_bytes (code, (char *)&msg_store_v2, STORMESSLEN_V2))
     {
	bbftpd_log (BBFTPD_ERR,"Error writing MSG_TRANS_OK_V2/3");
	return -1;
     }

   if (send_ports == 0) return 0;

   /* Passive mode:
    * The server creates the sockets and then send the resulting
    * ports to the client
    */
   if (-1 == bbftpd_getdatasock(requestedstreamnumber))
     {
	bbftpd_log (BBFTPD_ERR,"Error creating data sockets") ;
	return -1 ;
     }
   /* At this point, myports has been initialized.  Convert them
    * to network byte order and send them.  After that, the
    * myports array will not be needed.  So byteswap them in place.
    * This is accomplished by the _bbftpd_write_int32_array function.
    */

   if (-1 == _bbftpd_write_int32_array (myports, requestedstreamnumber))
     {
	bbftpd_log(BBFTPD_ERR,"Error writing MSG_TRANS_OK_V3 (ports)") ;
	return -1;
     }

   return 0;
}

static int do_msg_filename_store (struct message *msg)
{
   char logmessage[1024];
   const char *errmsg = NULL;
   int realfile_buflen;
   int status, send_ports;


   if (msg->msglen == 0)
     {
	bbftpd_log(BBFTPD_ERR,"Filename length null") ;
	return -1 ;
     }

   if (0 != read_string (msg->msglen, "MSG_FILENAME", &curfilename))
     {
	free_all_var ();
	return -1;
     }

   realfile_buflen = msg->msglen + 30;
   /* tmp name = <name>.bbftp.tmp.<hostname in 10 chars>.<pid in 5 chars> */
   if (NULL == (realfilename = (char *)malloc(realfile_buflen)))
     {
	bbftpd_log(BBFTPD_ERR,"Error allocating memory for realfilename : %s",strerror(errno)) ;
	free_all_var() ;
	return -1 ;
     }

   bbftpd_log (BBFTPD_DEBUG, "Request to store file %s", curfilename);

   status = -1;
   errmsg = "Fail to store : invalid options";
   if ((transferoption & TROPT_RFIO) == TROPT_RFIO)
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
	status = bbftpd_storecheckoptions_rfio ();
#else
	errmsg = "Fail to store : RFIO not supported";
#endif
     }
   else
     status = bbftpd_storecheckoptions ();

   if (status == -1)
     {
	bbftpd_log (BBFTPD_ERR, "%s\n", errmsg);
	reply (MSG_BAD_NO_RETRY, errmsg);
	free_all_var() ;
	state = S_LOGGED ;
	return 0;
     }

   /* The options are ok */

   if ( (transferoption & TROPT_TMP ) == TROPT_TMP )
     {
	/*
	 ** Caculate tempfile name if TROPT_TMP is set
	 ** To avoid multiple access to temp file in HPSS
	 ** file is called .bbftp.tmp.$host.$pid
	 */
	char hostname[10 + 1];
	if (-1 == gethostname(hostname, sizeof(hostname)))
	  strcpy (hostname, "-");
	hostname[sizeof(hostname) - 1] = '\0';
	snprintf (realfilename,realfile_buflen, "%s.bbftp.tmp.%s.%d", curfilename, hostname, getpid());
     }
   else strcpy (realfilename, curfilename);

   /*
    ** Create the file
    */
   status = bbftpd_storecreatefile (realfilename, logmessage, sizeof(logmessage));
   if (status != 0)
     {
	bbftpd_log (BBFTPD_ERR,"%s\n", logmessage) ;
	reply(((status < 0) ? MSG_BAD_NO_RETRY : MSG_BAD), logmessage);
	free_all_var() ;
	state = S_LOGGED ;
	return 0 ;
     }

   if ( filesize == 0 )
     {
	if (0 == bbftp_store_process_transfer (realfilename, curfilename, 1))
	  reply(MSG_OK,"");

	free_all_var ();
	state = S_LOGGED ;
	return 0 ;
     }

   if (-1 == allocate_memory_for_transfer ())
     {
	free_all_var ();
	bbftpd_storeunlink(realfilename) ;
	errmsg = "Unable to allocate memory for the transfer";
	bbftpd_log (BBFTPD_ERR, "%s\n", errmsg);
	reply (MSG_BAD, errmsg);
	state = S_LOGGED;
	return 0;
     }

   send_ports = (protocolversion >= 3);
   if (-1 == init_and_send_msg_store_v2 (send_ports))
     {
	bbftpd_storeunlink (realfilename);
	free_all_var ();
	return -1;
     }
   state = S_WAITING_STORE_START ;
   return 0 ;
}


static int do_msg_filename_retr (struct message *msg)
{
   char logmessage[1024] ;
   const char *errmsg = NULL;
   int send_ports;
   int status;

   bbftpd_log(BBFTPD_DEBUG,"Receiving MSG_FILENAME in S_WAITING_FILENAME_RETR state") ;

   if (msg->msglen == 0)
     {
	bbftpd_log(BBFTPD_ERR,"Filename length null") ;
	return -1 ;
     }

   if (0 !=  read_string (msg->msglen, "MSG_FILENAME", &curfilename))
     {
	free_all_var ();
	return -1;
     }

   bbftpd_log(BBFTPD_DEBUG,"Request to retreive file %s",curfilename) ;

   /*
    ** Create the file
    */
   status = bbftpd_retrcheckfile (curfilename, logmessage, sizeof(logmessage));
   if (status != 0)
     {
	bbftpd_log(BBFTPD_ERR, "%s\n", logmessage) ;
	reply(((status < 0) ? MSG_BAD_NO_RETRY : MSG_BAD), logmessage);
	free_all_var() ;
	state = S_LOGGED ;
	return 0 ;
     }

   if (filesize == 0)
     {
	if (-1 == init_and_send_msg_store_v2 (0))
	  {
	     bbftpd_storeunlink(realfilename) ;
	     free_all_var() ;
	     return -1 ;
	  }

	bbftpd_log (BBFTPD_NOTICE, "GET %s %s 0 0 0.0 0.0", currentusername, curfilename);
	free_all_var() ;
	state = S_WAITING_CREATE_ZERO ;
	return 0;
     }

   if (-1 == allocate_memory_for_transfer ())
     {
	bbftpd_storeunlink(realfilename) ;
	free_all_var ();
	errmsg = "Unable to allocated memory for the transfer";
	bbftpd_log (BBFTPD_ERR, "%s\n", errmsg);
	reply (MSG_BAD, errmsg);
	state = S_LOGGED;
	return 0;
     }

   send_ports = (protocolversion >= 3);
   if (-1 == init_and_send_msg_store_v2 (send_ports))
     {
	bbftpd_storeunlink(realfilename) ;
	free_all_var() ;
	return -1 ;
     }

   state = S_WAITING_RETR_START;
   return 0 ;
}

static int do_msg_trans_start (struct message *msg, int simulate, int is_store, int is_active)
{
   char logmessage[1024] ;
   int retcode ;

   if ((unsigned long) msg->msglen != requestedstreamnumber * sizeof(int32_t))
     {
	if (is_store) bbftpd_storeunlink(realfilename);
	free_all_var() ;
	reply(MSG_BAD,"Inconsistency between MSG_TRANS_START* and message length");
	bbftpd_log(BBFTPD_ERR,"Inconsistency between MSG_TRANS_START* and message length");
	return -1 ;
     }
   if (is_active)
     {
	/*
	 ** Retreive port numbers
	 */
	if (-1 == bbftpd_msgread_int32_array (myports, requestedstreamnumber))
	  {
	     if (is_store) bbftpd_storeunlink (realfilename);
	     free_all_var() ;
	     bbftpd_log(BBFTPD_ERR,"Error reading MSG_TRANS_START_V2") ;
	     return -1 ;
	  }
     }

   if (is_store)
     retcode = bbftpd_storetransferfile(realfilename,simulate,logmessage);
   else
     retcode = bbftpd_retrtransferfile(curfilename,simulate,logmessage);

   if (retcode == 0) return 0;
   if (retcode > 0)
     {
	/* recoverable error */
	if (is_store) bbftpd_storeunlink(realfilename) ;
	free_all_var() ;
	state = S_LOGGED ;
	return 0 ;
     }

   /* Failed */
   bbftpd_log(BBFTPD_ERR,"retrtransferfile retcode < 0") ;
   if (is_store) bbftpd_storeunlink (realfilename) ;
   free_all_var() ;
   state = S_LOGGED ;
   return -1 ;
}

static int do_msg_cd (struct message *msg)
{
   char buf[1024];
   struct mess_dir *msg_dir;
   char *dir;

   /* If we cannot change a directory, then all subsequent commands will be affected.
    * This should be a fatal error where processing a script that involves subsequent transfers
    * If the memory allocation failed, then the server in trouble.  So just return a fatal error.
    */
   if (0 !=  read_string (msg->msglen, "MSG_CHDIR_V2", (char **)&msg_dir))
     return -1;

   transferoption = msg_dir->transferoption;
   dir = msg_dir->dirname;

   if (TROPT_RFIO == (transferoption & TROPT_RFIO))
     {
	char *err = "Changing directory is not allowed under RFIO";
	bbftpd_log (BBFTPD_ERR, "%s\n", err);
	reply (MSG_BAD_NO_RETRY, err);
	free (msg_dir);
	return -1;
     }
   bbftpd_log(BBFTPD_DEBUG, "Changing directory to %s", dir) ;

   if (-1 == chdir (dir))
     {
	int code, e = errno;

	if ((e == EACCES) || (e == ELOOP) || (e == ENAMETOOLONG)
	    || (e == ENOTDIR) || (e == ENOENT))
	  code = MSG_BAD_NO_RETRY;
	else
	  code = MSG_BAD;

	bbftpd_msg_reply (code, "Error changing directory %s : %s", dir, strerror(e));
	bbftpd_log (BBFTPD_ERR, "Error changing directory %s : %s", dir, strerror(e));

	free (msg_dir);
	return -1;
     }

   if (NULL == getcwd (buf, sizeof(buf)))
     {
	int e = errno;
	bbftpd_msg_reply (MSG_BAD, "Unable to get the current directory: %s", strerror(e));
	bbftpd_log (BBFTPD_ERR, "getcwd failed: %s", strerror(e));
	free (msg_dir);
	return -1;
     }
   free (msg_dir);
   bbftpd_msg_reply (MSG_OK, "%s", buf);
   return 0;
}

static int do_msg_mkdir (struct message *msg)
{
   char logmsg[1024];
   struct mess_dir *msg_dir;
   char *dir;
   int recurse, status;

   /* If we cannot change a directory, then all subsequent commands will be affected.
    * This should be a fatal error where processing a script that involves subsequent transfers
    * If the memory allocation failed, then the server in trouble.  So just return a fatal error.
    */
   if (0 !=  read_string (msg->msglen, "MSG_MKDIR_V2", (char **)&msg_dir))
     return -1;

   transferoption = msg_dir->transferoption;
   dir = msg_dir->dirname;

   recurse = (TROPT_DIR == (transferoption & TROPT_DIR));
   status = bbftpd_storemkdir (dir, logmsg, sizeof(logmsg), recurse);
   if (status != 0)
     {
	reply (((status < 0) ? MSG_BAD_NO_RETRY : MSG_BAD), logmsg);
	bbftpd_log (BBFTPD_ERR, "MSG_MKDIR_V2: %s", logmsg);
	free (msg_dir);
     }

   bbftpd_msg_reply (MSG_OK, "Directory %s created", dir);
   free (msg_dir);

   return 0;
}

static int do_msg_rm (struct message *msg)
{
   struct mess_dir *msg_file;
   char *file;
   int status;

   status = read_string (msg->msglen, "MSG_RM", (char **)&msg_file);
   if (status < 0)
     {
	if (status == -2) return 0;
	return -1;
     }

   file = msg_file->dirname;

   status = bbftpd_storeunlink (file);
   if (status != 0)
     bbftpd_msg_reply (MSG_BAD_NO_RETRY, "Unable to delete %s", file);
   else
     bbftpd_msg_reply (MSG_OK, "%s has been deleted", file);

   free (msg_file);
   return status;
}


static int do_msg_stat (struct message *msg)
{
   struct mess_dir *msg_file;
   int status;

   status = read_string (msg->msglen, "MSG_STAT", (char **)&msg_file);
   if (status < 0)
     {
	if (status == -2) return 0;
	return -1;
     }

   status = bbftpd_stat (msg_file);
   /* Error messages and replies handled by bbftpd_stat */

   free (msg_file);
   return status;
}

static int do_msg_statfs (struct message *msg)
{
   struct mess_dir *msg_file;
   int status;

   status = read_string (msg->msglen, "MSG_STAT", (char **)&msg_file);
   if (status < 0)
     {
	if (status == -2) return 0;
	return -1;
     }

   status = bbftpd_statfs (msg_file);
   /* Error messages and replies handled by bbftpd_statfs */

   free (msg_file);
   return status;
}

static int do_invalid_msg_for_state (struct message *msg, const char *state_name)
{
   bbftpd_log (BBFTPD_ERR,"Unknown message %d in state %s", msg->code, state_name);
   /* retcode = discardmessage (incontrolsock,msglen,recvcontrolto) ; */
   reply (MSG_BAD_NO_RETRY, "Unknown message") ;
   return -1;
}


static int handle_logged (struct message *msg)
{
   /*
    ** Case S_LOGGED :
    **      Nothing is going on, so message accepted are :
    **
    **         MSG_CHCOS
    **         MSG_CHDIR_V2
    **         MSG_CHUMASK
    **         MSG_CLOSE_CONN
    **         MSG_LIST_V2
    **         MSG_MKDIR_V2
    **         MSG_RETR_V2
    **         MSG_STORE_V2
    **
    */
   switch (msg->code)
     {
      case MSG_CHCOS:
	return do_msg_chcos (msg);

      case MSG_CHDIR_V2:
	return do_msg_cd (msg);

      case MSG_CHUMASK:
	return do_msg_chumask (msg);

      case MSG_CLOSE_CONN:
	return do_msg_close_conn (msg);

      case MSG_LIST_V2:
	return do_msg_list_v2 (msg);

      case MSG_MKDIR_V2:
	return do_msg_mkdir (msg);

      case MSG_RM:
	return do_msg_rm (msg);

      case MSG_STAT:
	return do_msg_stat (msg);

      case MSG_DF:
	return do_msg_statfs (msg);

      case MSG_RETR_V2:
	return do_msg_retr_v2 (msg);

      case MSG_STORE_V2:
	return do_msg_store_v2 (msg);

      default :
	return do_invalid_msg_for_state (msg, "S_LOGGED");
     }
}

static int handle_receiving (struct message *msg)
{
   char logmessage[1024];
   switch (msg->code)
     {
      case MSG_ABORT:
	bbftpd_log(BBFTPD_ERR,"Receive MSG_ABORT while receiving") ;
	bbftpd_storeclosecastfile(realfilename,logmessage) ;
	bbftpd_storeunlink(realfilename) ;
	return -1;

      default:
	bbftpd_log(BBFTPD_ERR,"Unknown message in S_RECEIVING state : %d",msg->code) ;
	bbftpd_storeclosecastfile(realfilename,logmessage) ;
	bbftpd_storeunlink(realfilename) ;
	return -1 ;
     }
}


static int handle_sending (struct message *msg)
{
   switch (msg->code)
     {
      case MSG_ABORT:
	bbftpd_log(BBFTPD_ERR,"Receive MSG_ABORT while sending\n") ;
	return -1;

      default:
	bbftpd_log (BBFTPD_ERR,"Unknown message %d in S_SENDING state\n", msg->code);
	return -1;
     }
}

static int handle_waiting_filename_store (struct message *msg)
{
   switch (msg->code)
     {
      case MSG_FILENAME:
	return do_msg_filename_store (msg);

      default:
	return do_invalid_msg_for_state (msg, "S_WAITING_FILENAME_STORE");
     }
}

static int handle_waiting_filename_retr (struct message *msg)
{
   switch (msg->code)
     {
      case MSG_FILENAME:
	return do_msg_filename_retr (msg);

      default :
	return do_invalid_msg_for_state (msg, "S_WAITING_FILENAME_RETR");
     }
}

static int handle_waiting_store_start (struct message *msg)
{
   switch (msg->code)
     {
      case MSG_TRANS_SIMUL:
	return do_msg_trans_start (msg, 1, 1, 1);
      case MSG_TRANS_START_V2:
	return do_msg_trans_start (msg, 0, 1, 1);
      case MSG_TRANS_SIMUL_V3:
	return do_msg_trans_start (msg, 1, 1, 0);
      case MSG_TRANS_START_V3:
	return do_msg_trans_start (msg, 0, 1, 0);

      case MSG_ABORT:
	bbftpd_log(BBFTPD_ERR,"Receive ABORT message") ;
	bbftpd_storeunlink(realfilename) ;
	free_all_var() ;
	state = S_LOGGED ;
	return 0;

      default:
	bbftpd_storeunlink(realfilename) ;
	free_all_var() ;
	return do_invalid_msg_for_state (msg, "S_WAITING_STORE_START");
     }
}

static int handle_waiting_retr_start (struct message *msg)
{
   switch (msg->code)
     {
      case MSG_TRANS_SIMUL:
	return do_msg_trans_start (msg, 1, 0, 1);
      case MSG_TRANS_START_V2:
	return do_msg_trans_start (msg, 0, 0, 1);
      case MSG_TRANS_SIMUL_V3:
	return do_msg_trans_start (msg, 1, 0, 0);
      case MSG_TRANS_START_V3:
	return do_msg_trans_start (msg, 0, 0, 0);

      case MSG_ABORT:
	bbftpd_log(BBFTPD_ERR,"Receive ABORT message") ;
	free_all_var() ;
	state = S_LOGGED ;
	return 0 ;

      default:
	return do_invalid_msg_for_state (msg, "S_WAITING_RETR_START");
     }
}

static int handle_create_zero (struct message *msg)
{
   switch (msg->code)
     {
      case MSG_CREATE_ZERO:
	state = S_LOGGED;
	return 0;

      case MSG_ABORT:
	state = S_LOGGED;
	return 0;

      default:
	return do_invalid_msg_for_state (msg, "S_WAITING_CREATE_ZERO");
     }
}

/*
** Loop for the v2 protocol (also available for v3)
*/

int bbftp_run_protocol_2 (void)
{
   /*
    ** Initialize the variables
    */
   mychildren = NULL ;
   nbpidchild = 0 ;
   curfilename = NULL ;
   curfilenamelen = 0 ;

    /*
    ** Set up v2 handlers
    */
    if ( bbftpd_setsignals() < 0 ) {
        exit(1) ;
    }
    /*
    ** Set the umask ; first unset it and then reset to the default value
    */
    umask(0) ;
    umask (BBFTPD_DEFAULT_UMASK);
    while (1)
     {
	struct message msg;
	int timeout, status;

        switch (state)
	  {
	   case S_WAITING_STORE_START :
	   case S_WAITING_FILENAME_STORE :
	   case S_WAITING_FILENAME_RETR :
	     /*
	      ** Timer of 10s between XX_V2 and FILENAME_XX
	      */
	     timeout = 10;
	     break;

            case S_SENDING :
            case S_RECEIVING :
	     timeout = -1;	       /* block indefinitely */
	     break;

            default:
                /*
                ** Timer of 900s between commands
                */
	       timeout = 900;
	       break ;
	  }

	while (-1 == (status = bbftpd_msg_pending (timeout)))
	  {
	     /* Should we exit?  */
	     sleep (5);
	  }

	if (status == 0)	       /* timer expired */
	  {
	     if ( state == S_WAITING_STORE_START )
	       bbftpd_storeunlink(realfilename) ;

	     return 0;
	  }

	/* Otherwise there is input on the channel */
	if (-1 == bbftpd_msgread_msg (&msg))
	  {
	     if ( state == S_WAITING_STORE_START  || state == S_RECEIVING) 
	       {
		  bbftpd_storeunlink (realfilename) ;
		  sleep(5) ;
	       }
	     return 0;
	  }

	switch (state)
	  {
	   case S_LOGGED:
	     status = handle_logged (&msg);
	     break;
	   case S_RECEIVING:
	     status = handle_receiving (&msg);
	     break;
	   case S_SENDING:
	     status = handle_sending (&msg);
	     break;
           case S_WAITING_FILENAME_STORE:
	     status = handle_waiting_filename_store (&msg);
	     break;
           case S_WAITING_FILENAME_RETR:
	     status = handle_waiting_filename_retr (&msg);
	     break;
	   case S_WAITING_STORE_START:
	     status = handle_waiting_store_start (&msg);
	     break;
	   case S_WAITING_RETR_START:
	     status = handle_waiting_retr_start (&msg);
	     break;
	   case S_WAITING_CREATE_ZERO:
	     status = handle_create_zero (&msg);
	     break;

	   default:
	     break;
	  }
	if (status == -1)
	  return status;
     }
}

int bbftp_run_protocol_3 (void)
{
   return bbftp_run_protocol_2 ();
}
