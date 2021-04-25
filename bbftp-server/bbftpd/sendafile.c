/*
 * bbftpd/sendafile.c
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
         0  Keep the connection open (does not mean that the file has been
           successfully transfered)
        -1 Tell the calling program to close the connection

 Child Exit code:
        = 0    Success
        > 0    MES_BAD has to be sent to the client
        
        In fact only the first child ending in error will be taken in account.
        The absolute value of the exit code will be an errno code and the
        strerror(errno) will be sent as an explanation message.
        There is no 255 exit code because no error from the child may be
        considered as fatal
  
 sendafile.c  v 1.6.0  2000/03/25   - Creation of the routine. This part of
                                      code was contained in readcontrol.c
              v 1.6.1  2000/03/28   - Portage to OSF1
                                    - use data socket in non bloking mode
              v 1.6.2  2000/03/31   - Correct error on timer
              v 1.8.0  2000/04/14   - Change location of headers file for zlib
              v 1.8.3  2000/04/18   - Implement better return code to client
              v 1.8.7  2000/05/24   - Modify headers
              v 1.8.8  2000/05/31   - Check if file to read is not a directory
                                    - Take care of a zero length file
              v 1.8.10 2000/08/11   - Portage to Linux
                                    - Better start child (wait only if HUP was
                                      not sent)
              v 1.9.0  2000/08/18   - Use configure to help portage
              v 1.9.4  2000/10/16   - Close correctly data socket in child 
                                      in order not to keep socket in TIME_WAIT
                                      on the server
              v 2.0.0  2000/10/18   - Set state before starting children 
              v 2.0.1  2001/04/23   - Correct indentation
              v 2.1.0  2001/05/30   - Correct syslog level
                                     
 *****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <signal.h>
/* #include <syslog.h> */
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <status.h>
#include <structures.h>

#ifdef WITH_GZIP
#include <zlib.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftpd.h"

int sendafile(int code) {

    char    receive_buffer[MAXMESSLEN] ;
    char    receive_buffer_sup[MAXMESSLEN] ;
    char    send_buffer[MAXMESSLEN] ;
    int        savederrno ;

    char    lreadbuffer[READBUFLEN] ;
    char    buffercomp[READBUFLEN] ;
    struct    mess_compress *msg_compress ;
#ifdef WITH_GZIP
    uLong    buflen ;
    uLong    bufcomplen ;
#endif
    my64_t    filelen64 ;
    my64_t    toprint64 ;
    
#ifdef STANDART_FILE_CALL
    off_t         nbperchild ;
    off_t         nbtosend ;
    off_t         nbread ;
    off_t         nbsent ;
    off_t         numberread ;
    off_t         startpoint ;
    off_t        realnbtosend ;
    off_t        filelen ;
    struct stat statbuf ;
#else
    off64_t     nbperchild ;
    off64_t     nbtosend ;
    off64_t     nbread ;
    off64_t     nbsent ;
    off64_t     numberread ;
    off64_t     startpoint ;
    off64_t        realnbtosend ;
    off64_t        filelen ;
    struct stat64 statbuf ;
#endif

    int        lentosend ;

    char    readfilename[MAXLENFILE] ;
    
    int        retcode ;
    int        fd ;
    int        i ;
    int        sendsock ;
    char    logmessage[1024] ;
    int        compressiontype ;
    
    struct message *msg ;
    struct mess_store *msg_store ;
    struct mess_retr_ok *msg_retr_ok ;
    
    int        nfds ;
    fd_set    selectmask ; /* Select mask */
    struct timeval    wait_timer;
    
    
    /*
    ** Initilize the pid array
    */
     for ( i=0 ; i< MAXPORT ; i++) {
        pid_child[i] = 0 ;
    }
    childendinerror = 0 ; /* No child so no error */

    strcpy(currentfilename,"") ;
    if ( (retcode = readmessage(msgsock,receive_buffer,STORMESSLEN,recvcontrolto) ) < 0 ) {
        bbftpd_syslog(BBFTPD_ERR,"Error receiving file char") ;
        return retcode ;
    }
    msg_store = (struct mess_store *) receive_buffer ;
    strcpy(readfilename,msg_store->filename) ;
#ifndef WORDS_BIGENDIAN
    msg_store->nbport = ntohl(msg_store->nbport) ;
    for (i = 0 ; i< MAXPORT ; i++ ) {
        msg_store->port[i] = ntohl(msg_store->port[i]) ;
    }
#endif
    if ( code == MSG_RETR ) {
        compressiontype = NOCOMPRESSION ;
        bbftpd_syslog(BBFTPD_DEBUG,"Retreiving file %s with %d children",readfilename,msg_store->nbport) ;
    } else {
        compressiontype = COMPRESSION ;
        bbftpd_syslog(BBFTPD_DEBUG,"Retreiving file %s with %d children in compressed mode",readfilename,msg_store->nbport) ;
    }
    /*
    ** WARNING ----------
    **
    ** Do not use the receive buffer in father before having forked
    ** because all data related to the ports are in it
    **
    ** WARNING ----------
    */
    /*
    ** First stat the file in order to know if it is a directory
    */
#ifdef STANDART_FILE_CALL
    if ( stat(readfilename,&statbuf) < 0 ) {
#else
    if ( stat64(readfilename,&statbuf) < 0 ) {
#endif
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EACCES        : Search permission denied
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG: Path argument too long
        **        ENOENT        : The file does not exists
        **        ENOTDIR        : A component in path is not a directory
        */
        savederrno = errno ;
        sprintf(logmessage,"Error stating file %s : %s ",readfilename,strerror(savederrno)) ;
        bbftpd_syslog(BBFTPD_ERR,"Error stating file %s : %s ",readfilename,strerror(savederrno)) ;
        if ( savederrno == EACCES ||
            savederrno == ELOOP ||
            savederrno == ENAMETOOLONG ||
            savederrno == ENOENT ||
            savederrno == ENOTDIR ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return 0 ;
        } else {
            reply(MSG_BAD,logmessage) ;
            return 0 ;
        }
    } else {
        /*
        ** The file exists so check if it is a directory
        */
        if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
            bbftpd_syslog(BBFTPD_ERR,"file %s is a directory",readfilename) ;
            sprintf(logmessage,"File %s is a directory",readfilename) ;
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return 0 ;
        }
    }                
    /*
    ** Getting filesize in order to send it to the client
    */
#ifdef STANDART_FILE_CALL
    if ( (fd = open(readfilename,O_RDONLY)) < 0 ) {
#else
    if ( (fd = open64(readfilename,O_RDONLY)) < 0 ) {
#endif
        /*
        ** An error on openning the local file is considered
        ** as fatal. Maybe this need to be improved depending
        ** on errno
        */
        savederrno = errno ;
        bbftpd_syslog(BBFTPD_ERR,"Error opening local file %s : %s",readfilename,strerror(errno)) ;
        sprintf(logmessage,"Error opening local file %s : %s ",readfilename,strerror(errno)) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EACCES        : Search permission denied
        **        ELOOP        : To many symbolic links on path
        **        ENOENT        : No such file or directory
        **        ENAMETOOLONG: Path argument too long
        **        ENOTDIR        : A component in path is not a directory
        */
        if ( savederrno == EACCES ||
                savederrno == ELOOP ||
                savederrno == ENOENT ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
        } else {
            reply(MSG_BAD,logmessage) ;
        }
        return 0 ;
    }
#ifdef STANDART_FILE_CALL
    if ( (filelen = lseek(fd,0,SEEK_END) ) < 0 ) {
#else
    if ( (filelen = lseek64(fd,0,SEEK_END) ) < 0 ) {
#endif
        /*
        ** An error on seekin the local file is considered
        ** as fatal. lseek error  in this case is completly
        ** abnormal
        */
        close(fd) ;
        bbftpd_syslog(BBFTPD_ERR,"Error seeking local file %s : %s",readfilename,strerror(errno)) ;
        sprintf(logmessage,"Error seeking local file %s : %s ",readfilename,strerror(errno)) ;
        reply(MSG_BAD,logmessage) ;
        return 0 ;
    }
    filelen64 = filelen ;
    /*
    ** Close the file as the only interresting thing was the length
    */
    close(fd) ;
    /*
    ** We are going to send the file length 
    */
    msg = (struct message *) send_buffer ;
    msg->code = MSG_RETR_OK ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(RETROKMESSLEN) ;
#else
    msg->msglen = RETROKMESSLEN ;
#endif
    if ( writemessage(msgsock,send_buffer,MINMESSLEN,recvcontrolto) < 0 ) {
        /*
        ** Something wrong in sending message
        ** tell calling to close the connection
        */
        bbftpd_syslog(BBFTPD_ERR,"Error sending RETROK part 1") ;
        return -1 ;
    }
    msg_retr_ok = (struct mess_retr_ok *) send_buffer ;
#ifndef WORDS_BIGENDIAN
    msg_retr_ok->filesize = ntohll(filelen64) ;
#else
    msg_retr_ok->filesize = filelen64 ;
#endif
    if ( writemessage(msgsock,send_buffer,RETROKMESSLEN,recvcontrolto) < 0 ) {
        /*
        ** Something wrong in sending message
        ** tell calling to close the connection
        */
        bbftpd_syslog(BBFTPD_ERR,"Error sending RETROK part 2") ;
        return -1 ;
    }
    /*
    ** Wait for the START message
    */
    if ( readmessage(msgsock,receive_buffer_sup,MINMESSLEN,RETRSTARTTO) < 0 ) {
        /*
        ** Something wrong in receiving message
        ** tell calling to close the connection
        */
        bbftpd_syslog(BBFTPD_ERR,"Error receiving RETRSTART") ;
        return -1 ;
    }
    msg = (struct message *) receive_buffer_sup ;
    if ( msg->code == MSG_ABR) {
        /*
        ** In this case the client will not close the connection
        ** so do the same
        */
        bbftpd_syslog(BBFTPD_ERR,"Receive ABORT message") ;
        return 0 ;
    } else if ( msg->code == MSG_CREATE_ZERO ) {
        bbftpd_syslog(BBFTPD_INFO,"Send zero length file") ;
        return 0 ;
    } else if ( msg->code != MSG_RETR_START ) {
        bbftpd_syslog(BBFTPD_ERR,"Receive Unknown message code while waiting RETRSTART %d",msg->code) ;
        return -1 ;
    }
    /*
    ** So we receive the start message ...
    */
    /*
    ** Now start all our children
    */
    nbperchild = filelen/msg_store->nbport ;
    for (i = 1 ; i <= msg_store->nbport ; i++) {
        if ( i == msg_store->nbport) {
            startpoint = (i-1)*nbperchild;
            nbtosend = filelen-(nbperchild*(msg_store->nbport-1)) ;
        } else {
            startpoint = (i-1)*nbperchild;
            nbtosend = nbperchild ;
        }
        /*
        ** Now create the socket to send
        */
        sendsock = 0 ;
        while (sendsock == 0 ) {
            sendsock = createreceivesock(msg_store->port[i-1],i,logmessage) ;
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
            return 0 ;
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
            /*
            ** We are in child
            */
            /*
            ** Pause until father send a SIGHUP in order to prevent
            ** child to die before father has started all children
            */
            if ( flagsighup == 0) {
                nfds = sysconf(_SC_OPEN_MAX) ;
                wait_timer.tv_sec  = STARTCHILDTO ;
                wait_timer.tv_usec = 0 ;
                select(nfds,0,0,0,&wait_timer) ;
            }
            bbftpd_syslog(BBFTPD_DEBUG,"Child Starting") ;
            /*
            ** Close all unnecessary stuff
            */
            close(msgsock) ;
            /*
            ** And open the file 
            */
#ifdef STANDART_FILE_CALL
            if ( (fd = open(readfilename,O_RDONLY)) < 0 ) {
#else
            if ( (fd = open64(readfilename,O_RDONLY)) < 0 ) {
#endif
                /*
                ** An error on openning the local file is considered
                ** as fatal. Maybe this need to be improved depending
                ** on errno
                */
                i = errno ;
                bbftpd_syslog(BBFTPD_ERR,"Error opening local file %s : %s",readfilename,strerror(errno)) ;
                close(sendsock) ;
                exit(i) ;
            }
#ifdef STANDART_FILE_CALL
            if ( lseek(fd,startpoint,SEEK_SET) < 0 ) {
#else
            if ( lseek64(fd,startpoint,SEEK_SET) < 0 ) {
#endif
                i = errno ;
                close(fd) ;
                bbftpd_syslog(BBFTPD_ERR,"Error seeking file : %s",strerror(errno)) ;
                close(sendsock) ;
                exit(i)  ;
            }
            /*
            ** Start the sending loop
            */
            nbread = 0 ;
            while ( nbread < nbtosend ) {
	       size_t dnum;

	       dnum = nbtosend - nbread;
	       if (dnum > sizeof(lreadbuffer)) dnum = sizeof (lreadbuffer);
                if ( (numberread = read ( fd, lreadbuffer, dnum)) > 0) {
                    nbread = nbread+numberread ;
#ifdef WITH_GZIP
                    if ( compressiontype == COMPRESSION ) {
                        /*
                        ** In case of compression we are going to use
                        ** a temporary buffer
                        */
                        bufcomplen = READBUFLEN ;
                        buflen = numberread ;
                        msg_compress = ( struct mess_compress *) send_buffer;
                        retcode = compress((Bytef *)buffercomp,&bufcomplen,(Bytef *)lreadbuffer,buflen) ;
                        if ( retcode != 0 ) {
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
                            msg_compress->code = DATA_COMPRESS ;
                            lentosend = bufcomplen ;
#ifndef WORDS_BIGENDIAN
                            msg_compress->datalen = ntohl(lentosend) ;
#else
                            msg_compress->datalen = lentosend ;
#endif
                            realnbtosend =  bufcomplen ;
                            memcpy(lreadbuffer,buffercomp,READBUFLEN) ;
                        }
                        /*
                        ** Send the header
                        */
                        if ( writemessage(sendsock,send_buffer,COMPMESSLEN,datato) < 0 ) {
                            i = ETIMEDOUT ;
                            bbftpd_syslog(BBFTPD_ERR,"Error sending header data") ;
                            close(sendsock) ;
                            exit(i) ;
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
                        FD_SET(sendsock,&selectmask) ;
                        wait_timer.tv_sec  = datato  ;
                        wait_timer.tv_usec = 0 ;
                        if ( (retcode = select(nfds,0,&selectmask,0,&wait_timer) ) == -1 ) {
                            /*
                            ** Select error
                            */
                            i = errno ;
                            bbftpd_syslog(BBFTPD_ERR,"Error select while sending : %s",strerror(errno)) ;
                            close(fd) ;
                            close(sendsock) ;
                            exit(i) ;
                        } else if ( retcode == 0 ) {
                            bbftpd_syslog(BBFTPD_ERR,"Time out while sending") ;
                            close(fd) ;
                            i=ETIMEDOUT ;
                            close(sendsock) ;
                            exit(i) ;
                        } else {
                            retcode = send(sendsock,&lreadbuffer[nbsent],lentosend,0) ;
                            if ( retcode < 0 ) {
                                i = errno ;
                                bbftpd_syslog(BBFTPD_ERR,"Error while sending %s",strerror(i)) ;
                                close(sendsock) ;
                                exit(i) ;
                            } else if ( retcode == 0 ) {
                                i = ECONNRESET ;
                                bbftpd_syslog(BBFTPD_ERR,"Connexion breaks") ;
                                close(fd) ;
                                close(sendsock) ;
                                exit(i) ;
                            } else {
                                nbsent = nbsent+retcode ;
                            }
                        }
                    }
                } else {
                    i = errno ;
                    bbftpd_syslog(BBFTPD_ERR,"Child Error reading : %s",strerror(errno)) ;
                    close(sendsock) ;
                    exit(i) ;
                }
            }
            /*
            ** All data has been sent so wait for the acknoledge
            */
            if ( readmessage(sendsock,receive_buffer,MINMESSLEN,ackto) < 0 ) {
                bbftpd_syslog(BBFTPD_ERR,"Error waiting ACK") ;
                close(sendsock) ;
                exit(ETIMEDOUT) ;
            }
            msg = (struct message *) receive_buffer ;
            if ( msg->code != MSG_ACK) {
                bbftpd_syslog(BBFTPD_ERR,"Error unknown messge while waiting ACK %d",msg->code) ;
                close(sendsock) ;
                exit(1) ;
            }
            toprint64 = nbtosend ;
            bbftpd_syslog(BBFTPD_DEBUG,"Child send %" LONG_LONG_FORMAT " bytes ; end correct ",toprint64) ;
            close(sendsock) ;
            exit(0) ;
        } else {
            /*
            ** We are in father
            */
            if ( retcode == -1 ) {
                /*
                ** Fork failed ...
                */
                bbftpd_syslog(BBFTPD_ERR,"fork failed : %s",strerror(errno)) ;
                sprintf(logmessage,"fork failed : %s ",strerror(errno)) ;
                if ( childendinerror == 0 ) {
                    childendinerror = 1 ;
                    reply(MSG_BAD,logmessage) ;
                }
                clean_child() ;
                return 0 ;
            } else {
                bbftpd_syslog(BBFTPD_DEBUG,"Started child pid %d",retcode) ;
                pid_child[i-1] = retcode ;
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
    for (i = 0 ; i<MAXPORT ; i++) {
        if (pid_child[i] != 0) {
            kill(pid_child[i],SIGHUP) ;
        }
    }
    return 0 ;
}
