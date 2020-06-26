/*
 * bbftpc/bbftpstatus.c
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

#include <bbftp.h>

#include <errno.h> 
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
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
#if HAVE_STRING_H
# include <string.h>
#endif

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <config.h>
#include <structures.h>
#include <version.h>

#define SETTOZERO    0
#define SETTOONE     1

#ifdef PRIVATE_AUTH
#define OPTIONS "qbcde:f:i:l:mno:p:P:r:R:tu:vVw:WD::"
#else
#define OPTIONS "qbcde:E:f:g:i:I:l:L:mno:p:r:R:sStu:vVw:WD::"
#endif
/*
#endif
*/

int     newcontrolport = CONTROLPORT ;
/* 
** hisctladdr:
**      the remote address
*/
struct sockaddr_in hisctladdr ;
/* 
** myctladdr:
**      the local address
*/
struct sockaddr_in myctladdr ;

int     incontrolsock ;
int     outcontrolsock ;
/*
** myexitcode :
**      Contains the first error code that has to be return when program
**      is ended
*/
int     myexitcode = SETTOZERO ;
char    *hostname   = NULL ;
struct hostent  *hp = NULL ;
char    *username   = NULL ;
char    *password   = NULL ;
/*
**
*/
int connectionisbroken = SETTOZERO ;

int	timestamp ;
int	debug ;
int		recvcontrolto	= CONTROLSOCKTO;
int		sendcontrolto	= SENDCONTROLTO;

void printmessage(FILE *strm , int flag, int errcode, int tok, char *fmt, ...) 
{
    va_list ap;
    time_t  logtime ;
    char    clogtime[50] ;
    
    va_start(ap,fmt);
    
    /*
    ** If timestamp start to print the time
    */
    if (tok) {
        /*
        ** Get time
        */
        if ( time(&logtime) == -1 ) {
            strcpy(clogtime,"") ;
        } else {
            if ( strftime(clogtime,sizeof(clogtime),"%a %b %d %H:%M:%S (%Z)",localtime(&logtime)) == 0 )    {
                strcpy(clogtime,"") ;
            }
        }
        /*
        ** And print it
        */
        if ( strlen(clogtime) > 0 ) {
            (void) fprintf(strm,"%s : ",clogtime) ;
        }
    }
    /*
    ** Check if it is an error
    */
    if ( flag == CASE_ERROR || flag == CASE_FATAL_ERROR ) {
        /*
        ** It is an error
        */
        (void) fprintf(strm,"BBFTP-ERROR-%05d : ",errcode) ;
    } else if ( flag == CASE_WARNING ) {
        /*
        ** It is a warning
        */
        (void) fprintf(strm,"BBFTP-WARNING-%05d : ",errcode) ;
    }
    /*
    ** And print the requested string 
    */
    (void) vfprintf(strm, fmt, ap);
    fflush(strm) ;
    if ( flag == CASE_FATAL_ERROR ) {
        /*
        ** exit in case of error
        */
        exit(errcode) ;
    }
    va_end(ap);
}

void bbftp_close_control() 
{

    close(incontrolsock) ;
    if ( incontrolsock != outcontrolsock) close(outcontrolsock) ;
}

int main(argc, argv, envp)
    int argc;
    char **argv;
    char **envp;
{
    extern char *optarg;
    extern int optind, opterr, optopt;
/*
** Variable set by options
*/
    char    *inputfile  = NULL ;
    char    *resultfile = NULL ;
    char    *outputfile = NULL ;
    char    *errorfile  = NULL ;
    char    *bbftpcmd   = NULL ;
    int     background  = SETTOZERO ;
/*
** For hostname
*/  
    int     hosttype = 0 ;
    char    *calchostname ;
/*
** For local user
*/
    struct  passwd  *mypasswd ;
    char    *bbftprcfile = NULL ;
    int     fd ;
    char    *carret ;
    char    *startcmd ;
    int     nooption ;
    
    struct  stat    statbuf ;
    int     retcode ;
    int     i, j, k ;
    int     stderrfd ;
    int     stdoutfd ;
    int     infd ;
    char    calcmaxline[1] ;
    int     maxlen ;
    int     lengthread ;
    char    *buffercmd ;
    int     alluse ;
    char    *tmpsshremotecmd ;
    char    logmessage[1024] ;
    char    minbuffer[MINMESSLEN] ;
    struct  message *msg ;
/*
** Get local umask 
*/
    /*localumask = umask(0) ;*/
/*
** and reset it to the correct value
*/
    /*umask(localumask) ;*/
/*
** Check for -v option
*/
    debug = 0 ;

    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'v' :{
                fprintf(stdout,"bbftpstatus version %s\n",VERSION) ;
                fprintf(stdout,"Compiled with  :   default port %d\n",CONTROLPORT) ;
                exit(0) ;
            }
        }
    }

/*
** Check for stderr replacement
*/
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'w' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                    fprintf(stderr,"Control port must be numeric\n") ;
		    exit(1);
                }
                newcontrolport = alluse ;
                break ;
            }
            default : {
                fprintf(stderr,"Error on command line (unsupported option -%c)\n",optopt) ;
		exit(1);
            }
        }
    }
/*
** Check hostname
*/          
    if ( optind == argc-1 ) {
        hostname= argv[optind] ;
    } else {
        fprintf(stderr,"No hostname on command line\n") ;
	exit(1);
    }    
/*
** Check if hostname is in numeric format 
*/
    for (j=0 ; j < strlen(hostname) ; j++) {
        if ( isalpha(hostname[j]) ) {
            /*
            ** One alpha caractere means no numeric form
            */
            hosttype = 1 ;
            break ;
        } else if ( isdigit(hostname[j]) ) {
        } else if ( hostname[j] == '.' ) {
        } else {
            fprintf(stderr,"Invalid hostname (%s)\n",hostname) ;
	    exit(1);
        }
    }
    if ( hosttype == 0 ) {
       /*
       ** Numeric format 
       */
        hisctladdr.sin_addr.s_addr = 0 ;
        hisctladdr.sin_addr.s_addr = inet_addr(hostname) ;
        if (hisctladdr.sin_addr.s_addr == -1 ) {
            fprintf(stderr,"Invalid IP address (%s)\n",hostname) ;
	    exit(1);
        }
        calchostname = (char *)inet_ntoa(hisctladdr.sin_addr) ;
        if ( strcmp(hostname,calchostname) ) {
            fprintf(stderr,"Invalid IP address (%s)\n",hostname) ;
	    exit(1);
        }
    } else {
       /*
       ** Alpha format 
       */
        if ( (hp = gethostbyname((char *)hostname) ) == NULL ) {
            fprintf(stderr,"Hostname no found (%s)\n",hostname) ;
	    exit(1);
        } else {
            if (hp->h_length > (int)sizeof(hisctladdr.sin_addr)) {
                hp->h_length = sizeof(hisctladdr.sin_addr);
            }
            memcpy(&hisctladdr.sin_addr, hp->h_addr_list[0], hp->h_length) ;
        }
    }

/*
** Now we've got all informations to make the connection
*/

   
/*
** Send the STATUS message
*/
    {
    char    *buffer ;
    
    char    minbuffer[MINMESSLEN] ;
    int     retcode ;
    int     msglen ;
    int     code ;
    fd_set  selectmask ; /* Select mask */
    int     nfds ; /* Max number of file descriptor */
    struct  message *msg ;
    struct  mess_integer *msg_integer ;
   
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int     addrlen ;
#else
    socklen_t  addrlen ;
#endif
    int     tmpctrlsock ;
    char    *readbuffer ;

    hisctladdr.sin_family = AF_INET;
    hisctladdr.sin_port = htons(newcontrolport);
    if ( (tmpctrlsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )) < 0 ) {
        printmessage(stderr,CASE_ERROR,51,timestamp, "Cannot create control socket : %s\n",strerror(errno));
        return -1 ;
    }
    /*
    ** Connect to the server
    */
    addrlen = sizeof(hisctladdr) ;
    if ( connect(tmpctrlsock,(struct sockaddr*)&hisctladdr,addrlen) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,52,timestamp, "Cannot connect to control socket: %s\n",strerror(errno));
        return -1 ;
    }
    /*
    ** Get the socket name
    */
    addrlen = sizeof(myctladdr) ;
    if (getsockname(tmpctrlsock,(struct sockaddr*) &myctladdr, &addrlen) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,53,timestamp,"Error getsockname on control socket: %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** Connection is correct get the encryption
    */

	if (debug) printmessage(stdout,CASE_NORMAL,0,timestamp,"Connection established\n") ;
    /*
    **    Read the encryption supported
    */
    if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,54,timestamp,"Error reading encryption message\n") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    if ( msg->code != MSG_CRYPT) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,55,timestamp,"No encryption message \n") ;
        return -1 ;
    }
	if (debug) printmessage(stdout,CASE_NORMAL,0,timestamp,"Received message 1\n") ;
#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msg->msglen) ;
#else
    msglen = msg->msglen ;
#endif
    if ( ( readbuffer = (char *) malloc (msglen + 1) ) == NULL ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,54,timestamp,"Error reading encryption message : malloc failed (%s)\n",strerror(errno)) ;
        return -1 ;
    }
    if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
        free(readbuffer) ;
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s\n","type") ;
        return -1 ;
    }
	if (debug) printmessage(stdout,CASE_NORMAL,0,timestamp,"Received message 2\n") ;
    msg = (struct message *)minbuffer ;
    msg->code = MSG_SERVER_STATUS ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(CRYPTMESSLEN+RSAMESSLEN) ;
#else
    msg->msglen = CRYPTMESSLEN+RSAMESSLEN ;
#endif
    if ( writemessage(tmpctrlsock,minbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        fprintf(stderr,"Error sending %s message\n","MSG_SERVER_STATUS");
        close(tmpctrlsock) ;
        return -1 ;
    }
	if (debug) printmessage(stdout,CASE_NORMAL,0,timestamp,"Sent message \n") ;
    /*
    ** Now we are going to wait for the message on the control 
    ** connection
    */
        if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
            fprintf(stderr,"Error waiting %s message\n","MSG_OK (on MSG_SERVER_STATUS)");
            close(tmpctrlsock) ;
            return -1 ;
        }
        msg = (struct message *) minbuffer ;
        code = msg->code ;

        if (msg->code == MSG_OK ) {
            /*
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( (buffer = (char *) malloc(msglen+1) ) == NULL) {
                fprintf(stderr,"Error allocating memory for %s : %s\n","buffer (bbftp_statfs)",strerror(errno)) ;
                close(tmpctrlsock) ;
                return -1 ;
            }
            if ( readmessage(tmpctrlsock,buffer,msglen,recvcontrolto,0) < 0) {
                fprintf(stderr,"Error reading data for %s message\n","MSG_OK (on MSG_DF)");
                free(buffer) ;
                close(tmpctrlsock) ;
                return -1 ;
            } 
            buffer[msglen] = '\0' ;
            printmessage(stdout,CASE_NORMAL,0,timestamp,"%s\n", buffer) ;
            free(buffer) ;
            close(tmpctrlsock) ;
            return 0 ;
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
            fprintf(stderr,"The server cannot understand the STATUS command\n");
            close(tmpctrlsock) ;
            return -1 ;
        }


    msg = (struct message *)minbuffer ;
    msg->code = MSG_CLOSE_CONN ;
    msg->msglen = 0 ;
    /*
    ** We do not care of the result because this routine is called
    ** only at the end of the client
    */
    writemessage(tmpctrlsock,minbuffer,MINMESSLEN,sendcontrolto,0) ;
    sleep(1) ;
    bbftp_close_control() ;
    exit(0) ;
    }
}
