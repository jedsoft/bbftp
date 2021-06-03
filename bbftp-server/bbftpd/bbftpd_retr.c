/*
 * bbftpd/bbftpd_retr.c
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

 
 bbftpd_retr.c  v 2.0.0 2001/02/22  - Routines creation
                v 2.0.1 2001/04/17  - Realy wait STARTCHILDTO 
                                    - Correct indentation
                                    - Port to IRIX
                v 2.0.2 2001/05/04  - Correct include for RFIO
                v 2.1.0 2001/05/30  - Correct bbftpd_log level
                                    - Reorganise routines as in bbftp_
              
*****************************************************************************/
#include <bbftpd.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <netinet/in.h>
#include <signal.h>
/* #include <syslog.h> */
#include <sys/stat.h>
#include <sys/types.h>
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

#if defined(STANDART_FILE_CALL) || defined(STANDART_READDIR_CALL)
# define READDIR readdir
#else
# define READDIR readdir64
#endif

#ifdef STANDART_FILE_CALL
# define LSTAT lstat
# define STAT stat
# define OPEN open
# define LSEEK lseek
# define OFF_T off_t
#else
# define LSTAT lstat64
# define STAT stat64
# define OPEN open64
# define LSEEK lseek64
# define OFF_T off64_t
#endif

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

static int do_opendir (const char *dir, DIR **dp, char *msgbuf, size_t msgbuf_size)
{
   DIR *d;
   int status;

   if (NULL != (d = opendir (dir)))
     {
	*dp = d;
	return 0;
     }

   if (errno == EACCES)
     status = -1;
   else
     status = 1;

   (void) snprintf (msgbuf, msgbuf_size, "opendir %s failed : %s ", dir, strerror(errno));
   return status;
}

typedef struct Keep_File_Type
{
   char    *filename ;
   struct  Keep_File_Type *next ;
   char    filechar[3] ;
}
Keep_File_Type;

static void free_keepfile_list (Keep_File_Type *list)
{
   while (list != NULL)
     {
	Keep_File_Type *next;
	free (list->filename);
	next = list->next;
	free (list);
	list = next;
     }
}

static char *get_basename (char *path)
{
   char *p = path + strlen (path);

   while (p != path)
     {
	p--;
	if (*p == '/') return p+1;
     }
   return p;
}

int bbftpd_retrlistdir(char *pattern,char **filelist,int *filelistlen,
		       char *msgbuf, size_t msgbuf_size)
{
   char    *pointer ;
   char    *dirpath ;
   DIR     *curdir ;
#ifdef STANDART_FILE_CALL
   struct dirent *dp ;
   struct stat statbuf;
#else
# ifdef STANDART_READDIR_CALL
   struct dirent *dp ;
# else
   struct dirent64 *dp ;
# endif
   struct stat64 statbuf ;
#endif
   int     lengthtosend;
   int status;
   /*
    ** Structure to keep the filenames
    */
   Keep_File_Type *first_item, *current_item, *prev_item;
   char    *filepos ;

    /*
    **  We are going allocate memory for the directory name
    **  We allocate stlen(pattern) + 1 char for \0 + 3 char
    **  if the directory is "." 
    */
   if (NULL == (dirpath = (char *) bbftpd_malloc ( strlen(pattern) + 1 + 3 )))
     {
	snprintf (msgbuf, msgbuf_size, "%s", "Out of memory");
	return -1;
     }

   pointer = get_basename (pattern);
   if (*pointer == 0)
     {
	snprintf (msgbuf, msgbuf_size, "Pattern %s ends with a /", pattern);
        free(dirpath);
	return -1;
     }

   if (pointer == pattern)
     strcpy(dirpath, "./") ;
   else
     {
	*(pointer-1) = 0;	       /* zero out the final / */
        strcpy (dirpath, pattern);
        /*
	 ** Strip unnecessary / at the end of dirpath and reset 
	 ** only one
	 */
        strip_trailing_slashes(dirpath);
	strcat (dirpath, "/");
     }

   status = do_opendir (dirpath, &curdir, msgbuf, msgbuf_size);
   if (status != 0)
     {
	free(dirpath);
	return status;
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
   prev_item = first_item = NULL ;

   while (1)
     {
	int is_link;

	errno = 0;
	if (NULL == (dp = READDIR(curdir)))
	  {
	     if (errno == 0)
	       break;

	     snprintf (msgbuf, msgbuf_size, "Error on readdir %s (%s) ",dirpath, strerror(errno)) ;
	     free_keepfile_list (first_item);
	     closedir (curdir);
	     free (dirpath);
	     return 1;
	  }

        if (0 != fnmatch(pointer, dp->d_name, 0))
	  continue;

	lengthtosend = lengthtosend +  strlen(dirpath) + strlen(dp->d_name) + 1 + 3 ;
	if ((NULL == (current_item = (Keep_File_Type *) bbftpd_malloc(sizeof(Keep_File_Type))))
	    || (NULL == (current_item->filename = (char *) malloc( strlen(dirpath) + strlen(dp->d_name) + 1))))
	  {
	     snprintf (msgbuf, msgbuf_size, "Error getting memory for keepfile item : %s", strerror(errno));
	     if (current_item != NULL) free (current_item);
	     free_keepfile_list (first_item);
	     closedir(curdir) ;
	     free(dirpath) ;
	     return 1 ;
	  }
	current_item->next = NULL;
	sprintf (current_item->filename, "%s%s", dirpath, dp->d_name) ;

	if ( first_item == NULL )
	  first_item = current_item ;
	else
	  prev_item->next = current_item;

	prev_item = current_item;

	if (-1 == LSTAT(current_item->filename, &statbuf))
	  {
	     snprintf (msgbuf, msgbuf_size, "Error lstating file %s", current_item->filename) ;
	     free_keepfile_list (first_item);
	     closedir(curdir) ;
	     free(dirpath) ;
	     return 1 ;
	  }

	is_link = ((statbuf.st_mode & S_IFLNK) == S_IFLNK);
	current_item->filechar[0] = (is_link ? 'l' : ' ');
	if (is_link && (-1 == STAT(current_item->filename, &statbuf)))
	  current_item->filechar[1] = 'u';
	else if ((statbuf.st_mode & S_IFDIR) == S_IFDIR)
	  current_item->filechar[1] = 'd';
	else
	  current_item->filechar[1] = 'f';

	current_item->filechar[2] = '\0' ;
     }

   closedir(curdir) ;

    /*
    ** Check if numberoffile is zero and reply now in this
    ** case 
    */
   if (first_item == NULL)
     {
        *filelistlen = 0 ;
        free (dirpath);
        return 0 ;
     }

   /*
    ** Now everything is ready so prepare the answer
    */
   if (NULL == (*filelist = (char *) bbftpd_malloc (lengthtosend)))
     {
	snprintf (msgbuf, msgbuf_size,  "Error allocating memory for filelist %s",strerror(errno));
	free_keepfile_list (first_item);
	free(dirpath);
	return -1 ;
    }

   current_item = first_item ;
   filepos = *filelist ;
   while ( current_item != NULL )
     {
	strcpy (filepos, current_item->filename);
	filepos += strlen(filepos)+1;  /* skip trailing \0 */
	strcpy (filepos, current_item->filechar);
	filepos += strlen(filepos)+1;  /* skip trailing \0 */

        current_item = current_item->next ;
     }
   *filelistlen = lengthtosend ;

   free_keepfile_list (first_item);
   free(dirpath) ;
   return 0;
}



/*******************************************************************************
** bbftpd_retrcheckfile :                                                      *
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
**          -1  Incorrect check unrecoverable error                            *
**           0  OK                                                             *
**          >0  Incorrect check recoverable error                              *
**                                                                             *
*******************************************************************************/

int bbftpd_retrcheckfile(char *filename, char *msgbuf, size_t msgbuf_size)
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
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        return bbftpd_retrcheckfile_rfio(filename,msgbuf) ;
#else
        snprintf(msgbuf,msgbuf_size,"Fail to retreive : RFIO not supported") ;
        return -1 ;
#endif
    }

   if (-1 == STAT(filename, &statbuf))
     {
        /*
        ** It may be normal to get an error if the file
        ** does not exist but some error code must lead
        ** to the interruption of the transfer:
        **        EACCES        : Search permission denied
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG: Path argument too long
        **        ENOENT        : The file does not exists
        **        ENOTDIR        : A component in path is not a directory
        */
        savederrno = errno ;
	snprintf(msgbuf, msgbuf_size, "Error stating file %s : %s ", filename, strerror(savederrno)) ;
	if ((savederrno == EACCES)
            || (savederrno == ELOOP)
            || (savederrno == ENAMETOOLONG)
            || (savederrno == ENOENT)
            || (savederrno == ENOTDIR))
	  return -1 ;

	return 1;
    }

   /*
    ** The file exists so check if it is a directory
    */
   if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR)
     {
	snprintf (msgbuf, msgbuf_size, "File %s is a directory",filename) ;
	return -1 ;
     }

    sprintf (lastaccess, "%08lx", (unsigned long)statbuf.st_atime) ;
    sprintf (lastmodif, "%08lx", (unsigned long)statbuf.st_mtime) ;
    lastaccess[8] = '\0' ;
    lastmodif[8]  = '\0' ;

   if (S_ISREG(statbuf.st_mode))
     filemode = statbuf.st_mode & ~S_IFREG;
   else
     filemode = statbuf.st_mode;

    filesize = statbuf.st_size ;

   tmpnbport = filesize / (buffersizeperstream*1024) ;
   if ( tmpnbport == 0 )
     requestedstreamnumber = 1 ;
   else if ( tmpnbport < requestedstreamnumber )
     requestedstreamnumber = tmpnbport ;

   if ( requestedstreamnumber > maxstreams ) requestedstreamnumber = maxstreams ;
   return 0 ;
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
** bbftpd_retrtransferfile :                                                   *
**                                                                             *
**      Routine to transfer a file                                             *
**                                                                             *
**      INPUT variable :                                                       *
**          filename    :  file to send    NOT MODIFIED                        *
**                                                                             *
**      OUTPUT variable :                                                      *
**          msgbuf :  to write the error message in case of error          *
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
 
int bbftpd_retrtransferfile(char *filename, int simulation, char *msgbuf, size_t msgbuf_size)
{
   OFF_T         nbperchild ;
   OFF_T         nbtosend;
   OFF_T         startpoint ;
   OFF_T         nbread ;
   OFF_T         numberread ;
   OFF_T        realnbtosend ;
   my64_t    toprint64 ;
#ifdef WITH_GZIP
   uLong    buflen ;
   uLong    bufcomplen ;
#endif
   int     sendsock ;
   int        retcode ;

   int     *pidfree ;
   int     *port_or_sock;
   int     i ;
   int     fd ;

   struct message msg ;
   /*
    ** Check if it is a rfio transfer
    */
   if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
      return bbftpd_retrtransferfile_rfio(filename,simulation,msgbuf) ;
#else
      return 0 ;
#endif
   }

   childendinerror = 0 ; /* No child so no error */
   if ( protocolversion <= 2 ) /* Active mode */
     port_or_sock = myports ;
   else
     port_or_sock = mysockets ;

   nbperchild = filesize/requestedstreamnumber ;
   pidfree = mychildren ;
   nbpidchild = 0 ;
   unlinkfile = 3 ;

   /*
    ** Now start all our children
    */
   for (i = 1 ; i <= requestedstreamnumber ; i++)
     {
	int ns, err;

	startpoint = (i-1)*nbperchild;
        if ( i == requestedstreamnumber )
	  nbtosend = filesize - (nbperchild*(requestedstreamnumber-1)) ;
	else
	  nbtosend = nbperchild ;

        if (protocolversion <= 2)
	  {
	     /* ACTIVE MODE */
	     /*
	      ** Now create the socket to send
	      */
	     while (1)
	       {
		  sendsock = bbftpd_createreceivesocket (*port_or_sock, msgbuf, msgbuf_size);
		  if (sendsock > 0) break;
		  if (sendsock == 0) continue;   /* try again */

		  /*
		   ** We set childendinerror to 1 in order to prevent the father
		   ** to send a BAD message which can desynchronize the client and the
		   ** server (We need only one error message)
		   ** Bug discovered by amlutz on 2000/03/11
		   */
		  if (childendinerror == 0)
		    {
		       childendinerror = 1 ;
		       reply(MSG_BAD, msgbuf) ;
		    }
		  clean_child() ;
		  return 1 ;
	       }
	  }
	else /* PASSIVE MODE */
	  sendsock = *port_or_sock;

	port_or_sock++;

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
	if (-1 == (retcode = fork ()))
	  {
	     bbftpd_log(BBFTPD_ERR,"fork failed : %s",strerror(errno)) ;
	     snprintf (msgbuf, msgbuf_size, "fork failed : %s ",strerror(errno)) ;
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
	     close (sendsock) ;
	     continue;
	  }

	/* Child */
	wait_for_parent ();
	bbftpd_log(BBFTPD_DEBUG,"Child %d starting",getpid()) ;
	/*
	 ** Close all unnecessary stuff
	 */
	close(incontrolsock) ;
	close(outcontrolsock) ;

	if (-1 == (fd = open_file_fragment (filename, O_RDONLY, startpoint, &err)))
	  {
	     close (sendsock);
	     _exit (err);
	  }

	/*
	 ** PASSIVE MODE: accept connection
	 */
	if ( protocolversion >= 3 )
	  {
	     while (-1 == (ns = accept(sendsock,0,0)))
	       {
		  if (errno == EINTR)
		    continue;

		  i = errno ;
		  close(fd) ;
		  bbftpd_log(BBFTPD_ERR,"Error accept socket : %s",strerror(i)) ;
		  close(sendsock) ;
		  _exit(i);
	       }
	     close(sendsock) ;
	  }
	else
	  {
	     ns = sendsock ;
	  }

	if (simulation)
	  {
	     /* Don't send anything */
	     close(fd) ;
	     close(ns) ;
	     _exit(0) ;
	  }

	/*
	 ** Start the sending loop
	 */
	nbread = 0 ;
	while ( nbread < nbtosend )
	  {
	     OFF_T num_to_read = nbtosend - nbread;
	     char *databuffer = readbuffer;

	     /* The readbuffer array has size buffersizeperstream*1024 */
	     if (num_to_read > buffersizeperstream*1024)
	       num_to_read = (buffersizeperstream*1024);

	     while (-1 == (numberread = read (fd, readbuffer, num_to_read)))
	       {
		  if (errno == EINTR)
		    continue;

		  err = errno;
		  bbftpd_log(BBFTPD_ERR,"Child Error reading : %s",strerror(errno)) ;
		  close(ns) ;
		  close(fd) ;
		  _exit(err);
	       }

	     nbread = nbread + numberread ;
	     realnbtosend = numberread ;
	     databuffer = readbuffer;

	     if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP )
	       {
		  struct mess_compress msg_compress;
		  int code = DATA_NOCOMPRESS;
#ifdef WITH_GZIP
		  /*
		   ** In case of compression we are going to use a temporary buffer
		   */
		  bufcomplen = buffersizeperstream*1024 ;
		  buflen = numberread ;
		  if (0 == (retcode = compress((Bytef *)compbuffer,&bufcomplen,(Bytef *)readbuffer,buflen)))
		    {
		       databuffer = compbuffer;
		       realnbtosend =  bufcomplen ;
		       code = DATA_COMPRESS ;
		    }
#endif
		  msg_compress.code = code;
		  msg_compress.datalen = htonl(realnbtosend);

		  /*
		   ** Send the header
		   */
		  if ( writemessage(ns, (char *)&msg_compress, sizeof(msg_compress), datato))
		    {
		       err = errno;
		       bbftpd_log(BBFTPD_ERR,"Error sending header data") ;
		       close (fd);
		       close(ns) ;
		       _exit(err) ;
		    }
	       }

	     if (-1 == writemessage (ns, databuffer, realnbtosend, datato))
	       {
		  err = errno;
		  bbftpd_log(BBFTPD_ERR,"Error occurred select while sending : %s",strerror(errno));
		  close(fd) ;
		  close(ns) ;
		  _exit(err);
	       }
	  }

	close(fd) ;

	/*
	 ** All data has been sent so wait for the acknowledgement
	 */
	if (-1 == bbftpd_fd_msgread_msg (ns, &msg, ackto))
	  {
	     err = errno;
	     bbftpd_log (BBFTPD_ERR,"Error waiting/reading ACK") ;
	     close(ns) ;
	     _exit(err) ;
	  }
	if (msg.code != MSG_ACK)
	  {
	     bbftpd_log(BBFTPD_ERR,"Error unknown messge while waiting ACK %d", msg.code) ;
	     close(ns) ;
	     _exit(1);

	     toprint64 = nbtosend ;
	     bbftpd_log(BBFTPD_DEBUG,"Child send %" LONG_LONG_FORMAT " bytes ; end correct ",toprint64) ;
	  }
	close(ns) ;
	_exit(0);
     }

   /* Parent again */

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
