/*
 * bbftpd/bbftpd_store.c
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



 bbftpd_store.c v 2.0.0 2000/12/19  - Routines creation
                v 2.0.1 2001/04/17  - Realy wait STARTCHILDTO 
                                    - Correct indentation
                                    - Port to IRIX
                v 2.0.2 2001/05/09  - Add storeclosecastfile function
                v 2.1.0 2001/05/30  - Correct syslog level
                                    - Reorganise routines as in bbftp_

*****************************************************************************/
#include <bbftpd.h>

#include <stdio.h>
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
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
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

#ifdef WITH_GZIP
#include <zlib.h>
#endif

extern int  transferoption ;
extern my64_t  filesize ;
extern int  requestedstreamnumber ;
extern int  buffersizeperstream ;
extern int  maxstreams ;
extern int  *myports ;
extern int  *mysockets ;
extern char *readbuffer ;
extern char *compbuffer ;
extern int  *mychildren ;
extern int  nbpidchild ;
extern int  unlinkfile ;
extern int  incontrolsock ;
extern int  outcontrolsock ;
extern	int	datato ;
extern	int	sendcontrolto ;
extern int  state ;
extern int  childendinerror ;
extern int  flagsighup ;
extern struct  timeval  tstart ;
extern int  protocolversion ;
extern  char            currentusername[MAXLEN] ;

/*******************************************************************************
** bbftpdstoreclosecastfile :                                                  *
**                                                                             *
**      Close CASTOR file                                                      *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          castfd                  MODIFIED                                   * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storeclosecastfile(char *filename,char *logmessage)
{
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storeclosecastfile_rfio(filename,logmessage) ;
#else
        return 0 ;
#endif
    }
    return 0 ;
}

/*******************************************************************************
** bbftpd_storeutime :                                                         *
**                                                                             *
**      Routine to chage access time a file                                    *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storeutime(char *filename,struct utimbuf *ftime,char *logmessage)
{
    int     retcode ;
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
        return 0 ;
    }
    retcode = utime(filename,ftime) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error utime on file %s : %s",filename,strerror(errno)) ;
        return retcode ;
    }
    return 0 ;
}

/*******************************************************************************
** bbftpd_storechmod :                                                         *
**                                                                             *
**      Routine to chmod a file                                                *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storechmod(char *filename,int mode,char *logmessage)
{
    int     retcode ;
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storechmod_rfio(filename,mode,logmessage) ;
#else
        return 0 ;
#endif
    }
    retcode = chmod(filename,mode) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error chmod on file %s : %s",filename,strerror(errno)) ;
        return retcode ;
    }
    return 0 ;
}

/*******************************************************************************
** bbftpd_storerename :                                                        *
**                                                                             *
**      Routine to unlink a file                                               *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storerename(char *newfilename,char *oldfilename,char *logmessage)
{
    int     retcode ;
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storerename_rfio(newfilename,oldfilename,logmessage) ;
#else
        return 0 ;
#endif
    }
    retcode = rename(newfilename,oldfilename) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error renaming %s to %s : %s",newfilename,oldfilename,strerror(errno)) ;
        return retcode ;
    }
    return 0 ;
}

/*******************************************************************************
** bbftpd_storeunlink :                                                        *
**                                                                             *
**      Routine to unlink a file                                               *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storeunlink(char *filename)
{
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storeunlink_rfio(filename) ;
#else
        return 0 ;
#endif
    }
    return unlink(filename) ;
}

/*******************************************************************************
** bbftpd_storecheckoptions :                                                  *
**                                                                             *
**      Routine to check the options parameters                                *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                   POSSIBLY MODIFIED                 * 
**          requestedstreamnumber            POSSIBLY MODIFIED                 *
**                                                                             *
**      RETURN:                                                                *
**          -1  Incorrect options                                              *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storecheckoptions(char *logmessage)
{
    int tmpoptions ;
#ifndef WITH_GZIP
    /*
    ** Check if Compress is available int    case of GZIP option
    */
    if ( (transferoption & TROPT_GZIP) == TROPT_GZIP ) {
        syslog(BBFTPD_WARNING,"Compression is not available for this server: option ignored") ;
        transferoption = transferoption & ~TROPT_GZIP ;
    }
#endif
    /*
    ** Check if it is a rfio creation
    */
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storecheckoptions_rfio(logmessage) ;
#else
        sprintf(logmessage,"Fail to store : RFIO not supported") ;
        return -1 ;
#endif
    }
    /*
    ** Check all known options
    */
    tmpoptions = TROPT_TMP | TROPT_ACC | TROPT_MODE | TROPT_DIR | TROPT_GZIP ;
    transferoption = transferoption & tmpoptions ;
    /*
    ** Check the maxstreams
    */
    if ( requestedstreamnumber > maxstreams ) requestedstreamnumber = maxstreams ;
    return 0 ;
}

/*******************************************************************************
** bbftpd_storemkdir :                                                         *
**                                                                             *
**      Routine to create a bunch of directory                                 *
**                                                                             *
**      INPUT variable :                                                       *
**          dirname    :  directory to create    NOT MODIFIED                  *
**          recursif   :  1 recursif             NOT MODIFIED                  *
**                        0 non recursif                                       *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      TO FREE before any return : dirpath                                    *
**                                                                             *
**      RETURN:                                                                *
**          -1   Creation failed unrecoverable error                           *
**           0   OK                                                            *
**           >0  Creation failed recoverable error                             *
**                                                                             *
*******************************************************************************/
int bbftpd_storemkdir(char *dirname,char *logmessage,int recursif)
{
    char    *dirpath;
    char    *basedir ;
    char    *slash ;
    int     savederrno ;
#ifdef STANDART_FILE_CALL
    struct stat statbuf;
#else
    struct stat64 statbuf ;
#endif

    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storemkdir_rfio(dirname,logmessage,recursif) ;
#else
        sprintf(logmessage,"Fail to mkdir : RFIO not supported") ;
        return -1 ;
#endif
    }
    /*
    ** make a copy of dirname for security
    */
    if ( (dirpath = (char *) malloc (strlen(dirname)+1) ) == NULL ) {
        sprintf(logmessage,"Error allocating memory for dirpath : %s",strerror(errno)) ;
        return 1 ;
    }
    strcpy (dirpath, dirname);
    /*
    ** Strip trailing slash
    */
    strip_trailing_slashes(dirpath) ;
    slash = dirpath ;
    if ( recursif == 1 ) {
        /*
        ** Recursif, we are going to create all directory
        */
        /*
        ** If this is an absolute path strip the leading slash
        */
        if ( *dirpath == '/' ) {
            while (*slash == '/') slash++;
        }
        while (1) {
            slash = (char *) strchr (slash, '/');
            if ( slash == NULL ) break ;
            basedir = dirpath ;
            /*
            ** The mkdir and stat calls below appear to be reversed.
            ** They are not.  It is important to call mkdir first and then to
            ** call stat (to distinguish the three cases) only if mkdir fails.
            ** The alternative to this approach is to `stat' each directory,
            ** then to call mkdir if it doesn't exist.  But if some other process
            ** were to create the directory between the stat & mkdir, the mkdir
            ** would fail with EEXIST. 
            ** We create it with mode = 777 because the umask is set by bbftpd
            */
             *slash = '\0';
            if ( mkdir (basedir, 0777) ) {
                savederrno = errno ;
#ifdef STANDART_FILE_CALL
                if ( stat (basedir,&statbuf ) ) {
#else
                if ( stat64 (basedir,&statbuf ) ) {
#endif
                    sprintf(logmessage,"Error creation directory %s (%s)",basedir,strerror(savederrno)) ; 
                    /*
                    ** We tell the client not to retry in the following case (even in waiting
                    ** WAITRETRYTIME the problem will not be solved) :
                    **        EACCES        : Search permission denied
                    **        EDQUOT        : No more quota 
                    **        ENOSPC        : No more space
                    **        ELOOP        : To many symbolic links on path
                    **        ENAMETOOLONG: Path argument too long
                    **        ENOTDIR        : A component in path is not a directory
                    **        EROFS        : The path prefix resides on a read-only file system.
                    **        ENOENT      : A component of the path prefix does not exist or is a null pathname.
                    **        EEXIST      : The named file already exists.
                    */
                     if ( savederrno == EACCES ||
                            savederrno == EDQUOT ||
                            savederrno == ENOSPC ||
                            savederrno == ELOOP ||
                            savederrno == ENAMETOOLONG ||
                            savederrno == ENOTDIR ||
                            savederrno == EROFS ||
                            savederrno == ENOENT ||
                            savederrno == EEXIST ) {
                        FREE(dirpath) ;
                        return -1 ;
                    } else {
                        FREE(dirpath) ;
                        return 1 ;
                    }
                } else if ( (statbuf.st_mode & S_IFDIR) != S_IFDIR) {
                    sprintf(logmessage,"%s exists but is not a directory",basedir) ; 
                    FREE(dirpath) ;
                    return -1 ;
                } else {
                     /*
                    ** dirpath already exists and is a directory. 
                    */
                }
            }
             *slash++ = '/';

            /* 
            ** Avoid unnecessary calls to `stat' when given
            ** pathnames containing multiple adjacent slashes.  
            */
            while (*slash == '/') slash++;
       }
    }
    /*
    ** We have created all leading directories so let see the last one
    */
    basedir = dirpath ;
    if ( mkdir (basedir, 0777) ) {
        savederrno = errno ;
#ifdef STANDART_FILE_CALL
        if ( stat (basedir,&statbuf ) ) {
#else
        if ( stat64 (basedir,&statbuf ) ) {
#endif
            sprintf(logmessage,"Error creation directory %s (%s)",basedir,strerror(savederrno)) ; 
            /*
            ** We tell the client not to retry in the following case (even in waiting
            ** WAITRETRYTIME the problem will not be solved) :
            **        EACCES        : Search permission denied
            **        EDQUOT        : No more quota 
            **        ENOSPC        : No more space
            **        ELOOP        : To many symbolic links on path
            **        ENAMETOOLONG: Path argument too long
            **        ENOTDIR        : A component in path is not a directory
            **        EROFS        : The path prefix resides on a read-only file system.
            **        ENOENT      : A component of the path prefix does not exist or is a null pathname.
            **        EEXIST      : The named file already exists.
            */
             if ( savederrno == EACCES ||
                    savederrno == EDQUOT ||
                    savederrno == ENOSPC ||
                    savederrno == ELOOP ||
                    savederrno == ENAMETOOLONG ||
                    savederrno == ENOTDIR ||
                    savederrno == EROFS ||
                    savederrno == ENOENT ||
                    savederrno == EEXIST ) {
                FREE(dirpath) ;
                return -1 ;
            } else {
                FREE(dirpath) ;
                return 1 ;
            }
        } else if ( (statbuf.st_mode & S_IFDIR) != S_IFDIR) {
            sprintf(logmessage,"%s exists but is not a directory",basedir) ; 
            FREE(dirpath) ;
            return -1 ;
        } else {
             /*
            ** dirpath already exists and is a directory. 
            */
            FREE(dirpath) ;
            return 0 ;
        }
    }
    FREE(dirpath) ;
    return 0 ;
}
/*******************************************************************************
** bbftpd_storecreatefile :                                                    *
**                                                                             *
**      Routine to create a file                                               *
**                                                                             *
**      OUTPUT variable :                                                      *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          transferoption                   NOT MODIFIED                      * 
**          filesize                         NOT MODIFIED                      *
**                                                                             *
**      TO FREE before any return : filepath                                   *
**                                                                             *
**      RETURN:                                                                *
**          -1  Transfer failed calling has to close the connection no retry   *
**           0  OK  (Does not mean the transfer is successfull                 *
**           1  Transfer failed calling has to close the connection            *
**                                                                             *
*******************************************************************************/
 
int bbftpd_storecreatefile(char *filename, char *logmessage) 
{
    char    *filepath ;
    int     fd ;
    int     lastslash ;
    int     savederrno ;
    int     retcode ;
#ifdef STANDART_FILE_CALL
     off_t        toseek ;
    struct stat statbuf ;
#else
    off64_t        toseek ;
    struct stat64 statbuf ;
#endif
   
    /*
    ** Check if it is a rfio creation
    */
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storecreatefile_rfio(filename,logmessage) ;
#else
        sprintf(logmessage,"Fail to create file : RFIO not supported") ;
        return -1 ;
#endif
    }
    /*
    ** make a copy of filename for security
    */
    if ( (filepath = (char *) malloc (strlen(filename)+1) ) == NULL ) {
        sprintf(logmessage,"Error allocating memory for filepath : %s",strerror(errno)) ;
        return 1 ;
    }
    strcpy (filepath, filename);
    /*
    ** Check if the file exist
    **      We are going to first see if the directory exist
    **      and then look at the options 
    **      and then look if the file exist
    */
    lastslash = strlen(filepath) - 1 ;
    while ( lastslash >= 0 && filepath[lastslash] != '/') lastslash-- ;
    if ( lastslash == -1 ) {
        /*
        ** No slash in the path, that means that we have done a chdir
        ** before so do not care for the directory problem
        */
    } else if ( lastslash == 0 ) {
        /*
        ** A slash in first position so we suppose the "/" directory 
        ** exist ... nothing to do
        */
    } else if ( lastslash == strlen(filepath) - 1 ) {
        /*
        ** The filename end with a slash ..... error
        */
        sprintf(logmessage,"Filename %s ends with a /",filepath) ;
        FREE(filepath) ;
        return -1 ;
    } else {
        filepath[lastslash] = '\0';
        /*
        ** Check the existence of the directory 
        */
#ifdef STANDART_FILE_CALL
        if ( stat(filepath,&statbuf) < 0 ) {
#else
        if ( stat64(filepath,&statbuf) < 0 ) {
#endif
            /*
            ** It may be normal to get an error if the directory
            ** does not exist but some error code must lead
            ** to the interruption of the transfer:
            **        EACCES        : Search permission denied
            **        ELOOP        : To many symbolic links on path
            **        ENAMETOOLONG: Path argument too long
            **        ENOTDIR        : A component in path is not a directory
            */
             savederrno = errno ;
            if ( savederrno == EACCES ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ) {
                sprintf(logmessage,"Error stating directory %s : %s ",filepath,strerror(savederrno)) ;
                FREE(filepath) ;
                return -1 ;
            } else if (savederrno == ENOENT) {
                /*
                ** The directory does not exist so check for the TROPT_DIR
                ** option 
                */
                if ( (transferoption & TROPT_DIR ) != TROPT_DIR ) {
                    sprintf(logmessage,"Directory (%s) creation needed but TROPT_DIR not set",filepath) ;
                    FREE(filepath) ;
                    return -1 ;
                } else {
                    if ( (retcode = bbftpd_storemkdir(filepath,logmessage,1)) != 0 ) {
                        FREE(filepath) ;
                        return retcode ;
                    }
                    filepath[lastslash] = '/';
                }
            } else {
                sprintf(logmessage,"Error stating directory %s : %s ",filepath,strerror(savederrno)) ;
                FREE(filepath) ;
                return 1 ;
            }
        } else {
            /*
            ** The directory exist, check if it is a directory
            */
            if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
                /*
                ** OK correct
                */
                filepath[lastslash] = '/';
            } else {
                sprintf(logmessage,"%s is a not a directory",filepath) ;
                FREE(filepath) ;
                return -1 ;
            }
        }
    }
    /*
    ** At this stage all directory exists so check for the file 
    */
#ifdef STANDART_FILE_CALL
    if ( stat(filepath,&statbuf) < 0 ) {
#else
    if ( stat64(filepath,&statbuf) < 0 ) {
#endif
        /*
        ** It may be normal to get an error if the file
        ** does not exist but some error code must lead
        ** to the interruption of the transfer:
        **        EACCES        : Search permission denied
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG: Path argument too long
        **        ENOTDIR        : A component in path is not a directory
        */
        savederrno = errno ;
        if ( savederrno == EACCES ||
            savederrno == ELOOP ||
            savederrno == ENAMETOOLONG ||
            savederrno == ENOTDIR ) {
            sprintf(logmessage,"Error stating file %s : %s ",filepath,strerror(savederrno)) ;
            FREE(filepath) ;
            return -1 ;
        } else if (savederrno == ENOENT) {
            /*
            ** That is normal the file does not exist
            */
        } else {
            sprintf(logmessage,"Error stating file %s : %s ",filepath,strerror(savederrno)) ;
            FREE(filepath) ;
            return 1 ;
        }
    } else {
        /*
        ** The file exists so check if it is a directory
        */
        if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
            sprintf(logmessage,"File %s is a directory",filepath) ;
            FREE(filepath) ;
            return -1 ;
        }
        /*
        ** check if it is writable
        */
        if ( (statbuf.st_mode & S_IWUSR) != S_IWUSR) {
            sprintf(logmessage,"File %s is not writable",filepath) ;
            FREE(filepath) ;
            return -1 ;
        }
        /*
        ** In order to set correctly the user and group id we
        ** erase the file first
        */
        unlink(filepath) ;
    }                
    /*
    ** We create the file
    */
#ifdef STANDART_FILE_CALL
    if ((fd = open(filepath,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#else
    if ((fd = open64(filepath,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#endif
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
        */
        savederrno = errno ;
        sprintf(logmessage,"Error creation file %s : %s ",filepath,strerror(errno)) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EACCES        : Search permission denied
        **        EDQUOT        : No more quota 
        **        ENOSPC        : No more space
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG: Path argument too long
        **        ENOTDIR        : A component in path is not a directory
        */
        if ( savederrno == EACCES ||
                savederrno == EDQUOT ||
                savederrno == ENOSPC ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ) {
                FREE(filepath) ;
                return -1 ;
        } else {
            FREE(filepath) ;
            return 1 ;
        }
    }
    if ( filesize == 0 ) {
	char statmessage[1024];
        close(fd) ;
        sprintf(statmessage,"PUT %s %s 0 0 0.0 0.0", currentusername, filepath);
        syslog(BBFTPD_NOTICE,statmessage);
        FREE(filepath) ;
        return 0 ;
    }
    /*
    ** Lseek to set it to the correct size
    ** We use toseek because filesize is of type my64t and 
    ** call to lseek can be done with off_t which may be
    ** of length 64 bits or 32 bits
    */
/*    toseek = filesize-1 ;
*#ifdef STANDART_FILE_CALL
*    if ( lseek(fd,toseek,SEEK_SET) < 0 ) {
*#else
*    if ( lseek64(fd,toseek,SEEK_SET) < 0 ) {
*#endif
*        sprintf(logmessage,"Error seeking file %s : %s ",filepath,strerror(errno)) ;
*        close(fd) ;
*        unlink(filepath) ;
*        free(filepath) ;
*        return 1 ;
*    }
*/
    /*
    ** Write one byte 
    */
/*    if ( write(fd,"\0",1) != 1) {
*        savederrno = errno ;
*        sprintf(logmessage,"Error writing file %s : %s ",filepath,strerror(errno)) ;
*        close(fd) ;
*        unlink(filepath) ;
*/
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EDQUOT        : No more quota 
        **        ENOSPC        : No space on device
        */
/*        if ( savederrno == EDQUOT ||
*                savederrno == ENOSPC ) {
*            free(filepath) ;
*            return -1 ;
*        } else {
*            free(filepath) ;
*            return 1 ;
*        }
*    }
*/
    /*
    ** And close the file
    */
    if ( close(fd) < 0 ) {
        savederrno = errno ;
        unlink(filepath) ;
        sprintf(logmessage,"Error closing file %s : %s ",filepath,strerror(savederrno)) ;
        if ( savederrno == ENOSPC ) {
            FREE(filepath) ;
            return -1 ;
        } else {
            FREE(filepath) ;
            return 1 ;
        }
    }
    FREE(filepath) ;
    return 0 ;
}
/*******************************************************************************
** bbftpd_storetransferfile :                                                  *
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
 
int bbftpd_storetransferfile(char *filename,int simulation,char *logmessage) 
{
#ifdef STANDART_FILE_CALL
    off_t         nbperchild ;
    off_t         nbtoget;
    off_t         startpoint ;
    off_t         nbget ;
    struct stat statbuf ;
#else
    off64_t     nbperchild ;
    off64_t     nbtoget;
    off64_t     startpoint ;
    off64_t     nbget ;
    struct stat64 statbuf ;
#endif
#ifdef WITH_GZIP
    uLong    buflen ;
    uLong    bufcomplen ;
#endif
    int     lentowrite;
    int     lenwrited;
    int     datatoreceive;
    int     dataonone;

    int     *pidfree ;
    int     *sockfree ; /* for PASV mode only */
    int     *portnumber ;
    int     i ;
    int     recsock ;
    int        retcode ;
    int        compressionon ;
    int     nfds ; 
    fd_set    selectmask ;
    struct timeval    wait_timer;
    int     fd ;
    my64_t    toprint64 ;
    struct mess_compress *msg_compress ;
    struct message *msg ;
    int     waitedtime ;
    /*
    ** Check if it is a rfio transfer
    */
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storetransferfile_rfio(filename,simulation,logmessage) ;
#else
        return 0 ;
#endif
    }
    
    if ( protocolversion <= 2 ) { /* Active mode */
      portnumber = myports ;
	} else {
	  sockfree = mysockets ;
	}
    nbperchild = filesize/requestedstreamnumber ;
    pidfree = mychildren ;
    nbpidchild = 0 ;
    /*
    ** unlinkfile is set to one in case of death of a children by a kill -9
    ** In this case the child is unable to unlink the file so that
    ** has to be done by the father
    */
    unlinkfile = 1 ;
    childendinerror = 0 ; /* No child so no error */
    for (i = 1 ; i <= requestedstreamnumber ; i++) {
        if ( i == requestedstreamnumber ) {
            startpoint = (i-1)*nbperchild;
            nbtoget = filesize-(nbperchild*(requestedstreamnumber-1)) ;
        } else {
            startpoint = (i-1)*nbperchild;
            nbtoget = nbperchild ;
        }
        if (protocolversion <= 2) { /* ACTIVE MODE */
          /*
          ** Now create the socket to receive
          */
          recsock = 0 ;
          while (recsock == 0 ) {
            recsock = bbftpd_createreceivesocket(*portnumber,logmessage) ;
          }
          if ( recsock < 0 ) {
            
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
          recsock = *sockfree ;
          sockfree++ ;
        }
        /*
        ** Set flagsighup to zero in order to be able in child
        ** not to wait STARTCHILDTO if signal was sent before 
        ** entering select. (Seen on Linux with one child)
        */
        flagsighup = 0 ;
        if ( ( retcode = fork() ) == 0 ) {
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
                wait_timer.tv_sec  = 1 ;
                wait_timer.tv_usec = 0 ;
                nfds = sysconf(_SC_OPEN_MAX) ;
                select(nfds,0,0,0,&wait_timer) ;
                waitedtime = waitedtime + 1 ;
            }
            syslog(BBFTPD_DEBUG,"Child %d starting",getpid()) ;
            /*
            ** Close all unnecessary stuff
            */
            close(incontrolsock) ;
            close(outcontrolsock) ; 
            /*
            ** Check if file exist
            */
#ifdef STANDART_FILE_CALL
            if ( stat(filename,&statbuf) < 0 ) {
#else
            if ( stat64(filename,&statbuf) < 0 ) {
#endif
                /*
                ** If the file does not exist that means that another
                ** child has detroyed it
                */ 
                i = errno ;
                syslog(BBFTPD_ERR,"Error stating file %s : %s ",filename,strerror(errno)) ;
                close(recsock) ;
                exit(i) ;
            }
            /*
            ** Open and seek to position
            */
#ifdef STANDART_FILE_CALL
            if ((fd = open(filename,O_RDWR)) < 0 ) {
#else
            if ((fd = open64(filename,O_RDWR)) < 0 ) {
#endif
                i = errno ;
                unlink(filename) ;
                syslog(BBFTPD_ERR,"Error opening file %s : %s ",filename,strerror(errno)) ;
                /*
                ** At this point a non recoverable error is 
                **        EDQUOT        : No more quota 
                **        ENOSPC        : No more space
                */
                if ( i == EDQUOT ||
                        i == ENOSPC ) {
                    close(recsock) ;
                    exit(255) ;
                } else {
                    close(recsock) ;
                    exit(i) ;
                }
            }
#ifdef STANDART_FILE_CALL
            if ( lseek(fd,startpoint,SEEK_SET) < 0 ) {
#else
            if ( lseek64(fd,startpoint,SEEK_SET) < 0 ) {
#endif
                i = errno ;
                close(fd) ;
                unlink(filename) ;
                syslog(BBFTPD_ERR,"error seeking file : %s",strerror(errno)) ;
                close(recsock) ;
                exit(i)  ;
            }
            if ( protocolversion >= 3 ) {
              if ( (ns = accept(recsock,0,0) ) < 0 ) {
                i = errno ;
                close(fd) ;
                unlink(filename) ;
                syslog(BBFTPD_ERR,"Error accept socket : %s",strerror(errno)) ;
                close(recsock) ;
                exit(i)  ;
              }
              close(recsock) ;
            } else {
              ns = recsock ;
            }
            /*
            ** start the reading loop
            ** Handle the simulation mode
            */
            if (!simulation) {
              nbget = 0 ;
              while ( nbget < nbtoget) {
                if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
                    /*
                    ** Receive the header first 
                    */
                    if (readmessage(ns,readbuffer,COMPMESSLEN,datato) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading compression header") ;
                        close(fd) ;
                        unlink(filename) ;
                        i = ETIMEDOUT ;
                        exit(i) ;
                    }
                    msg_compress = ( struct mess_compress *) readbuffer ;
#ifndef WORDS_BIGENDIAN
                    msg_compress->datalen = ntohl(msg_compress->datalen) ;
#endif
                    if ( msg_compress->code == DATA_COMPRESS) {
                        compressionon = 1 ;
                    } else {
                        compressionon = 0 ;
                    }
                    datatoreceive = msg_compress->datalen ;
                } else {
                    /*
                    ** No compression just adjust the length to receive
                    */
                    if (buffersizeperstream*1024  <= nbtoget-nbget ) {
                        datatoreceive =  buffersizeperstream*1024 ;
                    } else {
                        datatoreceive = nbtoget-nbget ;
                    }
                }
                /*
                ** Start the data collection
                */
                dataonone = 0 ;
                while ( dataonone < datatoreceive ) {
                    nfds = sysconf(_SC_OPEN_MAX) ;
                    FD_ZERO(&selectmask) ;
                    FD_SET(ns,&selectmask) ;
                    wait_timer.tv_sec  = datato ;
                    wait_timer.tv_usec = 0 ;
                    if ( (retcode = select(nfds,&selectmask,0,0,&wait_timer) ) == -1 ) {
                        /*
                        ** Select error
                        */
                        i = errno ;
                        syslog(BBFTPD_ERR,"Error select while receiving : %s",strerror(errno)) ;
                        close(fd) ;
                        unlink(filename) ;
                        close(ns) ;
                        exit(i) ;
                    } else if ( retcode == 0 ) {
                        syslog(BBFTPD_ERR,"Time out while receiving") ;
                        close(fd) ;
                        unlink(filename) ;
                        i=ETIMEDOUT ;
                        close(ns) ;
                        exit(i) ;
                    } else {
                        retcode = recv(ns,&readbuffer[dataonone],datatoreceive-dataonone,0) ;
                        if ( retcode < 0 ) {
                            i = errno ;
                            syslog(BBFTPD_ERR,"Error while receiving : %s",strerror(errno)) ;
                            close(fd) ;
                            unlink(filename) ;
                            close(ns) ;
                            exit(i) ;
                        } else if ( retcode == 0 ) {
                            i = ECONNRESET ;
                            syslog(BBFTPD_ERR,"Connexion breaks") ;
                            close(fd) ;
                            unlink(filename) ;
                            close(ns) ;
                            exit(i) ;
                        } else {
                            dataonone = dataonone + retcode ;
                        }
                    }
                }
                /*
                ** We have received all data needed
                */
#ifdef WITH_GZIP
                if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
                    if ( compressionon == 1 ) {
                        bufcomplen = buffersizeperstream*1024 ;
                        buflen = dataonone ;
                        retcode = uncompress((Bytef *) compbuffer, &bufcomplen, (Bytef *) readbuffer, buflen) ;
                        if ( retcode != 0 ) {
                            i = EILSEQ ;
                            syslog(BBFTPD_ERR,"Error while decompressing %d ",retcode) ;
                            close(fd) ;
                            unlink(filename) ;
                            close(ns) ;
                            exit(i) ;
                        }
                        memcpy(readbuffer,compbuffer,buffersizeperstream*1024) ;
                        lentowrite = bufcomplen ;
                    } else {
                        lentowrite = dataonone ;
                    }
                } else {
                    lentowrite = dataonone ;
                }
#else
                lentowrite = dataonone ;
#endif
                /*
                ** Write it to the file
                */
                lenwrited = 0 ;
                while ( lenwrited < lentowrite ) {
                    if ( (retcode = write(fd,&readbuffer[lenwrited],lentowrite-lenwrited)) < 0 ) {
                        i = errno ;
                        syslog(BBFTPD_ERR,"error writing file : %s",strerror(errno)) ;
                        close(fd) ;
                        unlink(filename) ;
                        if ( i == EDQUOT ||
                                i == ENOSPC ) {
                            close(ns) ;
                            exit(255) ;
                        } else {
                            close(ns) ;
                            exit(i) ;
                        }
                    } 
                    lenwrited = lenwrited + retcode ;
                }
                nbget = nbget+lenwrited ;
              }
              /*
              ** All data have been received so send the ACK message
              */
              msg = (struct message *) readbuffer ;
              msg->code = MSG_ACK ;
              msg->msglen = 0 ;
              if ( writemessage(ns,readbuffer,MINMESSLEN,sendcontrolto) < 0 ) {
                close(fd) ;
                unlink(filename) ;
                syslog(BBFTPD_ERR,"Error sending ACK ") ;
                close(ns) ;
                exit(ETIMEDOUT) ;
              }
              toprint64 = nbget ;
              syslog(BBFTPD_DEBUG,"Child received %" LONG_LONG_FORMAT " bytes ; end correct ",toprint64) ;
            }
            close(fd) ;
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
                unlink(filename) ;
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
		close(recsock) ;
            }
        }
    }
    /*
    ** Set the state before starting children because if the file was
    ** small the child has ended before state was setup to correct value
    */
    state = S_RECEIVING ;
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
    if (simulation) {
       /*
       ** set unlinkfile = 4 in order to delete the file after
       ** the simulated transfer
       */
       unlinkfile = 4 ;
    }
    (void) gettimeofday(&tstart, (struct timezone *)0);
    return 0 ;
}
