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
/* #include <syslog.h> */
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

#include "_bbftpd.h"

#ifdef STANDART_FILE_CALL
# define STAT stat
# define OPEN open
# define LSEEK lseek
# define OFF_T off_t
#else
# define STAT stat64
# define OPEN open64
# define LSEEK lseek64
# define OFF_T off64_t
#endif

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
   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO )
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storeclosecastfile_rfio(filename,logmessage) ;
#else
	(void) filename;
	(void) logmessage;
        return 0 ;
#endif
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

int bbftpd_storecheckoptions (void)
{
   int tmpoptions ;

#ifndef WITH_GZIP
   /*
    ** Check if Compress is available int    case of GZIP option
    */
   if ( (transferoption & TROPT_GZIP) == TROPT_GZIP )
     {
        bbftpd_log(BBFTPD_WARNING,"Compression is not available for this server: option ignored") ;
        transferoption = transferoption & ~TROPT_GZIP ;
     }
#endif

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

static int do_mkdir (const char *basedir, char *msgbuf, size_t msgbuf_size)
{
   struct STAT statbuf;
   int savederrno;

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

   if (0 == mkdir (basedir, 0777))
     return 0;

   savederrno = errno ;
   if (0 == STAT (basedir,&statbuf))
     {
	if ((statbuf.st_mode & S_IFDIR) != S_IFDIR)
	  {
	     (void) snprintf (msgbuf, msgbuf_size, "%s exists but is not a directory",basedir) ; 
	     return -1 ;
	  }
	return 0;
     }

   (void) snprintf (msgbuf, msgbuf_size, "Error creation directory %s (%s)",basedir,strerror(savederrno)) ;
   /*
    ** We tell the client not to retry in the following cases (even in waiting
    ** WAITRETRYTIME the problem will not be solved) :
    */
   if ((savederrno == EACCES)   /* Search permission denied */
       || (savederrno == EDQUOT)  /* no more quota */
       || (savederrno == ENOSPC)   /* no more space */
       || (savederrno == ELOOP)   /* too many symbolic links */
       || (savederrno == ENAMETOOLONG)   /* pathname too long */
       || (savederrno == ENOTDIR) /* path component is not a dir */
       || (savederrno == EROFS)   /* read-only filesystem */
       || (savederrno == ENOENT)  /* component of the path does not exist */
       || (savederrno == EEXIST)) /* the named file already exists */
     return -1 ;

   return 1 ;
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
int bbftpd_storemkdir (char *dirname,char *msgbuf, size_t msgbuf_size, int recursif)
{
   char *dirpath;
   int status;

   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO )
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storemkdir_rfio(dirname,logmessage,recursif) ;
#else
        (void) snprintf (msgbuf, msgbuf_size, "Fail to mkdir : RFIO not supported") ;
        return -1 ;
#endif
    }

   /*
    ** make a copy of dirname that can be modified
    */
   if (NULL == (dirpath = bbftpd_strdup (dirname)))
     {
        (void) snprintf (msgbuf, msgbuf_size, "Error allocating memory for dirpath : %s",strerror(errno)) ;
        return 1 ;
     }

   /*
    ** Strip trailing slash
    */
   strip_trailing_slashes(dirpath) ;
   if ( recursif == 1 )		       /* create all directories */
     {
	char *slash = dirpath ;
        while (1)
	  {
	     while (*slash == '/') slash++;   /* skip leading slashes */
	     slash = (char *) strchr (slash, '/');
	     if ( slash == NULL ) break ;
	     *slash = 0;	       /* truncate the path */

	     if (0 != (status = do_mkdir (dirpath, msgbuf, msgbuf_size)))
	       {
		  FREE(dirpath);
		  return status;
	       }
             *slash++ = '/'; 	       /* restore the slash */
	  }
     }

   status = do_mkdir (dirpath, msgbuf, msgbuf_size);
   FREE (dirpath);
   return status;
}

static int do_check_dir (char *dir, char *msgbuf, size_t msgbuf_size)
{
   struct STAT statbuf;

   /*
    ** Check the existence of the directory 
    */
   if (0 == STAT (dir, &statbuf))
     {
	if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR)
	  return 0;

	(void) snprintf (msgbuf, msgbuf_size, "%s is a not a directory", dir) ;
	return -1 ;
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
   if ((errno == EACCES)
       || (errno == ELOOP)
       || (errno == ENAMETOOLONG)
       || (errno == ENOTDIR))
     {
	(void) snprintf (msgbuf, msgbuf_size, "Error stating directory %s : %s ", dir, strerror(errno)) ;
	return -1 ;
     }

   if (errno == ENOENT)
     {
	/* dir does not exist.  Create it if allowed */
	if ( (transferoption & TROPT_DIR ) != TROPT_DIR )
	  {
	     (void) snprintf (msgbuf, msgbuf_size, "Directory (%s) creation needed but TROPT_DIR not set", dir) ;
	     return -1 ;
	  }
	return bbftpd_storemkdir (dir, msgbuf, msgbuf_size, 1);
     }

   (void) snprintf (msgbuf, msgbuf_size, "Error stating directory %s : %s ", dir, strerror(errno)) ;
   return 1 ;
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

int bbftpd_storecreatefile (char *filename, char *msgbuf, size_t msgbuf_size)
{
   struct STAT statbuf;
   char    *filepath ;
   int     fd ;
   int     lastslash ;
   int status, savederrno;

   /*
    ** Check if it is a rfio creation
    */
   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO )
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storecreatefile_rfio(filename, msgbuf);
#else
        (void) snprintf (msgbuf, msgbuf_size, "Fail to create file : RFIO not supported") ;
        return -1 ;
#endif
    }

   if (*filename == 0)
     {
	(void) snprintf (msgbuf, msgbuf_size, "%s", "filepath is empty");
	return -1;
     }

   /*
    ** make a copy of filename for security
    */
   if (NULL == (filepath = bbftpd_strdup (filename)))
     {
	(void) snprintf (msgbuf, msgbuf_size, "Error allocating memory for filepath : %s",strerror(errno)) ;
        return 1;
     }

   /*
    ** Check if the file exist
    **      We are going to first see if the directory exist
    **      and then look at the options 
    **      and then look if the file exist
    */
   lastslash = strlen(filepath) - 1;
   if (filepath[lastslash] == '/')
     {
	/*
	 ** The filename ends with a slash ..... error
	 */
        (void) snprintf (msgbuf, msgbuf_size, "Filename %s ends with a /", filepath) ;
        FREE(filepath);
        return -1 ;
     }

   while ((lastslash >= 0)
	  && (filepath[lastslash] != '/'))
     lastslash-- ;

   /* If lastslash == 0, it refers to root dir.
    * If lastslash == -1, it refers to current working dir
    * Otherwise a directory was specified
    */
   if (lastslash > 0)
     {
        filepath[lastslash] = 0;

	status = do_check_dir (filepath, msgbuf, msgbuf_size);
	if (status != 0)
	  {
	     FREE(filepath);
	     return status;
	  }
	filepath[lastslash] = '/';
     }

    /*
    ** At this stage all directory exists so check for the file 
    */
   if (-1 == (status = STAT (filepath, &statbuf))
       && (errno != ENOENT))
     {
	savederrno = errno;

	(void) snprintf (msgbuf, msgbuf_size, "Error stating file %s : %s ",filepath,strerror(savederrno)) ;
	FREE(filepath) ;

        if ((savederrno == EACCES)
	    || (savederrno == ELOOP)
            || (savederrno == ENAMETOOLONG)
            || (savederrno == ENOTDIR))
	  return -1 ;		       /* non-recoverable */

	return 1 ;		       /* can recover */
     }

   if (status == 0)
     {
	/*
	 ** The file exists so check if it is a directory
	 */
	if ((statbuf.st_mode & S_IFDIR) == S_IFDIR)
	  {
	     (void) snprintf (msgbuf, msgbuf_size, "File %s is a directory",filepath) ;
	     FREE(filepath) ;
	     return -1 ;
	  }

	/*
	 ** check if it is writable
	 */
	if ( (statbuf.st_mode & S_IWUSR) != S_IWUSR)
	  {
	     (void) snprintf (msgbuf, msgbuf_size, "File %s is not writable",filepath) ;
	     FREE(filepath) ;
	     return -1 ;
	  }
     }

   /*
    ** In order to set correctly the user and group id we
    ** remove the file first
    */
   (void) unlink(filepath) ;

    /*
    ** We create the file
    */
   if (-1 == (fd = OPEN (filepath, O_WRONLY|O_CREAT|O_TRUNC, 0666)))
     {
        /*
	 ** Depending on errno we are going to tell the client to 
	 ** retry or not
	 */
        savederrno = errno ;
        (void) snprintf (msgbuf, msgbuf_size, "Error creation file %s : %s ",filepath,strerror(errno)) ;
	FREE(filepath) ;
        /*
	 ** We tell the client not to retry in the following case (even in waiting
	 ** WAITRETRYTIME the problem will not be solved) :
	 */
        if ((savederrno == EACCES)
	    || (savederrno == EDQUOT)
	    || (savederrno == ENOSPC)
	    || (savederrno == ELOOP)
	    || (savederrno == ENAMETOOLONG)
	    || (savederrno == ENOTDIR))
	  return -1;
	else
	  return 1 ;
     }

   /*
    ** And close the file
    */
   if (-1 == (status = close (fd)))
     {
        savederrno = errno ;
        unlink(filepath) ;

        (void) snprintf (msgbuf, msgbuf_size, "Error closing file %s : %s ", filepath, strerror(savederrno)) ;
        if (savederrno == ENOSPC)
	  status = -1;
	else
	  status = 1;
     }
   else if (filesize == 0)
     bbftpd_log (BBFTPD_NOTICE,"PUT %s %s 0 0 0.0 0.0", currentusername, filepath);;

   FREE(filepath) ;
   return status;
}

static void wait_for_parent (void)
{
   int waitedtime = 0;
   /*
    ** Pause until father send a SIGHUP in order to prevent
    ** child to die before father has started all children
    */
   while ((flagsighup == 0) && (waitedtime < STARTCHILDTO))
     {
	sleep (1);
	waitedtime = waitedtime + 1 ;
     }
}

static int open_file_fragment (const char *filename, int flags, OFF_T ofs, int *errp)
{
   int fd;

   while (-1 == (fd = OPEN (filename, flags)))
     {
	/*
	 ** An error on openning the local file is considered
	 ** as fatal. Maybe this need to be improved depending
	 ** on errno
	 */
	if (errno == EINTR) continue;
	*errp = errno;
	bbftpd_log (BBFTPD_ERR, "Error opening local file %s : %s",filename,strerror(errno)) ;
	return -1;
     }

   while (-1 == LSEEK (fd, ofs, SEEK_SET))
     {
	if (errno == EINTR) continue;
	*errp = errno;
	bbftpd_log(BBFTPD_ERR,"Error seeking file : %s",strerror(errno)) ;
	close(fd);
	return -1;
     }

   return fd;
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
 
int bbftpd_storetransferfile(char *filename,int simulation,char *msgbuf, size_t msgbuf_size)
{
   struct STAT statbuf ;
   OFF_T         nbperchild ;
   OFF_T         nbtoget;
   OFF_T         startpoint ;
   OFF_T         nbget ;
#ifdef WITH_GZIP
   uLong    bufcomplen ;
#endif
    int     lentowrite;
    int     lenwrited;
    int     datatoreceive;
    int     dataonone;
    int     *pidfree ;
   int *port_or_sock;
    int     i ;
    int     recsock ;
    int        retcode ;
    int        compressionon ;
    int     fd ;

    /*
    ** Check if it is a rfio transfer
    */
   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO )
     {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_storetransferfile_rfio(filename,simulation,msgbuf) ;
#else
        return 0 ;
#endif
     }

   if ( protocolversion <= 2 )
     port_or_sock = myports ;  /* Active mode */
   else
     port_or_sock = mysockets ;

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

   for (i = 1 ; i <= requestedstreamnumber ; i++)
     {
	int ns, err;

	startpoint = (i-1)*nbperchild;
        if ( i == requestedstreamnumber )
	  nbtoget = filesize - (nbperchild*(requestedstreamnumber-1)) ;
        else
	  nbtoget = nbperchild;

        if (protocolversion <= 2)
	  {
	     /* ACTIVE MODE */
	     /*
	      ** Now create the socket to receive
	      */
	     while (1)
	       {
		  recsock = bbftpd_createreceivesocket (*port_or_sock, msgbuf, msgbuf_size);
		  if (recsock > 0) break;
		  if (recsock == 0) continue;   /* try again */

		  /*
		   ** We set childendinerror to 1 in order to prevent the father
		   ** to send a BAD message which can desynchronize the client and the
		   ** server (We need only one error message)
		   ** Bug discovered by amlutz on 2000/03/11
		   */
		  if ( childendinerror == 0 )
		    {
		       childendinerror = 1 ;
		       reply(MSG_BAD, msgbuf) ;
		    }
		  clean_child() ;
		  return 1 ;
	       }
	  }
	else /* PASSIVE MODE */
	  recsock = *port_or_sock;

	port_or_sock++;

        /*
	 ** Set flagsighup to zero in order to be able in child
	 ** not to wait STARTCHILDTO if signal was sent before 
	 ** entering select. (Seen on Linux with one child)
	 */
        flagsighup = 0 ;

	if (-1 == (retcode = fork ()))
	  {
	     bbftpd_log(BBFTPD_ERR,"fork failed : %s",strerror(errno)) ;
	     snprintf (msgbuf, msgbuf_size, "fork failed : %s ",strerror(errno)) ;
	     unlink (filename);
	     if ( childendinerror == 0 )
	       {
		  childendinerror = 1 ;
		  reply(MSG_BAD,msgbuf) ;
	       }
	     clean_child() ;
	     return 1 ;
	  }

	if (retcode != 0)
	  {
	     /* Parent */
	     nbpidchild++ ;
	     *pidfree++ = retcode ;
	     close (recsock) ;
	     continue;
	  }

	/* Child */
	wait_for_parent ();
	bbftpd_log(BBFTPD_DEBUG,"Child %d starting", getpid ());
	/*
	 ** Close all unnecessary stuff
	 */
	close(incontrolsock) ;
	close(outcontrolsock) ;
	/*
	 ** Check if file exist
	 */
	if (-1 == stat (filename, &statbuf))
	  {
	     /*
	      ** If the file does not exist that means that another
	      ** child has detroyed it
	      */ 
	     err = errno ;
	     bbftpd_log(BBFTPD_ERR,"Error stating file %s : %s ",filename,strerror(err)) ;
	     close(recsock) ;
	     _exit(err) ;
	  }

	if (-1 == (fd = open_file_fragment (filename, O_RDWR, startpoint, &err)))
	  {
	     unlink(filename) ;
	     close (recsock);
	     if ((err == EDQUOT) || (err == ENOSPC))
	       err = 255;

	     _exit(err);
	  }

	/*
	 ** PASSIVE MODE: accept connection
	 */
	if ( protocolversion >= 3 )
	  {
	     while (-1 == (ns = accept(recsock,0,0)))
	       {
		  if (errno == EINTR)
		    continue;

		  err = errno ;
		  close (fd) ;
		  close (recsock);
		  unlink (filename);
		  bbftpd_log(BBFTPD_ERR,"Error accept socket : %s",strerror(err)) ;
		  _exit(err);
	       }
	     bbftpd_log (BBFTPD_DEBUG, "Accepted connection");
	     close(recsock) ;
	  }
	else
	  {
	     ns = recsock ;
	  }

	bbftpd_log (BBFTPD_DEBUG, "Using ns=%d, will read range %" LONG_LONG_FORMAT "-%" LONG_LONG_FORMAT " of %" LONG_LONG_FORMAT,
		    ns, (long long) startpoint, (long long)nbtoget + startpoint - 1, (long long) filesize);

	if (simulation)
	  {
	     /* Don't send anything */
	     close(fd) ;
	     close(ns) ;
	     _exit(0) ;
	  }

	/*
	 ** start the reading loop
	 */
	nbget = 0 ;
	while (nbget < nbtoget)
	  {
#if 0
	     bbftpd_log (BBFTPD_DEBUG, "Read %" LONG_LONG_FORMAT "/%" LONG_LONG_FORMAT ", need %" LONG_LONG_FORMAT,
			 (long long) nbget, (long long) nbtoget, (long long) nbtoget - nbget);
#endif

	     if ((transferoption & TROPT_GZIP ) == TROPT_GZIP)
	       {
		  struct mess_compress msg_compress;
		  /*
		   ** Receive the header first 
		   */
		  if (-1 == readmessage (ns, (char *)&msg_compress, sizeof(msg_compress), datato))
		    {
		       bbftpd_log(BBFTPD_ERR,"Error reading compression header") ;
		       close(fd) ;
		       close (ns);
		       unlink(filename) ;
		       _exit (ETIMEDOUT);
                    }
		  msg_compress.datalen = ntohl(msg_compress.datalen) ;

		  compressionon = (msg_compress.code == DATA_COMPRESS);
		  datatoreceive = msg_compress.datalen ;
#if 0
		  bbftpd_log (BBFTPD_DEBUG, "Compressed data, code=0x%X, datalen=%d", msg_compress.code, datatoreceive);
#endif
	       }
	     else
	       {
		  /*
		   ** No compression just adjust the length to receive
		   */
		  compressionon = 0;
		  datatoreceive = nbtoget-nbget;
		  if (datatoreceive > buffersizeperstream*1024)
		    datatoreceive =  buffersizeperstream*1024 ;
	       }

	     /*
	      ** Start the data collection
	      */
	     dataonone = 0 ;

	     if (-1 == readmessage (ns, readbuffer, datatoreceive, datato))
	       {
		  err = errno;
		  close (fd);
		  close (ns);
		  unlink (filename);
		  bbftpd_log (BBFTPD_DEBUG, "Child exiting in error; read %" LONG_LONG_FORMAT "/%" LONG_LONG_FORMAT " bytes on %d",
			      (long long) nbget + dataonone, (long long) nbtoget, ns);
		  _exit (err);
	       }
	     /*
	      ** We have received all data needed
	      */
	     lentowrite = datatoreceive ;
#ifdef WITH_GZIP
	     if (compressionon)
	       {
		  bufcomplen = buffersizeperstream*1024 ;   /* compbuffer allocated size */
		  retcode = uncompress((Bytef *) compbuffer, &bufcomplen, (Bytef *) readbuffer, datatoreceive) ;
		  if (retcode != 0)
		    {
		       bbftpd_log(BBFTPD_ERR, "Error %d while decompressing", retcode) ;
		       close(fd) ;
		       close(ns) ;
		       unlink(filename) ;
		       _exit(EILSEQ) ;
		    }
		  memcpy (readbuffer, compbuffer, bufcomplen);
		  lentowrite = bufcomplen;
	       }
#endif
	     /*
	      ** Write it to the file
	      */
	     lenwrited = 0 ;
	     while ( lenwrited < lentowrite )
	       {
		  int dn;

		  if (-1 == (dn = write (fd, readbuffer+lenwrited, lentowrite-lenwrited)))
		    {
		       err = errno;
		       if (err == EINTR) continue;
		       bbftpd_log (BBFTPD_ERR, "error writing file : %s", strerror(err)) ;
		       close(fd) ;
		       unlink(filename) ;
		       close (ns);
		       if ((err == EDQUOT) || (err == ENOSPC))
			 err = 255;
		       _exit (err);
		    }

		  lenwrited = lenwrited + dn ;
	       }

	     nbget = nbget + lentowrite ;
	  }

	/*
	 ** All data have been received so send the ACK message
	 */
	if (-1 == bbftpd_fd_msgwrite_len (ns, MSG_ACK, 0))
	  {
	     close(fd) ;
	     unlink(filename) ;
	     close(ns) ;
	     bbftpd_log(BBFTPD_ERR,"Error sending ACK ") ;
	     _exit(ETIMEDOUT);
	  }

	bbftpd_log(BBFTPD_DEBUG,"Child received %" LONG_LONG_FORMAT " bytes ; end correct ", (long long) nbget) ;

	/* drop */
	close(fd) ;
	close(ns) ;
	_exit(0) ;
	/* End of child code */
     }

   /* Parent code */

    /*
    ** Set the state before starting children because if the file was
    ** small the child has ended before state was setup to correct value
    */
    state = S_RECEIVING ;
    /*
    ** Start all children
    */
    pidfree = mychildren ;
    for (i = 0 ; i<nbpidchild ; i++)
     {
        if ( *pidfree != 0 ) kill(*pidfree,SIGHUP);
	pidfree++ ;
     }

   if (simulation)
     {
	/*
	 ** set unlinkfile = 4 in order to delete the file after
	 ** the simulated transfer
	 */
	unlinkfile = 4 ;
     }

   (void) gettimeofday(&tstart, (struct timezone *)0);
   return 0 ;
}

int bbftp_store_process_transfer (char *rfile, char *cfile, int unlink_rfile_upon_fail)
{
   char logmsg[1024];
   const char *syscall_name = NULL;
   int status;

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
   if (TROPT_RFIO == (transferoption & TROPT_RFIO))
     return bbftp_store_transfer_opts_rfio (rfile, cfile, unlink_rfile_upon_fail);
#endif

   status = -1;
   if (TROPT_ACC == (transferoption & TROPT_ACC))
     {
	struct utimbuf ftime;
	unsigned long ul;

	sscanf (lastaccess, "%08lx", &ul); ftime.actime = ul;
	sscanf (lastmodif, "%08lx", &ul); ftime.modtime = ul;
	if (-1 == utime (rfile, &ftime))
	  {
	     syscall_name = "utime";
	     goto return_status;
	  }
     }

   if (TROPT_MODE == (transferoption & TROPT_MODE))
     {
	if (-1 == chmod (rfile, filemode))
	  {
	     syscall_name = "chmod";
	     goto return_status;
	  }
     }

   if (TROPT_TMP == (transferoption & TROPT_TMP))
     {
	if (-1 == rename (rfile, cfile))
	  {
	     syscall_name = "rename";
	     goto return_status;
	  }
     }

   status = 0;
   /* drop */

return_status:
   if (status == -1)
     {
	if (unlink_rfile_upon_fail) (void) unlink (rfile);
	snprintf (logmsg, sizeof(logmsg), "Error processing %s: %s failed: %s",
		  rfile, syscall_name, strerror(errno));
	reply (MSG_BAD, logmsg);
	bbftpd_log (BBFTPD_ERR, "%s\n", logmsg);
     }

   return status;
}
