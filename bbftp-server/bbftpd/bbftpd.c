/*
 * bbftpd/bbftpd.c
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

  
  
 bbftpd.c v 0.0.0  1999/11/24
          v 1.3.0  2000/03/16   - Version in openlog
          v 1.4.0  2000/03/22   - Modify the connection protocole
                                  in order to send the crypt type
                                  after the connection.
          v 1.5.0  2000/03/24   - Change the wait timer in default
          v 1.6.0  2000/03/24   - Change the main loop to make it more
                                    modular
          v 1.6.1  2000/03/24   - Portage on OSF1
                                - Make the control socket non blocking
          v 1.8.0  2000/04/14   - Introduce RSA Cryptage
          v 1.8.6  2000/05/21   - Allow to run with inetd
          v 1.8.7  2000/05/24   - Introduce version.h and config.h
          v 1.8.10 2000/08/11   - Portage to Linux
          v 1.9.0  2000/08/18   - Use configure to help portage
                                - default time out set to 900 s
          v 1.9.3  2000/10/12   - Add -b and -w option in order to overwrite
                                  the fixed values
          v 1.9.4  2000/10/16   - Make all sockets blocking in order
                                  to prevent the error in case of lack 
                                  of memory
                                - Supress %m
          v 2.0.0  2001/03/26   - Protocol V2 implementation
          v 2.0.1  2001/04/23   - Correct indentation
                                - Port to IRIX
          v 2.0.2  2001/05/07   - Add debug option for RFIO
          v 2.1.0  2001/05/28   - Add private authentication
                                - Correct syslog level
                                - Add -l option
		  v 2.2.0  2001/10/03   - Add the certificate authentication process

*****************************************************************************/


#include <bbftpd.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/wait.h>
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
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <common.h>
#include <config.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>
#include <version.h>
#ifdef CERTIFICATE_AUTH
#include <gssapi.h>
#include <gfw.h>
#endif
#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#include <openssl/rsa.h>

#ifdef WITH_GZIP
#include <zlib.h>
#endif

#ifdef CERTIFICATE_AUTH
#define OPTIONS    "bcd:fl:m:pR:suvw:"
#else
#define OPTIONS    "bd:e:fl:m:R:suvw:"
#endif
/*
** Common variables for BBFTP protocole version 1 and 2
*/
/*
** be_daemon :
**      That is the way the daemon is going to be run.
**      0 means run by inetd (that can be done trought 
**      a wrapper)                                      (run as bbftpd)
**      1 means that the control socket are stdin and 
**      stdout                                          (run as bbftpd -s)
**      2 means running in background                   (run as bbftpd -b)
**      Default 0
*/
int        be_daemon = 0 ;
/*
** daemonchar:
**      String used to initialize openlog
*/
char        daemonchar[50] ;
/*
** fixeddataport :
**      If set to 1 then the server will use CONTROLPORT-1 
**      to use as local port number for the data socket. 
**      It is useful to set it to 1 if the server is 
**      running behing a firewall.                      (run as bbftpd)
**      If set to 0 then the server will not fixe the 
**      local data port                                 (run as bbftpd -f)
**      Default 1
*/
int     fixeddataport = 1 ;
/*
** state :
**      Define the state of the server. values differ depending
**      of the BBFTP protocol version
*/
int        state ;                         
/*
** newcontrolport :
**      Define the control port to listen on
*/
int        newcontrolport ; 
/*
** currentusername :
**      Define the local username
*/
char currentusername[MAXLEN] ;
/*
** myrsa :
**      Define the local location where is store the key pair
*/
RSA        *myrsa ;
/*
** his_addr :
**      Remote address (the client)
*/
struct sockaddr_in his_addr;
/*
** ctrl_addr :
**      Local adresse (the server)
*/
struct sockaddr_in ctrl_addr;
/*
** bbftpdrc:
**      Where to store the bbftpdrc file
*/
char    *bbftpdrc = NULL ;
/*
** protocolversion :
**      Set to 0 before any PROT message exchange, that also means
**      that no children have been started
*/
int     protocolversion ;
int     protocolmin = 1  ;
int     protocolmax = 3  ;
/*
** fatherpid :
**      The pid of the process keepng the control connection
*/
pid_t    fatherpid ;
/* 
** flagsighup:
**      Set to one when a SIGHUP is received
**/
int    flagsighup = 0 ;
/*
** killdone:
**      Set to one when the father has send a kill to all chidren
*/
int    killdone= 0 ;
/*
** childendinerror:
**      Set to one when one child has been detected in error in order
**      to prevent father to send multiple answer
*/
int    childendinerror = 0 ;
/*
** unlinkfile:
**      Set to one when father has to unlink the file to store. Used
**      in case of child ended incorrectly (kill -9 for example)
*/
int    unlinkfile = 0  ;
/*
** debug:
**      Set to one for RFIO debug. Valid only with the -b option
**      set to the value needed for RFIO_TRACE
*/
int     debug = 0 ;
/*
** castfd:
**      CASTOR file descriptor use with RFIO and CASTOR
*/
int     castfd ;
/*
** castfilename:
**      CASTOR/SHIFT real file name
*/
char    *castfilename = NULL ;
/*
** End Common variables for BBFTP protocole version 1 and 2 ********************
*/
/*
** Variables for BBFTP protocole version 1 
*/
/*
** msgsock :
**      Define control socket for the server
*/
int     msgsock ;
/*
** currentfilename :
**      Define the file we are working on 
*/
char    currentfilename[MAXLENFILE];
/*
** pid_child :
**      Array to store the children pid
*/
pid_t pid_child[MAXPORT] ;
/*
** End Variables for BBFTP protocole version 1 *********************************
*/
/*
** Variables for BBFTP protocole version 2
*/
/*
** incontrolsock :
**      Define the control socket for reading
** outcontrolsock :
**      Define the control socket for writing
**
**      For be_daemon equal to 0 or 2 incontrolsock = outcontrolsock
*/
int     incontrolsock ;
int     outcontrolsock ;
/*
** curfilename :
**      Define the pointer to the current file
*/
char    *curfilename = NULL ;
/*
** curfilenamelen :
**      Define the length of the memory used by curfilename
*/
int     curfilenamelen ;
/*
** realfilename :
**      Define the pointer to the real file (= curfilename if TROPT_TMP not
**      set)
*/
char    *realfilename = NULL ;
/*
** mychildren :
**      Pointer to the first pid of children
*/
int     *mychildren = NULL ;
/*
** nbpidchid :
**      Number of pid pointed by mychildren 
*/
int     nbpidchild ;
/*
** myports :
**      Pointer to the first port
*/
int     *myports = NULL ;
/*
** mysockets :
**      Pointer to the first socket
*/
int     *mysockets = NULL ;
/*
** readbuffer :
**      Pointer to the readbuffer
*/
int     *readbuffer = NULL ;
/*
** compbuffer :
**      Pointer to the compression buffer
*/
int     *compbuffer = NULL ;
/*
** myumask :
**      Umask for the bbftpd process, the default will be 022 which
**      means that group and other write permissions are denied
**      This can be changed by the bbftp umask command
**/
int     myumask = 022 ;
/*
**	Parameters describing the connection
*/
int	    ackto           = ACKTO;
int	    recvcontrolto   = CONTROLSOCKTO;
int	    sendcontrolto   = SENDCONTROLTO;
int	    datato          = DATASOCKTO;
int	    checkstdinto    = CHECKSTDINTO;
int     force_encoding  = 1;
#ifdef CERTIFICATE_AUTH
int     accept_certs_only = 0;
int     accept_pass_only = 0;
#endif
/*
** Parameters describing the transfer
*/
int     transferoption ;
int     filemode ;
char    lastaccess[9] ;
char    lastmodif[9] ;
int     sendwinsize ;        
int     recvwinsize ;        
int     buffersizeperstream ;
int     requestedstreamnumber ;
my64_t  filesize ;
int     mycos = 0 ;
/*
** maxstreams :
**      That is to fix the maximum number of stream the deamon will accept.
**      (run as bbftpd -m 25)
**      Default 25
*/
int     maxstreams = 25 ;
/*
** End Variables for BBFTP protocole version 2 *********************************
*/
/*
 * Variables for protocol version 3 (PASV mode) ******************************
*/
/*
 * Range for the ephemeral ports for data connections
 */
int     pasvport_min ;
int     pasvport_max ;
/*
** End Variables for BBFTP protocole version 3 *********************************
*/
/*
**  credentials for certificate authentication
*/
#ifdef CERTIFICATE_AUTH
gss_cred_id_t   server_creds;
#endif

/*
 * Stats
*/
struct  timeval  tstart;

main (argc,argv,envp)
    int     argc ;
    char    **argv ;
    char    **envp ;
{
    extern char *optarg;
    extern int optind, opterr, optopt;
/*
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int        addrlen ;
#else
    size_t        addrlen ;
#endif
*/
#ifdef HAVE_SOCKLEN_T
    socklen_t     addrlen ;
#else
    int           addrlen ;
#endif
    struct  timeval    wait_timer;
    fd_set  selectmask ; 
    int        nfds ; 
    int        retcode ;
    int        i, j, k ;
    struct  message *msg ;
    char    buffer[MINMESSLEN] ;
    char    logmessage[1024] ;
    char    rfio_trace[20] ;
    struct  passwd  *mypasswd ;
    char    *bbftpdrcfile = NULL ;
    int     fd ;
    char    *carret ;
    char    *startcmd ;
    int     nooption ;
    struct  stat    statbuf ;
    int     alluse ;
    
    sprintf(daemonchar,"bbftpd v%s",VERSION) ;
    openlog(daemonchar, LOG_PID | LOG_NDELAY, BBFTPD_FACILITY);
    /*
    ** Set the log mask to BBFTPD_EMERG (0) 
    */
    setlogmask(LOG_UPTO(BBFTPD_DEFAULT));
    /*
    ** Initialize variables
    */
    protocolversion = 0 ;
#ifdef PORT_RANGE
    sscanf(PORT_RANGE,"%d:%d",&pasvport_min, &pasvport_max) ;
#endif

    newcontrolport = CONTROLPORT ;
    opterr = 0 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'v' :{
                printf("bbftpd version %s\n",VERSION) ;
                printf("Compiled with  :   default port %d\n",CONTROLPORT) ;
                printf("                   default maximum streams = %d \n",maxstreams) ;
#ifdef PORT_RANGE
                printf("                   data ports range = %s \n", PORT_RANGE) ;
#endif
#ifdef WITH_GZIP
                printf("                   compression with Zlib-%s\n", zlibVersion()) ;
#endif
                printf("                   encryption with %s \n",SSLeay_version(SSLEAY_VERSION)) ;
#ifdef WITH_RFIO
# ifdef CASTOR
                printf("                   CASTOR support (RFIO)\n") ;
# else
                printf("                   HPSS support (RFIO)\n") ;
# endif
#endif
#ifdef WITH_RFIO64
# ifdef CASTOR
                printf("                   CASTOR support (RFIO64)\n") ;
# else
                printf("                   HPSS support (RFIO64)\n") ;
# endif
#endif
#ifdef AFS
                printf("                   AFS authentication \n") ;
#endif
#ifdef CERTIFICATE_AUTH
                printf("                   GSI authentication \n") ;
#endif
#ifdef PRIVATE_AUTH
                printf("                   private authentication \n") ;
#endif
                exit(0) ;
            }
        }
    }
/* Check zlib version */
#ifdef WITH_GZIP
    {
	int z0, z1;
	if (0 != strcmp(zlibVersion(), ZLIB_VERSION)) {
		printf ("WARNING zlib version changed since last compile.\n");
		printf ("Compiled Version is %s\n", ZLIB_VERSION);
		printf ("Linked   Version is %s\n", zlibVersion());
	}
	z0 = atoi (ZLIB_VERSION);
	z1 = atoi (zlibVersion());
	if (z0 != z1) {
		printf ("ERROR: zlib is not compatible!\n");
		printf ("zlib source can found at http://www.gzip.org/zlib/\n");
		exit (0);
	}

    }
#endif
    /*
    ** Look at the loglevel option 
    */
    i = 0 ;
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'l' :{
                for ( i=0 ; i< strlen(optarg) ; i++ ) {
                    optarg[i] = toupper(optarg[i]) ;
                }
                i = 0 ;
                if ( !strcmp(optarg,"EMERGENCY") ) {
                    i = BBFTPD_EMERG;
                } else if ( !strcmp(optarg,"ALERT") ) {
                    i = BBFTPD_ALERT;
                } else if ( !strcmp(optarg,"CRITICAL") ) {
                    i = BBFTPD_CRIT;
                } else if ( !strcmp(optarg,"ERROR") ) {
                    i = BBFTPD_ERR;
                } else if ( !strcmp(optarg,"WARNING") ) {
                    i = BBFTPD_WARNING;
                } else if ( !strcmp(optarg,"NOTICE") ) {
                    i = BBFTPD_NOTICE;
                } else if ( !strcmp(optarg,"INFORMATION") ) {
                    i = BBFTPD_INFO;
                } else if ( !strcmp(optarg,"DEBUG") ) {
                    i = BBFTPD_DEBUG;
                }
                if ( i > 0 ) {
                    setlogmask(LOG_UPTO(i));
                }
                break ;
            }
        }
    }
    syslog(BBFTPD_DEBUG,"Starting bbftpd") ;
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'b' :{
                if ( be_daemon != 0 ) {
                    syslog(BBFTPD_ERR,"-b and -s options are incompatibles") ;
                    exit(1) ;
                }
                be_daemon = 2 ;
                break ;
            }
            case 'd' :{
                sscanf(optarg,"%d",&i) ;
                if ( i > 0 ) debug = i ;
                break ;
            }
            case 'e' :{
                if ((sscanf(optarg,"%d:%d",&i, &k) == 2) && (i < k)) {
                  pasvport_min = i; pasvport_max = k;
                } else {
                  syslog(BBFTPD_ERR,"Invalid port range : %s",optarg) ;
                  fprintf(stderr,"Invalid port range : %s\n",optarg) ;
                  exit(1) ;
                }
                break ;
            }
            case 'f' :{
                fixeddataport = 0 ;
                break ;
            }
            case 'm' :{
                sscanf(optarg,"%d",&i) ;
                if ( i > 0 ) maxstreams = i ;
                break ;
            }
            case 'R' :{
                bbftpdrcfile = optarg ;
                break ;
            }
            case 's' :{
                if ( be_daemon != 0 ) {
                    syslog(BBFTPD_ERR,"-b and -s options are incompatibles") ;
                    exit(1) ;
                }
#ifdef PRIVATE_AUTH
                syslog(BBFTPD_ERR,"-s option cannot be used with private authentication") ;
                exit(1) ;                
#endif
                be_daemon = 1 ;
                break ;
            }
            case 'u' :{
                force_encoding = 0 ;
                break ;
            }
            case 'w' :{
                sscanf(optarg,"%d",&newcontrolport) ;
                break ;
            }
#ifdef CERTIFICATE_AUTH
            case 'c' :{
                accept_certs_only = 1 ;
                break ;
            }
            case 'p' :{
                accept_pass_only = 1 ;
                break ;
            }
#endif
            default : {
                break ;
            }
        }
    }

/*
** Check for the local user in order to find the .bbftpdrc file
*/
    if ( bbftpdrcfile == NULL ) {
        /*
        ** look for the local user in order to find the .bbftpdrc file
	** use /etc/bbftpd.conf if root
        */
	if ( getuid() == 0) {
            if ( (bbftpdrcfile = (char *) malloc (strlen(BBFTPD_CONF)+1 )) == NULL ) {
                syslog(BBFTPD_ERR, "Error allocationg space for config file name.\n") ;
	    } else {
		strcpy(bbftpdrcfile,BBFTPD_CONF);
	    }
	} else if ( (mypasswd = getpwuid(getuid())) == NULL ) {
            syslog(BBFTPD_WARNING, "Unable to get passwd entry, using %s\n", BBFTPD_CONF) ;
            if ( (bbftpdrcfile = (char *) malloc (strlen(BBFTPD_CONF)+1) ) != NULL ) {
	      strcpy(bbftpdrcfile,BBFTPD_CONF);
	    }
        } else if ( mypasswd->pw_dir == NULL ) {
            syslog(BBFTPD_WARNING, "No home directory, using %s\n", BBFTPD_CONF) ;
            if ( (bbftpdrcfile = (char *) malloc (strlen(BBFTPD_CONF)+1) ) != NULL ) {
	      strcpy(bbftpdrcfile,BBFTPD_CONF);
	    }
        } else if ( (bbftpdrcfile = (char *) malloc (strlen(mypasswd->pw_dir)+11) ) == NULL ) {
            syslog(BBFTPD_ERR, "Error allocationg space for bbftpdrc file name, .bbftpdrc will not be used\n") ;
        } else {
            strcpy(bbftpdrcfile,mypasswd->pw_dir) ;
            strcat(bbftpdrcfile,"/.bbftpdrc") ;
            if ( stat(bbftpdrcfile,&statbuf) < 0  ) {
                bbftpdrcfile == NULL;
                if ( (bbftpdrcfile = (char *) malloc (strlen(BBFTPD_CONF)+1) ) != NULL ) {
	          strcpy(bbftpdrcfile,BBFTPD_CONF);
	        }
	    }
        }
    }
    if ( bbftpdrcfile != NULL ) {
        if ( strncmp(bbftpdrcfile,"none",4) != 0 ) {
            /*
            ** Stat the file in order to get the length
            */
            if ( stat(bbftpdrcfile,&statbuf) < 0  ) {
                /*
		  syslog(BBFTPD_WARNING, "Error stating bbftpdrc file (%s)\n",bbftpdrcfile) ;
		 */
            } else if ( statbuf.st_size == 0 ) {
                /*
                ** do nothing 
                */
            } else if ( (bbftpdrc = (char *) malloc (statbuf.st_size + 1 ) ) == NULL ) {
                syslog(BBFTPD_ERR, "Error allocation memory for bbftpdrc, .bbftpdrc will not be used\n") ;
            } else if ( ( fd  = open (bbftpdrcfile,O_RDONLY) )  < 0 ) {
                syslog(BBFTPD_ERR, "Error openning .bbftpdrc file (%s) : %s \n",bbftpdrcfile,strerror(errno)) ;
            } else if ( ( j = read( fd, bbftpdrc , statbuf.st_size )) != statbuf.st_size ) {
                syslog(BBFTPD_ERR, "Error reading .bbftpdrc file (%s)\n",bbftpdrcfile) ;
            } else {
                bbftpdrc[j] = '\0' ;
            }
        }
    }
/*
** Analyse the bbftpdrc command in order to supress forbiden command
** Allowed commands are :
**          setackto %d
**          setrecvcontrolto %d
**          setsendcontrolto %d
**          setdatato %d
**          setcheckstdinto %d
**          setoption fixeddataport
*/
    if ( bbftpdrc != NULL ) {
        carret = bbftpdrc ;
        startcmd = bbftpdrc ;
        /*
        ** Strip starting CR
        */
        while (1) {
            while ( *carret == 10 || *carret == ' ' ) carret++ ;
            startcmd = carret ;
            carret = (char *) strchr (carret, 10);
            if ( carret == NULL ) break ;
            *carret = '\0' ;
            if (!strncmp(startcmd,"setackto",8)) {
                retcode = sscanf(startcmd,"setackto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    syslog(BBFTPD_WARNING, "Acknowledge timeout must be numeric and > 0\n") ;
                } else {
                    ackto = alluse ;
                }
            } else if (!strncmp(startcmd,"setrecvcontrolto",16)) {
                retcode = sscanf(startcmd,"setrecvcontrolto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    syslog(BBFTPD_WARNING, "Input control timeout must be numeric and > 0\n") ;
                } else {
                    recvcontrolto = alluse ;
                }
            } else if (!strncmp(startcmd,"setsendcontrolto",16)) {
                retcode = sscanf(startcmd,"setsendcontrolto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    syslog(BBFTPD_WARNING, "Output control timeout must be numeric and > 0\n") ;
                } else {
                    sendcontrolto = alluse ;
                }
            } else if (!strncmp(startcmd,"setdatato",9)) {
                retcode = sscanf(startcmd,"setdatato %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    syslog(BBFTPD_WARNING, "Data timeout must be numeric and > 0\n") ;
                } else {
                    datato = alluse ;
                }
            } else if (!strncmp(startcmd,"setcheckstdinto",15)) {
                retcode = sscanf(startcmd,"setcheckstdinto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    syslog(BBFTPD_WARNING, "Check input timeout must be numeric and > 0\n") ;
                } else {
                    checkstdinto = alluse ;
                }
            } else if (!strncmp(startcmd,"setoption",9)) {
                /*
                ** look for the option
                */
                startcmd = startcmd + 9 ;
                while (*startcmd == ' ' && *startcmd != '\0' ) startcmd++ ;
                if ( !strncmp(startcmd,"no",2) ) {
                    nooption = 1 ;
                    startcmd = startcmd+2 ;
                } else {
                    nooption = 0 ;
                }
                if ( !strncmp(startcmd,"fixeddataport",9) ) {
                    if ( nooption ) {
                        if (fixeddataport == 1) { /* default value */
                            fixeddataport = 0;
			}
                    }
                }
            } else {
                syslog(BBFTPD_WARNING, "Unkown command in .bbftpdrc file (%s)\n",startcmd) ;
            }
            carret++ ;
        }
    }
    /*
    ** Get 5K for castfilename in order to work with CASTOR
    ** software (even if we are not using it)
    */
    if ( (castfilename = (char *) malloc (5000)) == NULL ) {
        /*
        ** Starting badly if we are unable to malloc 5K
        */
        syslog(BBFTPD_ERR,"No memory for CASTOR : %s",strerror(errno)) ;
        fprintf(stderr,"No memory for CASTOR : %s\n",strerror(errno)) ;
        exit(1) ;
    }
    /*
    ** Reset debug to zero if -b option is not present
    */
    if ( (be_daemon != 2) ) debug = 0 ;
    /*
    ** Check if be_daemon = 0 and in this case reset the 
    ** control port to CONTROLPORT
    */
    if ( be_daemon == 0 ) newcontrolport = CONTROLPORT ;

#ifdef CERTIFICATE_AUTH                
    if (be_daemon != 1 && !accept_pass_only) {
        OM_uint32 min_stat, maj_stat;
	maj_stat = gfw_acquire_cred(&min_stat, NULL, &server_creds);
        if (maj_stat != GSS_S_COMPLETE) {
            gfw_msgs_list *messages = NULL;
            gfw_status_to_strings(maj_stat, min_stat, &messages) ;
            while (messages != NULL) {
                syslog(BBFTPD_ERR,"gfw_acquire_cred: %s", messages->msg) ;
                if (be_daemon == 2) fprintf(stderr,"Acquire credentials: %s\n", messages->msg) ;
                messages = messages->next;
            }
            exit(1);
        }
    }
#endif

    if ( be_daemon == 2 ) {
        /*
        ** Run as a daemon 
        */
        do_daemon(argc, argv, envp);
        /*
        ** Check for debug
        */
        if ( debug != 0 ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
            sprintf(rfio_trace,"RFIO_TRACE=%d",debug) ;
            retcode = putenv(rfio_trace) ;
#endif
            if ( retcode == 0 ) {
                /*
                ** reopen stdout to a file like /tmp/bbftpd.rfio.trace.pid
                */
                close(STDOUT_FILENO) ;
                sprintf(logmessage,"/tmp/bbftp.rfio.trace.level.%d.%d",debug,getpid()) ;
                (void) freopen(logmessage,"w",stdout) ;
            }
        }
    } else if ( be_daemon == 1 ) {
        /*
        ** Run by a remote user
        */
        /*
        ** Get the username 
        */
        struct passwd *result ;
        if ( (result = getpwuid(getuid())) == NULL ) {
            syslog(BBFTPD_WARNING,"Error getting username") ;
            sprintf(currentusername,"UID %d",getuid()) ;
        } else {
            strcpy(currentusername,result->pw_name) ;
        }
        /*
        ** Set the control sock to stdin and stdout
        */
        incontrolsock = 0  ;
        outcontrolsock = 1 ;
        /*
        ** As we cannot rely on the env variables to know the
        ** remote host, we are going to wait on an undefined 
        ** port, send the MSG_LOGGED_STDIN and the port number
        ** and wait for a connection...
        */
        checkfromwhere() ;
        syslog(BBFTPD_INFO,"bbftpd started by : %s from %s",currentusername,inet_ntoa(his_addr.sin_addr)) ;
    } else {
        char    buffrand[NBITSINKEY] ;
        struct timeval tp ;
        unsigned int seed ;
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
        incontrolsock = 0  ;
        outcontrolsock = incontrolsock ;
        msgsock = incontrolsock ;
        /* 
        ** set the state
        */
        state = S_PROTWAIT ;
    }
    fatherpid = getpid() ;
    if ( be_daemon == 2 || be_daemon == 0 ) {
        /* Get the remote address */
        addrlen = sizeof(his_addr);
        if (getpeername(incontrolsock, (struct sockaddr *) &his_addr, &addrlen) < 0) {
            syslog(BBFTPD_ERR, "getpeername (%s): %s", argv[0],strerror(errno));
            exit(1);
        }
        addrlen = sizeof(ctrl_addr);
        if (getsockname(incontrolsock, (struct sockaddr *) &ctrl_addr, &addrlen) < 0) {
            syslog(BBFTPD_ERR, "getsockname (%s): %s", argv[0],strerror(errno));
            exit(1);
        }
        syslog(BBFTPD_INFO,"Getting new bbftp connexion from : %s",inet_ntoa(his_addr.sin_addr)) ;
        /*
        ** Send the encryption supported 
        */
        sendcrypt() ;
        /* 
        ** set the state
        */
login:
        state = S_CONN ;
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(incontrolsock,&selectmask) ;
        wait_timer.tv_sec  = 10 ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(nfds,&selectmask,0,0,&wait_timer) ) == -1 ) {
            syslog(BBFTPD_ERR,"Select error : %s",strerror(errno)) ;
            exit(1) ;
        } else if ( retcode == 0 ) {
            syslog(BBFTPD_ERR,"Time OUT ") ;
            exit(1) ;
        } else {
            if ( (retcode = readmessage(incontrolsock,buffer,MINMESSLEN,recvcontrolto)) < 0 ) {
                syslog(BBFTPD_ERR,"Error reading MSG_LOG ") ;
                exit(1) ;
            }
            msg = (struct message *) buffer ;
            if ( msg->code == MSG_SERVER_STATUS ) {
                sprintf(logmessage, "bbftpd version %s\n",VERSION) ;
                sprintf(logmessage,"%sCompiled with  :   default port %d\n",logmessage,CONTROLPORT) ;
#ifdef WITH_GZIP
                sprintf(logmessage,"%s                   compression with Zlib-%s\n", logmessage,zlibVersion()) ;
#endif
                sprintf(logmessage,"%s                   encryption with %s \n",logmessage,SSLeay_version(SSLEAY_VERSION)) ;
#ifdef WITH_RFIO
# ifdef CASTOR
                sprintf(logmessage,"%s                   CASTOR support (RFIO)\n",logmessage) ;
# else
                sprintf(logmessage,"%s                   HPSS support (RFIO)\n",logmessage) ;
# endif
#endif
#ifdef WITH_RFIO64
# ifdef CASTOR
                sprintf(logmessage,"%s                   CASTOR support (RFIO64)\n",logmessage) ;
# else
                sprintf(logmessage,"%s                   HPSS support (RFIO64)\n",logmessage) ;
# endif
#endif
#ifdef AFS
                sprintf(logmessage,"%s                   AFS authentication \n",logmessage) ;
#endif
#ifdef CERTIFICATE_AUTH
                sprintf(logmessage,"%s                   GSI authentication \n",logmessage) ;
#endif
#ifdef PRIVATE_AUTH
                sprintf(logmessage,"%s                   private authentication \n",logmessage) ;
#endif
                sprintf(logmessage,"%sRunning options:\n",logmessage) ;
                sprintf(logmessage,"%s                   Maximum number of streams = %d \n",logmessage,maxstreams) ;
                if (pasvport_min) {
                    sprintf(logmessage,"%s                   data ports range = [%d-%d] \n", logmessage,pasvport_min, pasvport_max) ;
                }
#ifdef CERTIFICATE_AUTH
                if (accept_pass_only) {
                    sprintf(logmessage, "%s                   The server only accepts USER/PASS\n",logmessage);
                }
                if (accept_certs_only) {
                    sprintf(logmessage, "%s                   The server only accepts certificates\n",logmessage);
                }
#endif
#ifndef PRIVATE_AUTH
                if (force_encoding) {
                    sprintf(logmessage, "%s                   The server requires encrypted login\n",logmessage);
                } else {
                    sprintf(logmessage, "%s                   The server allows non-encrypted login\n",logmessage);
                }
#endif

               
                    reply(MSG_OK,logmessage);
                    exit(0);
            }
#ifdef CERTIFICATE_AUTH
            if ( msg->code == MSG_CERT_LOG ) {
                int retval;
                if (accept_pass_only) {
                    sprintf(logmessage, "The server only accepts USER/PASS");
                    syslog(BBFTPD_ERR,"%s",logmessage) ;
                    reply(MSG_BAD_NO_RETRY,logmessage);
                    exit(1);
                }
                retval=bbftpd_cert_receive_connection(msg->msglen);
                if ( retval < 0 ) {
                    /*
                    ** The login failed, do not wait for a new one
                    */
					exit(1) ;
                }
                /*
                ** If retval >0, means an MSG_WARN has been sent already
                */
                    state = S_PROTWAIT ;
                if (retval == 0) {
                    sprintf(logmessage,"bbftpd version %s : OK",VERSION) ;
                    reply(MSG_OK,logmessage) ;
                }
            } else if ( msg->code == MSG_LOG ) {
                if (accept_certs_only) {
                    sprintf(logmessage, "The server only accepts certificates");
                    syslog(BBFTPD_ERR,"%s",logmessage) ;
                    reply(MSG_BAD_NO_RETRY,logmessage);
                } else {
                    /*
                    ** It seems that it is the message we were waiting
                    ** so lets decrypt 
                    */
                    if ( loginsequence() < 0 ) {
                        /*
                        ** The login failed, do not wait for a new one
                        */
                        exit(1) ;
                    }
                    state = S_PROTWAIT ;
                    sprintf(logmessage,"bbftpd version %s : OK",VERSION) ;
                    reply(MSG_OK,logmessage) ;
		}
            } else if (msg->code == MSG_CRYPT) { 
                if (accept_certs_only) {
                    sprintf(logmessage, "The server only accepts certificates");
                    reply(MSG_BAD_NO_RETRY,logmessage);
                } else if (force_encoding) {
                    /*
                    **  The client can't encode and ask if it can send uncrypted
                    **  login information
                    */
                    sprintf(logmessage, "The server requires encrypted login");
                    reply(MSG_BAD_NO_RETRY,logmessage);
                } else {
                    sprintf(logmessage, "Uncrypted login dialog accepted");
                    reply(MSG_OK,logmessage);
                    goto login;
		}
            } else {
                syslog(BBFTPD_ERR,"Unkown message in connected state : %d",msg->code) ;
                reply(MSG_BAD,"Unkown message in connected state") ;
                exit(1) ;
            }
#else
#ifndef PRIVATE_AUTH
            if ( msg->code == MSG_LOG ) {
                /*
                ** It seems that it is the message we were waiting
                ** so lets decrypt 
                */
                if ( loginsequence() < 0 ) {
                    /*
                    ** The login failed, do not wait for a new one
                    */
                    exit(1) ;
                }
                state = S_PROTWAIT ;
                sprintf(logmessage,"bbftpd version %s : OK",VERSION) ;
                reply(MSG_OK,logmessage) ;
            } else if (msg->code == MSG_CRYPT) { 
                /*
                **  The client can't encode and ask if it can send uncrypted
                **  login information
                */
                if (force_encoding) {
                    sprintf(logmessage, "The server requires encrypted login");
                    reply(MSG_BAD_NO_RETRY,logmessage);
                    exit(1);
                }
                sprintf(logmessage, "Uncrypted login dialog accepted");
                reply(MSG_OK,logmessage);
                goto login;
            } else {
                syslog(BBFTPD_ERR,"Unkown message in connected state : %d",msg->code) ;
                reply(MSG_BAD,"Unkown message in connected state") ;
                exit(1) ;
            }
#else
            if ( msg->code == MSG_PRIV_LOG ) {
                if ( bbftpd_private_receive_connection(msg->msglen) < 0 ) {
                    /*
                    ** The login failed, do not wait for a new one
                    */
                    exit(1) ;
                }
                state = S_PROTWAIT ;
                sprintf(logmessage,"bbftpd version %s : OK",VERSION) ;
                reply(MSG_OK,logmessage) ;
            } else {
                syslog(BBFTPD_ERR,"Unkown message in connected state : %d",msg->code) ;
                reply(MSG_BAD,"Unkown message in connected state") ;
                exit(1) ;
            }
#endif
#endif
        }
    }

    /*
    ** At this stage we are in the S_PROTWAIT state
    */
    nfds = sysconf(_SC_OPEN_MAX) ;
    FD_ZERO(&selectmask) ;
    FD_SET(incontrolsock,&selectmask) ;
    wait_timer.tv_sec  = 10 ;
    wait_timer.tv_usec = 0 ;
    if ( (retcode = select(nfds,&selectmask,0,0,&wait_timer) ) == -1 ) {
        syslog(BBFTPD_ERR,"Select error in S_PROTWAIT state : %s",strerror(errno)) ;
        exit(1) ;
    } else if ( retcode == 0 ) {
        syslog(BBFTPD_ERR,"Time OUT in S_PROTWAIT state") ;
        exit(1) ;
    } else {
        if ( (retcode = readmessage(incontrolsock,buffer,MINMESSLEN,recvcontrolto)) < 0 ) {
            syslog(BBFTPD_ERR,"Error reading in S_PROTWAIT state") ;
            exit(1) ;
        }
        msg = (struct message *) buffer ;
        if ( msg->code == MSG_PROT ) {
            /*
            ** The client is using bbftp v2 protocol or higher
            */
            if ( checkprotocol() < 0 ) {
                exit_clean() ;
                exit(1) ;
            }
            syslog(BBFTPD_INFO,"Using bbftp protocol version %d",protocolversion) ;
            state = S_LOGGED ;
            /*
            ** Initialize the variables
            */
            mychildren = NULL ;
            nbpidchild = 0 ;
            curfilename = NULL ;
            curfilenamelen = 0 ;
        } else {
            /*
            ** This is a bbftp v1 client 
            */
            protocolversion = 1 ;
            syslog(BBFTPD_INFO,"Using bbftp protocol version 1") ;
            state = S_LOGGED ;
            /*
            ** So set up the v1 handlers
            */
            if ( set_signals_v1() < 0 ) {
                exit(1) ;
            }
            /*
            ** Initialize the pid array
            */
            for ( i=0 ; i< MAXPORT ; i++) {
                pid_child[i] = 0 ;
            }
            /*
            ** As we have already read the message 
            */
            if ( readcontrol(msg->code,msg->msglen) < 0 ) { 
                clean_child() ;
                exit_clean() ;
                exit(0) ;
            }
        }
    }
    if ( protocolversion == 1 ) goto loopv1 ;
    if ( protocolversion == 2 || protocolversion == 3) goto loopv2 ;
    syslog(BBFTPD_ERR,"Unknown protocol version %d",protocolversion) ;
    exit(1) ;
/*
** Loop for the v2 protocol (also available for v3)
*/
loopv2:
    /*
    ** Set up v2 handlers
    */
    if ( bbftpd_setsignals() < 0 ) {
        exit(1) ;
    }
    /*
    ** Set the umask ; first unset it and then reset to the default value
    */
    umask(0) ;
    umask(myumask) ;
    for (;;) {
        /*
        ** Initialize the selectmask
        */
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(incontrolsock,&selectmask) ;
        /*
        ** Depending on the state set a timer or not 
        */
        switch (state) {
            case S_WAITING_STORE_START :
            case S_WAITING_FILENAME_STORE :
            case S_WAITING_FILENAME_RETR : {
                /*
                ** Timer of 10s between XX_V2 and FILENAME_XX
                */
                wait_timer.tv_sec  = 10 ;
                wait_timer.tv_usec = 0 ;
                break ;
            }
            
            case S_SENDING : 
            case S_RECEIVING : {
                /*
                ** No timer while receiving or sending
                */
                wait_timer.tv_sec  = 0 ;
                wait_timer.tv_usec = 0 ;
                break ;
            }
            default : {
                /*
                ** Timer of 900s between commands
                */
                wait_timer.tv_sec  = 900 ;
                wait_timer.tv_usec = 0 ;
                break ;
            }
        }
        if ( (retcode = select(nfds,&selectmask,0,0,(wait_timer.tv_sec == 0) ? NULL : &wait_timer) ) == -1 ) {
            if ( errno != EINTR ) {
                syslog(BBFTPD_ERR,"Select error : %s",strerror(errno)) ;
            }
        } else if ( retcode == 0 ) {
            syslog(BBFTPD_ERR,"Time OUT ") ;
            if ( state == S_WAITING_STORE_START ) {
                bbftpd_storeunlink(realfilename) ;
            }
            clean_child() ;
            exit_clean() ;
            exit(0) ;
        } else {
            /*
            ** At this stage we can only receive a command
            */
            if ( (retcode = readmessage(incontrolsock,buffer,MINMESSLEN,recvcontrolto)) < 0 ) {
                if ( state == S_WAITING_STORE_START  || state == S_RECEIVING) {
                    bbftpd_storeunlink(realfilename) ;
                    sleep(5) ;
                }
                clean_child() ;
                exit_clean() ;
                exit(0) ;
            }
            if ( bbftpd_readcontrol(msg->code,msg->msglen) < 0 ) { 
                clean_child() ;
                exit_clean() ;
                exit(0) ;
            } else {
            }
        }
    }
/*
** Loop for the v1 protocol 
*/
loopv1:
    for (;;) {
        /*
        ** Initialize the selectmask
        */
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(incontrolsock,&selectmask) ;
        /*
        ** Depending on the state set a timer or not 
        */
        switch (state) {
            case S_SENDING : 
            case S_RECEIVING : {
                /*
                ** No timer while receiving or sending
                */
                wait_timer.tv_sec  = 0 ;
                wait_timer.tv_usec = 0 ;
                break ;
            }
            default : {
                /*
                ** Timer of 900s between commands
                */
                wait_timer.tv_sec  = 900 ;
                wait_timer.tv_usec = 0 ;
                break ;
            }
        }
        if ( (retcode = select(nfds,&selectmask,0,0,(wait_timer.tv_sec == 0) ? NULL : &wait_timer) ) == -1 ) {
            if ( errno != EINTR ) {
                syslog(BBFTPD_ERR,"Select error : %s",strerror(errno)) ;
            }
        } else if ( retcode == 0 ) {
            syslog(BBFTPD_ERR,"Time OUT ") ;
            clean_child() ;
            exit_clean() ;
            exit(0) ;
        } else {
            /*
            ** At this stage we can only receive a command
            */
            if ( (retcode = readmessage(incontrolsock,buffer,MINMESSLEN,recvcontrolto)) < 0 ) {
                clean_child() ;
                exit_clean() ;
                exit(0) ;
            }
            if ( readcontrol(msg->code,msg->msglen) < 0 ) { 
                clean_child() ;
                exit_clean() ;
                exit(0) ;
            } else {
            }
        }
    }
}

void clean_child() 
{
    int    *pidfree ;
    int    i ;
    
    if ( protocolversion == 0 ) return ;
    if ( protocolversion == 1 ) {
        if ( killdone == 0 ) {
            killdone = 1 ;
            for ( i=0 ; i<MAXPORT ; i++) {
                if ( pid_child[i] != 0 ) {
                    syslog(BBFTPD_DEBUG,"Killing child %d",pid_child[i]) ;
                    kill(pid_child[i],SIGKILL) ;
                }
            }
        }
        return ;
    }
    if ( protocolversion >= 2 ) {
        if ( killdone == 0 ) {
            killdone = 1 ;
            pidfree = mychildren ;
            for ( i=0 ; i<nbpidchild ; i++) {
                if ( *pidfree != 0 ) {
                    syslog(BBFTPD_DEBUG,"Killing child %d",*pidfree) ;
                    kill(*pidfree,SIGKILL) ;
                }
                pidfree++ ;
            }
        }
        return ;
    }
}

void exit_clean() 
{
    
    switch (state) {
        case S_CONN : {
            return ;
        }
        case S_LOGGED :
        case S_PROTWAIT :
        case S_WAITING_FILENAME_STORE :
        case S_WAITING_STORE_START :
        case S_SENDING:
        case S_RECEIVING : {
            syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
            return ;
        }
        default :{
            return ;
        }
    }
}

my64_t convertlong(my64_t v) {
    struct bb {
        int    fb ;
        int sb ;
    } ;
    struct bb *bbpt ;
    int     tmp ;
    my64_t    tmp64 ;
    
    tmp64 = v ;
    bbpt = (struct bb *) &tmp64 ;
    tmp = bbpt->fb ;
    bbpt->fb = ntohl(bbpt->sb) ;
    bbpt->sb = ntohl(tmp) ;    
    return tmp64 ;
}

#ifndef HAVE_NTOHLL
my64_t ntohll(my64_t v) {
# ifdef HAVE_BYTESWAP_H
    return bswap_64(v);
# else
    long lo = v & 0xffffffff;
    long hi = v >> 32U;
    lo = ntohl(lo);
    hi = ntohl(hi);
    return ((my64_t) lo) << 32U | hi;
# endif
}
#define htonll ntohll
#endif

