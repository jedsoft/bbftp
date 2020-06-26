/*
 * includes/client_proto.h
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

  
  
 client_proto.h v 2.0.0 2001/03/21    - Header creation
		v 2.2.1 2013/02/01    - Fix implicit declaration issue

*****************************************************************************/
/*
** Prototype for store routines
*/
int bbftp_storecheckfile(char *filename,char *logmessage,int *errcode) ;
int bbftp_storechmod(char *filename,int mode,char *logmessage,int  *errcode) ;
int bbftp_storeclosecastfile(char *filename,char *logmessage) ;
int bbftp_storecreatefile(char *filename,char *logmessage, int *errcode)  ;
int bbftp_storemkdir(char *dirname,char *logmessage,int recursif,int *errcode) ;
int bbftp_storerename(char *newfilename,char *oldfilename,char *logmessage,int  *errcode) ;
int bbftp_storetransferfile(char *filename,char *logmessage, int *errcode)  ;
int bbftp_storeunlink(char *filename) ;

int bbftp_storecheckfile_rfio(char *filename,char *logmessage,int *errcode) ;
int bbftp_storechmod_rfio(char *filename,int mode,char *logmessage,int  *errcode) ;
int bbftp_storeclosecastfile_rfio(char *filename,char *logmessage) ;
int bbftp_storecreatefile_rfio(char *filename,char *logmessage, int *errcode)  ;
int bbftp_storemkdir_rfio(char *dirname,char *logmessage,int recursif,int *errcode) ;
int bbftp_storerename_rfio(char *newfilename,char *oldfilename,char *logmessage,int  *errcode) ;
int bbftp_storetransferfile_rfio(char *filename,char *logmessage, int *errcode)  ;
int bbftp_storeunlink_rfio(char *filename) ;
/*
** Prototype for retr routines
*/
int bbftp_retrlistdir(char *pattern,char **filelist,int *filelistlen,char *logmessage,int *errcode) ;
int bbftp_retrcheckdir(char *remotefilename,char *localdir,int *errcode) ;
int bbftp_retrcheckfile(char *filename,char *logmessage,int *errcode) ;
int bbftp_retrtransferfile(char *filename,char *logmessage,int *errcode)  ;

int bbftp_retrlistdir_rfio(char *pattern,char **filelist,int *filelistlen,char *logmessage,int *errcode) ;
int bbftp_retrcheckdir_rfio(char *remotefilename,char *localdir,int *errcode) ;
int bbftp_retrcheckfile_rfio(char *filename,char *logmessage,int *errcode) ;
int bbftp_retrtransferfile_rfio(char *filename,char *logmessage,int *errcode)  ;
/*
** Prototype for mains routines
*/
int bbftp_cd(char *dirpath,int  *errcode) ;
int bbftp_lcd(char *dirpath,int  *errcode) ;
int bbftp_list(char *line,char **filelist,int *filelistlen,int *errcode) ;
int bbftp_get(char *remotefilename,int  *errcode) ;
int bbftp_mget(char *remotefile,char *localdir, int  *errcode) ;
int bbftp_mkdir(char *dirpath,int  *errcode) ;
int bbftp_mput(char *localfile,char *remotedir, int  *errcode) ;
int bbftp_put(char *remotefilename,int  *errcode) ;
int bbftp_setremotecos(int cos,int  *errcode) ;
int bbftp_setremoteumask(int mask,int  *errcode) ;
/*
** Prototype for utilities
*/
void strip_trailing_slashes (char *path) ;
void bbftp_close_control()  ;
void bbftp_free_all_var()  ;
void bbftp_clean_child() ;
void printmessage(FILE *strm , int flag, int errcode, int tok, char *fmt, ...) ;
void Usage() ;
/*
** Prototype for signal routines
*/
void bbftp_sigchld(int sig) ;
void bbftp_sigint(int sig) ;
void bbftp_sigterm(int sig) ;
void blockallsignals() ;
void bbftp_setsignals() ;
void bbftp_setsignal_sigchld() ;
void bbftp_unsetsignal_sigchld() ;
/*
** Prototype for connection routines
*/
/* static int splitargs (const char* s, char** argv, size_t maxargs, char* buf, size_t maxbuf); */
int connectviassh() ;
int connectviapassword() ;
int todoafterconnection();
void reconnecttoserver() ;
/*
** Prototype for sockets routines
*/
int bbftp_createdatasock(int portnumber/*,char *logmessage*/);
int discardmessage(int sock,int msglen,int to,int fromchild) ;
int discardandprintmessage(int sock,int to,int fromchild) ;
int getdatasock(int nbsock, int *errcode) ;
int readmessage(int sock,char *buffer,int msglen,int to,int fromchild) ;
int sendproto() ;
int writemessage(int sock,char *buffer,int msglen,int to,int fromchild) ;
/*
** Prototype for command routines
*/
int treatcommand(char *buffercmd);

