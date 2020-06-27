/*
 * bbftpd/bbftpd_private.c
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

  
  
 bbftpd_private.c   v 2.1.0 2001/05/21  - Routines creation

*****************************************************************************/
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
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
#include <utime.h>

#include <openssl/rsa.h>

#include <bbftpd.h>
#include <bbftpd_private.h>
#include <config.h>
#include <common.h>
#include <daemon_proto.h>
#include <daemon.h>
#include <structures.h>
#include <version.h>

extern  int     incontrolsock ;
extern  int     outcontrolsock ;
extern	int	    recvcontrolto ;
extern	int	    sendcontrolto ;
extern  RSA     *myrsa ;

/*******************************************************************************
** bbftpd_private_receive_connection :                                         *
**                                                                             *
**      This routine is called when a connection occurs in private             *
**      authentication mode. It receives the plublic key of the client and     *
**      gives the hand to the bbftpd_private_auth routine and then if this     * 
**      routine returns 0 to set up correctly the global parameters            *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          hisrsa              MODIFIED                                       *
**                                                                             *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/
int bbftpd_private_receive_connection(int msglen) 
{

    char    *receive_buffer ;
    char    logmessage[1024] ;
    int     lenkey ;
    int     lenexpo ;
    unsigned char   *pubkey ;
    unsigned char   *pubexponent ;
    int     retcode ;
    struct  mess_sec    *msg_sec ;
    
    sprintf(logmessage,"bbftpd version %s",VERSION) ;
#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msglen) ;
#endif
    /*
    ** First allocate the buffer
    */
    if ( (receive_buffer = (char *) malloc (msglen+1)) == NULL ) {
        syslog(BBFTPD_ERR,"Error allocation memory for receive_buffer (bbftpd_private_receive_connection) ") ;
        strcat(logmessage," : Error allocation memory for receive_buffer") ;
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    if ( (retcode = readmessage(incontrolsock,receive_buffer,msglen,recvcontrolto)) < 0 ) {
        FREE(receive_buffer) ;
        /*
        ** Error ...
        */
        return(retcode) ;
    }
    msg_sec = (struct mess_sec *)receive_buffer ;
    if ( msg_sec->crtype ==  CRYPT_RSA_PKCS1_OAEP_PADDING) {
#ifndef WORDS_BIGENDIAN
        lenkey = ntohl(msg_sec->pubkeylen) ;
        lenexpo = ntohl(msg_sec->expolen) ;
#else
        lenkey = msg_sec->pubkeylen ;
        lenexpo = msg_sec->expolen ;
#endif
        pubkey  = (unsigned char *)receive_buffer  + CRYPTMESSLEN ;
        pubexponent = pubkey + lenkey ;
        if ( (hisrsa = RSA_new()) == NULL) {
            FREE(receive_buffer) ;
            syslog(BBFTPD_ERR,"Error RSA_new (bbftpd_private_receive_connection) ") ;
            strcat(logmessage," : RSA Error") ;
            reply(MSG_BAD,logmessage) ;
            return -1 ;
        }
        /*
        ** Getting BIGNUM structures to store the key and exponent
        */
        if ( (hisrsa->n = BN_new()) == NULL) {
            FREE(receive_buffer) ;
            syslog(BBFTPD_ERR,"Error BN_new (bbftpd_private_receive_connection) ") ;
            strcat(logmessage," : RSA Error") ;
            reply(MSG_BAD,logmessage) ;
            return -1 ;
        }
        if ( (hisrsa->e = BN_new()) == NULL) { 
            FREE(receive_buffer) ;
            syslog(BBFTPD_ERR,"Error BN_new (bbftpd_private_receive_connection) ") ;
            strcat(logmessage," : RSA Error") ;
            reply(MSG_BAD,logmessage) ;
            return -1 ;
        }
        /*
        ** Copy the key and exponent received
        */
        if ( BN_mpi2bn(pubkey,lenkey,hisrsa->n) == NULL ) {
            FREE(receive_buffer) ;
            syslog(BBFTPD_ERR,"Error BN_mpi2bn (bbftpd_private_receive_connection) ") ;
            strcat(logmessage," : RSA Error") ;
            reply(MSG_BAD,logmessage) ;
            return -1 ;
        }
        if ( BN_mpi2bn(pubexponent,lenexpo,hisrsa->e) == NULL ) {
            FREE(receive_buffer) ;
            syslog(BBFTPD_ERR,"Error BN_mpi2bn (bbftpd_private_receive_connection) ") ;
            strcat(logmessage," : RSA Error") ;
            reply(MSG_BAD,logmessage) ;
            return -1 ;
        }
    } else {
        syslog(BBFTPD_ERR,"Unkwown encryption %d",msg_sec->crtype) ;
        strcat(logmessage," : Unknown encryption") ;
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    /*
    ** Now give hand to user routine
    */
    if ( bbftpd_private_auth(logmessage) < 0 ) {
        syslog(BBFTPD_ERR,"bbftpd_private_auth failed : %s",logmessage) ;
        reply(MSG_BAD,logmessage) ;
        return -1 ;
    }
    return 0 ;
}
/*******************************************************************************
** bbftpd_private_send    :                                                    *
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
int bbftpd_private_send(char *buffertosend, int buffertosendlength, char *logmessage)
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
    if ( writemessage(outcontrolsock,minbuffer,MINMESSLEN,recvcontrolto) < 0) {
        sprintf(logmessage,"Error sending data") ;
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
        if ( (msg_private->lengthdata = RSA_public_encrypt(lentocrypt,(unsigned char *)strtocrypt,msg_private->cryptdata,hisrsa,RSA_PKCS1_OAEP_PADDING)) < 0 ) {
            sprintf(logmessage,"Error crypting message : %s ",(char *) ERR_error_string(ERR_get_error(),NULL)) ;
            return -1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg_private->lengthdata = ntohl(msg_private->lengthdata) ;
#endif
        if ( writemessage(outcontrolsock,privatebuffer,PRIVRSAMESSLEN,recvcontrolto) < 0) {
            sprintf(logmessage,"Error sending encrypted data") ;
            return -1 ;
        }
        strtocrypt = strtocrypt + lentocrypt ;
    }
    return 0 ;
}
/*******************************************************************************
** bbftpd_private_recv :                                                       *
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
int bbftpd_private_recv(char *buffertorecv, int lengthtorecv, char *logmessage)
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

    totallendecrypted = 0 ;
    tostore = buffertorecv ;
    /*
    ** Now wait for the MSG_PRIV_DATA
    */
    if ( readmessage(incontrolsock,minbuffer,MINMESSLEN,recvcontrolto) < 0 ) {
        sprintf(logmessage,"Error reading data ") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    code = msg->code ;
    if (  code != MSG_PRIV_DATA ) {
        sprintf(logmessage,"Incorrect message header") ;
        return -1 ;
    } 
#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msg->msglen) ;
#else
    msglen = msg->msglen ;
#endif
    expectedpackets = msglen/PRIVRSAMESSLEN ;
    if ( msglen != (PRIVRSAMESSLEN * expectedpackets ) ) {
        sprintf(logmessage,"Incorrect message length ") ;
        return -1 ;
    }
    msg_private = (struct  mess_private *) privatebuffer ;
    for ( i = 1 ; i <= expectedpackets ; i++ ) {
        if ( readmessage(incontrolsock,privatebuffer,PRIVRSAMESSLEN,recvcontrolto) < 0 ) {
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
    return totallendecrypted ;
}
