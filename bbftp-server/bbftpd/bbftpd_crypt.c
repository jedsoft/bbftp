/*
 * bbftpd/bbftpd_crypt.c
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

  
  
 bbftpd_crypt.c v 1.4.0  2000/03/22
                v 1.6.1  2000/03/28 - Portage to OSF1
                v 1.8.0  2000/04/14 - Introduce RSA Cryptage
                v 1.8.2  2000/04/17 - Portage to OSF1
                v 1.8.4  2000/04/21 - Random seed done in do_deamon.c
                v 1.8.7  2000/05/24 - Modify headers
                v 1.8.10 2000/08/11 - Portage to Linux
                v 1.9.0  2000/08/18 - Use configure to help portage
                v 2.0.0  2000/12/18 - Use incontrolsock and outcontrolsock
                v 2.0.1  2001/04/23 - Correct indentation
                v 2.1.0  2001/06/11 - Change file name

*****************************************************************************/
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
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

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <structures.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>

extern int  outcontrolsock ;
extern	int	sendcontrolto ;
extern RSA  *myrsa ;

void sendcrypt() 
{
    struct message    *mess ;
    struct mess_sec    *msg_sec ;
    char    buf[MAXMESSLEN] ;
    unsigned char    pubkey[NBITSINKEY] ;
    unsigned char    pubexponent[NBITSINKEY] ;
    int        lenkey ;
    int        lenexpo ;
    
    /*
    ** Ask for the private and public Key
    */
    if ( (myrsa = RSA_generate_key(NBITSINKEY,3,NULL,NULL)) == NULL) {
        syslog(BBFTPD_ERR,"%s",ERR_error_string(ERR_get_error(),NULL) ) ;
        exit(1) ;
    }
    /*
    ** Now extract the public key in order to send it
    */
    lenkey  = BN_bn2mpi(myrsa->n,pubkey) ;
    lenexpo = BN_bn2mpi(myrsa->e,pubexponent) ;
    mess = (struct message *) buf ;
    mess->code = MSG_CRYPT ;
#ifndef WORDS_BIGENDIAN
    mess->msglen = ntohl(CRYPTMESSLEN+lenkey+lenexpo) ;
#else
    mess->msglen = CRYPTMESSLEN+lenkey+lenexpo ;
#endif
    if (writemessage(outcontrolsock,buf,MINMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error on sendcrypt 1") ;
        exit(1) ;
    }
    msg_sec  = (struct mess_sec    *) buf ;
    msg_sec->crtype  = CRYPT_RSA_PKCS1_OAEP_PADDING ;
#ifndef WORDS_BIGENDIAN
    msg_sec->pubkeylen  = ntohl(lenkey) ;
    msg_sec->expolen  = ntohl(lenexpo) ;
#else
    msg_sec->pubkeylen  = lenkey ;
    msg_sec->expolen  = lenexpo ;
#endif
    if (writemessage(outcontrolsock,buf,CRYPTMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error on sendcrypt 2") ;
        exit(1) ;
    }
    /*
    ** Send Key and exponent
    */
    if (writemessage(outcontrolsock,pubkey,lenkey,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error on sendcrypt pubkey") ;
        exit(1) ;
    }
    if (writemessage(outcontrolsock,pubexponent,lenexpo,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error on sendcrypt pubexponent") ;
        exit(1) ;
    }
}

int decodersapass(char *buffer, char *username, char *password) 
{
    struct mess_rsa *msg_rsa ;
    int    lenuser ;
    int lenpass ;
    
    msg_rsa = (struct mess_rsa *) buffer ;

#ifndef WORDS_BIGENDIAN
    msg_rsa->numuser = ntohl(msg_rsa->numuser) ;
    msg_rsa->numpass = ntohl(msg_rsa->numpass) ;
#endif
    lenuser = RSA_private_decrypt(msg_rsa->numuser,msg_rsa->cryptuser,(unsigned char *)username,myrsa,RSA_PKCS1_OAEP_PADDING) ;
    username[lenuser] = '\0' ;
    lenpass = RSA_private_decrypt(msg_rsa->numpass,msg_rsa->cryptpass,(unsigned char *)password,myrsa,RSA_PKCS1_OAEP_PADDING) ;
    password[lenpass] = '\0' ;
    
    return 0 ;
}
