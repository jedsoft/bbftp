/*
 * bbftpc/bbftp_store.c
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


 bbftp_store.c v 2.0.0 2000/12/19   - Routines creation
               v 2.0.1 2001/04/19   - Correct indentation 
                                    - Port to IRIX
                                    - Close all file descriptor not needed in child
                                      (bug shown by Alvise Dorigo)
               v 2.0.2 2001/05/16   - Add bbftp_storeclosecastfile

*****************************************************************************/
#include <bbftp.h>

#include <errno.h>
#include <fcntl.h>
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
** bbftp_storeclosecastfile :                                                  *
**                                                                             *
**      Close CASTOR file                                                      *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Cast_Fd                          MODIFIED                           * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_storeclosecastfile(char *filename,char *logmessage)
{
   (void) filename; (void) logmessage;

#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storeclosecastfile_rfio(filename,logmessage) ;
    }
#endif
    return 0 ;
}
/*******************************************************************************
** bbftp_storechmod :                                                          *
**                                                                             *
**      Routine to chmod a file                                                *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_storechmod(char *filename,int mode,char *logmessage,int  *errcode)
{
    int     retcode ;
    
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storechmod_rfio(filename,mode,logmessage,errcode) ;
    }
#endif
    retcode = chmod(filename,mode) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error chmod on file %s : %s",filename,strerror(errno)) ;
        *errcode = 85 ;
        return retcode ;
    }
    return 0 ;
}

/*******************************************************************************
** bbftp_storerename :                                                         *
**                                                                             *
**      Routine to unlink a file                                               *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_storerename(char *newfilename,char *oldfilename,char *logmessage,int  *errcode)
{
    int     retcode ;
    
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storerename_rfio(newfilename,oldfilename,logmessage,errcode) ;
    }
#endif
    retcode = rename(newfilename,oldfilename) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error renaming %s to %s : %s",newfilename,oldfilename,strerror(errno)) ;
        *errcode = 81 ;
        return retcode ;
    }
    return 0 ;
}
/*******************************************************************************
** bbftp_storeunlink :                                                         *
**                                                                             *
**      Routine to unlink a file                                               *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_storeunlink(char *filename)
{
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storeunlink_rfio(filename) ;
    }
#endif
    unlink(filename) ;
    return 0 ;
}

/*******************************************************************************
** bbftp_storecheckfile :                                                      *
**                                                                             *
**      Routine to check a file                                                *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Transferoption                   NOT MODIFIED                      * 
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed no retry                                                *
**           0  OK                                                             *
**           1  Failed                                                         *
**                                                                             *
*******************************************************************************/

int bbftp_storecheckfile(char *filename,char *logmessage,int *errcode)
{
#ifdef STANDART_FILE_CALL
    struct  stat    statbuf ;
#else
    struct  stat64  statbuf ;
#endif

    int     savederrno ;
    
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storecheckfile_rfio(filename,logmessage,errcode) ;
    }
#endif

#ifdef STANDART_FILE_CALL
    if ( stat(filename,&statbuf) < 0 ) {
#else
    if ( stat64(filename,&statbuf) < 0 ) {
#endif
        /*
        ** It may be normal to get an error if the file
        ** does not exist but some error code must lead
        ** to the interruption of the transfer:
        **        EACCES       : Search permission denied
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG : Path argument too long
        **        ENOTDIR      : A component in path is not a directory
        */
        savederrno = errno ;
        if ( savederrno == EACCES ||
            savederrno == ELOOP ||
            savederrno == ENAMETOOLONG ||
            savederrno == ENOTDIR ) {
            sprintf(logmessage,"Error stating file %s : %s",filename,strerror(errno)) ;
            *errcode = 72; 
            return -1 ;
        } else if (savederrno == ENOENT) {
            /*
            ** That is normal the file does not exist
            */
        } else {
            sprintf(logmessage,"Error stating file %s : %s\n",filename,strerror(errno)) ;
            *errcode = 72; 
            return 1 ;
        }
    } else {
        /*
        ** The file exists so check if it is a directory
        */
        if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
            sprintf(logmessage,"File %s is a directory",filename) ;
            *errcode = 73;
            return -1 ;
        }
        /*
        ** check if it is writable
        */
        if ( (statbuf.st_mode & S_IWUSR) != S_IWUSR) {
            sprintf(logmessage,"File %s is not writable",filename) ;
            *errcode = 74;
            return -1 ;
        }
    }
    return 0 ;
}

/*******************************************************************************
** bbftp_storemkdir :                                                          *
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
int bbftp_storemkdir(char *dirname,char *logmessage,int recursif,int *errcode)
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

#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storemkdir_rfio(dirname,logmessage,recursif,errcode) ;
    }
#endif
    /*
    ** make a copy of dirname for security
    */
    if ( (dirpath = (char *) malloc (strlen(dirname)+1) ) == NULL ) {
        sprintf(logmessage,"Error allocating memory for dirpath : %s",strerror(errno)) ;
        *errcode = 35 ;
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
                    sprintf(logmessage,"Error creating directory %s : %s\n",basedir,strerror(savederrno)) ; 
                    *errcode = 82 ;
                    /*
                    ** We tell the client not to retry in the following case (even in waiting
                    ** WAITRETRYTIME the problem will not be solved) :
                    **        EACCES       : Search permission denied
                    **        EDQUOT       : No more quota 
                    **        ENOSPC       : No more space
                    **        ELOOP        : To many symbolic links on path
                    **        ENAMETOOLONG : Path argument too long
                    **        ENOTDIR      : A component in path is not a directory
                    **        EROFS        : The path prefix resides on a read-only file system.
                    **        ENOENT       : A component of the path prefix does not exist or is a null pathname.
                    **        EEXIST       : The named file already exists.
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
                        free(dirpath) ;
                        return -1 ;
                    } else {
                        free(dirpath) ;
                        return 1 ;
                    }
                } else if ( (statbuf.st_mode & S_IFDIR) != S_IFDIR) {
                    sprintf(logmessage,"Files %s exists but is not a directory",basedir) ; 
                    *errcode = 83 ;
                    free(dirpath) ;
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
            sprintf(logmessage,"Error creating directory %s (%s)",basedir,strerror(savederrno)) ; 
            *errcode = 82 ;
            /*
            ** We tell the client not to retry in the following case (even in waiting
            ** WAITRETRYTIME the problem will not be solved) :
            **        EACCES        : Search permission denied
            **        EDQUOT        : No more quota 
            **        ENOSPC        : No more space
            **        ELOOP         : To many symbolic links on path
            **        ENAMETOOLONG  : Path argument too long
            **        ENOTDIR       : A component in path is not a directory
            **        EROFS         : The path prefix resides on a read-only file system.
            **        ENOENT        : A component of the path prefix does not exist or is a null pathname.
            **        EEXIST        : The named file already exists.
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
                free(dirpath) ;
                return -1 ;
            } else {
                free(dirpath) ;
                return 1 ;
            }
        } else if ( (statbuf.st_mode & S_IFDIR) != S_IFDIR) {
            sprintf(logmessage,"File %s exists but is not a directory",basedir) ; 
            *errcode = 83 ;
            free(dirpath) ;
            return -1 ;
        } else {
             /*
            ** dirpath already exists and is a directory. 
            */
            free(dirpath) ;
            return 0 ;
        }
    }
    free(dirpath) ;
    return 0 ;
}
/*******************************************************************************
** bbftp_storecreatefile :                                                     *
**                                                                             *
**      Routine to create a file                                               *
**                                                                             *
**      OUTPUT variable :                                                      *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Transferoption                   NOT MODIFIED                      * 
**          BBftp_Filesize                         NOT MODIFIED                      *
**                                                                             *
**      TO FREE before any return : filepath                                   *
**                                                                             *
**      RETURN:                                                                *
**          -1   Creation failed unrecoverable error                           *
**           0   OK                                                            *
**           >0  Creation failed recoverable error                             *
**                                                                             *
*******************************************************************************/
 
int bbftp_storecreatefile(char *filename,char *logmessage, int *errcode) 
{
    char    *filepath ;
    int     fd ;
    int     lastslash ;
    int     savederrno ;
    int     retcode ;
#ifdef STANDART_FILE_CALL
    /* off_t     toseek ; */
    struct stat statbuf ;
#else
    /* off64_t     toseek ; */
    struct stat64 statbuf ;
#endif
    
    /*
    ** Check if it is a rfio creation
    */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storecreatefile_rfio(filename,logmessage,errcode) ;
    }
#endif
    /*
    ** make a copy of filename for security
    */
    if ( (filepath = (char *) malloc (strlen(filename)+1) ) == NULL ) {
        sprintf(logmessage,"Error allocating memory for %s : %s","buffer (bbftp_get)",strerror(errno)) ;
        *errcode = 35 ;
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
    } else if ( lastslash == (int)strlen(filepath) - 1 ) {
        /*
        ** The filename end with a slash ..... error
        */
        sprintf(logmessage,"Filename %s ends with a /",filepath) ;
        *errcode = 74 ;
        free(filepath) ;
        return -1 ;
    } else {
        filepath[lastslash] = '\0';
        /*
        ** Check the existence of the directory 
        */
#ifdef STANDART_FILE_CALL
        if ( stat(filepath,&statbuf) < 0 )
#else
        if ( stat64(filepath,&statbuf) < 0 )
#endif
	   {
            /*
            ** It may be normal to get an error if the directory
            ** does not exist but some error code must lead
            ** to the interruption of the transfer:
            **        EACCES       : Search permission denied
            **        ELOOP        : To many symbolic links on path
            **        ENAMETOOLONG : Path argument too long
            **        ENOTDIR      : A component in path is not a directory
            */
            savederrno = errno ;
            if ( savederrno == EACCES ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ) {
                sprintf(logmessage,"Error stating file %s : %s ",filepath,strerror(savederrno)) ;
                *errcode = 72 ;
                free(filepath) ;
                return -1 ;
            } else if (savederrno == ENOENT) {
                /*
                ** The directory does not exist so check for the TROPT_DIR
                ** option 
                */
                if ( (BBftp_Transferoption & TROPT_DIR ) != TROPT_DIR ) {
                    sprintf(logmessage,"Directory (%s) creation needed but TROPT_DIR not set",filepath) ;
                    *errcode = 75 ;
                    free(filepath) ;
                    return -1 ;
                } else {
                    if ( (retcode = bbftp_storemkdir(filepath,logmessage,1,errcode)) != 0 ) {
                        free(filepath) ;
                        return retcode ;
                    }
                    filepath[lastslash] = '/';
                }
            } else {
                sprintf(logmessage,"Error stating file %s : %s ",filepath,strerror(savederrno)) ;
                *errcode = 72 ;
                free(filepath) ;
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
                sprintf(logmessage,"File %s is a not a directory",filepath) ;
                free(filepath) ;
                *errcode = 76 ;
                return -1 ;
            }
        }
    }
    /*
    ** At this stage all directory exists so check for the file 
    */
    if ( (retcode = bbftp_storecheckfile(filepath,logmessage,errcode) ) != 0 ) {
        free(filepath) ;
        return retcode ;
    } 
    unlink(filepath) ;
    /*
    ** We create the file
    */
#ifdef STANDART_FILE_CALL
    if ((fd = open(filepath,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666)) < 0 )
#else
    if ((fd = open64(filepath,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666)) < 0 )
#endif
      {
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
        */
        savederrno = errno ;
        sprintf(logmessage,"File %s creation error : %s",filepath,strerror(errno)) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EACCES       : Search permission denied
        **        EDQUOT       : No more quota 
        **        ENOSPC       : No more space
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG : Path argument too long
        **        ENOTDIR      : A component in path is not a directory
        */
        if ( savederrno == EACCES ||
                savederrno == EDQUOT ||
                savederrno == ENOSPC ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ) {
            *errcode = 77 ;
            free(filepath) ;
            return -1 ;
        } else {
            *errcode = 77 ;
            free(filepath) ;
            return 1 ;
        }
    }
    if ( BBftp_Filesize == 0 ) {
        close(fd) ;
        free(filepath) ;
        return 0 ;
    }
    /*
    ** Lseek to set it to the correct size
    ** We use toseek because BBftp_Filesize is of type my64t and 
    ** call to lseek can be done with off_t which may be
    ** of length 64 bits or 32 bits
    */
/*
*    toseek = BBftp_Filesize-1 ;
*#ifdef STANDART_FILE_CALL
*    if ( lseek(fd,toseek,SEEK_SET) < 0 ) {
*#else
*    if ( lseek64(fd,toseek,SEEK_SET) < 0 ) {
*#endif
*        sprintf(logmessage,"Error seeking file %s : %s ",filepath,strerror(errno)) ;
*        *errcode = 78 ;
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
*        syslog(LOG_ERR,"Error writing file %s : %s ",filepath,strerror(errno)) ;
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
*            *errcode = 79 ;
*            free(filepath) ;
*            return -1 ;
*        } else {
*            *errcode = 79 ;
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
            *errcode = 80 ;
            free(filepath) ;
            return -1 ;
        } else {
            *errcode = 80 ;
            free(filepath) ;
           return 1 ;
       }
    }
   free(filepath);
    return 0 ;
}
/*******************************************************************************
** bbftp_storetransferfile :                                                   *
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
**              successfully transfered)                                       *
**          >0  Recoverable error but calling has the cleaning to do           *
**                                                                             *
*******************************************************************************/
 
int bbftp_storetransferfile(char *filename,char *logmessage, int *errcode) 
{
#ifdef STANDART_FILE_CALL
    off_t       nbperchild ;
    off_t       nbtoget;
    off_t       startpoint ;
    off_t       nbget ;
    struct stat statbuf ;
#else
    off64_t     nbperchild ;
    off64_t     nbtoget;
    off64_t     startpoint ;
    off64_t     nbget ;
    struct stat64 statbuf ;
#endif
#ifdef WITH_GZIP
    uLong   buflen ;
    uLong   bufcomplen ;
#endif
    int     lentowrite;
    int     lenwrited;
    int     datatoreceive;
    int     dataonone;

    int     *pidfree ;
    int     *socknumber ;
    int     i ;
    int     retcode ;
    int     nfds ; 
    fd_set  selectmask ;
    struct timeval    wait_timer;
    int     fd ;
    struct mess_compress *msg_compress ;
    struct message *msg ;
#ifdef WITH_GZIP
    int     compressionon ;
#endif
    int     sendsock ;
    int     *portnumber ;
    /*
    ** Check if it is a rfio transfer
    */
#if defined(WITH_RFIO) || defined(WITH_RFIO64)    
    if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
        return bbftp_storetransferfile_rfio(filename,logmessage,errcode) ;
    }
#endif
    
    if (BBftp_Protocol == 2) { /* Active mode */
      socknumber = BBftp_Mysockets ;
    } else { /* Passive mode */
      portnumber = BBftp_Myports ;
    }
    nbperchild = BBftp_Filesize/BBftp_Requestedstreamnumber ;
    pidfree = BBftp_Mychildren ;
    for (i = 1 ; i <= BBftp_Requestedstreamnumber ; i++) {
        if ( i == BBftp_Requestedstreamnumber ) {
            startpoint = (i-1)*nbperchild;
            nbtoget = BBftp_Filesize-(nbperchild*(BBftp_Requestedstreamnumber-1)) ;
        } else {
            startpoint = (i-1)*nbperchild;
            nbtoget = nbperchild ;
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
        if ( ( retcode = fork() ) == 0 ) {
            int     ns ;
            blockallsignals() ;
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
                close(sendsock) ;
                _exit(i) ;
            }
            /*
            ** Open and seek to position
            */
#ifdef STANDART_FILE_CALL
            if ((fd = open(filename,O_RDWR|O_BINARY)) < 0 ) {
#else
            if ((fd = open64(filename,O_RDWR|O_BINARY)) < 0 ) {
#endif
                i = errno ;
                /*
                ** At this point a non recoverable error is 
                **        EDQUOT        : No more quota 
                **        ENOSPC        : No more space
                */
                if ( i == EDQUOT ||
                        i == ENOSPC ) {
                    _exit(255) ;
                } else {
                    _exit(i) ;
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
                unlink(filename) ;
                sleep(CHILDWAITTIME) ;
                _exit(i) ;
            }
            if ( retcode == 0 ) {
                /*
                ** That is a time out let us exit 
                ** with the time out error
                */
                close(fd) ;
                unlink(filename) ;
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
                unlink(filename) ;
                _exit(i) ;
              }
              /*
              ** Close the listening socket
              */
              close(sendsock) ;
			} else {
			  ns = sendsock ;
			}
            /*
            ** start the reading loop
            ** In simulation mode, simulate the transfer
            */
            if (!BBftp_Simulation_Mode) {
              nbget = 0 ;
              while ( nbget < nbtoget) {
#ifdef WITH_GZIP                
                if ( (BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
                    /*
                    ** Receive the header first 
                    */
                    if (readmessage(ns,BBftp_Readbuffer,COMPMESSLEN,BBftp_Datato,1) < 0 ) {
                        close(fd) ;
                        unlink(filename) ;
                        i = ETIMEDOUT ;
                        exit(i) ;
                    }
                    msg_compress = ( struct mess_compress *) BBftp_Readbuffer ;
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
                    if (BBftp_Buffersizeperstream*1024  <= nbtoget-nbget ) {
                        datatoreceive =  BBftp_Buffersizeperstream*1024 ;
                    } else {
                        datatoreceive = nbtoget-nbget ;
                    }
                }
#else
                if (BBftp_Buffersizeperstream*1024  <= nbtoget-nbget ) {
                    datatoreceive =  BBftp_Buffersizeperstream*1024 ;
                } else {
                    datatoreceive = nbtoget-nbget ;
                }
#endif                
                /*
                ** Start the data collection
                */
                dataonone = 0 ;
                while ( dataonone < datatoreceive ) {
                    nfds = sysconf(_SC_OPEN_MAX) ;
                    FD_ZERO(&selectmask) ;
                    FD_SET(ns,&selectmask) ;
                    wait_timer.tv_sec  = BBftp_Datato ;
                    wait_timer.tv_usec = 0 ;
                    if ( (retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ) == -1 ) {
                        /*
                        ** Select error
                        */
                        i = errno ;
                        close(fd) ;
                        unlink(filename) ;
                        _exit(i) ;
                    } else if ( retcode == 0 ) {
                        close(fd) ;
                        unlink(filename) ;
                        i=ETIMEDOUT ;
                        _exit(i) ;
                    } else {
                        retcode = recv(ns,&BBftp_Readbuffer[dataonone],datatoreceive-dataonone,0) ;
                        if ( retcode < 0 ) {
                            i = errno ;
                            close(fd) ;
                            unlink(filename) ;
                            _exit(i) ;
                        } else if ( retcode == 0 ) {
                            i = ECONNRESET ;
                            close(fd) ;
                            unlink(filename) ;
                            _exit(i) ;
                        } else {
                            dataonone = dataonone + retcode ;
                        }
                    }
                }
                /*
                ** We have received all data needed
                */
#ifdef WITH_GZIP                
                if ( (BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
                    if ( compressionon == 1 ) {
                        bufcomplen = BBftp_Buffersizeperstream*1024 ;
                        buflen = dataonone ;
                        retcode = uncompress((Bytef *) BBftp_Compbuffer, &bufcomplen, (Bytef *) BBftp_Readbuffer, buflen) ;
                        if ( retcode != 0 ) {
                            i = EILSEQ ;
                            close(fd) ;
                            unlink(filename) ;
                            _exit(i) ;
                        }
                        memcpy(BBftp_Readbuffer,BBftp_Compbuffer,BBftp_Buffersizeperstream*1024) ;
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
                    if ( (retcode = write(fd,&BBftp_Readbuffer[lenwrited],lentowrite-lenwrited)) < 0 ) {
                        i = errno ;
                        close(fd) ;
                        unlink(filename) ;
                        if ( i == EDQUOT ||
                                i == ENOSPC ) {
                            _exit(255) ;
                        } else {
                            _exit(i) ;
                        }
                    } 
                    lenwrited = lenwrited + retcode ;
                }
                nbget = nbget+lenwrited ;
              }
              /*
              ** All data have been received so send the ACK message
              */
              msg = (struct message *) BBftp_Readbuffer ;
              msg->code = MSG_ACK ;
              msg->msglen = 0 ;
              if ( writemessage(ns,BBftp_Readbuffer,MINMESSLEN,BBftp_Sendcontrolto,1) < 0 ) {
                close(fd) ;
                unlink(filename) ;
                exit(ETIMEDOUT) ;
              }
			}
            /*
            ** Close the file
            */
            if ( close(fd) < 0 ) {
                /*
                ** Close failed 
                */
                i = errno ;
                unlink(filename) ;
                _exit(i) ;
            }
            _exit(0) ;
        } else {
            /*
            ** We are in father
            */
            if ( retcode == -1 ) {
                /*
                ** Fork failed ...
                **      kill all children
                **      unlink file
                */
                sprintf(logmessage,"Fork failed : %s\n",strerror(errno)) ;
                *errcode = 36 ;
                unlink(filename) ;
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
