/*
 * bbftpd/storeafile.c
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
         255    MES_BAD_NO_RETRY has to be sent to the client.
        = 0    Success
        > 0    MES_BAD has to be sent to the client
        
        In fact only the first child ending in error will be taken in account.
        The absolute value of the exit code will be an errno code and the
        strerror(errno) will be sent as an explanation message.
  
 storeafile.c v 1.6.0  2000/03/25   - Creation of the routine. This part of
                                      code was contained in readcontrol.c
              v 1.6.1  2000/03/28   - Portage to OSF1
                                    - use data socket in non bloking mode
              v 1.8.0  2000/04/14   - Change location of headers file for zlib
              v 1.8.3  2000/04/18   - Implement better return code to client
              v 1.8.7  2000/05/24   - Modify headers
              v 1.8.8  2000/05/31   - Check if file to store is not already a directory
                                    - Take care of zero length file
              v 1.8.10 2000/08/11   - Portage to Linux
                                    - Better start child (wait only if HUP was
                                      not sent)
              v 1.9.0  2000/08/18   - Use configure to help portage
              v 1.9.4  2000/10/16   - Close correctly data socket in child 
                                      in order not to keep socket in TIME_WAIT
                                      on the server
              v 2.0.0  2000/10/18   - Set state before starting children 
              v 2.0.1  2001/04/23   - Correct indentation
              v 2.0.2  2001/05/04   - Correct return code treatment
              v 2.1.0  2001/05/30   - Correct syslog level
                                      
 *****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <status.h>
#include <structures.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#ifdef WITH_GZIP
#include <zlib.h>
#endif

extern int flagsighup ;
extern int msgsock ;
extern char currentfilename[MAXLENFILE];
extern int childendinerror ;
extern int pid_child[MAXPORT] ;
extern int state ;
extern int unlinkfile ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;
extern	int	datato ;

#ifndef WORDS_BIGENDIAN
#ifndef HAVE_NTOHLL
my64_t    ntohll(my64_t v) ;
#endif
#endif

int storeafile(int code) {

    int        retcode ;
    int        i ;
    char    receive_buffer[MAXMESSLEN] ;
    struct message *msg ;
    struct mess_store *msg_store ;
    struct mess_compress *msg_compress ;
    int        compressionon ;
    int        savederrno ;

    char    data_buffer[READBUFLEN] ;
    char    comp_buffer[READBUFLEN] ;
    
    char    logmessage[256] ;
        
    int     fd ;

    int    nfds ; /* Max number of file descriptor */
    fd_set    selectmask ; /* Select mask */
    struct timeval    wait_timer;

    my64_t    toprint64 ;
    
#ifdef STANDART_FILE_CALL
    off_t         nbperchild ;
    off_t         nbtoget;
    off_t         nbget ;
    off_t         startpoint ;
    off_t        toseek ;
    struct stat statbuf ;
#else
    off64_t     nbperchild ;
    off64_t     nbtoget;
    off64_t     nbget ;
    off64_t        startpoint ;
    off64_t        toseek ;
    struct stat64 statbuf ;
#endif
    int lentowrite;
    int lenwrited;
    int datatoreceive;
    int dataonone;
    int    recsock ;

#ifdef WITH_GZIP    
    uLong    buflen ;
    uLong    bufcomplen ;
#endif

    /*
    ** Initilize the pid array
    */
     for ( i=0 ; i< MAXPORT ; i++) {
        pid_child[i] = 0 ;
    }
    childendinerror = 0 ; /* No child so no error */
    /*
    ** Read the characteristics of the file
    */
    if ( (retcode = readmessage(msgsock,receive_buffer,STORMESSLEN,recvcontrolto)) < 0 ) {
        /*
        ** Error ...
        */
        return -1 ;
    }
    msg_store = (struct mess_store *)receive_buffer ;
    strcpy(currentfilename,msg_store->filename) ;
#ifndef WORDS_BIGENDIAN
    msg_store->nbport = ntohl(msg_store->nbport) ;
    for (i = 0 ; i< MAXPORT ; i++ ) {
        msg_store->port[i] = ntohl(msg_store->port[i]) ;
    }
    msg_store->filesize = ntohll(msg_store->filesize) ;
#endif
    if ( code == MSG_STORE ) {
        syslog(BBFTPD_DEBUG,"Storing file %s of size %" LONG_LONG_FORMAT " with %d children",currentfilename,msg_store->filesize,msg_store->nbport) ;
    } else {
        syslog(BBFTPD_DEBUG,"Storing file %s of size %" LONG_LONG_FORMAT " with %d children in compressed mode",currentfilename,msg_store->filesize,msg_store->nbport) ;
    }
    /*
    ** First stat the file in order to know if it is a directory
    */
#ifdef STANDART_FILE_CALL
    if ( stat(currentfilename,&statbuf) < 0 ) {
#else
    if ( stat64(currentfilename,&statbuf) < 0 ) {
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
            sprintf(logmessage,"Error stating file %s : %s ",currentfilename,strerror(savederrno)) ;
            syslog(BBFTPD_ERR,"Error stating file %s : %s ",currentfilename,strerror(savederrno)) ;
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return 0 ;
        } else if (savederrno == ENOENT) {
            /*
            ** That is normal the file does not exist
            */
        } else {
            sprintf(logmessage,"Error stating file %s : %s ",currentfilename,strerror(savederrno)) ;
            syslog(BBFTPD_ERR,"Error stating file %s : %s ",currentfilename,strerror(savederrno)) ;
            reply(MSG_BAD,logmessage) ;
            return 0 ;
        }
    } else {
        /*
        ** The file exists so check if it is a directory
        */
        if ( (statbuf.st_mode & S_IFDIR) == S_IFDIR) {
            syslog(BBFTPD_ERR,"file %s is a directory",currentfilename) ;
            sprintf(logmessage,"File %s is a directory",currentfilename) ;
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            return 0 ;
        }
    }                
    /*
    ** We create the file
    */
#ifdef STANDART_FILE_CALL
    if ((fd = open(currentfilename,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) < 0 ) {
#else
    if ((fd = open64(currentfilename,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) < 0 ) {
#endif
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
        */
        savederrno = errno ;
        sprintf(logmessage,"Error creation file %s : %s ",currentfilename,strerror(errno)) ;
        syslog(BBFTPD_ERR,"Error creation file %s : %s ",currentfilename,strerror(errno)) ;
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
            reply(MSG_BAD_NO_RETRY,logmessage) ;
        } else {
            reply(MSG_BAD,logmessage) ;
        }
        return 0 ;
    }
    /*
    ** If it is a zero length file close it and reply OK
    */
    if ( msg_store->filesize == 0 ) {
        close(fd) ;
        reply(MSG_OK,"OK") ;
        return 0 ;
    }
    unlinkfile = 1 ;
    /*
    ** Lseek to set it to the correct size
    */
    toseek = msg_store->filesize-1 ;
#ifdef STANDART_FILE_CALL
    if ( lseek(fd,toseek,SEEK_SET) < 0 ) {
#else
    if ( lseek64(fd,toseek,SEEK_SET) < 0 ) {
#endif
        sprintf(logmessage,"Error seeking file %s : %s ",currentfilename,strerror(errno)) ;
        close(fd) ;
        unlink(currentfilename) ;
        syslog(BBFTPD_ERR,"Error seeking file %s : %s ",currentfilename,strerror(errno)) ;
        reply(MSG_BAD,logmessage) ;
        return 0 ;
    }
    /*
    ** Write one byte 
    */
    if ( write(fd,"\0",1) != 1) {
        savederrno = errno ;
        sprintf(logmessage,"Error writing file %s : %s ",currentfilename,strerror(errno)) ;
        close(fd) ;
        unlink(currentfilename) ;
        syslog(BBFTPD_ERR,"Error writing file %s : %s ",currentfilename,strerror(errno)) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EDQUOT        : No more quota 
        **        ENOSPC        : No space on device
        */
        if ( savederrno == EDQUOT ||
                savederrno == ENOSPC ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
        } else {
            reply(MSG_BAD,logmessage) ;
        }
        return 0 ;
    }
    /*
    ** And close the file
    */
    if ( close(fd) < 0 ) {
        savederrno = errno ;
        unlink(currentfilename) ;
        syslog(BBFTPD_ERR,"Error closing file %s : %s ",currentfilename,strerror(savederrno)) ;
        sprintf(logmessage,"Error closing file %s : %s ",currentfilename,strerror(savederrno)) ;
        if ( savederrno == ENOSPC ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
        } else {
            reply(MSG_BAD,logmessage) ;
        }
        return 0 ;
    }
    /* 
    ** We calculate the starting point for each child
    */
    nbperchild = msg_store->filesize/msg_store->nbport ;
    for (i = 1 ; i <= msg_store->nbport ; i++) {
        if ( i == msg_store->nbport) {
            startpoint = (i-1)*nbperchild;
            nbtoget = msg_store->filesize-(nbperchild*(msg_store->nbport-1)) ;
        } else {
            startpoint = (i-1)*nbperchild;
            nbtoget = nbperchild ;
        }
                
        /*
        ** Now create the socket to receive
        */
        recsock = 0 ;
        while (recsock == 0 ) {
            recsock = createreceivesock(msg_store->port[i-1],i,logmessage) ;
        }
        if ( recsock < 0 ) {
            unlink(currentfilename) ;
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
                wait_timer.tv_sec  = STARTCHILDTO ;
                wait_timer.tv_usec = 0 ;
                nfds = sysconf(_SC_OPEN_MAX) ;
                select(nfds,0,0,0,&wait_timer) ;
            }
            syslog(BBFTPD_DEBUG,"Child Starting") ;
            /*
            ** Close all unnecessary stuff
            */
            close(msgsock) ;
            /*
            ** Check if file exist
            */
#ifdef STANDART_FILE_CALL
            if ( stat(currentfilename,&statbuf) < 0 ) {
#else
            if ( stat64(currentfilename,&statbuf) < 0 ) {
#endif
                /*
                ** If the file does not exist that means that another
                ** child has detroyed it
                */ 
                i = errno ;
                syslog(BBFTPD_ERR,"Error stating file %s : %s ",currentfilename,strerror(errno)) ;
                close(recsock) ;
                exit(i) ;
            }
            /*
            ** Open and seek to position
            */
#ifdef STANDART_FILE_CALL
            if ((fd = open(currentfilename,O_RDWR,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) < 0 ) {
#else
            if ((fd = open64(currentfilename,O_RDWR,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) < 0 ) {
#endif
                i = errno ;
                unlink(currentfilename) ;
                syslog(BBFTPD_ERR,"Error opening file %s : %s ",currentfilename,strerror(errno)) ;
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
                unlink(currentfilename) ;
                syslog(BBFTPD_ERR,"error seeking file : %s",strerror(errno)) ;
                close(recsock) ;
                exit(i)  ;
            }
            /*
            ** start the reading loop
            */
            nbget = 0 ;
            while ( nbget < nbtoget) {
#ifdef WITH_GZIP
                if ( code == MSG_STORE_C ) {
                    /*
                    ** Receive the header first 
                    */
                    if (readmessage(recsock,receive_buffer,COMPMESSLEN,datato) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading compression header") ;
                        close(fd) ;
                        unlink(currentfilename) ;
                        i = ETIMEDOUT ;
                        exit(i) ;
                    }
                    msg_compress = ( struct mess_compress *) receive_buffer ;
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
                    if ( READBUFLEN <= nbtoget-nbget ) {
                        datatoreceive = READBUFLEN  ;
                    } else {
                        datatoreceive = nbtoget-nbget ;
                    }
                }
#else
                if ( READBUFLEN <= nbtoget-nbget ) {
                    datatoreceive = READBUFLEN  ;
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
                    FD_SET(recsock,&selectmask) ;
                    wait_timer.tv_sec  = datato ;
                    wait_timer.tv_usec = 0 ;
                    if ( (retcode = select(nfds,&selectmask,0,0,&wait_timer) ) == -1 ) {
                        /*
                        ** Select error
                        */
                        i = errno ;
                        syslog(BBFTPD_ERR,"Error select while receiving : %s",strerror(errno)) ;
                        close(fd) ;
                        unlink(currentfilename) ;
                        close(recsock) ;
                        exit(i) ;
                    } else if ( retcode == 0 ) {
                        syslog(BBFTPD_ERR,"Time out while receiving") ;
                        close(fd) ;
                        unlink(currentfilename) ;
                        i=ETIMEDOUT ;
                        close(recsock) ;
                        exit(i) ;
                    } else {
                        retcode = recv(recsock,&data_buffer[dataonone],datatoreceive-dataonone,0) ;
                        if ( retcode < 0 ) {
                            i = errno ;
                            syslog(BBFTPD_ERR,"Error while receiving : %s",strerror(errno)) ;
                            close(fd) ;
                            unlink(currentfilename) ;
                            close(recsock) ;
                            exit(i) ;
                        } else if ( retcode == 0 ) {
                            i = ECONNRESET ;
                            syslog(BBFTPD_ERR,"Connexion breaks") ;
                            close(fd) ;
                            unlink(currentfilename) ;
                            close(recsock) ;
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
                if ( code == MSG_STORE_C ) {
                    if ( compressionon == 1 ) {
                        bufcomplen = READBUFLEN ;
                        buflen = dataonone ;
                        retcode = uncompress((Bytef *) comp_buffer, &bufcomplen, (Bytef *) data_buffer, buflen) ;
                        if ( retcode != 0 ) {
                            i = EILSEQ ;
                            syslog(BBFTPD_ERR,"Error while decompressing %d ",retcode) ;
                            close(fd) ;
                            unlink(currentfilename) ;
                            close(recsock) ;
                            exit(i) ;
                        }
                        memcpy(data_buffer,comp_buffer,READBUFLEN) ;
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
                    if ( (retcode = write(fd,&data_buffer[lenwrited],lentowrite-lenwrited)) < 0 ) {
                        i = errno ;
                        syslog(BBFTPD_ERR,"error writing file : %s",strerror(errno)) ;
                        close(fd) ;
                        unlink(currentfilename) ;
                        if ( i == EDQUOT ||
                                i == ENOSPC ) {
                            close(recsock) ;
                            exit(255) ;
                        } else {
                            close(recsock) ;
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
            msg = (struct message *) data_buffer ;
            msg->code = MSG_ACK ;
            msg->msglen = 0 ;
            if ( writemessage(recsock,data_buffer,MINMESSLEN,sendcontrolto) < 0 ) {
                close(fd) ;
                unlink(currentfilename) ;
                syslog(BBFTPD_ERR,"Error sending ACK ") ;
                close(recsock) ;
                exit(ETIMEDOUT) ;
            }
            toprint64 = nbget ;
            syslog(BBFTPD_DEBUG,"Child received %" LONG_LONG_FORMAT " bytes ; end correct ",toprint64) ;
            close(recsock) ;
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
                unlink(currentfilename) ;
                sprintf(logmessage,"fork failed : %s ",strerror(errno)) ;
                if ( childendinerror == 0 ) {
                    childendinerror = 1 ;
                    reply(MSG_BAD,logmessage) ;
                }
                clean_child() ;
                return 0 ;
            } else {
                /*
                ** Set the parameter telling the sig child routine to unlink
                ** in case of error 
                */
                syslog(BBFTPD_DEBUG,"Started child pid %d",retcode) ;
                pid_child[i-1] = retcode ;
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
    for (i = 0 ; i<MAXPORT ; i++) {
        if (pid_child[i] != 0) {
            kill(pid_child[i],SIGHUP) ;
        }
    }
    return 0 ;
}
