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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "_bbftpd.h"

/*
** myrsa :
**      Define the local location where is store the key pair
*/
RSA        *myrsa ;

static RSA *generate_key (void)
{
   RSA *r;
#if (OPENSSL_VERSION_NUMBER < 0x00908000L)
   if (NULL != (r = RSA_generate_key(NBITSINKEY,3,NULL,NULL)))
     return r;
   /* fall through to error */
#else
   if (NULL != (r = RSA_new ()))
     {
	BIGNUM *bne = BN_new ();

	if (bne == NULL)
	  {
	     RSA_free (r);
	     goto return_error;
	  }
	if ((0 == BN_set_word (bne, RSA_F4))
	    || (0 == RSA_generate_key_ex (r, NBITSINKEY, bne, NULL)))
	  {
	     BN_free (bne);
	     RSA_free (r);
	     goto return_error;
	  }
	BN_free (bne);
	return r;
     }

return_error:
#endif

   bbftpd_log (BBFTPD_ERR, "%s", ERR_error_string(ERR_get_error(),NULL));

   return r;
}


void sendcrypt(void)
{
   struct mess_sec msg_sec ;
   unsigned char    pubkey[NBITSINKEY] ;
   unsigned char    pubexponent[NBITSINKEY] ;
   int        lenkey ;
   int        lenexpo ;
   const BIGNUM *rsa_n, *rsa_e, *rsa_d;

    /*
    ** Ask for the private and public Key
    */
   if (NULL == (myrsa = generate_key ()))
     exit (1);

    /*
    ** Now extract the public key in order to send it
    */
#if (OPENSSL_VERSION_NUMBER >= 0x10101000L)
   RSA_get0_key (myrsa, &rsa_n, &rsa_e, &rsa_d);
#else
   rsa_n = myrsa->n;
   rsa_e = myrsa->e;
   (void) rsa_d;
#endif

   lenkey  = BN_bn2mpi(rsa_n,pubkey) ;
   lenexpo = BN_bn2mpi(rsa_e,pubexponent) ;

   if (-1 == bbftpd_msgwrite_len (MSG_CRYPT, CRYPTMESSLEN+lenkey+lenexpo))
     {
        bbftpd_log(BBFTPD_ERR,"Error on sendcrypt 1") ;
        exit(1) ;
     }

   msg_sec.crtype = CRYPT_RSA_PKCS1_OAEP_PADDING;
   msg_sec.pubkeylen = htonl (lenkey);
   msg_sec.expolen = htonl (lenexpo);

   if (-1 == bbftpd_msgwrite_bytes ((char *)&msg_sec, sizeof(msg_sec)))
     {
	bbftpd_log(BBFTPD_ERR,"Error on sendcrypt 2") ;
	exit(1) ;
     }

   /*
    ** Send Key and exponent
    */
   if (-1 == bbftpd_msgwrite_bytes ((char *)pubkey, lenkey))
     {
        bbftpd_log(BBFTPD_ERR,"Error on sendcrypt pubkey") ;
        exit(1) ;
     }
   if (-1 == bbftpd_msgwrite_bytes ((char *)pubexponent, lenexpo))
     {
        bbftpd_log(BBFTPD_ERR,"Error on sendcrypt pubexponent") ;
        exit(1) ;
    }
}

int decodersapass(struct mess_rsa *msg_rsa, char *username, char *password)
{
   int lenuser ;
   int lenpass ;

#ifndef WORDS_BIGENDIAN
   msg_rsa->numuser = ntohl(msg_rsa->numuser) ;
   msg_rsa->numpass = ntohl(msg_rsa->numpass) ;
#endif

   lenuser = RSA_private_decrypt(msg_rsa->numuser,(unsigned char *)msg_rsa->cryptuser,(unsigned char *)username,myrsa,RSA_PKCS1_OAEP_PADDING) ;
   username[lenuser] = '\0' ;
   lenpass = RSA_private_decrypt(msg_rsa->numpass,(unsigned char *)msg_rsa->cryptpass,(unsigned char *)password,myrsa,RSA_PKCS1_OAEP_PADDING) ;
   password[lenpass] = '\0' ;

   return 0 ;
}

int bbftpd_crypt_init_random (void)
{
   char buffrand[NBITSINKEY] ;
   struct timeval tp ;
   unsigned int i, seed;

   /*
    ** Load the error message from the crypto lib
    */
#if !defined(OPENSSL_API_COMPAT) || (OPENSSL_API_COMPAT < 0x10100000L)
   ERR_load_crypto_strings() ;
#endif
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

   for (i=0; i < (int) sizeof(buffrand) ; i++)
     buffrand[i] = random() ;

   /*
    ** Initialize the random generator
    */
   RAND_seed(buffrand,NBITSINKEY) ;

   return 0;
}
