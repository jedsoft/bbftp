/*
 * bbftpd/readcontrol.c
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
         0  calling program return to main loop
        -1 calling program exit
 
 readcontrol.c v 0.0.0 1999/11/24
               v 1.2.0 2000/01/26
               v 1.3.0 2000/03/20   - Externalise the socket creation in order
                                      to be able to retry the connection if we
                                      where interrupted by a signal.  
                                      (createreceivesock.c) 
                                      Bug shown by amlutz
                                    - In case of problem in the father send
                                      a reply bad but prevent children to resend
                                      it by setting childendinerror to 1 
                                      Bug shown by amlutz 
               v 1.4.0 2000/03/22   - Use new encryption protocol
               v 1.6.0 2000/03/24   - Reorganize the logic to split readcontrol
                                      in several files
                                    - Add sendafile call 
               v 1.8.5 2000/04/28   - Add createadir call
               v 1.8.7 2000/05/24   - Modify headers
               v 1.8.8 2000/06/02   - Add changetodir call
                                    - Add sendlist call
               v 1.9.0 2000/08/18   - Use configure to help portage
               v 2.0.0 2000/12/07   - Discard completly unkown messages in
                                      logged state
               v 2.0.1 2001/04/23   - Correct indentation
              
*****************************************************************************/
#include <sys/time.h>
#include <netinet/in.h>
#include <syslog.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <status.h>
#include <structures.h>
#include <version.h>

extern int state ;
extern int msgsock ;
extern int unlinkfile ;
extern int pid_child[MAXPORT] ;
extern	int	recvcontrolto ;

int readcontrol(int msgcode,int msglen) {

    int        retcode ;
    int        i ;
    
    /*
    ** The message on control socket has already been read
    */
    /*
    ** Depending on the state the accepted message may be different
    */
    switch (state) {
        /*
        ** State connected : Not called here started with v2
        */
        case S_LOGGED : {
            /*
            ** Initilize the pid array
            */
            for ( i=0 ; i< MAXPORT ; i++) {
                pid_child[i] = 0 ;
            }
            unlinkfile = 0 ;
            switch (msgcode) {
#ifdef WITH_GZIP
                case MSG_STORE_C :
#else
                case MSG_STORE_C : {
                    syslog(BBFTPD_ERR,"Compression is not available on this server") ;
                    retcode = discardmessage(msgsock,msglen,recvcontrolto) ;
                    reply(MSG_BAD_NO_RETRY,"Compression is not available on this server") ;
                    return retcode ;
                }
#endif
                case MSG_STORE : {
                    retcode = storeafile(msgcode) ;
                    return retcode ;
                }


#ifdef WITH_GZIP
                case MSG_RETR_C :
#else
                case MSG_RETR_C : {
                    syslog(BBFTPD_ERR,"Compression is not available on this server") ;
                    retcode = discardmessage(msgsock,msglen,recvcontrolto) ;
                    reply(MSG_BAD_NO_RETRY,"Compression is not available on this server") ;
                    return retcode ;
                }
#endif
                case MSG_RETR : {
                    retcode = sendafile(msgcode) ;
                    return retcode ;
                }

#ifdef WITH_GZIP
                case MSG_RETR_RFIO_C :
#else
                case MSG_RETR_RFIO_C : {
                    syslog(BBFTPD_ERR,"Compression is not available on this server") ;
                    retcode = discardmessage(msgsock,msglen,recvcontrolto) ;
                    reply(MSG_BAD_NO_RETRY,"Compression is not available on this server") ;
                    return retcode ;
                }
#endif
                case MSG_RETR_RFIO : {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
                    retcode = sendafilerfio(msgcode) ;
                    return retcode ;
#else
                    syslog(BBFTPD_ERR,"Retreive RFIO not supported") ;
                    reply(MSG_BAD_NO_RETRY,"Retreive RFIO not supported") ;
                    return 0 ;
#endif
                }


                case MSG_MKDIR :{
                    retcode = createadir(msgcode,msglen) ;
                    return retcode ;
                }
                case MSG_CHDIR :{
                    retcode = changetodir(msgcode,msglen) ;
                    return retcode ;
                }
                case MSG_LIST :{
                    retcode = sendlist(msgcode,msglen) ;
                    return retcode ;
                }
                default : {
                    syslog(BBFTPD_ERR,"Unkown message in logged state %d ",msgcode) ;
                    retcode = discardmessage(msgsock,msglen,recvcontrolto) ;
                    reply(MSG_BAD_NO_RETRY,"Unkown message in logged state") ;
                    return retcode ;
                }
            }
        }
        default :
            syslog(BBFTPD_ERR,"Receive message in state %d",state) ;
            return -1 ;
    }
}
