/*
 * bbftpc/connecttoserver.c
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

  
     In case of repeated errors this routine exits the program.
       
 connecttoserver.c v 2.0.0 2001/03/07   - Routine rewriting 
                   v 2.0.1 2001/04/19   - Correct indentation 
                                        - Port to IRIX
                   v 2.1.0 2001/05/25   - Add private authentication mode
				   v 2.1.2 2001/11/19   - Fix COS 0 case
				   v 2.2.0  2001/10/03  - Add certificate authentication mode
				   						- Send negative COS to server

*****************************************************************************/
#include <bbftp.h>

#include <errno.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ctype.h>

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
  /* add  multiple addresses  */
#include <netdb.h>

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>
#include <config.h>

#ifdef WITH_SSL
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#define SETTOZERO    0
#define SETTOONE     1

#define JED_SSL_PATCH 1

extern  int     timestamp ;
extern  int     usessh ;
extern	int		usecert ;
extern  int     sshbatchmode ;
extern  int     sshchildpid ;
extern  char    *sshidentityfile ;
extern  char    *sshremotecmd  ;
extern  char    *sshcmd ;
extern  char    *hostname ;
extern  char    *username ;
extern  char    *password ;
extern  int     incontrolsock ;
extern  int     outcontrolsock ;
extern	int	    recvcontrolto ;
extern	int	    sendcontrolto ;
extern  int     globaltrymax ;
extern  int     warning ;
extern  int     debug ;
extern  int     verbose ;
extern  int     newcontrolport ;
extern  int     transferoption ;
extern  int     remoteumask ;
extern  int     remotecos ;
extern  char    *remotedir ;

  /*  add multiple addresses  */
extern  struct hostent      *hp;
extern  struct  sockaddr_in hisctladdr ;
extern  struct  sockaddr_in myctladdr ;

/****************************************************************************
  Separate STR into arguments, respecting quote and escaped characters.
  Returns the number of arguments (or < 0 if limits are passed), which are
  copied into BUF and referenced from ARGV.
*****************************************************************************/

static int splitargs (const char* s, char** argv, size_t maxargs,
                      char* buf, size_t maxbuf)
{
  char c, quote= 0;
  static const char esc[]= "abfnrtv";
  static const char rep[]= "\a\b\f\n\r\t\v";
  const char *p;
  size_t argc= 0, i= 0;
  int sp= 1;
  int c2;
  char *r;
  char num[4];

  if (!argv) return -1;
  if (!buf)  return -2;
  maxargs--;   /* leave room for final NULL */
  maxbuf--;    /* leave room for final '\0' */
  for (p= s; (c= *p); p++) {
    if (sp) {
      if (isspace (c)) continue;
      if (argc >= maxargs) return -1;
      argv[argc++]= &buf[i];
      sp= 0;
    }
    if (c == '\\') {
      c= *(++p);
      if (c == 'x') {
        strncpy (num, p+1, 2); num[2]= '\0';
        c2= (int) strtol (num, &r, 16);
        if (c2 && r && r>num) {
          p += r-num;
          c= c2;
        }
      } else if (isdigit (c)) {
        strncpy (num, p, 3); num[3]= '\0';
        c2= (int) strtol (num, &r, 8);
        if (c2 && r && r>num) {
          p += r-num-1;
          c= c2;
        }
      } else {
        r= (char *) strchr (esc, c);
        if (r) c= rep[r-esc];
      }
    } else if (quote) {
      if (c == quote) {
          quote= 0;
          continue;
      }
    } else if (c == '\'' || c == '\"') {
      quote= c;
      continue;
    } else if (isspace (c)) {
      c= '\0';
      sp= 1;
    }
    if (i>=maxbuf) return -2;
    buf[i++]= c;
  }
  if (!sp) buf[i]= '\0';
  argv[argc]= NULL;
  return argc;
}

/****************************************************************************

 connectviassh is based on ideas from do_ssh.c written by Tim Adye 
 <T.J.Adye@rl.ac.uk> in version bbftp v 1.9.4. It has been adapted 
 in order to fit bbftp version 2.0.0 design.
 
 Some variable have been left in order to add further possibilities

*****************************************************************************/

int connectviassh() 
{
/* 
** This is set to non-zero if compression is desired. 
*/
    int sshcompress = SETTOZERO;
/* 
** This is to call ssh with argument -P (for using non-privileged
** ports to get through some firewalls.) 
*/
    int use_privileged_port = SETTOONE ;
/* 
** This is set to the cipher type string if given on the command line. 
*/
    char *cipher = NULL;

/* 
** Ssh options 
*/
    char **ssh_options = NULL;
    int ssh_options_cnt = 0;
    
    int pin[2], pout[2], reserved[2];
    int retcode ;
    
    char    buffer[MINMESSLEN] ;
    struct  message *msg ;
    int     tmpctrlsock ;
#ifdef SUNOS
    int     addrlen ;
#else
    socklen_t  addrlen ;
#endif
    struct sockaddr_in data_source ;
    int     on = 1 ;

  /*  add multiple addresses  */
    char **sav_h_addr_list;
    if (hp) sav_h_addr_list = hp->h_addr_list;

/* 
** Reserve two descriptors so that the real pipes won't get descriptors
** 0 and 1 because that will screw up dup2 below. 
*/
    pipe(reserved);
/*
** Create a socket pair for communicating with ssh. 
*/
    if (pipe(pin) < 0)
        printmessage(stderr,CASE_FATAL_ERROR,41,timestamp,"Pipe for ssh failed : %s", strerror(errno)) ;

    if (pipe(pout) < 0)
        printmessage(stderr,CASE_FATAL_ERROR,41,timestamp,"Pipe for ssh failed : %s", strerror(errno)) ;

/*
** Free the reserved descriptors. 
*/
    close(reserved[0]);
    close(reserved[1]);
    sshchildpid = 0 ;

/*
** Fork a child to execute the command on the remote host using ssh. 
*/
    if ( (retcode = fork()) == 0 ) {
        /*
        ** We are in child
        */
        char *args[256], *argbuf;
        unsigned int i, j, largbuf;
        int  ntok;
        
        close(pin[1]);
        close(pout[0]);
        dup2(pin[0], 0);
        dup2(pout[1], 1);
        close(pin[0]);
        close(pout[1]);
        
        i = 0;
        largbuf= strlen (sshcmd)+1;
        argbuf= (char*) malloc (largbuf * sizeof (char));
        ntok= splitargs(sshcmd,args,250,argbuf,largbuf);
        if (ntok<0)
            printmessage(stderr,CASE_FATAL_ERROR,42,timestamp,"Too many arguments in ssh command :%s \n",sshcmd) ;
        if (ntok==0)
            printmessage(stderr,CASE_FATAL_ERROR,43,timestamp,"No ssh command specified\n") ;
        i += ntok;

        for(j = 0; j < ssh_options_cnt; j++) {
            args[i++] = "-o";
            args[i++] = ssh_options[j];
            if (i > 250)
                printmessage(stderr,CASE_FATAL_ERROR,44,timestamp,"Too many -o options (total number of arguments is more than 256)");
        }
        args[i++] = "-x";
        args[i++] = "-a";
        args[i++] = "-oFallBackToRsh no";
        if (sshcompress) args[i++] = "-C";
        if (!use_privileged_port) args[i++] = "-P";
        if (sshbatchmode) {
            args[i++] = "-oBatchMode yes";
            args[i++] = "-oStrictHostKeyChecking no" ;
        }
        if (cipher != NULL) {
            args[i++] = "-c";
            args[i++] = cipher;
        }
        if (sshidentityfile  != NULL) {
            args[i++] = "-i";
            args[i++] = sshidentityfile;
        }
	if (username != NULL) {
            args[i++] = "-l";
            args[i++] = username;
	}
        args[i++] = hostname;
        /*args[i++] = inet_ntoa(hisctladdr.sin_addr);*/
        args[i++] = sshremotecmd;
        args[i++] = NULL;
        
        if ( debug ) {
            /*
            ** We write on stderr and not on stdout because writing on
            ** stdout will cause problem on the contrlo connection
            */
            printmessage(stderr,CASE_NORMAL,0,timestamp,"Executing :%s",args[0]) ;
            for (j= 1; args[j]; j++)  printmessage(stderr,CASE_NORMAL,0,timestamp," %s", args[j]);
            printmessage(stderr,CASE_NORMAL,0,timestamp,"\n");
        }
            
        execvp(args[0], args);
        printmessage(stderr,CASE_FATAL_ERROR,46,timestamp,"Error while execvp ssh command (%s) : %s\n",sshcmd,strerror(errno)) ;
    } else if ( retcode < 0 ) {
        /*
        ** fork error
        */
        printmessage(stderr,CASE_ERROR,45,timestamp,"Fork for ssh command failed\n");
        return -1 ;
    } else {
        /*
        ** We are in father
        */
        close(pin[0]);
        outcontrolsock = pin[1];
        close(pout[1]);
        incontrolsock = pout[0];
        sshchildpid = retcode ;
        /*
        ** Now we are going to wait for the remote port to connect to
        */
        if ( readmessage(incontrolsock,buffer,MINMESSLEN,2 * recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,61,timestamp,"Error waiting %s message\n","MSG_LOGGED_STDIN");
            kill(sshchildpid,SIGKILL) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
        msg = (struct message *)buffer ;
        if ( msg->code != MSG_LOGGED_STDIN) {
            printmessage(stderr,CASE_ERROR,62,timestamp,"Unknown message while waiting for %s message\n","MSG_LOGGED_STDIN");
            if ( debug ) {
                printmessage(stdout,CASE_NORMAL,0,timestamp,"Incorrect message is : ") ;
                buffer[MINMESSLEN] = '\0' ;
                printmessage(stdout,CASE_NORMAL,0,timestamp,"%s",buffer) ;
                discardandprintmessage(incontrolsock,recvcontrolto,0) ;
            }
            kill(sshchildpid,SIGKILL) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(msg->msglen) ;
#endif
        if ( msg->msglen != 4) {
            printmessage(stderr,CASE_ERROR,63,timestamp,"Unexpected message length while waiting for %s message\n","MSG_LOGGED_STDIN");
            kill(sshchildpid,SIGKILL) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
        /*
        ** Read for the port number
        */
        if ( readmessage(incontrolsock,buffer,4,recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_LOGGED_STDIN");
            kill(sshchildpid,SIGKILL) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg->code = ntohl(msg->code) ;
#endif
        if (debug) 
            printmessage(stderr,CASE_NORMAL,0,timestamp,"Port number = %d\n",msg->code) ;
        while (1) {
           if ( (tmpctrlsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )) < 0 ) {
               printmessage(stderr,CASE_ERROR,47,timestamp,"Cannot get socket for MSG_IPADDR message : %s\n",strerror(errno));
               kill(sshchildpid,SIGKILL) ;
               close(incontrolsock) ;
               close(outcontrolsock) ;
               return -1 ;
           }
           if ( setsockopt(tmpctrlsock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
               close(tmpctrlsock) ;
               printmessage(stderr,CASE_ERROR,47,timestamp,"Cannot set SO_REUSEADDR on socket for MSG_IPADDR message : %s\n",strerror(errno));
               kill(sshchildpid,SIGKILL) ;
               close(incontrolsock) ;
               close(outcontrolsock) ;
               return -1 ;
           }
           data_source.sin_family = AF_INET;
           data_source.sin_addr.s_addr = INADDR_ANY;
           /*
           data_source.sin_port = htons(newcontrolport+1);
           */
           data_source.sin_port = 0;
           if ( bind(tmpctrlsock, (struct sockaddr *) &data_source,sizeof(data_source)) < 0) {
               printmessage(stderr,CASE_ERROR,49,timestamp,"Cannot bind socket for MSG_IPADDR message : %s\n",strerror(errno));
               close(tmpctrlsock) ;
               kill(sshchildpid,SIGKILL) ;
               close(incontrolsock) ;
               close(outcontrolsock) ;
           }
           if (debug) printmessage(stderr,CASE_NORMAL,0,timestamp,"connect control to address %s\n", inet_ntoa(hisctladdr.sin_addr)) ;
           hisctladdr.sin_family = AF_INET ;
           hisctladdr.sin_port = htons(msg->code);
           addrlen = sizeof(hisctladdr) ;
           if ( connect(tmpctrlsock,(struct sockaddr*)&hisctladdr,addrlen) < 0 ) {
               if ( hp ) {
                  if ( hp->h_addr_list[1] ) {
                      close(tmpctrlsock) ;
                      hp->h_addr_list++;
                      memcpy(&hisctladdr.sin_addr,hp->h_addr_list[0],  hp->h_length);
                      /* Sleep a moment before retrying. */
                      sleep(1);
                      continue;
                   } else {
                       hp->h_addr_list = sav_h_addr_list;
                       memcpy(&hisctladdr.sin_addr,hp->h_addr_list[0],  hp->h_length);
                   }
               }
               printmessage(stderr,CASE_ERROR,48,timestamp,"Cannot connect socket for MSG_IPADDR message : %s\n",strerror(errno));
               close(tmpctrlsock) ;
               kill(sshchildpid,SIGKILL) ;
               close(incontrolsock) ;
               close(outcontrolsock) ;
               return -1 ;
           }
           break;
        }
        msg->code = MSG_IPADDR ;
        msg->msglen = 0 ;
        /*
        ** Read the message 
        */
        if ( writemessage(tmpctrlsock,buffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_IPADDR");
            kill(sshchildpid,SIGKILL) ;
            close(tmpctrlsock) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
        /*
        ** And wait for MSG_IPADDR_OK
        */
        if ( readmessage(tmpctrlsock,buffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,61,timestamp,"Error waiting %s message\n","MSG_IPADDR_OK");
            kill(sshchildpid,SIGKILL) ;
            close(tmpctrlsock) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
        if ( msg->code != MSG_IPADDR_OK) {
            printmessage(stderr,CASE_ERROR,65,timestamp,"Unknown message while waiting for %s message\n","MSG_IPADDR_OK");
            close(tmpctrlsock) ;
            kill(sshchildpid,SIGKILL) ;
            close(incontrolsock) ;
            close(outcontrolsock) ;
            return -1 ;
        }
        close(tmpctrlsock) ;
        return 0;
    }
    /*
    ** Never reach this point but to in order to avoid stupid messages
    ** from IRIX compiler set a return code to -1
    */
    return -1 ;
}
/****************************************************************************

 connectviapassword is the standart method to connect to bbftpd

*****************************************************************************/

int connectviapassword(void)
{
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int     addrlen ;
#else
    socklen_t  addrlen ;
#endif
    int     tmpctrlsock ;
    char    minbuffer[MINMESSLEN] ;
    struct  message     *msg ;
    struct  mess_sec    *msg_sec ;
    struct  mess_rsa    *msg_rsa ;
    char    *readbuffer ;
    int     msglen ;
    int     crtype ;
    int     code ; 
    unsigned int seed ;
    struct timeval tp ;

    char            buffrand[NBITSINKEY] ;
    unsigned char   *pubkey ;
    unsigned char   *pubexponent ;
    char            rsabuffer[RSAMESSLEN] ;
    char            cryptbuffer[CRYPTMESSLEN] ;
    int     lenkey ;
    int     lenexpo ;
    int     i ;

#ifdef WITH_SSL
    RSA     *hisrsa ;
    int lenrsa ;
#endif
    /*
    ** Get the socket
    */
    
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
    ** Connection is correct so start the login procedure
    */
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
    /*
    ** Get the message length and alloc readbuffer
    */
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

    msg_sec = (struct mess_sec    *) readbuffer ;

#ifdef WITH_SSL
    if ( (msg_sec->crtype & CRYPT_RSA_PKCS1_OAEP_PADDING ) == CRYPT_RSA_PKCS1_OAEP_PADDING) {
        /* 
        ** RSA
        */
       BIGNUM *rsa_n, *rsa_e;
        /*
        ** Load the error message from the crypto lib
        */
        ERR_load_crypto_strings() ;
        /*
        ** Initialize the buffrand buffer which is giong to be used to initialize the 
        ** random generator
        */
        /*
        ** Take the usec to initialize the random session
        */
        gettimeofday(&tp,NULL) ;
        seed = tp.tv_usec ;
        srandom(seed) ;
        for (i=0; i < sizeof(buffrand) ; i++) {
            buffrand[i] = random() ;
        }
        /*
        ** Initialize the random generator
        */
        RAND_seed(buffrand,NBITSINKEY) ;
#ifndef WORDS_BIGENDIAN
        lenkey = ntohl(msg_sec->pubkeylen) ;
        lenexpo = ntohl(msg_sec->expolen) ;
#else
        lenkey = msg_sec->pubkeylen ;
        lenexpo = msg_sec->expolen ;
#endif
        pubkey  = (unsigned char *) readbuffer + CRYPTMESSLEN ;
        pubexponent = pubkey + lenkey ;
        if ( (hisrsa = RSA_new()) == NULL) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","getting RSA",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        /*
        ** Getting BIGNUM structures to store the key and exponent
        */
#ifdef JED_SSL_PATCH
       if (NULL == (rsa_n = BN_new()))
	 {
	    RSA_free (hisrsa);
	    free (readbuffer);
	    close (tmpctrlsock);
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","getting BIGNUM",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
	 }
       if (NULL == (rsa_e = BN_new()))
	 {
	    BN_free (rsa_n);
	    RSA_free (hisrsa);
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","getting BIGNUM",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
	 }
       if (1 != RSA_set0_key (hisrsa, rsa_n, rsa_e, NULL))
	 {
	    /* The man page does not say what to do with rsa_n/e if RSA_set0_key fails.
	     * To avoid a possible double free, do not free rsa_n/e.
	     */
	    RSA_free (hisrsa);
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","RSA_set0_key",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
	 }
#else
        if ( (hisrsa->n = BN_new()) == NULL) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","getting BIGNUM",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        if ( (hisrsa->e = BN_new()) == NULL) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","getting BIGNUM",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }

       rsa_n = hisrsa->n;	       /* Added by JED */
       rsa_e = hisrsa->e;	       /* Added by JED */
#endif				       /* JED_SSL_PATCH */
       
        /*
        ** Copy the key and exponent received
        */
        if ( BN_mpi2bn(pubkey,lenkey,rsa_n) == NULL ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","copying pubkey",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        if ( BN_mpi2bn(pubexponent,lenexpo,rsa_e) == NULL ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","copying pubexponent",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        lenrsa = RSA_size(hisrsa) ;
       
        if (strlen(username) > lenrsa - 41 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_FATAL_ERROR,56,timestamp,"Error reading encrypted message : %s (%d/%d)\n","username too long",strlen(username),lenrsa-41) ;
        }
        if (strlen(password) > lenrsa - 41 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_FATAL_ERROR,56,timestamp,"Error reading encrypted message : %s (%d/%d)\n","password too long",strlen(password),lenrsa-41) ;
            return -2 ;
        }
        msg_rsa = ( struct mess_rsa *) rsabuffer ;
        if ( (msg_rsa->numuser = RSA_public_encrypt(strlen(username),(unsigned char *)username,msg_rsa->cryptuser,hisrsa,RSA_PKCS1_OAEP_PADDING)) < 0 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","RSA_public_encrypt username",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg_rsa->numuser = ntohl(msg_rsa->numuser) ;
#endif
        if ( (msg_rsa->numpass = RSA_public_encrypt(strlen(password),(unsigned char *)password,msg_rsa->cryptpass,hisrsa,RSA_PKCS1_OAEP_PADDING)) < 0 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,timestamp,"Error reading encrypted message : %s (%s)\n","RSA_public_encrypt password",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg_rsa->numpass = ntohl(msg_rsa->numpass) ;
#endif
        crtype = CRYPT_RSA_PKCS1_OAEP_PADDING ;
    } else {
        free(readbuffer) ;
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,57,timestamp,"Unkown encryption method \n") ;
        return -1  ;
    }
    free(readbuffer) ;
    /*
    ** Send login information
    */
    /*
    ** First message code 
    */
    msg = (struct message *)minbuffer ;
    msg->code = MSG_LOG ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(CRYPTMESSLEN+RSAMESSLEN) ;
#else
    msg->msglen = CRYPTMESSLEN+RSAMESSLEN ;
#endif
    if ( writemessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** Then crypte type
    */
    msg_sec = (struct mess_sec *)cryptbuffer ;
    msg_sec->crtype = crtype ;
    if ( writemessage(tmpctrlsock,cryptbuffer,CRYPTMESSLEN,recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** Then real informations
    */
    if ( writemessage(tmpctrlsock,rsabuffer,RSAMESSLEN,recvcontrolto,0) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> USER %s PASS\n",username) ;
    /*
    ** Now wait for the OK message
    */
    if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    code = msg->code ;
    if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else
        msglen = msg->msglen ;
#endif
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : malloc failed (%s) BAD message\n",strerror(errno)) ;
            return -1 ;
        }
        if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
            close(tmpctrlsock) ;
            free(readbuffer) ;
            if ( code == MSG_BAD ) {
                printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","BAD message") ;
                return -1 ;
            } else {
                printmessage(stderr,CASE_FATAL_ERROR,59,timestamp,"Error reading login message answer : %s\n","BAD NO RETRY message") ;
            }
        } else {
            close(tmpctrlsock) ;
            readbuffer[msglen] = '\0' ;
            if ( code == MSG_BAD ) {
                printmessage(stderr,CASE_ERROR,100,timestamp,"%s\n",readbuffer) ;
                free(readbuffer) ;
                return -1 ;
            } else {
                 printmessage(stderr,CASE_FATAL_ERROR,100,timestamp,"%s\n",readbuffer) ;
            }
        }
    } else if ( code == MSG_OK ) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else
        msglen = msg->msglen ;
#endif
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : OK message : malloc failed (%s)\n",strerror(errno)) ;
            return -1 ;
        }
        if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","OK message") ;
            return -1 ;
        } else {
            readbuffer[msglen] = '\0' ;
            if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< %s\n",readbuffer) ;
            free(readbuffer) ;
        }    
    } else {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","Unkown answer message") ;
        return -1 ;
    }

#else 
/* Client does not support encryption */
    if ( (msg_sec->crtype & CRYPT_RSA_PKCS1_OAEP_PADDING ) == CRYPT_RSA_PKCS1_OAEP_PADDING) {
        crtype = NO_CRYPT ;
    } else {
        free(readbuffer) ;
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,57,timestamp,"Unkown encryption method \n") ;
        return -1  ;
    }
    free(readbuffer) ;
    /*
    ** Send message to tell the server we can't encode
    */
    msg = (struct message *)minbuffer ;
    msg->code = MSG_CRYPT ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(CRYPTMESSLEN+RSAMESSLEN) ;
#else
    msg->msglen = CRYPTMESSLEN+RSAMESSLEN ;
#endif
    if ( writemessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> MSG_CRYPT message sent\n") ;
    /*
    ** Now wait for the server decision
    */
    if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    code = msg->code ;
    if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else
        msglen = msg->msglen ;
#endif
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : malloc failed (%s) BAD message\n",strerror(errno)) ;
            return -1 ;
        }
        if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
            close(tmpctrlsock) ;
            free(readbuffer) ;
            if ( code == MSG_BAD ) {
                printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","BAD message") ;
                return -1 ;
            } else {
                printmessage(stderr,CASE_FATAL_ERROR,59,timestamp,"Error reading login message answer : %s\n","BAD NO RETRY message") ;
            }
        } else {
            close(tmpctrlsock) ;
            readbuffer[msglen] = '\0' ;
            if ( code == MSG_BAD ) {
                printmessage(stderr,CASE_ERROR,100,timestamp,"%s\n",readbuffer) ;
                free(readbuffer) ;
                return -1 ;
            } else {
                 printmessage(stderr,CASE_FATAL_ERROR,100,timestamp,"%s\n",readbuffer) ;
            }
        }
    } else if ( code == MSG_OK ) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else
        msglen = msg->msglen ;
#endif
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : OK message : malloc failed (%s)\n",strerror(errno)) ;
            return -1 ;
        }
        if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","OK message") ;
            return -1 ;
        } else {
            readbuffer[msglen] = '\0' ;
            if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"%s\n", readbuffer) ;
            free(readbuffer) ;
        }    
    } else {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","Unkown answer message") ;
        return -1 ;
    }
    /*
    **  Send new MSG_LOG message
    */
    msg = (struct message *)minbuffer ;
    msg->code = MSG_LOG ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(CRYPTMESSLEN+RSAMESSLEN) ;
#else
    msg->msglen = CRYPTMESSLEN+RSAMESSLEN ;
#endif
    if ( writemessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** send crypt type
    */
    msg_sec = (struct mess_sec *)cryptbuffer ;
    msg_sec->crtype = NO_CRYPT ;
    if ( writemessage(tmpctrlsock,cryptbuffer,CRYPTMESSLEN,recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** Send uncrypted logon and password
    */
    msg_rsa = ( struct mess_rsa *) rsabuffer ;
    msg_rsa->numuser = strlen(username) ;
    strcpy((char *)msg_rsa->cryptuser, (char *)username) ;
#ifndef WORDS_BIGENDIAN
    msg_rsa->numuser = ntohl(msg_rsa->numuser) ;
#endif
    msg_rsa->numpass = strlen(password) ;
    strcpy((char *) msg_rsa->cryptpass, ( char *)password) ;
#ifndef WORDS_BIGENDIAN
    msg_rsa->numpass = ntohl(msg_rsa->numpass) ;
#endif

    /*
    ** Then real informations
    */
    if ( writemessage(tmpctrlsock,rsabuffer,RSAMESSLEN,recvcontrolto,0) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,58,timestamp,"Error sending username : %s\n",strerror(errno)) ;
        return -1 ;
    }
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> USERNAME %s PASS\n",username) ;
    /*
    ** Now wait for the OK message
    */
    if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    code = msg->code ;
    if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else
        msglen = msg->msglen ;
#endif
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : malloc failed (%s) BAD message\n",strerror(errno)) ;
            return -1 ;
        }
        if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
            close(tmpctrlsock) ;
            free(readbuffer) ;
            if ( code == MSG_BAD ) {
                printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","BAD message") ;
                return -1 ;
            } else {
                printmessage(stderr,CASE_FATAL_ERROR,59,timestamp,"Error reading login message answer : %s\n","BAD NO RETRY message") ;
            }
        } else {
            close(tmpctrlsock) ;
            readbuffer[msglen] = '\0' ;
            if ( code == MSG_BAD ) {
                printmessage(stderr,CASE_ERROR,100,timestamp,"%s\n",readbuffer) ;
                free(readbuffer) ;
                return -1 ;
            } else {
                 printmessage(stderr,CASE_FATAL_ERROR,100,timestamp,"%s\n",readbuffer) ;
            }
        }
    } else if ( code == MSG_OK ) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else
        msglen = msg->msglen ;
#endif
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : OK message : malloc failed (%s)\n",strerror(errno)) ;
            return -1 ;
        }
        if ( readmessage(tmpctrlsock,readbuffer,msglen,recvcontrolto,0) < 0 ) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","OK message") ;
            return -1 ;
        } else {
            readbuffer[msglen] = '\0' ;
            if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< %s\n",readbuffer) ;
            free(readbuffer) ;
        }    
    } else {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,59,timestamp,"Error reading login message answer : %s\n","Unkown answer message") ;
        return -1 ;
    }

#endif    
    incontrolsock  = tmpctrlsock ;
    outcontrolsock = tmpctrlsock ;
    return 0 ;
}

int todoafterconnection()
{
    int     errcode ;
    int     oldtransferoption ;
    int     retcode ;
    /*
    ** Things to do are :
    **      chdir remote if remotedir != NULL
    **      chumask remote if remoteumask !=0
    **      remotecos if != -1
    */
    if ( remoteumask !=0 ) {
        if ( bbftp_setremoteumask(remoteumask,&errcode) != 0 ) {
            return -1 ;
        }
    }
    if ( remotecos != -1 ) {
        if ( bbftp_setremotecos(remotecos,&errcode) != 0 ) {
	    return -1 ;
    }
    }
    if ( remotedir != NULL ) {
        /*
        ** Care has to be taken if we are under remoterfio
        ** We have first to change the option to noremoterfio,
        ** make the cd command and reset the option
        */
        oldtransferoption = transferoption ;
        transferoption = transferoption & ~TROPT_RFIO ;
        retcode = bbftp_cd(remotedir,&errcode) ;
        transferoption = oldtransferoption ;
        if ( retcode != 0 ) return -1 ;
    }
    return 0 ;
}
void reconnecttoserver() 
{
    int     nbtrycon ;
    int     retcode ;
  /*  add multiple addresses  */
    char **sav_h_addr_list;
    if (hp) sav_h_addr_list = hp->h_addr_list;

    for ( nbtrycon = 1 ; nbtrycon <= globaltrymax ; nbtrycon++ ) {
        while (1) {
            if (warning) printmessage(stderr,CASE_WARNING,28,timestamp,"connect to address %s\n",inet_ntoa(hisctladdr.sin_addr)) ;
#if defined(PRIVATE_AUTH)
            if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Private authentication...\n") ;
            retcode = bbftp_private_connect() ;
#else
# ifdef CERTIFICATE_AUTH
            if (usecert) {
                if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Certificate authentication...\n") ;
                retcode = bbftp_cert_connect() ;
                /* stop at the first try */
		/* NOT ANYMORE because of multiple IP */
		/*
                if (retcode != 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,33,timestamp,"Connection not established\n") ;
                }
		*/
            } else
# endif
            if (usessh) {
                if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"SSH connection...\n") ;
                retcode = connectviassh() ;
            } else {
                if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Password authentication...\n") ;
                retcode = connectviapassword() ;
            }
#endif
            if ( retcode == 0 ) {
                if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Connection and authentication correct\n") ;
                if ( sendproto() == 0 ) {
                    retcode = todoafterconnection() ;
                    if ( retcode == 0 ) {
                        return ;
                    } else {
                        bbftp_close_control() ;
                    }
                } else {
                    bbftp_close_control() ;
                }   
            } else {
      /* add  multiple addresses  */
               if ( hp ) {
                   if ( hp->h_addr_list[1] ) {
                        hp->h_addr_list++;
                        memcpy(&hisctladdr.sin_addr,hp->h_addr_list[0],  hp->h_length);
                        continue;
                    } else {
                        hp->h_addr_list = sav_h_addr_list;
                        memcpy(&hisctladdr.sin_addr,hp->h_addr_list[0],  hp->h_length);
                    }
                }
                break;
            }
        }
        if ( nbtrycon != globaltrymax ) {
            if (warning) printmessage(stderr,CASE_WARNING,22,timestamp,"Retrying connection waiting %d s\n",WAITRETRYTIME) ;
            sleep(WAITRETRYTIME) ;
        }
    }
    if (nbtrycon == globaltrymax+1) {
        printmessage(stderr,CASE_FATAL_ERROR,33,timestamp,"Maximum try on connection reached (%d) aborting\n",globaltrymax) ;
    }
}
