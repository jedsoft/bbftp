/*
 * includes/structures.h
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

  
  
 structures.h v 0.0.0  1999/11/24
              v 1.2.0  2000/11/24
              v 1.4.0  2000/03/22   - Add message for crypt mode for
                                      username and password
              v 1.6.0  2000/03/24   - New parameters for get implementation
              v 1.6.1  2000/03/30   - Portage to OSF1
              v 1.8.0  2000/04/14   - Introduce RSA Cryptage
              v 1.8.10 2000/08/10   - Portage to Linux
              v 1.9.0  2000/08/18   - Use configure to help portage
              v 2.0.0  2000/12/13   - Introduce V2 protocol
              v 2.0.2  2001/05/04   - Add padding on mess_store in order to avoid usage
                                      of malign-double on Linux
              v 2.1.0  2001/05/25   - Add private authentication
	      v 2.2.0  2001/10/03   - Add certificate authentication message
	      v 2.2.1  2013/02/01   - Fix implicit declaration issues in 64 bits 
			  
*****************************************************************************/


#ifndef _BBFTP_STRUCTURES_H
#define _BBFTP_STRUCTURES_H       1

#include <arpa/inet.h>
#include <netinet/in.h>

/*
** BBFTP V1 protocol definitions
*/

#define MAXLEN	200
#define MAXLENFILE 500
#define MAXPORT 10
#define READBUFLEN 256000
/*
** Message code 
*/
#ifdef WORDS_BIGENDIAN
#define MSG_OK				0x00000000
#define MSG_LOG				0x00000001
#define MSG_STORE			0x00000002
#define MSG_BAD				0x00000003
#define MSG_ACK				0x00000004
#define MSG_STORE_C 		0x00000005
#define MSG_CRYPT			0x00000006
#define MSG_RETR			0x00000007
#define MSG_RETR_C			0x00000008
#define MSG_RETR_OK			0x00000009
#define MSG_RETR_RFIO		0x0000000A
#define MSG_RETR_RFIO_C		0x0000000B
#define MSG_RETR_START		0x0000000C
#define MSG_ABR				0x0000000D
#define MSG_BAD_NO_RETRY	0x0000000E
#define MSG_MKDIR			0x0000000F
#define MSG_CHDIR			0x00000010
#define MSG_LIST			0x00000011
#define MSG_LIST_REPL		0x00000012
#define MSG_CREATE_ZERO		0x00000013
#define MSG_WARN    		0x00000014
#else
#define MSG_OK				0x00000000
#define MSG_LOG				0x01000000
#define MSG_STORE			0x02000000
#define MSG_BAD				0x03000000
#define MSG_ACK				0x04000000
#define MSG_STORE_C 		0x05000000
#define MSG_CRYPT			0x06000000
#define MSG_RETR			0x07000000
#define MSG_RETR_C			0x08000000
#define MSG_RETR_OK			0x09000000
#define MSG_RETR_RFIO		0x0A000000
#define MSG_RETR_RFIO_C		0x0B000000
#define MSG_RETR_START		0x0C000000
#define MSG_ABR				0x0D000000
#define MSG_BAD_NO_RETRY	0x0E000000
#define MSG_MKDIR			0x0F000000
#define MSG_CHDIR			0x10000000
#define MSG_LIST			0x11000000
#define MSG_LIST_REPL		0x12000000
#define MSG_CREATE_ZERO		0x13000000
#define MSG_WARN    		0x14000000
#endif
/*
** End Message code ************************************************************
*/

/*
** Crypt type
*/
#ifdef WORDS_BIGENDIAN
#define NO_CRYPT	0x00000000	/* No encoding */
#define CRYPT_BR	0x00000001	/* Simple "brouillage" */
#define CRYPT_RSA_PKCS1_OAEP_PADDING	0x00000002	/* Cryptage RSA */
#else
#define NO_CRYPT	0x00000000	/* No encoding */
#define CRYPT_BR	0x01000000	/* Simple "brouillage" */
#define CRYPT_RSA_PKCS1_OAEP_PADDING	0x02000000	/* Cryptage RSA */
#endif

#ifdef WORDS_BIGENDIAN
#define DATA_NOCOMPRESS 0x00000000
#define DATA_COMPRESS   0x00000001
#else
#define DATA_NOCOMPRESS 0x00000000
#define DATA_COMPRESS   0x01000000
#endif

#define NBYTESRSA	128
/*
** End Crypt type **************************************************************
*/

/*
** Structures 
*/
/*
** message is only used as the minimum structure to get a message 
*/

struct message {
	int	code ;			/* Message code */
	int	msglen ;		/* Message length in bytes */
} ;

struct mess_rsa {
	unsigned char	cryptuser[NBYTESRSA] ;	
	unsigned char	cryptpass[NBYTESRSA] ;	
	int		numuser ;
	int		numpass ;
} ;

struct mess_store {
    char    filename[MAXLENFILE] ;		/* Filename to store or to get*/
    char    pad[4] ;          /*   Pad to be on a double boundary if MAXLENFILE = 500 */
    my64_t  filesize ;			/* Size of the file (0 for a get) */
    int     blocksize ;			/* Blocksize (not used)*/
    struct  sockaddr_in remote_host ;	/* Adresse of remote host */
    int     nbport ;			/* Number of ports */
    int     port[MAXPORT] ;			/* Ports Number */
} ;

struct mess_compress {
	int	code ;
	int	datalen ;
} ;

struct mess_sec {
	int	crtype ;
	int pubkeylen ;
	int expolen ;
} ;

struct mess_retr_ok {
	my64_t	filesize ;
} ;

#define USERMESS struct mess_user
#define USERMESSLEN sizeof(USERMESS)

#define MINMESS  struct message
#define RSAMESS struct mess_rsa
#define STORMESS struct mess_store
#define COMPMESS struct mess_compress
#define CRYPTMESS struct mess_sec
#define RETROKMESS struct mess_retr_ok
#define MINMESSLEN sizeof(MINMESS)
#define RSAMESSLEN sizeof(RSAMESS)
#define STORMESSLEN sizeof(STORMESS)
#define COMPMESSLEN sizeof(COMPMESS)
#define CRYPTMESSLEN sizeof(CRYPTMESS)
#define RETROKMESSLEN sizeof(RETROKMESS)
#define MAXMESSLEN STORMESSLEN
/*
** End Structures **************************************************************
*/

/*
** BBFTP V2 protocol definition
*/
/*
** Message code 
*/
#ifdef WORDS_BIGENDIAN
/* #define MSG_OK				0x00000000 */
/* #define MSG_LOG				0x00000001 */
/* #define MSG_BAD				0x00000003 */
/* #define MSG_ACK				0x00000004 */
/* #define MSG_CRYPT			0x00000006 */
/* #define MSG_BAD_NO_RETRY		0x0000000E */
/* #define MSG_MKDIR			0x0000000F */
/* #define MSG_CHDIR			0x00000010 */
/* #define MSG_LIST				0x00000011 */
/* #define MSG_LIST_REPL		0x00000012 */
/* #define MSG_CREATE_ZERO		0x00000013 */
#define MSG_LOGGED_STDIN		0x00000014
#define MSG_IPADDR				0x00000015
#define MSG_IPADDR_OK			0x00000016
#define MSG_PROT				0x00000017
#define MSG_PROT_ANS			0x00000018
#define MSG_STORE_V2			0x00000019
#define MSG_RETR_V2				0x0000001A
#define MSG_ABORT				0x0000001B
#define MSG_TRANS_OK_V2			0x0000001C
#define MSG_TRANS_START_V2		0x0000001D
#define MSG_INFO				0x0000001E
#define MSG_FILENAME			0x0000001F
#define MSG_CHUMASK 			0x00000020
#define MSG_MKDIR_V2 			0x00000021
#define MSG_CHDIR_V2 			0x00000022
#define MSG_LIST_V2 			0x00000023
#define MSG_LIST_REPL_V2 		0x00000024
#define MSG_CHCOS        		0x00000025
#define MSG_PRIV_LOG            0x00000026
#define MSG_PRIV_DATA           0x00000027
#define MSG_CLOSE_CONN          0x00000028
#define MSG_CERT_LOG          	0x00000029
#define MSG_TRANS_SIMUL         0x0000002A
#define MSG_TRANS_START_V3      0x0000002B
#define MSG_TRANS_SIMUL_V3      0x0000002C
#define MSG_TRANS_OK_V3         0x0000002D
#define MSG_RM			0x0000002E
#define MSG_STAT		0x0000002F
#define MSG_DF			0x00000030
#define MSG_SERVER_STATUS       0x00000031
#else
/* #define MSG_OK				0x00000000 */
/* #define MSG_LOG				0x01000000 */
/* #define MSG_BAD				0x03000000 */
/* #define MSG_ACK				0x04000000 */
/* #define MSG_CRYPT			0x06000000 */
/* #define MSG_MKDIR			0x0F000000 */
/* #define MSG_CHDIR			0x10000000 */
/* #define MSG_LIST				0x11000000 */
/* #define MSG_LIST_REPL		0x12000000 */
/* #define MSG_CREATE_ZERO		0x13000000 */
#define MSG_LOGGED_STDIN		0x14000000
#define MSG_IPADDR				0x15000000
#define MSG_IPADDR_OK			0x16000000
#define MSG_PROT				0x17000000
#define MSG_PROT_ANS			0x18000000
#define MSG_STORE_V2			0x19000000
#define MSG_RETR_V2				0x1A000000
#define MSG_ABORT				0x1B000000
#define MSG_TRANS_OK_V2			0x1C000000
#define MSG_TRANS_START_V2		0x1D000000
#define MSG_INFO				0x1E000000
#define MSG_FILENAME			0x1F000000
#define MSG_CHUMASK 			0x20000000
#define MSG_MKDIR_V2 			0x21000000
#define MSG_CHDIR_V2 			0x22000000
#define MSG_LIST_V2 			0x23000000
#define MSG_LIST_REPL_V2 		0x24000000
#define MSG_CHCOS        		0x25000000
#define MSG_PRIV_LOG        	0x26000000
#define MSG_PRIV_DATA           0x27000000
#define MSG_CLOSE_CONN          0x28000000
#define MSG_CERT_LOG          	0x29000000
#define MSG_TRANS_SIMUL         0x2A000000
#define MSG_TRANS_START_V3      0x2B000000
#define MSG_TRANS_SIMUL_V3      0x2C000000
#define MSG_TRANS_OK_V3         0x2D000000
#define MSG_RM			0x2E000000
#define MSG_STAT		0x2F000000
#define MSG_DF			0x30000000
#define MSG_SERVER_STATUS       0x31000000
#endif

/*
** Crypt type are the same the version 1
*/
/*
** End Crypt type **************************************************************
*/

/*
** Structures 
*/
/*
* The following structures from version 1 are still used
*			message
*			mess_rsa
*			mess_compress
*			mess_sec
*/
struct mess_store_v2 {
	my64_t	filesize ;			/* file size in bytes */
	int		transferoption ;	/* transfer otions : see TROPT_XXX */
	int		filemode ;			/* file mode */
	char	lastaccess[8] ;		/* last access in hexa form */
	char	lastmodif[8] ;		/* last modif in hexa form */
	int		sendwinsize ;		/* Send window size in KB */
	int		recvwinsize ;		/* Receive window size in KB */
	int		buffersizeperstream ;	/* Buffer size per stream */
	int		nbstream ;			/* Number of stream */
} ;

struct mess_integer {
    int     myint ;
} ;

struct mess_dir {
    int     transferoption ;
    char    dirname[1] ;
} ;

struct mess_private {
	unsigned char	cryptdata[NBYTESRSA] ;	
	int		lengthdata ;
} ;

	
#define STORMESS_V2 struct mess_store_v2
#define STORMESSLEN_V2 sizeof(STORMESS_V2)
#define PRIVRSAMESS struct mess_private
#define PRIVRSAMESSLEN sizeof(PRIVRSAMESS)

/*
** End Structures **************************************************************
*/
/*
** Define options bits
*/
#ifdef WORDS_BIGENDIAN
#define TROPT_TMP		0x00000001	/* Always use temporary name */
#define TROPT_ACC		0x00000002	/* Keep Access and modification time  */
#define TROPT_MODE		0x00000004	/* Keep Mode */
#define TROPT_DIR		0x00000008	/* Create all dir in path */
#define TROPT_GZIP		0x00000010	/* Compress with GZIP algorythme */
#define TROPT_RFIO		0x00000020	/* RFIO on the daemon side */
#define TROPT_RFIO_O	0x00000040	/* RFIO on the client side */
#define TROPT_QBSS		0x00000080	/* Mark traffic for QBSS */
#else
#define TROPT_TMP		0x01000000
#define TROPT_ACC		0x02000000
#define TROPT_MODE		0x04000000
#define TROPT_DIR		0x08000000
#define TROPT_GZIP		0x10000000
#define TROPT_RFIO		0x20000000	
#define TROPT_RFIO_O	0x40000000	
#define TROPT_QBSS		0x80000000
#endif
/*
** End Define options bits *****************************************************
*/

#endif /* _BBFTP_STRUCTURES_H */
