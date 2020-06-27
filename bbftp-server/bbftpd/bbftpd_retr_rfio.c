/*
 * bbftpd/bbftpd_retr_rfio.c
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

 
 bbftpd_retr_rfio.c v 2.0.0 2001/02/22  - Routines creation
                    v 2.0.1 2001/04/17  - Realy wait STARTCHILDTO 
                                        - Correct indentation
                                        - Port to IRIX
                    v 2.0.2 2001/05/04  - Correct include for RFIO
                                        - Add debug
                                        - Get serrno instead of errno
                    v 2.1.0 2001/05/30  - Correct syslog level
                                        - Reorganise routines as in bbftp_
              
*****************************************************************************/
#include <bbftpd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
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
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>

#ifdef WITH_RFIO64
#include <shift/rfio_api.h>
#include <shift/serrno.h>
#else
#include <shift.h>
#endif

#ifdef WITH_GZIP
#include <zlib.h>
#endif

extern int  transferoption ;
extern my64_t  filesize ;
extern int  requestedstreamnumber ;
extern int  buffersizeperstream ;
extern int  maxstreams ;
extern int  filemode ;
extern int  *myports ;
extern char *readbuffer ;
extern char *compbuffer ;
extern int  *mychildren ;
extern int  *mysockets ;
extern int  nbpidchild ;
extern int  unlinkfile ;
extern int  incontrolsock ;
extern int  outcontrolsock ;
extern	int	ackto ;
extern	int	datato ;
extern int  state ;
extern int  childendinerror ;
extern int  flagsighup ;
extern char lastaccess[9] ;
extern char lastmodif[9] ;
extern int  debug ;
extern struct  timeval  tstart ;
extern int  protocolversion ;

/*******************************************************************************
** bbftpd_retrlisdir :                                                         *
**                                                                             *
**      Routine to list a directory                                            *
**                                                                             *
**      OUPUT variable :                                                       *
**          filelist   :  Where to store the file list (to be freed by caller  *
**          filelistlen:  Length of filelist buffer                            *
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

int bbftpd_retrlistdir_rfio(char *pattern,char **filelist,int *filelistlen,char *logmessage)
{
    int     lastslash ;
    char    *pointer ;
    char    *dirpath ;
    DIR     *curdir ;
    struct dirent *dp ;
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    struct stat64 statbuf;
#else
    struct stat statbuf;
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
    char    *slash ;
    int     savederrno ;
    
    /*
    **  We are going allocate memory for the directory name
    **  We allocate stelent(pattern) + 1 char for \0 + 3 char
    **  if the directory is "." 
    */
    if ( (dirpath = (char *) malloc ( strlen(pattern) + 1 + 3 )) == NULL ) {
        sprintf(logmessage,"Error allocating memory for dirpath %s",strerror(errno)) ;
        return -1 ;
    } 
    pointer = pattern ;
    slash = pattern ;
    lastslash = strlen(pattern) - 1 ;
    /*
    ** Within rfio as we have no chdir the file has to be an absolute 
    ** path 
    */
    if ( *slash == '/' ) {
        /*
        ** It is an absolute path 
        */
    } else {
        /*
        ** find the :
        */
        i = 0 ;
        while (*slash != ':' && i < strlen(pattern) ) {
            slash++;
            i++ ;
        }
        if ( i == strlen(pattern) ) {
            sprintf(logmessage,"Incorrect RFIO pattern : %s",pattern) ;
            FREE(dirpath) ;
            return -1 ;
        }
    }
    while ( lastslash >= 0 && pointer[lastslash] != '/') lastslash-- ;
    if ( lastslash == -1 ) {
        /*
        ** No slash in the path, so the parre
        ** 
        */
        sprintf(logmessage,"Incorrect RFIO pattern : %s",pattern) ;
        FREE(dirpath) ;
        return -1 ;
    } else if ( lastslash == 0 ) {
        /*
        ** A slash in first position so we are going to open the
        ** / directory
        */
        if ( debug ) {
            fprintf(stdout,"**In list_rfio : rfio_opendir(/) \n") ;
        }
        if ( (curdir = rfio_opendir("/")) == NULL ) {
            if ( serrno != 0 ) {
                savederrno = serrno ;
            } else if ( rfio_errno != 0 ) {
                savederrno = rfio_errno ;
            } else if ( errno != 0 ){
                savederrno = errno ;
            } else {
                /*
                ** We use EBFONT in case of undescribed error
                */
                savederrno = 57 ;
            }
            sprintf(logmessage,"rfio_opendir / failed : %s ",rfio_serror()) ;
            if ( savederrno == EACCES ) {
                FREE(dirpath) ;
                return -1 ;
            } else {
                FREE(dirpath) ;
                return 1 ;
            }
        }
        strcpy(dirpath,"/") ;
        pointer++ ;
    } else if ( lastslash == strlen(pointer) - 1 ) {
        /*
        ** The pattern end with a slash ..... error
        */
        sprintf(logmessage,"Pattern %s ends with a /",pattern) ;
        FREE(dirpath) ;
        return -1 ;
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
        if ( debug ) {
            fprintf(stdout,"**In list_rfio : rfio_opendir(dirpath) (%s) \n",dirpath) ;
        }
        if ( (curdir = rfio_opendir(dirpath)) == NULL ) {
            if ( serrno != 0 ) {
                savederrno = serrno ;
            } else if ( rfio_errno != 0 ) {
                savederrno = rfio_errno ;
            } else if ( errno != 0 ){
                savederrno = errno ;
            } else {
                /*
                ** We use EBFONT in case of undescribed error
                */
                savederrno = 57 ;
            }
            sprintf(logmessage,"opendir %s failed : %s ",dirpath,rfio_serror()) ;
            if ( savederrno == EACCES ) {
                FREE(dirpath) ;
                return -1 ;
            } else {
                FREE(dirpath) ;
                return -1 ;
            }
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
    serrno = 0 ;
    rfio_errno = 0 ;
    first_item = NULL ;
    if ( debug ) {
        fprintf(stdout,"**In list_rfio : rfio_readdir(curdir) \n") ;
    }
    while ( (dp = rfio_readdir(curdir) ) != NULL) {
        if ( fnmatch(pointer, dp->d_name,0) == 0) {
            numberoffile++ ;
            lengthtosend = lengthtosend +  strlen(dirpath) + strlen(dp->d_name) + 1 + 3 ;
            if ( ( current_item = (struct keepfile *) malloc( sizeof(struct keepfile)) ) == NULL ) {
                sprintf(logmessage,"Error getting memory for structure : %s",strerror(errno)) ;
                if ( debug ) {
                    fprintf(stdout,"**In list_rfio : rfio_closedir(curdir) \n") ;
                }
                current_item = first_item ;
                while ( current_item != NULL ) {
                    free(current_item->filename) ;
                    used_item = current_item->next ;
                    free(current_item) ;
                    current_item = used_item ;
                }
                rfio_closedir(curdir) ;
                FREE(dirpath) ;
                return 1 ;
            }
            if ( ( current_item->filename = (char *) malloc( strlen(dirpath) + strlen(dp->d_name) + 1) ) == NULL ) {
                sprintf(logmessage,"Error getting memory for filename : %s",strerror(errno)) ;
                /*
                ** Clean memory
                */
                FREE(current_item) ;
                current_item = first_item ;
                while ( current_item != NULL ) {
                    free(current_item->filename) ;
                    used_item = current_item->next ;
                    free(current_item) ;
                    current_item = used_item ;
                }
                if ( debug ) {
                    fprintf(stdout,"**In list_rfio : rfio_closedir(curdir) \n") ;
                }
                rfio_closedir(curdir) ;
                FREE(dirpath) ;
                return 1 ;
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
            if ( debug ) {
                fprintf(stdout,"**In list_rfio : rfio_lstat(current_item->filename,&statbuf) (%s) \n",current_item->filename) ;
            }
#ifdef WITH_RFIO64
            if ( rfio_lstat64(current_item->filename,&statbuf) < 0 ) {
#else
            if ( rfio_lstat(current_item->filename,&statbuf) < 0 ) {
#endif                
                sprintf(logmessage,"Error lstating file %s",current_item->filename) ;
                current_item = first_item ;
                while ( current_item != NULL ) {
                    free(current_item->filename) ;
                    used_item = current_item->next ;
                    free(current_item) ;
                    current_item = used_item ;
                }
                rfio_closedir(curdir) ;
                FREE(dirpath) ;
                return 1 ;
             }
             if ( (statbuf.st_mode & S_IFLNK) == S_IFLNK) {
                 current_item->filechar[0] = 'l' ;
                 if ( debug ) {
                     fprintf(stdout,"**In list_rfio : rfio_stat(current_item->ilename,&statbuf) (%s) \n",current_item->filename) ;
                 }
#ifdef WITH_RFIO64
                if ( rfio_stat64(current_item->filename,&statbuf) < 0 ) {
#else
                if ( rfio_stat(current_item->filename,&statbuf) < 0 ) {
#endif                
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
        serrno = 0 ;
        rfio_errno = 0 ;
    }
    if ( debug ) {
        fprintf(stdout,"**In list_rfio : number of file found = %d \n",numberoffile) ;
    }
    /*
    ** Check errno in case of error during readdir
    ** for the following readir we are not going to check
    ** so that may be a cause of problem
    */
    savederrno = 0 ;
    if ( serrno != 0 ) {
        savederrno = serrno ;
    } else if ( rfio_errno != 0 ) {
        savederrno = rfio_errno ;
    } else if ( errno != 0 ){
        savederrno = errno ;
    }
    if ( savederrno != 0 ) {
        sprintf(logmessage,"Error on readdir %s (%s) ",dirpath,rfio_serror()) ;
        if ( debug ) {
            fprintf(stdout,"**In list_rfio : rfio_closedir(curdir) \n") ;
        }
        current_item = first_item ;
        while ( current_item != NULL ) {
            free(current_item->filename) ;
            used_item = current_item->next ;
            free(current_item) ;
            current_item = used_item ;
        }
        rfio_closedir(curdir) ;
        FREE(dirpath) ;
        return 1 ;
    }        
    /*
    ** Check if numberoffile is zero and reply now in this
    ** case 
    */
    if ( numberoffile == 0 ) {
        *filelistlen = 0 ;
        if ( debug ) {
            fprintf(stdout,"**In list_rfio : rfio_closedir(curdir) \n") ;
        }
        rfio_closedir(curdir) ;
        current_item = first_item ;
        while ( current_item != NULL ) {
            free(current_item->filename) ;
            used_item = current_item->next ;
            free(current_item) ;
            current_item = used_item ;
        }
        FREE(dirpath) ;
        return 0 ;
    }
    /*
    ** Now everything is ready so prepare the answer
    */
    if ( ( *filelist = (char *) malloc (lengthtosend) ) == NULL ) {
        sprintf(logmessage,"Error allocating memory for filelist %s",strerror(errno)) ;
        if ( debug ) {
            fprintf(stdout,"**In list_rfio : rfio_closedir(curdir) \n") ;
        }
        rfio_closedir(curdir) ;
        current_item = first_item ;
        while ( current_item != NULL ) {
            free(current_item->filename) ;
            used_item = current_item->next ;
            free(current_item) ;
            current_item = used_item ;
        }
        FREE(dirpath) ;
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
    if ( debug ) {
        fprintf(stdout,"**In list_rfio : rfio_closedir(curdir) The Last\n") ;
    }
    rfio_closedir(curdir) ;
    current_item = first_item ;
    while ( current_item != NULL ) {
        free(current_item->filename) ;
        used_item = current_item->next ;
        free(current_item) ;
        current_item = used_item ;
    }
    FREE(dirpath) ;
    return 0 ;
}

/*******************************************************************************
** bbftpd_retrcheckfile_rfio :                                                 *
**                                                                             *
**      Routine to check a file and set the global parameters                  *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                      NOT MODIFIED                   * 
**          filemode                            MODIFIED                       *
**          lastaccess                          MODIFIED                       *
**          lastmodif                           MODIFIED                       *
**          filesize                            MODIFIED                       *
**          requestedstreamnumber               POSSIBLY MODIFIED              *
**                                                                             *
**      RETURN:                                                                *
**          -1  Incorrect options                                              *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_retrcheckfile_rfio(char *filename,char *logmessage)
{
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    struct stat64 statbuf;
#else
    struct stat statbuf;
#endif
    int     savederrno ;
    int     tmpnbport ;

    if ( debug ) {
        fprintf(stdout,"**In retrcheckfile_rfio : rfio_stat(filename,&statbuf) (%s)\n",filename) ;
    }
#ifdef WITH_RFIO64
    if ( rfio_stat64(filename,&statbuf) < 0 ) {
#else
    if ( rfio_stat(filename,&statbuf) < 0 ) {
#endif
        /*
        ** It may be normal to get an error if the file
        ** does not exist but some error code must lead
        ** to the interruption of the transfer:
        **        EACCES            : Search permission denied
        **        ELOOP             : To many symbolic links on path
        **        ENAMETOOLONG      : Path argument too long
        **        ENOENT            : The file does not exists
        **        ENOTDIR           : A component in path is not a directory
        */
        if ( serrno != 0 ) {
            savederrno = serrno ;
        } else if ( rfio_errno != 0 ) {
            savederrno = rfio_errno ;
        } else if ( errno != 0 ){
            savederrno = errno ;
        } else {
            /*
            ** We use EBFONT in case of undescribed error
            */
            savederrno = 57 ;
        }
        sprintf(logmessage,"Error stating file %s : %s ",filename,rfio_serror()) ;
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
            return -1 ;
        }
    }
    if (S_ISREG(statbuf.st_mode)) {
        filemode = statbuf.st_mode & ~S_IFREG;
    } else {
        filemode = statbuf.st_mode;
    }
    sprintf(lastaccess,"%08x",statbuf.st_atime) ;
    sprintf(lastmodif,"%08x",statbuf.st_mtime) ;
    lastaccess[8] = '\0' ;
    lastmodif[8]  = '\0' ;
    filesize = statbuf.st_size ;
    tmpnbport = filesize/(buffersizeperstream*1024) ;
    if ( tmpnbport == 0 ) {
        requestedstreamnumber = 1 ;
    } else if ( tmpnbport < requestedstreamnumber ) {
        requestedstreamnumber = tmpnbport ;
    }
    if ( requestedstreamnumber > maxstreams ) requestedstreamnumber = maxstreams ;
    return 0 ;
}
/*******************************************************************************
** bbftpd_retrtransferfile_rfio :                                              *
**                                                                             *
**      Routine to transfer a file                                             *
**                                                                             *
**      INPUT variable :                                                       *
**          filename    :  file to create    NOT MODIFIED                      *
**                                                                             *
**      OUTPUT variable :                                                      *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  transfer failed unrecoverable error                            *
**           0  Keep the connection open (does not mean that the file has been *
**                successfully transfered)                                       *
**          >0  Recoverable error but calling has the cleaning to do           *
**                                                                             *
*******************************************************************************/
 
int bbftpd_retrtransferfile_rfio(char *filename,int simulation,char *logmessage) 
{
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    off64_t       nbperchild ;
    off64_t       nbtosend;
    off64_t       startpoint ;
    off64_t       nbread ;
    off64_t       numberread ;
    off64_t       nbsent ;
    off64_t       toseek ;
    off64_t       realnbtosend ;
#else
    off_t       nbperchild ;
    off_t       nbtosend;
    off_t       startpoint ;
    off_t       nbread ;
    off_t       numberread ;
    off_t       nbsent ;
    off_t       toseek ;
    off_t       realnbtosend ;
#endif
    /*struct stat statbuf ;*/
    my64_t    toprint64 ;
#ifdef WITH_GZIP                        
    uLong    buflen ;
    uLong    bufcomplen ;
#endif
    int        lentosend ;
    int     sendsock ;
    int        retcode ;

    int     *pidfree ;
    int     *sockfree ; /* for PASV mode only */
    int     *portnumber ;
    int     i ;

    int     nfds ; 
    fd_set    selectmask ;
    struct timeval    wait_timer;
    int     fd ;

    struct mess_compress *msg_compress ;
    struct message *msg ;
    int     waitedtime ;

    childendinerror = 0 ; /* No child so no error */
    if ( protocolversion <= 2 ) { /* Active mode */
      portnumber = myports ;
	} else {
	  sockfree = mysockets ;
	}
    nbperchild = filesize/requestedstreamnumber ;
    pidfree = mychildren ;
    nbpidchild = 0 ;
    unlinkfile = 3 ;

    /*
    ** Now start all our children
    */
    for (i = 1 ; i <= requestedstreamnumber ; i++) {
        if ( i == requestedstreamnumber ) {
            startpoint = (i-1)*nbperchild;
            nbtosend = filesize-(nbperchild*(requestedstreamnumber-1)) ;
        } else {
            startpoint = (i-1)*nbperchild;
            nbtosend = nbperchild ;
        }
        if (protocolversion <= 2) { /* ACTIVE MODE */
          /*
          ** Now create the socket to send
          */
          sendsock = 0 ;
          while (sendsock == 0 ) {
            sendsock = bbftpd_createreceivesocket(*portnumber,logmessage) ;
          }
          if ( sendsock < 0 ) {
            /*
            ** We set childendinerror to 1 in order to prevent the father
            ** to send a BAD message which can desynchronize the client and the
            ** server (We need only one error message)
            ** Bug discovered by amlutz on 2000/03/11
            */
            if ( childendinerror == 0 ) {
                childendinerror = 1 ;
                reply(MSG_BAD,logmessage) ;
            }
            clean_child() ;
            return 1 ;
          }
          portnumber++ ;
        } else { /* PASSIVE MODE */
          sendsock = *sockfree ;
          sockfree++ ;
        }
        /*
        ** Set flagsighup to zero in order to be able in child
        ** not to wait STARTCHILDTO if signal was sent before 
        ** entering select. (Seen on Linux with one child)
        */
        flagsighup = 0 ;
        /*
        ** At this stage we are ready to receive packets
        ** So we are going to fork
        */
        if ( (retcode = fork()) == 0 ) {
		    int     ns ;
            /*
            ** We are in child
            */
            /*
            ** Pause until father send a SIGHUP in order to prevent
            ** child to die before father has started all children
            */
            waitedtime = 0 ;
            while (flagsighup == 0 && waitedtime < STARTCHILDTO) {
                int nfds2 ;
				wait_timer.tv_sec  = 1 ;
                wait_timer.tv_usec = 0 ;
                nfds2 = sysconf(_SC_OPEN_MAX) ;
                select(nfds2,0,0,0,&wait_timer) ;
                waitedtime = waitedtime + 1 ;
            }
            syslog(BBFTPD_DEBUG,"Child %d starting",getpid()) ;
            /*
            ** Close all unnecessary stuff
            */
            close(incontrolsock) ;
            close(outcontrolsock) ; 
            if ( debug != 0 ) {
                sprintf(logmessage,"/tmp/bbftp.rfio.trace.level.%d.%d",debug,getpid()) ;
                (void) freopen(logmessage,"w",stdout) ;
                fprintf(stdout,"**Starting rfio trace for child number %d (pid=%d)\n",i,getpid()) ;
            }
            /*
            ** And open the file 
            */
            if ( debug ) {
                fprintf(stdout,"**In retrtransferfile_rfio : rfio_open(filename,O_RDONLY) (%s)\n",filename) ;
            }
#ifdef WITH_RFIO64
            if ( (fd = rfio_open64(filename,O_RDONLY,0)) < 0 ) {
#else
            if ( (fd = rfio_open(filename,O_RDONLY,0)) < 0 ) {
#endif
                /*
                ** An error on openning the local file is considered
                ** as fatal. Maybe this need to be improved depending
                ** on errno
                */
                if ( serrno != 0 ) {
                    i = serrno ;
                } else if ( rfio_errno != 0 ) {
                    i = rfio_errno ;
                } else if ( errno != 0 ){
                    i = errno ;
                } else {
                    /*
                    ** We use EBFONT in case of undescribed error
                    */
                    i = 57 ;
                }
                syslog(BBFTPD_ERR,"Error opening local file %s : %s",filename,rfio_serror()) ;
                close(sendsock) ;
                exit(i) ;
            }
#ifdef WITH_RFIO64
            if ( rfio_lseek64(fd,startpoint,SEEK_SET) < 0 ) {
#else
            if ( rfio_lseek(fd,startpoint,SEEK_SET) < 0 ) {
#endif
                if ( serrno != 0 ) {
                    i = serrno ;
                } else if ( rfio_errno != 0 ) {
                    i = rfio_errno ;
                } else if ( errno != 0 ){
                    i = errno ;
                } else {
                    /*
                    ** We use EBFONT in case of undescribed error
                    */
                    i = 57 ;
                }
                if ( debug ) {
                    fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                }
                rfio_close(fd) ;
                syslog(BBFTPD_ERR,"Error seeking file : %s",rfio_serror()) ;
                close(sendsock) ;
                exit(i)  ;
            }
            /*
			** PASSIVE MODE: accept connection
			*/
            if ( protocolversion >= 3 ) {
              if ( (ns = accept(sendsock,0,0) ) < 0 ) {
                i = errno ;
                rfio_close(fd) ;
                syslog(BBFTPD_ERR,"Error accept socket : %s",strerror(errno)) ;
                close(sendsock) ;
                exit(i)  ;
              }
			  close(sendsock) ;
			} else {
			  ns = sendsock ;
			}
            /*
            ** Start the sending loop
            ** Handle the simulation mode
            */
            if (!simulation) {
              nbread = 0 ;
              while ( nbread < nbtosend ) {
                if ( (numberread = rfio_read( fd, readbuffer, ( (buffersizeperstream*1024) <= nbtosend - nbread) ?  (buffersizeperstream*1024) : (int)(nbtosend-nbread)) ) > 0 ) {
                    nbread = nbread+numberread ;
                    if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
#ifdef WITH_GZIP
                        /*
                        ** In case of compression we are going to use
                        ** a temporary buffer
                        */
                        bufcomplen = buffersizeperstream*1024 ;
                        buflen = numberread ;
                        retcode = compress((Bytef *)compbuffer,&bufcomplen,(Bytef *)readbuffer,buflen) ;
                        if ( retcode != 0 ) {
                            msg_compress = ( struct mess_compress *) compbuffer;
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
                            memcpy(readbuffer,compbuffer,buffersizeperstream*1024) ;
                            msg_compress = ( struct mess_compress *) compbuffer;
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
                        if ( writemessage(ns,compbuffer,COMPMESSLEN,datato) < 0 ) {
                            i = ETIMEDOUT ;
                            syslog(BBFTPD_ERR,"Error sending header data") ;
                            close(ns) ;
                            if ( debug ) {
                                fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                            }
                            rfio_close(fd) ;
                            exit(i) ;
                        }
#else
                        msg_compress = ( struct mess_compress *) compbuffer;
                        /*
                        ** Compress unavailable, in this cas we are sending the
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
#endif                        
                    } else {
                        realnbtosend = numberread ;
                    }
                    /*
                    ** Send the data
                    */
                    nbsent = 0 ;
                    while ( nbsent < realnbtosend ) {
                        lentosend = realnbtosend-nbsent ;
                        nfds = sysconf(_SC_OPEN_MAX) ;
                        FD_ZERO(&selectmask) ;
                        FD_SET(ns,&selectmask) ;
                        wait_timer.tv_sec  = datato  ;
                        wait_timer.tv_usec = 0 ;
                        if ( (retcode = select(nfds,0,&selectmask,0,&wait_timer) ) == -1 ) {
                            /*
                            ** Select error
                            */
                            i = errno ;
                            syslog(BBFTPD_ERR,"Error select while sending : %s",strerror(errno)) ;
                            if ( debug ) {
                                fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                            }
                            rfio_close(fd) ;
                            close(ns) ;
                            exit(i) ;
                        } else if ( retcode == 0 ) {
                            syslog(BBFTPD_ERR,"Time out while sending") ;
                            if ( debug ) {
                                fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                            }
                            rfio_close(fd) ;
                            i=ETIMEDOUT ;
                            close(ns) ;
                            exit(i) ;
                        } else {
                            retcode = send(ns,&readbuffer[nbsent],lentosend,0) ;
                            if ( retcode < 0 ) {
                                i = errno ;
                                syslog(BBFTPD_ERR,"Error while sending %s",strerror(i)) ;
                                if ( debug ) {
                                    fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                                }
                                rfio_close(fd) ;
                                close(ns) ;
                                exit(i) ;
                            } else if ( retcode == 0 ) {
                                i = ECONNRESET ;
                                syslog(BBFTPD_ERR,"Connexion breaks") ;
                                if ( debug ) {
                                    fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                                }
                                rfio_close(fd) ;
                                close(ns) ;
                                exit(i) ;
                            } else {
                                nbsent = nbsent+retcode ;
                            }
                        }
                    }
                } else {
                    if ( debug ) {
                        fprintf(stdout,"**In retrtransferfile_rfio : rfio_read(%d) returns (%d)\n",fd,numberread) ;
                    }
                    if ( serrno != 0 ) {
                        i = serrno ;
                    } else if ( rfio_errno != 0 ) {
                        i = rfio_errno ;
                    } else if ( errno != 0 ){
                        i = errno ;
                    } else {
                        /*
                        ** We use EBFONT in case of undescribed error
                        */
                        i = 57 ;
                    }
                    syslog(BBFTPD_ERR,"Child Error reading fd (%d) : %s",fd,rfio_serror()) ;
                    close(ns) ;
                    if ( debug ) {
                        fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
                    }
                    rfio_close(fd) ;
                    exit(i) ;
                }
              }
              if ( debug ) {
                fprintf(stdout,"**In retrtransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
              }
              /*
              ** All data has been sent so wait for the acknoledge
              */
              if ( readmessage(ns,readbuffer,MINMESSLEN,ackto) < 0 ) {
                syslog(BBFTPD_ERR,"Error waiting ACK") ;
                close(ns) ;
                exit(ETIMEDOUT) ;
              }
              msg = (struct message *) readbuffer ;
              if ( msg->code != MSG_ACK) {
                syslog(BBFTPD_ERR,"Error unknown messge while waiting ACK %d",msg->code) ;
                close(ns) ;
                exit(1) ;
              }
              toprint64 = nbtosend ;
              syslog(BBFTPD_DEBUG,"Child send %" LONG_LONG_FORMAT " bytes ; end correct ",toprint64) ;
            }
            rfio_close(fd) ;
            close(ns) ;
            exit(0) ;
        } else {
            /*
            ** We are in father
            */
            if ( retcode == -1 ) {
                /*
                ** Fork failed ...
                */
                syslog(BBFTPD_ERR,"fork failed : %s",strerror(errno)) ;
                sprintf(logmessage,"fork failed : %s ",strerror(errno)) ;
                if ( childendinerror == 0 ) {
                    childendinerror = 1 ;
                    reply(MSG_BAD,logmessage) ;
                }
                clean_child() ;
                return 1 ;
            } else {
                nbpidchild++ ;
                *pidfree++ = retcode ;
                close(sendsock) ;
            }
        }
    }
    /*
    ** Set the state before starting children because if the file was
    ** small the child has ended before state was setup to correct value
    */
    state = S_SENDING ;
    /*
    ** Start all children
    */
    pidfree = mychildren ;
    for (i = 0 ; i<nbpidchild ; i++) {
        if ( *pidfree != 0 ) {
            kill(*pidfree,SIGHUP) ;
        }
       pidfree++ ;
    }
    (void) gettimeofday(&tstart, (struct timezone *)0);
    return 0 ;
}
