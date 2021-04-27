/*
 * bbftpc/bbftp_retr.c
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


 bbftp_retr.c v 2.0.0 2000/12/19    - Routines creation
              v 2.0.1 2001/04/19    - Correct indentation
                                    - Port to IRIX
                                    - Close all file descriptor not needed in child
                                      (bug shown by Alvise Dorigo)
              v 2.0.2 2001/05/12    - Change phylosophie due to CASTOR
                                      rewinddir bug
			   
*****************************************************************************/
#include <bbftp.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
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
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <common.h>
#include <client.h>
#include <client_proto.h>
#include <structures.h>

#ifdef WITH_GZIP
#include <zlib.h>
#endif

#if !defined (O_BINARY)
# define O_BINARY 0
#endif

/*******************************************************************************
** bbftp_retrlisdir :                                                          *
**                                                                             *
**      Routine to list a directory                                            *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**           1  recoverable error                                              *
**                                                                             *
*******************************************************************************/

int bbftp_retrlistdir(char *pattern,char **filelist,int *filelistlen,char *logmessage,int *errcode)
{
    int     lastslash ;
    char    *pointer ;
    char    *dirpath ;
    DIR     *curdir ;
#ifdef STANDART_FILE_CALL
    struct dirent *dp ;
    struct stat statbuf;
#else
#ifdef STANDART_READDIR_CALL
    struct dirent *dp ;
#else
    struct dirent64 *dp ;
#endif
    struct stat64 statbuf ;
#endif
    int     lengthtosend;
    int     numberoffile ;
    /*
    ** Structure to keep the filenames
    */
    struct keepfile {
        char    *filename ;
        char    filechar[3] ;
        struct  keepfile *next ;
    } ;
    struct keepfile *first_item ;
    struct keepfile *current_item ;
    struct keepfile *used_item ;
    char    *filepos ;
    int     i ;
    /*
    ** Check if it is a rfio creation
    */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_retrlistdir_rfio(pattern,filelist,filelistlen,logmessage,errcode) ;
    }
#endif
    if ( (dirpath = (char *) malloc ( strlen(pattern) + 1 + 3 )) == NULL ) {
        sprintf(logmessage,"Error allocating memory for dirpath %s",strerror(errno)) ;
        *errcode = 35 ;
        return -1 ;
    } 
    pointer = pattern ;
    lastslash = strlen(pointer) - 1 ;
    while ( lastslash >= 0 && pointer[lastslash] != '/') lastslash-- ;
    if ( lastslash == -1 ) {
        /*
        ** No slash in the path, so this is a pattern and we have
        ** to opendir .
        */
        if ( (curdir = opendir(".")) == NULL ) {
            sprintf(logmessage,"opendir . failed : %s ",strerror(errno)) ;
            *errcode = 86 ;
            free(dirpath) ;
            return -1 ;
        }
        strcpy(dirpath,"./") ;
    } else if ( lastslash == 0 ) {
        /*
        ** A slash in first position so we are going to open the
        ** / directory
        */
        if ( (curdir = opendir("/")) == NULL ) {
            sprintf(logmessage,"opendir / failed : %s ",strerror(errno)) ;
            *errcode = 86 ;
            free(dirpath) ;
            return -1 ;
        }
        strcpy(dirpath,"/") ;
        pointer++ ;
    } else if ( lastslash == (int)strlen(pointer) - 1 ) {
        /*
        ** The filename end with a slash ..... error
        */
        sprintf(logmessage,"Pattern %s ends with a /",pattern) ;
        *errcode = 87 ;
        free(dirpath) ;
        return -1;
    } else {
        pointer[lastslash] = '\0';
        /*
        ** Srip unnecessary / at the end of dirpath and reset 
        ** only one
        */
        strcpy(dirpath,pointer) ;
        strip_trailing_slashes(dirpath) ;
        dirpath[strlen(dirpath)+1] = '\0';
        dirpath[strlen(dirpath)] = '/';
        if ( (curdir = opendir(dirpath)) == NULL ) {
            sprintf(logmessage,"opendir %s failed : %s ",dirpath,strerror(errno)) ;
            *errcode = 86 ;
            free(dirpath) ;
            return -1 ;
        }
        for ( i = 0 ; i <= lastslash ; i++ ) pointer++ ;
    }
    /*
    ** At this stage pointer point to the pattern and curdir
    ** is the opened directory and dirpath contain the
    ** directory name
    */
    /*
    ** As we are using the fnmatch routine we are obliged to
    ** first count the number of bytes we are sending.
    */
    lengthtosend = 0 ;
    numberoffile = 0 ;
    errno = 0 ;
    first_item = NULL ;
#ifdef STANDART_FILE_CALL
    while ( (dp = readdir(curdir) ) != NULL)
#else
# ifdef STANDART_READDIR_CALL 
    while ( (dp = readdir(curdir) ) != NULL)
# else
       while ( (dp = readdir64(curdir) ) != NULL)
# endif
#endif
	 {
        if ( fnmatch(pointer, dp->d_name,0) == 0) {
            numberoffile++ ;
            lengthtosend = lengthtosend +  strlen(dirpath) + strlen(dp->d_name) + 1 + 3 ;
            if ( ( current_item = (struct keepfile *) malloc( sizeof(struct keepfile)) ) == NULL ) {
                sprintf(logmessage,"Error getting memory for structure : %s",strerror(errno)) ;
                *errcode = 35 ;
                current_item = first_item ;
                while ( current_item != NULL ) {
                    free(current_item->filename) ;
                    used_item = current_item->next ;
                    free(current_item) ;
                    current_item = used_item ;
                }
                closedir(curdir) ;
                free(dirpath) ;
                return -1 ;
            }
            if ( ( current_item->filename = (char *) malloc( strlen(dirpath) + strlen(dp->d_name) + 1) ) == NULL ) {
                sprintf(logmessage,"Error getting memory for filename : %s",strerror(errno)) ;
                *errcode = 35 ;
                /*
                ** Clean memory
                */
                free(current_item) ;
                current_item = first_item ;
                while ( current_item != NULL ) {
                    free(current_item->filename) ;
                    used_item = current_item->next ;
                    free(current_item) ;
                    current_item = used_item ;
                }
                closedir(curdir) ;
                free(dirpath) ;
                return -1 ;
            }
            current_item->next = NULL ;
            if ( first_item == NULL ) {
                first_item = current_item ;
                used_item = first_item ;
            } else {
                used_item = first_item ;
                while ( used_item->next != NULL ) used_item = used_item->next ;
                used_item->next = current_item ;
            }
            sprintf(current_item->filename,"%s%s",dirpath,dp->d_name) ;
#ifdef STANDART_FILE_CALL
            if ( lstat(current_item->filename,&statbuf) < 0 )
#else
            if ( lstat64(current_item->filename,&statbuf) < 0 )
#endif
	       {
                sprintf(logmessage,"Error lstating file %s",current_item->filename) ;
                *errcode = 89 ;
                current_item = first_item ;
                while ( current_item != NULL ) {
                    free(current_item->filename) ;
                    used_item = current_item->next ;
                    free(current_item) ;
                    current_item = used_item ;
                }
                closedir(curdir) ;
                free(dirpath) ;
                return -1 ;
            }
            if ( (statbuf.st_mode & S_IFLNK) == S_IFLNK) {
                current_item->filechar[0] = 'l' ;
#ifdef STANDART_FILE_CALL
                if ( stat(current_item->filename,&statbuf) < 0 )
#else
                if ( stat64(current_item->filename,&statbuf) < 0 )
#endif
		   {
                    /*
                    ** That means that the link refer to an unexisting file
                    */
                    current_item->filechar[1] = 'u' ;
                } else {
                    if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
                        current_item->filechar[1] = 'd' ;
                    } else {
                        current_item->filechar[1] = 'f' ;
                    }
                }
            } else {
                current_item->filechar[0] = ' ' ;
                if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
                    current_item->filechar[1] = 'd' ;
                } else {
                    current_item->filechar[1] = 'f' ;
                }
            }
            current_item->filechar[2] = '\0' ;
        }
        errno = 0 ;
    }
    /*
    ** Check errno in case of error during readdir
    ** for the following readir we are not going to check
    ** so that may be a cause of problem
    */
    if ( errno != 0 ) {
        sprintf(logmessage,"Error on readdir %s : %s ",dirpath,strerror(errno)) ;
        *errcode = 88 ;
        closedir(curdir) ;
        current_item = first_item ;
        while ( current_item != NULL ) {
            free(current_item->filename) ;
            used_item = current_item->next ;
            free(current_item) ;
            current_item = used_item ;
        }
        free(dirpath) ;
        return -1 ;
    }

    /*
    ** Check if numberoffile is zero and reply now in this
    ** case 
    */
    if ( numberoffile == 0 ) {
        *filelistlen = 0 ;
        closedir(curdir) ;
        current_item = first_item ;
        while ( current_item != NULL ) {
            free(current_item->filename) ;
            used_item = current_item->next ;
            free(current_item) ;
            current_item = used_item ;
        }
        free(dirpath) ;
        return 0 ;
    }
    /*
    ** Now everything is ready so prepare the answer
    */
    if ( ( *filelist = (char *) malloc (lengthtosend) ) == NULL ) {
        sprintf(logmessage,"Error allocating memory for filelist %s",strerror(errno)) ;
        *errcode = 35 ;
        closedir(curdir) ;
        current_item = first_item ;
        while ( current_item != NULL ) {
            free(current_item->filename) ;
            used_item = current_item->next ;
            free(current_item) ;
            current_item = used_item ;
        }
        free(dirpath) ;
        return -1 ;
    }
    current_item = first_item ;
    filepos = *filelist ;
    while ( current_item != NULL ) {
        sprintf(filepos,"%s",current_item->filename) ;
        filepos = filepos + strlen(filepos) + 1 ;
        sprintf(filepos,"%s",current_item->filechar) ;
        filepos = filepos + strlen(filepos) + 1 ;
        current_item = current_item->next ;
    }
    *filelistlen = lengthtosend ;
    closedir(curdir) ;
    current_item = first_item ;
    while ( current_item != NULL ) {
        free(current_item->filename) ;
        used_item = current_item->next ;
        free(current_item) ;
        current_item = used_item ;
    }
    free(dirpath) ;
    return 0 ;   
}

/*******************************************************************************
** bbftp_retrcheckdir :                                                        *
**                                                                             *
**      Routine to check a directory                                           *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**           1  recoverable error                                              *
**                                                                             *
*******************************************************************************/

int bbftp_retrcheckdir(char *filename,char *logmessage,int *errcode)
{
#ifdef STANDART_FILE_CALL
    struct stat statbuf;
#else
    struct stat64 statbuf ;
#endif
    int     savederrno ;
    /*
    ** Check if it is a rfio creation
    */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_retrcheckdir_rfio(filename,logmessage,errcode) ;
    }
#endif

#ifdef STANDART_FILE_CALL
    if ( stat(filename,&statbuf) < 0 )
#else
    if ( stat64(filename,&statbuf) < 0 )
#endif
       {
        /*
        ** It may be normal to get an error if the dir
        ** does not exist but some error code must lead
        ** to the interruption of the transfer:
        **        EACCES         : Search permission denied
        **        ELOOP          : To many symbolic links on path
        **        ENAMETOOLONG   : Path argument too long
        **        ENOTDIR        : A component in path is not a directory
        */
        savederrno = errno ;
        if ( savederrno == EACCES ||
            savederrno == ELOOP ||
            savederrno == ENAMETOOLONG ||
            savederrno == ENOTDIR ) {
            sprintf(logmessage,"Error stating file %s : %s",filename,strerror(savederrno)) ;
            *errcode = 72 ;
            return -1 ;
        } else {
            return 0 ;
        }
    } else {
        /*
        ** The file exists so check if it is a directory
        */
        if ( (statbuf.st_mode & S_IFDIR) != S_IFDIR) {
            sprintf(logmessage,"File %s is not a directory",filename) ;
            *errcode = 76 ;
            return -1 ;
        }
    }
    return 0 ;   
}

/*******************************************************************************
** bbftp_retrcheckfile :                                                       *
**                                                                             *
**      Routine to check a file and set the global parameters                  *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Transferoption                      NOT MODIFIED                   * 
**          BBftp_Filemode                            MODIFIED                       *
**          BBftp_Lastaccess                          MODIFIED                       *
**          BBftp_Lastmodif                           MODIFIED                       *
**          BBftp_Filesize                            MODIFIED                       *
**          BBftp_Requestedstreamnumber               POSSIBLY MODIFIED              *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**           1  recoverable error                                              *
**                                                                             *
*******************************************************************************/

int bbftp_retrcheckfile(char *filename,char *logmessage,int *errcode)
{
#ifdef STANDART_FILE_CALL
    struct stat statbuf;
#else
    struct stat64 statbuf ;
#endif
    int     tmpnbport ;
    int     savederrno ;
    /*
    ** Check if it is a rfio creation
    */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_retrcheckfile_rfio(filename,logmessage,errcode) ;
    }
#endif

#ifdef STANDART_FILE_CALL
    if ( stat(filename,&statbuf) < 0 )
#else
    if ( stat64(filename,&statbuf) < 0 )
#endif
       {
        /*
        ** It may be normal to get an error if the file
        ** does not exist but some error code must lead
        ** to the interruption of the transfer:
        **        EACCES        : Search permission denied
        **        ELOOP         : To many symbolic links on path
        **        ENAMETOOLONG  : Path argument too long
        **        ENOENT        : The file does not exists
        **        ENOTDIR       : A component in path is not a directory
        */
        savederrno = errno ;
        sprintf(logmessage,"Error stating file %s : %s ",filename,strerror(savederrno)) ;
        *errcode = 72 ;
        if ( savederrno == EACCES ||
            savederrno == ELOOP ||
            savederrno == ENAMETOOLONG ||
            savederrno == ENOENT ||
            savederrno == ENOTDIR ) {
            return -1 ;
        } else {
            return 1 ;
        }
    } else {
        /*
        ** The file exists so check if it is a directory
        */
        if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
            sprintf(logmessage,"File %s is a directory",filename) ;
            *errcode = 73 ;
            return -1 ;
        }
    }
    if (S_ISREG(statbuf.st_mode)) {
        BBftp_Filemode = statbuf.st_mode & ~S_IFREG;
    } else {
        BBftp_Filemode = statbuf.st_mode;
    }
    sprintf(BBftp_Lastaccess,"%08lx",statbuf.st_atime) ;
    sprintf(BBftp_Lastmodif,"%08lx",statbuf.st_mtime) ;
    BBftp_Lastaccess[8] = '\0' ;
    BBftp_Lastmodif[8]  = '\0' ;
    BBftp_Filesize = statbuf.st_size ;
    tmpnbport = BBftp_Filesize/(BBftp_Buffersizeperstream*1024) ;
    if ( tmpnbport == 0 ) {
        BBftp_Requestedstreamnumber = 1 ;
    } else if ( tmpnbport < BBftp_Nbport ) {
        BBftp_Requestedstreamnumber = tmpnbport ;
    } else {
        BBftp_Requestedstreamnumber = BBftp_Nbport ;
    }
    return 0 ;   
}
/*******************************************************************************
** bbftp_retrtransferfile :                                                    *
**                                                                             *
**      Routine to transfer a file                                             *
**                                                                             *
**      INPUT variable :                                                       *
**          filename    :  file to send    NOT MODIFIED                        *
**                                                                             *
**      OUTPUT variable :                                                      *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  transfer failed unrecoverable error                            *
**           0  Keep the connection open (does not mean that the file has been *
**              successfully transfered)                                       *
**          >0  Recoverable error but calling has the cleaning to do           *
**                                                                             *
*******************************************************************************/
 
int bbftp_retrtransferfile(char *filename,char *logmessage,int *errcode)
{
#ifdef STANDART_FILE_CALL
    off_t       nbperchild ;
    off_t       nbtosend;
    off_t       startpoint ;
    off_t       nbread ;
    off_t       numberread ;
    off_t       nbsent ;
    off_t       realnbtosend ;
#else
    off64_t     nbperchild ;
    off64_t     nbtosend;
    off64_t     startpoint ;
    off64_t     nbread ;
    off64_t     numberread ;
    off64_t     nbsent ;
    off64_t     realnbtosend ;
#endif
#ifdef WITH_GZIP
    uLong   buflen ;
    uLong   bufcomplen ;
#endif
    int     lentosend ;
    int     retcode ;

    int     *pidfree ;
    int     *socknumber ;
    int     i ;

    int     nfds ; 
    fd_set  selectmask ;
    struct timeval    wait_timer;
    int     fd ;

    struct mess_compress *msg_compress ;
    struct message *msg ;
    int     sendsock ;
    int     *portnumber ;

    /*
    ** Check if it is a rfio transfer
    */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_retrtransferfile_rfio(filename,logmessage,errcode) ;
    }
#endif
    if (BBftp_Protocol == 2) { /* Active mode */
      socknumber = BBftp_Mysockets ;
	} else { /* Passive mode */
	  portnumber = BBftp_Myports ;
	}
    nbperchild = BBftp_Filesize/BBftp_Requestedstreamnumber ;
    pidfree = BBftp_Mychildren ;

    /*
    ** Now start all our children
    */
    for (i = 1 ; i <= BBftp_Requestedstreamnumber ; i++) {
        if ( i == BBftp_Requestedstreamnumber ) {
            startpoint = (i-1)*nbperchild;
            nbtosend = BBftp_Filesize-(nbperchild*(BBftp_Requestedstreamnumber-1)) ;
        } else {
            startpoint = (i-1)*nbperchild;
            nbtosend = nbperchild ;
        }
        if (BBftp_Protocol == 2) {
		  sendsock = *socknumber ;
		  socknumber++ ;
		} else { /* Passive mode */
          /*
          ** Now create the socket to send
          */
          sendsock = 0 ;
          while (sendsock == 0 ) {
            sendsock = bbftp_createdatasock(*portnumber/*,logmessage*/) ;
          }
          if ( sendsock < 0 ) {
            /*
            ** We set childendinerror to 1 in order to prevent the father
            ** to send a BAD message which can desynchronize the client and the
            ** server (We need only one error message)
            ** Bug discovered by amlutz on 2000/03/11
            */
		  /*            if ( childendinerror == 0 ) {
                childendinerror = 1 ;
                reply(MSG_BAD,logmessage) ;
				}
				clean_child() ;*/
            return 1 ;
          }
          portnumber++ ;
          /*
          ** Set flagsighup to zero in order to be able in child
          ** not to wait STARTCHILDTO if signal was sent before 
          ** entering select. (Seen on Linux with one child)
          */
          /*flagsighup = 0 ;*/
		}
        if ( (retcode = fork()) == 0 ) {
            int     ns ;
            blockallsignals() ;
            /*
            ** We are in child
            */
            /* 
            ** In child the first thing to do is to 
            ** close the control socket, and all the 
            ** socket not concerning itself
            */
            close(STDIN_FILENO) ;
            close(STDOUT_FILENO) ;
            close(STDERR_FILENO) ;
            close(BBftp_Incontrolsock) ;
            close(BBftp_Outcontrolsock) ; 
			if (BBftp_Protocol == 2) { /* Active mode */
              int     *socktoclose ;
              socktoclose = BBftp_Mysockets ;
              for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
                if ( *socktoclose !=  sendsock) close(*socktoclose) ;
                socktoclose++ ;
              }
			}
            /*
            ** And open the file 
            */
#ifdef STANDART_FILE_CALL
            if ( (fd = open(filename,O_RDONLY|O_BINARY)) < 0 )
#else
            if ( (fd = open64(filename,O_RDONLY|O_BINARY)) < 0 )
#endif
			   {
                /*
                ** An error on openning the local file is considered
                ** as fatal. Maybe this need to be improved depending
                ** on errno
                */
                i = errno ;
                _exit(i) ;
            }
#ifdef STANDART_FILE_CALL
            if ( lseek(fd,startpoint,SEEK_SET) < 0 )
#else
            if ( lseek64(fd,startpoint,SEEK_SET) < 0 )
#endif
			   {
                i = errno ;
                close(fd) ;
                _exit(i)  ;
            }
            /*
            ** We are now just going to select on our 
            ** socket
            **/
            nfds = sysconf(_SC_OPEN_MAX) ;
	    (void) nfds;
            FD_ZERO(&selectmask) ;
            FD_SET(sendsock,&selectmask) ;
            /*
            ** Set the timer for the connection
            */
            wait_timer.tv_sec  = CHILDWAITTIME ;
            wait_timer.tv_usec = 0 ;
            if (BBftp_Protocol == 2) {
                retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ;
            } else {
                retcode = select(FD_SETSIZE,0,&selectmask,0,&wait_timer) ;
            }
            if ( retcode < 0 ) {
                /*
                ** select return in error 
                ** Let us sleep a while in order to let
                ** the father fork all its children
                */
                i = errno ;
                close(fd) ;
                sleep(CHILDWAITTIME) ;
                _exit(i) ;
            }
            if ( retcode == 0 ) {
                /*
                ** That is a time out let us exit 
                ** with the time out error
                */
                close(fd) ;
                _exit(ETIMEDOUT) ;
            }
            /*
            ** At this point as we only set one bit we have 
            ** got a connection
            */
			if (BBftp_Protocol == 2) { /* Active mode */
              if ( (ns = accept(sendsock,0,0) ) < 0 ) {
                i = errno ;
                close(fd) ;
                _exit(i) ;
              }
              /*
              ** Close the listening socket
              */
              close(sendsock) ;
			} else {
			  ns = sendsock ;
			  /*close(sendsock) ;*/
			}
            /*
            ** Start the sending loop
            ** In simulation mode, simulate the transfer
            */
            if (!BBftp_Simulation_Mode) {
              nbread = 0 ;
              while ( nbread < nbtosend ) {
                if ( (numberread = read ( fd, BBftp_Readbuffer, ( (BBftp_Buffersizeperstream*1024) <= nbtosend - nbread) ?  (BBftp_Buffersizeperstream*1024) : nbtosend-nbread) ) > 0 ) {
                    nbread = nbread+numberread ;
#ifdef WITH_GZIP
                    if ( (BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
                        /*
                        ** In case of compression we are going to use
                        ** a temporary buffer
                        */
                        bufcomplen = BBftp_Buffersizeperstream*1024 ;
                        buflen = numberread ;
                        retcode = compress((Bytef *)BBftp_Compbuffer,&bufcomplen,(Bytef *)BBftp_Readbuffer,buflen) ;
                        if ( retcode != 0 ) {
                            msg_compress = ( struct mess_compress *) BBftp_Compbuffer;
                            /*
                            ** Compress error, in this cas we are sending the
                            ** date uncompressed
                            */
                            msg_compress->code = DATA_NOCOMPRESS ;
                            lentosend = numberread ;
#ifndef WORDS_BIGENDIAN
                            msg_compress->datalen = ntohl(lentosend) ;
#else
                            msg_compress->datalen = lentosend ;
#endif
                            realnbtosend = numberread ;
                        } else {
                            memcpy(BBftp_Readbuffer,BBftp_Compbuffer,BBftp_Buffersizeperstream*1024) ;
                            msg_compress = ( struct mess_compress *) BBftp_Compbuffer;
                            msg_compress->code = DATA_COMPRESS ;
                            lentosend = bufcomplen ;
#ifndef WORDS_BIGENDIAN
                            msg_compress->datalen = ntohl(lentosend) ;
#else
                            msg_compress->datalen = lentosend ;
#endif
                            realnbtosend =  bufcomplen ;
                        }
                        /*
                        ** Send the header
                        */
                        if ( writemessage(ns,BBftp_Compbuffer,COMPMESSLEN,BBftp_Datato) < 0 ) {
                            i = ETIMEDOUT ;
                            _exit(i) ;
                        }
                    } else {
                        realnbtosend = numberread ;
                    }
#else
                    realnbtosend = numberread ;
#endif                    
                    /*
                    ** Send the data
                    */
                    nbsent = 0 ;
                    while ( nbsent < realnbtosend ) {
                        lentosend = realnbtosend-nbsent ;
                        nfds = sysconf(_SC_OPEN_MAX) ;
                        FD_ZERO(&selectmask) ;
                        FD_SET(ns,&selectmask) ;
                        wait_timer.tv_sec  = BBftp_Datato  ;
                        wait_timer.tv_usec = 0 ;
                        if ( (retcode = select(FD_SETSIZE,0,&selectmask,0,&wait_timer) ) == -1 ) {
                            /*
                            ** Select error
                            */
                            i = errno ;
                            close(fd) ;
                            close(ns) ;
                            _exit(i) ;
                        } else if ( retcode == 0 ) {
                            close(fd) ;
                            i=ETIMEDOUT ;
                            close(ns) ;
                            _exit(i) ;
                        } else {
                            retcode = send(ns,&BBftp_Readbuffer[nbsent],lentosend,0) ;
                            if ( retcode < 0 ) {
                                i = errno ;
                                close(fd) ;
                                close(ns) ;
                                _exit(i) ;
                            } else if ( retcode == 0 ) {
                                i = ECONNRESET ;
                                close(fd) ;
                                close(ns) ;
                                _exit(i) ;
                            } else {
                                nbsent = nbsent+retcode ;
                            }
                        }
                    }
                } else {
                    i = errno ;
                    close(ns) ;
                    close(fd) ;
                    _exit(i) ;
                }
              }
              close(fd) ;
              /*
              ** All data has been sent so wait for the acknoledge
              */
              if ( readmessage(ns,BBftp_Readbuffer,MINMESSLEN,BBftp_Ackto) < 0 ) {
                close(ns) ;
                _exit(ETIMEDOUT) ;
              }
              msg = (struct message *) BBftp_Readbuffer ;
              if ( msg->code != MSG_ACK) {
                close(ns) ;
                _exit(1) ;
              }
			}
            close(ns) ;
            _exit(0) ;
        } else {
            /*
            ** We are in father
            */
            if ( retcode == -1 ) {
                /*
                ** Fork failed ...
                */
                sprintf(logmessage,"Fork failed : %s\n",strerror(errno)) ;
                *errcode = 36 ;
                sprintf(logmessage,"fork failed : %s ",strerror(errno)) ;
                bbftp_clean_child() ;
                return -1 ;
            } else {
                *pidfree++ = retcode ;
		close(sendsock) ;
                /*socknumber++ ;*/
            }
        }
    }
    return 0 ;
}
