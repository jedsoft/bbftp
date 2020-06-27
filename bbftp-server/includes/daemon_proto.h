/*
 * includes/daemon_proto.h
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

  
  
 daemon_proto.h v 2.1.0 2001/03/21  - Header creation

*****************************************************************************/
/*
** Prototype for utilities
*/
void checkfromwhere() ;
int checkprotocol() ;
void clean_child() ;
void exit_clean() ;
int bbftpd_checkendchild(int status) ;
void strip_trailing_slashes (char *path) ;
void free_all_var() ;
/*
** Prototypes for message routines
*/
int discardmessage(int sock,int msglen,int to) ;
int readmessage(int sock,char *buffer,int msglen,int to) ;
void reply(int n, char *str) ;
int writemessage(int sock,char *buffer,int msglen,int to) ;
/*
** Prototypes for signal routines
*/
void bbftpd_sigchld(int sig) ;
void bbftpd_sighup( int sig) ;
void bbftpd_sigterm(int sig) ;
int bbftpd_checkendchild(int status) ;
int bbftpd_setsignals() ;
int bbftpd_blockallsignals() ;
/*
** Prototype for sockets
*/

/*
** Prototype for global private authentication routines
*/
int bbftpd_private_receive_connection(int msglen) ;
int bbftpd_private_send(char *buffertosend, int buffertosendlength, char *logmessage) ;
int bbftpd_private_recv(char *buffertorecv, int lengthtorecv, char *logmessage) ;
/*
** Prototype for user private authentication routines
*/
int bbftpd_private_auth(char *logmessage) ;
/*
** Prototype for retr routines
*/
int bbftpd_retrcheckfile(char *filename,char *logmessage) ;
int bbftpd_retrlistdir(char *pattern,char **filelist,int *filelistlen,char *logmessage) ;
int bbftpd_retrtransferfile(char *filename,int simulation,char *logmessage) ;

int bbftpd_retrcheckfile_rfio(char *filename,char *logmessage) ;
int bbftpd_retrlistdir_rfio(char *pattern,char **filelist,int *filelistlen,char *logmessage) ;
int bbftpd_retrtransferfile_rfio(char *filename,int simulation,char *logmessage) ;
/*
** Prototype for store routines
*/
int bbftpd_storecheckoptions(char *logmessage) ;
int bbftpd_storechmod(char *filename,int mode,char *logmessage) ;
int bbftpd_storeclosecastfile(char *filename,char *logmessage) ;
int bbftpd_storecreatefile(char *filename, char *logmessage) ;
int bbftpd_storemkdir(char *dirname,char *logmessage,int recursif) ;
int bbftpd_storerename(char *newfilename,char *oldfilename,char *logmessage) ;
int bbftpd_storetransferfile(char *filename,int simulation,char *logmessage) ;
int bbftpd_storeunlink(char *filename) ;
int bbftpd_storeutime(char *filename,struct utimbuf *ftime,char *logmessage) ;

int bbftpd_storecheckoptions_rfio(char *logmessage) ;
int bbftpd_storechmod_rfio(char *filename,int mode,char *logmessage) ;
int bbftpd_storeclosecastfile_rfio(char *filename,char *logmessage) ;
int bbftpd_storecreatefile_rfio(char *filename, char *logmessage) ;
int bbftpd_storemkdir_rfio(char *dirname,char *logmessage,int recursif) ;
int bbftpd_storerename_rfio(char *newfilename,char *oldfilename,char *logmessage) ;
int bbftpd_storetransferfile_rfio(char *filename,int simulation,char *logmessage) ;
int bbftpd_storeunlink_rfio(char *filename) ;
/*
** Prototype for mains routines
*/
int bbftpd_cd(int sock,int msglen) ;
int bbftpd_list(char *pattern,char *logmessage) ;
int bbftpd_mkdir(int sock,int msglen) ;
int bbftpd_readcontrol(int msgcode,int msglen) ;

