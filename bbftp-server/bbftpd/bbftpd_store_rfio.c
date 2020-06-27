/*
 * bbftpd/bbftpd_store_rfio.c
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


 bbftpd_store_rfio.c    v 2.0.0 2000/12/19  - Routines creation
                        v 2.0.1 2001/04/17 - Realy wait STARTCHILDTO 
                                            - Correct indentation
                                            - Port to IRIX
                        v 2.0.2 2001/05/04  - Correct include for RFIO
                                            - Get serrno instead of errno
                                            - Add debug
                                            - Support CASTOR
                        v 2.1.0 2001/05/30  - Correct syslog level
                                            - Reorganise routines as in bbftp_
		        v 2.1.2 2001/11/19  - Fix COS 0 case
              
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

#ifdef WITH_GZIP
#include <zlib.h>
#endif

#ifdef WITH_RFIO64
#include <shift/rfio_api.h>
#include <shift/serrno.h>
#else
#include <shift.h>
#endif

#ifdef CASTOR
#include <shift/stage_api.h>
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
extern int childendinerror ;
extern int flagsighup ;
extern int mycos ;
extern int  debug ;
extern int  castfd ;
extern char *castfilename ;
extern struct  timeval  tstart ;
extern int  protocolversion ;
extern  char            currentusername[MAXLEN] ;

/*******************************************************************************
** bbftpd_storeclosecastfile_rfio :                                            *
**                                                                             *
**      Routine to chage access time a file                                    *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storeclosecastfile_rfio(char *filename,char *logmessage)
{
    int     retcode ;
    
    if ( castfd > 0 ) {
        if ( debug ) {
            fprintf(stdout,"In storeclosecastfile_rfio : rfio_close(castfd) (%d)\n",castfd) ;
        }
        if ( ( retcode = rfio_close(castfd) ) < 0 ){
            sprintf(logmessage,"Error rfio_close on %s : %s",filename,rfio_serror()) ;
            castfd = -1 ;
            return -1 ;
        }
        castfd = -1 ;
    }
    return 0 ;
}
/*******************************************************************************
** bbftpd_storechmod_rfio :                                                    *
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

int bbftpd_storechmod_rfio(char *filename,int mode,char *logmessage)
{
    int     retcode ;
    
    if ( debug ) {
        fprintf(stdout,"In storechmod_rfio : rfio_chmod(filename,mode) (%s,%d)\n",filename,mode) ;
    }
    retcode = rfio_chmod(filename,mode) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error chmod on file %s : %s",filename,rfio_serror()) ;
        return retcode ;
    }
    return 0 ;
}

/*******************************************************************************
** bbftpd_storerename :                                                        *
**                                                                             *
**      Routine to rename a file                                               *
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

int bbftpd_storerename_rfio(char *newfilename,char *oldfilename,char *logmessage)
{
    
    int     retcode ;
    
    if ( debug ) {
        fprintf(stdout,"**In storerename_rfio : rfio_rename(newfilename,oldfilename) (%s,%s)\n",newfilename,oldfilename) ;
    }
    retcode = rfio_rename(newfilename,oldfilename) ;
    if ( retcode < 0 ) {
        sprintf(logmessage,"Error renaming %s to %s : %s",newfilename,oldfilename,rfio_serror()) ;
        return retcode ;
    }
    return 0 ;
}
/*******************************************************************************
** bbftpd_storeunlink_rfio :                                                   *
**                                                                             *
**      Routine to unlink a file                                               *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Failed                                                         *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storeunlink_rfio(char *filename)
{
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    struct  stat64    statbuf ;
#else
    struct  stat    statbuf ;
#endif

#ifdef WITH_RFIO64
    if ( rfio_stat64(filename,&statbuf ) == 0) {
#else
    if ( rfio_stat(filename,&statbuf ) == 0 ) {
#endif

#ifdef CASTOR
    /*
    ** We are suppose to fill the castfilename after an open 
    ** and to call this function only when needed
    */
    if ( castfd > 0 ) {
        rfio_close(castfd) ;
        castfd = -1 ;
    }
# ifdef HAVE_STAGECLR_PATH
    if ( strncmp(filename,"/castor/",8) == 0 ) {
        if ( debug ) {
            fprintf(stdout,"**In storeunlink_rfio : stageclr_Path(castfilename) (%s)\n",castfilename) ;
        }
        stageclr_Path((u_signed64) STAGE_REMOVEHSM,NULL,castfilename) ;
    } else {
        if ( debug ) {
            fprintf(stdout,"**In storeunlink_rfio (CASTOR case) : rfio_unlink(filename) (%s)\n",filename) ;
        }
        rfio_unlink(filename) ;
    }
# else
    if ( debug ) {
        fprintf(stdout,"**In storeunlink_rfio (CASTOR case) : rfio_unlink(filename) (%s)\n",filename) ;
    }
    rfio_unlink(filename) ;
# endif
#else
    if ( debug ) {
        fprintf(stdout,"**In storeunlink_rfio : rfio_unlink(filename) (%s)\n",filename) ;
    }
    rfio_unlink(filename) ;
#endif
    }
    return 0 ;
}
/*******************************************************************************
** bbftpd_storecheckoptions_rfio :                                             *
**                                                                             *
**      Routine to check the options parameters                                *
**                                                                             *
**      RETURN:                                                                *
**          -1  Incorrect options                                              *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftpd_storecheckoptions_rfio(char *logmessage)
{
    int tmpoptions ;
    
    /*
    ** Check the compatibility with other options
    */
    if ((transferoption & TROPT_ACC) == TROPT_ACC) {
        /*
        ** RFIO has no fonction to change access and modif time
        ** till now to have the same behaviour in deamon and client
        ** do not care
        */
    }
   /*
    ** Check all known options
    */
#ifdef CASTOR
    /*
    ** As CASTOR in unable to support a rename discard the TROPT_TMP option
    */
    tmpoptions = TROPT_ACC | TROPT_MODE | TROPT_DIR | TROPT_GZIP | TROPT_RFIO | TROPT_RFIO_O ;
#else
    tmpoptions = TROPT_TMP | TROPT_ACC | TROPT_MODE | TROPT_DIR | TROPT_GZIP | TROPT_RFIO | TROPT_RFIO_O  ;
#endif
    transferoption = transferoption & tmpoptions ;
    /*
    ** Check the maxstreams
    */
    if ( requestedstreamnumber > maxstreams ) requestedstreamnumber = maxstreams ;
    return 0 ;
}       
/*******************************************************************************
** bbftpd_storemkdir_rfio :                                                    *
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
int bbftpd_storemkdir_rfio(char *dirname,char *logmessage,int recursif)
{
    char    *dirpath;
    char    *basedir ;
    char    *slash ;
    int     savederrno ;
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    struct  stat64    statbuf ;
#else
    struct  stat    statbuf ;
#endif
    int     k ;
    
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
        ** This is an absolut path, but look if it is a syntax like :
        **      host:/.....
        **      /.....
        */
        if ( *dirpath == '/' ) {
            while (*slash == '/') slash++;
        } else {
            /*
            ** find the :
            */
            k = 0 ;
            while (*slash != ':' && k < strlen(dirpath) ) {
                slash++;
                k++ ;
            }
            if ( k == strlen(dirpath) ) {
                sprintf(logmessage,"Incorrect directory name : %s",dirpath) ;
                FREE(dirpath) ;
                return -1 ;
            } else {
                slash++ ;
                slash++ ;
            }
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
            if ( debug ) {
                fprintf(stdout,"**In storemkdir_rfio : rfio_mkdir(basedir,0777) (%s)\n",basedir) ;
            }
            if ( rfio_mkdir (basedir, 0777) ) {
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
                if ( debug ) {
                    fprintf(stdout,"**In storemkdir_rfio : rfio_stat(basedir,&statbuf) (%s)\n",basedir) ;
                }
#ifdef WITH_RFIO64
                if ( rfio_stat64(basedir,&statbuf ) ) {
#else
                if ( rfio_stat(basedir,&statbuf ) ) {
#endif
                    sprintf(logmessage,"Error creation directory %s (%s)",basedir,rfio_serror()) ; 
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
    if ( debug ) {
        fprintf(stdout,"**In storemkdir_rfio : rfio_mkdir(basedir,0777) (%s)\n",basedir) ;
    }
    if ( rfio_mkdir (basedir, 0777) ) {
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
        if ( debug ) {
            fprintf(stdout,"**In storemkdir_rfio : rfio_stat(basedir,&statbuf) (%s)\n",basedir) ;
        }
#ifdef WITH_RFIO64
        if ( rfio_stat64(basedir,&statbuf ) ) {
#else
        if ( rfio_stat(basedir,&statbuf ) ) {
#endif
            sprintf(logmessage,"Error creation directory %s (%s)",basedir,rfio_serror()) ; 
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
** bbftpd_storecreatefile_rfio :                                               *
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
**          -1  Transfer failed calling has to close the connection            *
**           0  OK  (Does not mean the transfer is successfull                 *
**                                                                             *
*******************************************************************************/

int bbftpd_storecreatefile_rfio(char *filename, char *logmessage) 
{
    char    *filepath ;
    int     fd ;
    int     lastslash ;
    int     savederrno ;
    int     retcode ;
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    off64_t   toseek ;
    struct  stat64    statbuf ;
#else
    off_t   toseek ;
    struct  stat    statbuf ;
#endif
    int		hpss_file = 0;
    
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
        if ( debug ) {
            fprintf(stdout,"**In storecreatefile_rfio : rfio_stat(filepath,&statbuf) (%s)\n",filepath) ;
        }
#ifdef WITH_RFIO64
        if ( rfio_stat64(filepath,&statbuf ) ) {
#else
        if ( rfio_stat(filepath,&statbuf ) ) {
#endif
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
            /*
            ** It may be normal to get an error if the directory
            ** does not exist but some error code must lead
            ** to the interruption of the transfer:
            **        EACCES        : Search permission denied
            **        ELOOP        : To many symbolic links on path
            **        ENAMETOOLONG: Path argument too long
            **        ENOTDIR        : A component in path is not a directory
            */
            if ( savederrno == EACCES ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ) {
                sprintf(logmessage,"Error stating directory %s : %s ",filepath,rfio_serror()) ;
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
                    if ( (retcode = bbftpd_storemkdir_rfio(filepath,logmessage,1)) != 0 ) {
                        FREE(filepath) ;
                        return retcode ;
                    }
		    /* Now check if the new dir is in HPSS */
#ifdef WITH_RFIO64
                    if ( rfio_stat64(filepath,&statbuf ) ) {
#else
                    if ( rfio_stat(filepath,&statbuf ) ) {
#endif
                        sprintf(logmessage,"Error stating directory %s : %s ",filepath,rfio_serror()) ;
                        FREE(filepath) ;
                        return -1 ;
	            }
		    if (statbuf.st_dev == 0 && statbuf.st_ino == 1) {
			hpss_file = 1;
			if ( debug ) {
				fprintf(stdout,"**In storecreatefile_rfio : HPSS file\n") ;
			}
 		    }
                    filepath[lastslash] = '/';
                }
            } else {
                sprintf(logmessage,"Error stating directory %s : %s ",filepath,rfio_serror()) ;
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
				/*
				**	check if it is a hpss file
				*/
				if (statbuf.st_dev == 0 && statbuf.st_ino == 1) {
					hpss_file = 1;
					if ( debug ) {
						fprintf(stdout,"**In storecreatefile_rfio : HPSS file\n") ;
					}
 				}
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
    if ( debug ) {
        fprintf(stdout,"**In storecreatefile_rfio : rfio_stat(filepath,&statbuf) (%s)\n",filepath) ;
    }
#ifdef WITH_RFIO64
    if ( rfio_stat64(filepath,&statbuf ) < 0 ) {
#else
    if ( rfio_stat(filepath,&statbuf ) < 0 ) {
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
        if ( savederrno == EACCES ||
            savederrno == ELOOP ||
            savederrno == ENAMETOOLONG ||
            savederrno == ENOTDIR ) {
            sprintf(logmessage,"Error stating file %s : %s ",filepath,rfio_serror()) ;
            FREE(filepath) ;
            return -1 ;
        } else if (savederrno == ENOENT) {
            /*
            ** That is normal the file does not exist
            */
        } else {
            sprintf(logmessage,"Error stating file %s : %s ",filepath,rfio_serror()) ;
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
        ** erase the file first excetp if it is CASTOR
        */
#ifndef CASTOR
        bbftpd_storeunlink_rfio(filepath) ;
#endif
    }
#ifdef CASTOR
    /*
    ** Do nothing except if the file length is null 
    */
    if ( filesize == 0 ) {
#ifdef WITH_RFIO64
        if ((fd = rfio_open64(filepath,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#else
        if ((fd = rfio_open(filepath,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#endif
            /*
            ** Depending on errno we are going to tell the client to 
            ** retry or not
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
            sprintf(logmessage,"Error creation file %s : %s ",filepath,rfio_serror()) ;
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
        if ( debug ) {
            fprintf(stdout,"**In storecreatefile_rfio : rfio_close(fd) (%d)\n",fd) ;
        }
        rfio_close(fd) ;
        FREE(filepath) ;
        return 0 ;
    } else {
        if ( debug ) {
            fprintf(stdout,"**In storecreatefile_rfio : I do nothing with Castor (%s)\n",filepath) ;
        }
        FREE(filepath) ;
        return 0 ;
    }
#else
    /*
    ** We create the file
    */
    if ( debug ) {
        fprintf(stdout,"**In storecreatefile_rfio : rfio_open(filepath) (%s)\n",filepath) ;
    }
#ifdef WITH_RFIO64
    if ((fd = rfio_open64(filepath,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#else
    if ((fd = rfio_open(filepath,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#endif
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
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
        sprintf(logmessage,"Error creation file %s : %s ",filepath,rfio_serror()) ;
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
    if ( mycos >= 0 && hpss_file) {
        if ( debug ) {
           fprintf(stdout,"**In storecreatefile_rfio : rfio_setcos(%d,%" LONG_LONG_FORMAT ",%d)\n", fd, filesize, mycos) ;
        }
#ifdef WITH_RFIO64
        rfio_setcos64(fd,filesize,mycos) ;
#else
        rfio_setcos(fd,(int)filesize,mycos) ;
#endif
    }
    if ( filesize == 0 ) {
	char statmessage[1024];
        if ( debug ) {
            fprintf(stdout,"**In storecreatefile_rfio : rfio_close(fd) (%d)\n",fd) ;
        }
        rfio_close(fd) ;
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
*    if ( debug ) {
*        fprintf(stdout,"**In storecreatefile_rfio : rfio_lseek(fd,toseek,SEEK_SET) (%d,%d)\n",fd,toseek) ;
*    }
*    if ( rfio_lseek(fd,toseek,SEEK_SET) < 0 ) {
*        sprintf(logmessage,"Error seeking file %s : %s ",filepath,rfio_serror()) ;
*        if ( debug ) {
*            fprintf(stdout,"**In storecreatefile_rfio : rfio_close(fd) (%d)\n",fd) ;
*        }
*        rfio_close(fd) ;
*        bbftpd_storeunlink_rfio(filepath) ;
*        free(filepath) ;
*        return 1 ;
*    }
*/
    /*
    ** Write one byte 
    */
/*    if ( debug ) {
*        fprintf(stdout,"**In storecreatefile_rfio : rfio_write(fd,) (%d)\n",fd) ;
*    }
*    if ( rfio_write(fd,"\0",1) != 1) {
*        if ( serrno != 0 ) {
*            savederrno = serrno ;
*        } else if ( rfio_errno != 0 ) {
*            savederrno = rfio_errno ;
*        } else if ( errno != 0 ){
*            savederrno = errno ;
*        } else {
*/
            /*
            ** We use EBFONT in case of undescribed error
            */
/*            savederrno = 57 ;
*        }
*        sprintf(logmessage,"Error writing file %s : %s ",filepath,rfio_serror()) ;
*        if ( debug ) {
*            fprintf(stdout,"**In storecreatefile_rfio : rfio_close(fd) (%d)\n",fd) ;
*        }
*        rfio_close(fd) ;
*        bbftpd_storeunlink_rfio(filepath) ;
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
    if ( debug ) {
        fprintf(stdout,"**In storecreatefile_rfio : rfio_close(fd) (%d)\n",fd) ;
    }
    if ( rfio_close(fd) < 0 ) {
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
        sprintf(logmessage,"Error closing file %s : %s ",filepath,rfio_serror()) ;
        bbftpd_storeunlink_rfio(filepath) ;
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
#endif
}
/*******************************************************************************
** bbftpd_storetransferfile_rfio :                                             *
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
**                successfully transfered)                                     *
**          >0  Recoverable error but calling has the cleaning to do           *
**                                                                             *
*******************************************************************************/
 
int bbftpd_storetransferfile_rfio(char *filename,int simulation,char *logmessage) 
{
#if defined (WITH_RFIO64) && !defined(STANDART_FILE_CALL)
    off64_t   nbperchild ;
    off64_t   nbtoget;
    off64_t   startpoint ;
    off64_t   nbget ;
    off64_t   toseek ;
    struct stat64 statbuf ;
#else
    off_t   nbperchild ;
    off_t   nbtoget;
    off_t   startpoint ;
    off_t   nbget ;
    off_t   toseek ;
    struct stat statbuf ;
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
    ** Set castfd to -1 to differentiate case
    */
    castfd = -1 ;
    if ( protocolversion <= 2 ) { /* Active mode */
      portnumber = myports ;
    } else {
      sockfree = mysockets ;
    }
    nbperchild = filesize/requestedstreamnumber ;
    pidfree = mychildren ;
    nbpidchild = 0 ;
    /*
    ** unlike is set to one in case of death of a children by a kill -9
    ** In this case the child is unable to unlink the file so that
    ** has to be done by the father
    */
    childendinerror = 0 ; /* No child so no error */
#ifdef CASTOR
    /*
    ** CASTOR has a very strange behaviour, you cannot open twice the
    ** same file with the castor name in write mode !!!. So if the
    ** name starts by /castor the father is going to open the file 
    ** in write mode, look at the real file name and children will 
    ** use this real file name. The close will be done by the 
    ** father
    ** CARE HAS TO BE TAKEN FOR THIS CLOSE BECAUSE THIS MAY LEAD
    ** TO OPEN DESCRIPTOR IN FATHER
    */
    if ( debug ) {
        fprintf(stdout,"**In bbftpd_storetransferfile_rfio : rfio_open(filename) (%s)\n",filename) ;
    }
#ifdef WITH_RFIO64
    if ((castfd = rfio_open64(filename,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#else
    if ((castfd = rfio_open(filename,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0 ) {
#endif
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
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
        sprintf(logmessage,"Error creation file %s : %s ",filename,rfio_serror()) ;
        syslog(BBFTPD_ERR,"Error creation file %s : %s ",filename,rfio_serror()) ;
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
        if ( i == EACCES ||
                i == EDQUOT ||
                i == ENOSPC ||
                i == ELOOP ||
                i == ENAMETOOLONG ||
                i == ENOTDIR ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return -1 ;
        } else {
            reply(MSG_BAD,logmessage) ;
            return 1 ;
        }
    }
    if ( mycos >= 0 ) {
#ifdef WITH_RFIO64
        if ( rfio_stat64(filename,&statbuf) < 0 ) {
#else
        if ( rfio_stat(filename,&statbuf) < 0 ) {
#endif			
            /*
            ** If the file does not exist that means that another
            ** child has detroyed it
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
            syslog(BBFTPD_ERR,"Error stating file %s : %s ",filename,rfio_serror()) ;
            close(recsock) ;
            exit(i) ;
        }
    }
    if ( strncmp(filename,"/castor/",8) == 0 ) {
        if ( debug ) {
            fprintf(stdout,"**In storetransferile_rfio : Case of a /castor/ file\n") ;
            fprintf(stdout,"**In storetransferfile_rfio : HsmIf_FindPhysicalPath(filename,&castfilename) (%s)\n",filename) ;
        }
        if (  rfio_HsmIf_FindPhysicalPath(filename,&castfilename) == 0 ) {
            syslog(BBFTPD_ERR,"Error finding castor physical path %s : %s ",filename,rfio_serror()) ;
            sprintf(logmessage,"Error finding castor physical path %s : %s ",filename,rfio_serror()) ;
            if ( debug ) {
                fprintf(stdout,"**In storecreatefile_rfio : rfio_close(castfd) (%s)\n",fd) ;
            }
            rfio_close(castfd) ;
            castfd = -1 ;
            /*
            ** Within the actual state of CASTOR I am unable to unlink the file
            ** if I have not the physical name
            */
            reply(MSG_BAD,logmessage) ;
            return 1 ;
        }
    } else {
        strcpy(castfilename,filename) ;
    }
    /*
    ** Write one byte 
    */
    if ( debug ) {
        fprintf(stdout,"**In storetransferfile_rfio : rfio_write(castfd,) (%d)\n",castfd) ;
    }
    if ( rfio_write(castfd,"\0",1) != 1) {
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
        sprintf(logmessage,"Error writing file %s : %s ",filename,rfio_serror()) ;
        syslog(BBFTPD_ERR,"Error writing file %s : %s ",filename,rfio_serror()) ;
        if ( debug ) {
            fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%d)\n",fd) ;
        }
        bbftpd_storeunlink_rfio(filename) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EDQUOT        : No more quota 
        **        ENOSPC        : No space on device
        */
        if ( i == EDQUOT ||
                i == ENOSPC ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return -1 ;
        } else {
            reply(MSG_BAD,logmessage) ;
            return 1 ;
        }
    }
    /*
    ** We do not need to look at the PhysicalPath, that 
    ** has already been done by storecreatefile
    */
    if ( debug ) {
        fprintf(stdout,"**In storetransferfile_rfio : Real castor file name : %s\n",castfilename) ;
        fflush(stdout) ;
    }
#endif
    unlinkfile = 2 ;
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
          ** Now create the socket to send
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
            ** Check if file exist
            */
            if ( debug ) {
#ifdef CASTOR
                fprintf(stdout,"**In storetransferfile_rfio : rfio_stat(castfilename,&statbuf) (%s)\n",castfilename) ;
#else 
                fprintf(stdout,"**In storetransferfile_rfio : rfio_stat(filename,&statbuf) (%s)\n",filename) ;
#endif
            }
#ifdef CASTOR
# ifdef WITH_RFIO64
            if ( rfio_stat64(castfilename,&statbuf) < 0 ) {
# else
            if ( rfio_stat(castfilename,&statbuf) < 0 ) {
# endif
#else 
# ifdef WITH_RFIO64
            if ( rfio_stat64(filename,&statbuf) < 0 ) {
# else
            if ( rfio_stat(filename,&statbuf) < 0 ) {
# endif
#endif
                /*
                ** If the file does not exist that means that another
                ** child has detroyed it
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
#ifdef CASTOR
                syslog(BBFTPD_ERR,"Error stating file %s : %s ",castfilename,rfio_serror()) ;
#else 
                syslog(BBFTPD_ERR,"Error stating file %s : %s ",filename,rfio_serror()) ;
#endif
                close(recsock) ;
                exit(i) ;
            }
            /*
            ** Open and seek to position
            */
            if ( debug ) {
#ifdef CASTOR
                fprintf(stdout,"**In storetransferfile_rfio : rfio_open(castfilename,O_RDWR) (%s)\n",castfilename) ;
#else 
                fprintf(stdout,"**In storetransferfile_rfio : rfio_open(filename,O_RDWR) (%s)\n",filename) ;
#endif
            }
#ifdef CASTOR
# ifdef WITH_RFIO64
            if ((fd = rfio_open64(castfilename,O_RDWR)) < 0 ) {
# else
            if ((fd = rfio_open(castfilename,O_RDWR)) < 0 ) {
# endif
#else 
# ifdef WITH_RFIO64
            if ((fd = rfio_open64(filename,O_RDWR)) < 0 ) {
# else
            if ((fd = rfio_open(filename,O_RDWR)) < 0 ) {
# endif
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
#ifdef CASTOR
                syslog(BBFTPD_ERR,"Error opening file %s : %s ",castfilename,rfio_serror()) ;
#else 
                syslog(BBFTPD_ERR,"Error opening file %s : %s ",filename,rfio_serror()) ;
#endif
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
            if ( debug ) {
                fprintf(stdout,"**In storetransferfile_rfio : rfio_lseek(fd,startpoint,SEEK_SET) (%d,%d)\n",fd,startpoint) ;
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
                syslog(BBFTPD_ERR,"error seeking file : %s",rfio_serror()) ;
                if ( debug ) {
                    fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                }
                rfio_close(fd) ;
                close(recsock) ;
                exit(i)  ;
            }
            if ( protocolversion >= 3 ) {
              if ( (ns = accept(recsock,0,0) ) < 0 ) {
                i = errno ;
                rfio_close(fd) ;
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
                        if ( debug ) {
                            fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                        }
                        rfio_close(fd) ;
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
                        if ( debug ) {
                            fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                        }
                        rfio_close(fd) ;
                        close(ns) ;
                        exit(i) ;
                    } else if ( retcode == 0 ) {
                        syslog(BBFTPD_ERR,"Time out while receiving") ;
                        if ( debug ) {
                            fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                        }
                        rfio_close(fd) ;
                        i=ETIMEDOUT ;
                        close(ns) ;
                        exit(i) ;
                    } else {
                        retcode = recv(ns,&readbuffer[dataonone],datatoreceive-dataonone,0) ;
                        if ( retcode < 0 ) {
                            i = errno ;
                            syslog(BBFTPD_ERR,"Error while receiving : %s",strerror(errno)) ;
                            if ( debug ) {
                                fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                            }
                            rfio_close(fd) ;
                            close(ns) ;
                            exit(i) ;
                        } else if ( retcode == 0 ) {
                            i = ECONNRESET ;
                            syslog(BBFTPD_ERR,"Connexion breaks") ;
                            if ( debug ) {
                                fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                            }
                            rfio_close(fd) ;
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
                            if ( debug ) {
                                fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                            }
                            rfio_close(fd) ;
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
                    if ( debug ) {
                        fprintf(stdout,"**In storetransferfile_rfio : rfio_write(fd,&readbuffer[lenwrited],lentowrite-lenwrited) (%d,add,%d)\n",fd,lentowrite-lenwrited) ;
                    }
                    if ( (retcode = rfio_write(fd,&readbuffer[lenwrited],lentowrite-lenwrited)) < 0 ) {
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
                        syslog(BBFTPD_ERR,"error writing file : %s",rfio_serror()) ;
                        if ( debug ) {
                            fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                        }
                        rfio_close(fd) ;
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
                if ( debug ) {
                    fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
                }
                rfio_close(fd) ;
                syslog(BBFTPD_ERR,"Error sending ACK ") ;
                close(ns) ;
                exit(ETIMEDOUT) ;
              }
              toprint64 = nbget ;
              syslog(BBFTPD_DEBUG,"Child received %" LONG_LONG_FORMAT " bytes ; end correct ",toprint64) ;
              if ( debug ) {
                fprintf(stdout,"**In storetransferfile_rfio : rfio_close(fd) (%s)\n",fd) ;
              }
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
                bbftpd_storeunlink_rfio(filename) ;
                sprintf(logmessage,"fork failed : %s ",strerror(errno)) ;
                if ( childendinerror == 0 ) {
                    childendinerror = 1 ;
                    reply(MSG_BAD,logmessage) ;
                }
                clean_child() ;
                return 1 ;
            } else {
                *pidfree++ = retcode ;
                nbpidchild++ ;
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
