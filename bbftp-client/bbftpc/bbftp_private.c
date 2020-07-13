/*
 * bbftpc/bbftp_private.c
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

  
  
 bbftp_private.c  v 2.1.0 2001/05/21    - Routines creation

*****************************************************************************/
#include <bbftp.h>

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
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

#include "_bbftp.h"

#include <openssl/rsa.h>
#include <openssl/rand.h>

#include <bbftp_private.h>
#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <structures.h>

/*******************************************************************************
** bbftp_private_connect :                                                     *
**                                                                             *
**      This routine is going to connect to the server, get the RSA key, and   *
**      give the hand to the bbftp_private_auth routine and then if this       *
**      routine returns 0 to set up correctly the global parameters            *
**                                                                             *
**      OUPUT variable :                                                       *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Incontrolsock       MAY BE MODIFIED                                *
**          BBftp_Outcontrolsock      MAY BE MODIFIED                                *
**          hisrsa              MAY BE MODIFIED                                *
**          prv_ctrlsock        MODIFIED                                       *
**                                                                             *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_private_connect (void)

{
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int     addrlen ;
#else
    socklen_t addrlen ;
#endif
    char    logmessage[1024] ;
    char    minbuffer[CRYPTMESSLEN] ;
    struct  message     *msg ;
    struct  mess_sec    *msg_sec ;
    char    *readbuffer ;
    int     msglen ;
    int     code ; 
    unsigned int seed ;
    struct timeval tp ;
    int     lenkey ;
    int     lenexpo ;
    unsigned char   *pubkey ;
    unsigned char   *pubexponent ;
    char    buffrand[NBITSINKEY] ;
    int     i ;
    unsigned char    mypubkey[NBITSINKEY] ;
    unsigned char    mypubexponent[NBITSINKEY] ;
    int        lenmykey ;
    int        lenmyexpo ;


    BBftp_His_Ctladdr.sin_family = AF_INET;
    BBftp_His_Ctladdr.sin_port = htons(BBftp_Newcontrolport);
    if ( (prv_ctrlsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )) < 0 ) {
        printmessage(stderr,CASE_ERROR,51,BBftp_Timestamp, "Cannot create control socket : %s\n",strerror(errno));
        return -1 ;
    }
    /*
    ** Connect to the server
    */
    addrlen = sizeof(BBftp_His_Ctladdr) ;
    if ( connect(prv_ctrlsock,(struct sockaddr*)&BBftp_His_Ctladdr,addrlen) < 0 ) {
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,52,BBftp_Timestamp, "Cannot connect to control socket: %s\n",strerror(errno));
        return -1 ;
    }
    /*
    ** Get the socket name
    */
    addrlen = sizeof(BBftp_My_Ctladdr) ;
    if (getsockname(prv_ctrlsock,(struct sockaddr*) &BBftp_My_Ctladdr, &addrlen) < 0) {
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,53,BBftp_Timestamp,"Error getsockname on control socket: %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** Connection is correct get the encryption
    */

    /*
    **    Read the encryption supported
    */
    if ( readmessage(prv_ctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,54,BBftp_Timestamp,"Error reading encryption message\n") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    if ( msg->code != MSG_CRYPT) {
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,55,BBftp_Timestamp,"No encryption message \n") ;
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
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,54,BBftp_Timestamp,"Error reading encryption message : malloc failed (%s)\n",strerror(errno)) ;
        return -1 ;
    }
    if ( readmessage(prv_ctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
        free(readbuffer) ;
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,56,BBftp_Timestamp,"Error reading encrypted message : %s\n","type") ;
        return -1 ;
    }
    msg_sec = (struct mess_sec *) readbuffer ;

    if ( (msg_sec->crtype & CRYPT_RSA_PKCS1_OAEP_PADDING ) == CRYPT_RSA_PKCS1_OAEP_PADDING) {
        /* 
        ** RSA
        */
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
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,BBftp_Timestamp,"Error reading encrypted message : %s (%s)\n","getting RSA",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        /*
        ** Getting BIGNUM structures to store the key and exponent
        */
        if ( (hisrsa->n = BN_new()) == NULL) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,BBftp_Timestamp,"Error reading encrypted message : %s (%s)\n","getting BIGNUM",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        if ( (hisrsa->e = BN_new()) == NULL) { 
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,BBftp_Timestamp,"Error reading encrypted message : %s (%s)\n","getting BIGNUM",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        /*
        ** Copy the key and exponent received
        */
        if ( BN_mpi2bn(pubkey,lenkey,hisrsa->n) == NULL ) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,BBftp_Timestamp,"Error reading encrypted message : %s (%s)\n","copying pubkey",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        if ( BN_mpi2bn(pubexponent,lenexpo,hisrsa->e) == NULL ) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,56,BBftp_Timestamp,"Error reading encrypted message : %s (%s)\n","copying pubexponent",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
        if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Public daemon RSA key received\n") ;
        /*
        ** Ask for the private and public Key
        */
        if ( (myrsa = RSA_generate_key(NBITSINKEY,3,NULL,NULL)) == NULL) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,27,BBftp_Timestamp,"Error while private authentication : cannot generate keys : %s\n",ERR_error_string(ERR_get_error(),NULL) ) ;
            return -1 ;
        }
        /*
        ** Now extract the public key in order to send it
        */
        lenmykey  = BN_bn2mpi(myrsa->n,mypubkey) ;
        lenmyexpo = BN_bn2mpi(myrsa->e,mypubexponent) ;
        msg = (struct message *) minbuffer ;
        msg->code = MSG_PRIV_LOG ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(CRYPTMESSLEN+lenmykey+lenmyexpo) ;
#else
        msg->msglen = CRYPTMESSLEN+lenmykey+lenmyexpo ;
#endif
        if ( writemessage(prv_ctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,58,BBftp_Timestamp,"Error sending LOG message : %s\n",strerror(errno)) ;
            return -1 ;
        }
        msg_sec  = (struct mess_sec *) minbuffer ;
        msg_sec->crtype  = CRYPT_RSA_PKCS1_OAEP_PADDING ;
#ifndef WORDS_BIGENDIAN
        msg_sec->pubkeylen  = ntohl(lenmykey) ;
        msg_sec->expolen    = ntohl(lenmyexpo) ;
#else
        msg_sec->pubkeylen  = lenmykey ;
        msg_sec->expolen    = lenmyexpo ;
#endif
        if (writemessage(prv_ctrlsock,minbuffer,CRYPTMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,58,BBftp_Timestamp,"Error sending LOG message : %s\n",strerror(errno)) ;
            return -1 ;
        }
        /*
        ** Send Key and exponent
        */
        if (writemessage(prv_ctrlsock,(char *)mypubkey,lenmykey,BBftp_Recvcontrolto,0) < 0 ) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,58,BBftp_Timestamp,"Error sending LOG message : %s\n",strerror(errno)) ;
            return -1 ;
        }
        if (writemessage(prv_ctrlsock,(char *)mypubexponent,lenmyexpo,BBftp_Recvcontrolto,0) < 0 ) {
            free(readbuffer) ;
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,58,BBftp_Timestamp,"Error sending LOG message : %s\n",strerror(errno)) ;
            return -1 ;
        }
        if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Public client RSA key sent\n") ;
        free(readbuffer) ;
        /*
        ** At this point we have got the public key so give hand to 
        ** the bbftp_private_auth routine
        */
        if ( bbftp_private_auth(logmessage) < 0 ) {
            close(prv_ctrlsock) ;
            printmessage(stderr,CASE_ERROR,27,BBftp_Timestamp,"Error while private authentication : %s\n",logmessage) ;
            return -1 ;
        } else {
            /*
            ** All private authentication seems OK so wait for the MSG_OK
            ** message
            */
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Client private authentication OK\n") ;
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Waiting for server answer\n") ;
            if ( readmessage(prv_ctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
                close(prv_ctrlsock) ;
                free(readbuffer) ;
                printmessage(stderr,CASE_ERROR,59,BBftp_Timestamp,"Error reading login message answer : %s\n","") ;
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
                    printmessage(stderr,CASE_ERROR,59,BBftp_Timestamp,"Error reading login message answer : malloc failed (%s)\n",strerror(errno)) ;
                    return -1 ;
                }
                if ( readmessage(prv_ctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
                    close(prv_ctrlsock) ;
                    free(readbuffer) ;
                    if ( code == MSG_BAD ) {
                        printmessage(stderr,CASE_ERROR,59,BBftp_Timestamp,"Error reading login message answer : %s\n","BAD message") ;
                        return -1 ;
                    } else {
                        printmessage(stderr,CASE_FATAL_ERROR,59,BBftp_Timestamp,"Error reading login message answer : %s\n","BAD NO RETRY message") ;
                    }
                } else {
                    close(prv_ctrlsock) ;
                    readbuffer[msglen] = '\0' ;
                    if ( code == MSG_BAD ) {
                        printmessage(stderr,CASE_ERROR,100,BBftp_Timestamp,"%s\n",readbuffer) ;
                        free(readbuffer) ;
                        return -1 ;
                    } else {
                        printmessage(stderr,CASE_FATAL_ERROR,100,BBftp_Timestamp,"%s\n",readbuffer) ;
                    }
                }
            } else if ( code == MSG_OK ) {
#ifndef WORDS_BIGENDIAN
                msglen = ntohl(msg->msglen) ;
#else
                msglen = msg->msglen ;
#endif
                if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
                    printmessage(stderr,CASE_ERROR,59,BBftp_Timestamp,"Error reading login message answer : OK message : malloc failed (%s)\n",strerror(errno)) ;
                    return -1 ;
                }
                if ( readmessage(prv_ctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
                    free(readbuffer) ;
                    close(prv_ctrlsock) ;
                    printmessage(stderr,CASE_ERROR,59,BBftp_Timestamp,"Error reading login message answer : %s\n","OK message") ;
                    return -1 ;
                } else {
                    readbuffer[msglen] = '\0' ;
                    if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"<< %s\n",readbuffer) ;
                }
            } else {
                free(readbuffer) ;
                close(prv_ctrlsock) ;
                printmessage(stderr,CASE_ERROR,59,BBftp_Timestamp,"Error reading login message answer : %s\n","Unkown answer message") ;
                return -1 ;
            }
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Server answer : OK\n") ;
            free(readbuffer) ;
            BBftp_Incontrolsock  = prv_ctrlsock ;
            BBftp_Outcontrolsock = prv_ctrlsock ;
            return 0 ;
        }
    } else {
        free(readbuffer) ;
        close(prv_ctrlsock) ;
        printmessage(stderr,CASE_ERROR,57,BBftp_Timestamp,"Unkown encryption method \n") ;
        return -1  ;
    }
}

/*******************************************************************************
** bbftp_private_send    :                                                     *
**                                                                             *
**      This routine is going to crypt the buffer and to send it to the server *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/
int bbftp_private_send(char *buffertosend, int buffertosendlength, char *logmessage)
{
    int     lenrsa ;
    int     nbpackets ;
    char    minbuffer[MINMESSLEN] ;
    struct  message     *msg ;
    struct  mess_private    *msg_private ;
    char    privatebuffer[PRIVRSAMESSLEN] ;
    char    *strtocrypt ;
    int     lentocrypt ;
    int     i ;
    
    /*
    ** Minimum check
    */
    if ( buffertosendlength <=0 ) {
        sprintf(logmessage,"buffertosendlength has to be > 0") ;
        return -1 ;
    }
    if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Going to send %d Bytes\n",buffertosendlength) ;

    /*
    ** First get the RSA length
    */
    lenrsa = RSA_size(hisrsa) ;
    /*
    ** Calculate the number of encrypted packets 
    ** we are going to send
    */
    nbpackets = buffertosendlength/(lenrsa - 42) ;
    if (  buffertosendlength - (nbpackets*(lenrsa - 42)) != 0 ) nbpackets++ ;
    if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Number of encrypted packet(s) : %d\n",nbpackets) ;
    /*
    ** So the total length to send is nbpackets*PRIVRSAMESSLEN+MINMESSLEN
    */
    msg = (struct message *)minbuffer ;
    msg->code = MSG_PRIV_DATA ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(nbpackets*PRIVRSAMESSLEN) ;
#else
    msg->msglen = nbpackets*PRIVRSAMESSLEN ;
#endif
    if ( writemessage(prv_ctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0) {
        sprintf(logmessage,"Error sending data : %s",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** loop on packets
    */
    msg_private = ( struct mess_private *) privatebuffer ;
    strtocrypt = buffertosend ;
    lentocrypt = lenrsa - 42 ;
    for (i=1; i<= nbpackets ; i++ ) {
        if ( i == nbpackets ) {
            lentocrypt = buffertosendlength - ((nbpackets-1)*(lenrsa - 42)) ;
        }
        if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Crypting %d Bytes\n",lentocrypt) ;
        if ( (msg_private->lengthdata = RSA_public_encrypt(lentocrypt,(unsigned char *)strtocrypt,msg_private->cryptdata,hisrsa,RSA_PKCS1_OAEP_PADDING)) < 0 ) {
            sprintf(logmessage,"Error crypting message : %s ",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg_private->lengthdata = ntohl(msg_private->lengthdata) ;
#endif
        if ( writemessage(prv_ctrlsock,privatebuffer,PRIVRSAMESSLEN,BBftp_Recvcontrolto,0) < 0) {
            sprintf(logmessage,"Error sending encrypted data : %s",strerror(errno)) ;
            return -1 ;
        }
        strtocrypt = strtocrypt + lentocrypt ;
    }
    if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"%d encrypted packet(s) sent\n",nbpackets) ;
    return 0 ;
}

/*******************************************************************************
** bbftp_private_recv :                                                        *
**                                                                             *
**      This routine is going get a message, decrypt it and fill the buffer    *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           >0 Number of bytes received                                       *
**                                                                             *
*******************************************************************************/
int bbftp_private_recv(char *buffertorecv, int lengthtorecv, char *logmessage)
{
    char    minbuffer[MINMESSLEN] ;
    struct  message     *msg ;
    struct  mess_private    *msg_private ;
    char    privatebuffer[PRIVRSAMESSLEN] ;
    char    tempbuffer[PRIVRSAMESSLEN] ;
    int     code ;
    int     msglen ;
    int     expectedpackets ;
    int      i ;
    int     lentodecrypt ;
    int     lendecrypted, totallendecrypted ;
    char    *tostore ;
    char    *readbuffer ;

    totallendecrypted = 0 ;
    tostore = buffertorecv ;
    /*
    ** Now wait for the MSG_PRIV_DATA
    */
    if ( readmessage(prv_ctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
        sprintf(logmessage,"Error reading data") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    code = msg->code ;
#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msg->msglen) ;
#else
    msglen = msg->msglen ;
#endif
    if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY ) {
        if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
            sprintf(logmessage,"Receive MSG_BAD message") ;
            return -1 ;
        }
        if ( readmessage(prv_ctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
            sprintf(logmessage,"Receive MSG_BAD message") ;
            free(readbuffer) ;
            return -1 ;
        }
        readbuffer[msglen] = '\0' ;
        strncpy(logmessage,readbuffer,1023) ;
        logmessage[1023] = '\0' ;
        return -1 ;
    } else if (  code != MSG_PRIV_DATA ) {
        sprintf(logmessage,"Incorrect message header") ;
        return -1 ;
    } 
    expectedpackets = msglen/PRIVRSAMESSLEN ;
    if ( msglen != (PRIVRSAMESSLEN * expectedpackets ) ) {
        sprintf(logmessage,"Incorrect message length") ;
        return -1 ;
    }
    if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Going to receive %d encrypted packet(s)\n",expectedpackets) ;
    msg_private = (struct  mess_private *) privatebuffer ;
    for ( i = 1 ; i <= expectedpackets ; i++ ) {
        if ( readmessage(prv_ctrlsock,privatebuffer,PRIVRSAMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
            sprintf(logmessage,"Error reading encrypted data") ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        lentodecrypt = ntohl(msg_private->lengthdata) ;
#else
        lentodecrypt = msg_private->lengthdata ;
#endif
        lendecrypted = RSA_private_decrypt(lentodecrypt,msg_private->cryptdata,(unsigned char *)tempbuffer,myrsa,RSA_PKCS1_OAEP_PADDING) ;
        if ( totallendecrypted + lendecrypted > lengthtorecv) {
            sprintf(logmessage,"Too much data (max=%d, receive=%d)",lengthtorecv,totallendecrypted+lendecrypted) ;
            return -1;
        }
        memcpy(tostore,tempbuffer,lendecrypted) ;
        tostore =  tostore + lendecrypted ;
        totallendecrypted = totallendecrypted + lendecrypted ;
    }
    buffertorecv[totallendecrypted] = '\0' ;
    if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0,BBftp_Timestamp,"Received %d Bytes\n",totallendecrypted) ;
    return totallendecrypted ;
}
