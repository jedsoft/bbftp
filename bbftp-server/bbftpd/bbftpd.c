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
#include <ctype.h>
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
#include <getopt.h>

#include <common.h>
#include <config.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>
#include <version.h>
#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#ifdef WITH_GZIP
#include <zlib.h>
#endif

#include "_bbftpd.h"

#ifdef CERTIFICATE_AUTH
#define OPTIONS    "Abcd:fhl:L:m:pR:suvw:"
#else
#define OPTIONS    "Abd:e:fhl:L:m:R:suvw:"
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
/* int        be_daemon = 0 ; --- local variable in main */

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
** currentusername :
**      Define the local username
*/
char currentusername[MAXLEN] ;
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
/* int     debug = 0 ; -- local to main */
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

/* For be_daemon == 2; incontrolsock = outcontrolsock
 * For be_daemon == 0: incontrolsock = outcontrolsock = 0
 */

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
char    *readbuffer = NULL ;
/*
** compbuffer :
**      Pointer to the compression buffer
*/
char     *compbuffer = NULL ;
/*
**	Parameters describing the connection
*/
int	    ackto           = ACKTO;
int	    recvcontrolto   = CONTROLSOCKTO;
int	    sendcontrolto   = SENDCONTROLTO;
int	    datato          = DATASOCKTO;
int	    checkstdinto    = CHECKSTDINTO;
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
 * Stats
*/
struct  timeval  tstart;

static int is_option_present (int argc, char **argv, int opt)
{
   int i;

   opterr = 0 ;
   optind = 1;

   while (-1 != (i = getopt(argc, argv, OPTIONS)))
     {
	if (i == opt) return 1;
     }
   return 0;
}


static void print_version_info (void)
{
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
}

static int check_libs (void)
{
#ifdef WITH_GZIP
     {
	/* Check zlib version */
	int z0, z1;
	if (0 != strcmp(zlibVersion(), ZLIB_VERSION))
	  {
	     printf ("WARNING zlib version changed since last compile.\n");
	     printf ("Compiled Version is %s\n", ZLIB_VERSION);
	     printf ("Linked   Version is %s\n", zlibVersion());
	  }
	z0 = atoi (ZLIB_VERSION);
	z1 = atoi (zlibVersion());
	if (z0 != z1)
	  {
	     printf ("ERROR: zlib is not compatible!\n");
	     printf ("zlib source can found at http://www.gzip.org/zlib/\n");
	     return -1;
	  }
     }
#endif

   return 0;
}



typedef struct
{
   int server_mode;
   int force_encoding;
   int accept_pass_only;
   int accept_certs_only;
   int debug;
   int control_port;
   int ask_remote_address;
   char *bbftpdrc_file;
   int argc;
   char **argv;
}
Server_Config_Type;

static int init_default_server_config (int argc, char **argv, Server_Config_Type *server_config)
{
   memset (server_config, 0, sizeof(Server_Config_Type));
   server_config->server_mode = 0;
   server_config->force_encoding = 1;
   server_config->accept_pass_only = 0;
   server_config->accept_certs_only = 0;
   server_config->debug = 0;
   server_config->ask_remote_address = 0;
   server_config->control_port = CONTROLPORT;
   server_config->bbftpdrc_file = NULL;
   server_config->argc = argc;
   server_config->argv = argv;
   /*
    ** Get 5K for castfilename in order to work with CASTOR
    ** software (even if we are not using it)
    */
   if ( (castfilename = (char *) malloc (5000)) == NULL ) {
      /*
       ** Starting badly if we are unable to malloc 5K
       */
      bbftpd_log(BBFTPD_ERR,"No memory for CASTOR : %s",strerror(errno)) ;
      fprintf(stderr,"No memory for CASTOR : %s\n",strerror(errno)) ;
      return -1;
   }
   return 0;
}

static void exit_usage (void)
{
   (void) fputs ("\
Usage: bbftpd [options]\n\
Options:\n\
  -A          Ask client for its IP address (SSH mode only)\n\
  -b          Run in the backgound as a daemon\n\
  -d          Run in debug mode (RFIO)\n\
  -e p0:p1    Use ephemeral ports p0 to p1\n\
  -f          Bind to the first available port\n\
  -l level    Set the log-level to the specified value\n\
  -L logfile  Write to logfile instead of using syslog\n\
  -m N        Limit the number of streams to N\n\
  -R file     Use file as the bbftpdrc file\n\
  -s          Use when started via ssh\n\
  -u          Permit non-encrypted username/password\n\
  -c          Force authentification via certificates\n\
  -p          Force authentification via username/password\n\
  -v          Show version\n\
  -h          Show this usage message\n\
",
		 stdout);
   exit (0);
}

static int process_cmdline_options (int argc, char **argv, Server_Config_Type *server_config)
{
   int i, j, k;
   int be_daemon = 0;

   opterr = 0 ;
   optind = 1 ;
   while ((j = getopt(argc, argv, OPTIONS)) != -1)
     {
	switch (j)
	  {
	   case 'A':
	     server_config->ask_remote_address = 1;
	     break;

	   case 'b' :
	     if (be_daemon && (be_daemon != 2))
	       {
		  bbftpd_log(BBFTPD_ERR,"-b and -s options are incompatible");
		  return -1;
	       }
	     server_config->server_mode = be_daemon = 2;
	     break;

	   case 'd' :
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
	     sscanf(optarg,"%d",&i) ;
	     if ( i > 0 ) server_config->debug = i;
#endif
	     break ;

	 case 'e' :
	     if ((2 == sscanf(optarg,"%d:%d",&i, &k)) && (i < k))
	       {
                  pasvport_min = i; pasvport_max = k;
	       }
	     else
	       {
                  bbftpd_log(BBFTPD_ERR,"Invalid port range : %s", optarg) ;
                  fprintf(stderr,"Invalid port range : %s\n", optarg);
		  return -1;
	       }
	     break;

	   case 'f' :
	     fixeddataport = 0 ;
	     break ;

	   case 'h':
	     exit_usage ();
	     break;

	   case 'm' :
	     sscanf(optarg,"%d",&i) ;
	     if ( i > 0 ) maxstreams = i ;
	     break ;

	   case 'R' :
	     server_config->bbftpdrc_file = optarg;
	     break ;

	   case 's' :
#ifdef PRIVATE_AUTH
	     bbftpd_log(BBFTPD_ERR,"-s option cannot be used with private authentication") ;
	     return -1;
#else
	     if (be_daemon && (be_daemon != 1))
	       {
		  bbftpd_log(BBFTPD_ERR,"-b and -s options are incompatible") ;
		  return -1;
	       }
	     server_config->server_mode = be_daemon = 1;
	     break ;
#endif
            case 'u' :
	     server_config->force_encoding = 0;
	     break ;

            case 'w' :
	     sscanf(optarg, "%d", &server_config->control_port);
	     break ;

#ifdef CERTIFICATE_AUTH
            case 'c' :
	     server_config->accept_certs_only = 1 ;
	     break ;

            case 'p' :
	     server_config->accept_pass_only = 1 ;
	     break;
#endif
            default :
	     break;
	  }
     }
   return 0;
}

static char *System_BBftpdrc_File = BBFTPD_CONF;
/* returns either a malloced string for *filep or System_BBftpdrc_File */
static int get_bbftprc_file (char **filep)
{
   struct  passwd  *mypasswd;
   struct stat st;
   char *file;

   *filep = System_BBftpdrc_File;	       /* default */

   /*
    ** look for the local user in order to find the .bbftpdrc file
    ** use /etc/bbftpd.conf if root
    */
   if ( getuid() == 0)
     return 0;

   if (NULL == (mypasswd = getpwuid(getuid())))
     {
	bbftpd_log(BBFTPD_WARNING, "Unable to get passwd entry, using %s\n", System_BBftpdrc_File);
	return 0;
     }

    if (mypasswd->pw_dir == NULL)
     {
	bbftpd_log(BBFTPD_WARNING, "No home directory, using %s\n", System_BBftpdrc_File) ;
	return 0;
     }

   if (NULL == (file = bbftpd_strcat (mypasswd->pw_dir, "/.bbftpdrc")))
     {
	bbftpd_log(BBFTPD_ERR, "Error allocating space for ~/.bbftpdrc file\n");
	*filep = NULL;
	return -1;
     }

   if (-1 == stat(file, &st))
     {
	/* fall back to system config file */
	free (file);
	return 0;
     }

   return 0;
}

typedef struct
{
   const char *name;
   const char *usage_string;
   int *valp;
}
Setcmd_Type;

static Setcmd_Type Setcmd_Table [] =
{
   {"setackto", "Acknowledge timeout must be numeric and > 0", &ackto},
   {"setrecvcontrolto", "Input control timeout must be numeric and > 0", &recvcontrolto},
   {"setsendcontrolto", "Output control timeout must be numeric and > 0", &sendcontrolto},
   {"setdatato", "Data timeout must be numeric and > 0", &datato},
   {"setcheckstdinto", "Check input timeout must be numeric and > 0", &checkstdinto},
   {NULL, NULL, NULL}
};

static int process_bbftpdrc_line (char *line)
{
   Setcmd_Type *setcmd;

   if (*line == '#') return 0;

   setcmd = Setcmd_Table;
   while (setcmd->name != NULL)
     {
	size_t len = strlen (setcmd->name);
	int val;

	if ((strncmp (setcmd->name, line, len))
	    || ((line[len] != ' ') && (line[len] != '\t')))
	  {
	     setcmd++;
	     continue;
	  }
	if ((1 != sscanf (line + len + 1, "%d", &val))
	    || (val <= 0))
	  {
	     bbftpd_log (BBFTPD_WARNING, "%s: %s\n", setcmd->name, setcmd->usage_string);
	     return 0;
	  }
	*setcmd->valp = val;
	return 0;
     }

   /* The only setoption allowed in the file is [no]fixeddataport */
   if ((0 == strncmp (line, "setoption", 9))
       && ((line[9] == ' ') || (line[9] == '\t')))
     {
	int has_no;

	line += 9;
	while ((*line == ' ') || (*line == '\t')) line++;

	has_no = (0 == strncmp (line, "no", 2));
	if (has_no) line += 2;

	if ((0 == strncmp (line, "fixeddataport", 13))
	    && ((line[13] == 0) || (line[13] == '#') || isspace(line[13])))
	  {
	     /* The default value for fixeddataport is 1.  This can be set to
	      * 0 via a command line argument.  The assumption seems to be that if
	      * it is 0, then it has been set on the command line.  In that case,
	      * do not override it here.
	      */
	     if (has_no) fixeddataport = 0;
	     return 0;
	  }

	bbftpd_log (BBFTPD_WARNING, "Unsupported or illegal setoption command in bbftpdrc file (%s)\n", line);
	return 0;
     }

   bbftpd_log (BBFTPD_WARNING, "Unsupported or illegal setoption command in bbftpdrc file (%s)\n", line);
   return 0;
}

static int process_bbftpd_rcfile (Server_Config_Type *server_config)
{
   struct stat st;
   size_t len;
   char *file;
   char *line, *carret, *contents = NULL;
   int status = -1;

   if (NULL == (file = server_config->bbftpdrc_file))
     {
	if (-1 == get_bbftprc_file (&file))
	  return -1;

	if (file == NULL)
	  return 0;
     }

   if (0 == strcmp (file, "none"))
     return 0;

   if (-1 == stat (file, &st))
     {
	if (file == System_BBftpdrc_File)
	  return 0;

	bbftpd_log (BBFTPD_ERR, "Error stating bbftpdrc file (%s)\n", file);
	goto return_status;
     }

   if (st.st_size == 0)
     {
	status = 0;
	goto return_status;
     }

   /* bbftpd_read_file will null terminate the string */
   if (NULL == (contents = bbftpd_read_file (file, &len)))
     goto return_status;

   carret = contents;
   line = contents;
   /*
    ** Strip starting CR
    */
   while (*carret != 0)
     {
	while (isspace (*carret)) carret++;
	line = carret;
	carret = (char *) strchr (carret, '\n');
	if ( carret != NULL )
	  *carret++ = 0;
	else
	  carret = line + strlen (line);

	(void) process_bbftpdrc_line (line);
     }
   status = 0;
   /* drop */

return_status:
   if (contents != NULL) free (contents);
   if ((file != server_config->bbftpdrc_file)
       && (file != System_BBftpdrc_File))
     free (file);

   return status;
}

static void append_to_logmessage (char *buf, size_t buflen, char *dbuf)
{
   size_t dn = strlen (buf);
   buf += dn;
   buflen -= dn;
   strncat (buf, dbuf, buflen);
   buf[buflen-1] = 0;
}

static void send_server_status (Server_Config_Type *server_config)
{
   char dmsgbuf[128];
   char logmessage[1024];

   *logmessage = 0;
   sprintf(dmsgbuf, "bbftpd version %s\n",VERSION);
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
   sprintf(dmsgbuf, "Compiled with  :   default port %d\n", CONTROLPORT) ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#ifdef WITH_GZIP
   sprintf(dmsgbuf, "  compression with Zlib-%s\n", zlibVersion()) ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif
   sprintf(dmsgbuf, "  encryption with %s \n", SSLeay_version(SSLEAY_VERSION)) ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#ifdef WITH_RFIO
# ifdef CASTOR
   sprintf(dmsgbuf, "  CASTOR support (RFIO)\n") ;
# else
   sprintf(dmsgbuf, "  HPSS support (RFIO)\n") ;
# endif
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif
#ifdef WITH_RFIO64
# ifdef CASTOR
   sprintf(dmsgbuf, "  CASTOR support (RFIO64)\n") ;
# else
   sprintf(dmsgbuf, "  HPSS support (RFIO64)\n") ;
# endif
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif
#ifdef AFS
   sprintf(dmsgbuf, "  AFS authentication \n") ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif
#ifdef CERTIFICATE_AUTH
   sprintf(dmsgbuf, "  GSI authentication \n") ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif
#ifdef PRIVATE_AUTH
   sprintf(dmsgbuf, "  private authentication \n") ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif
   sprintf(dmsgbuf, "Running options:\n") ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
   sprintf(dmsgbuf, "  Maximum number of streams = %d\n",maxstreams) ;
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
   if (pasvport_min)
     {
	sprintf(dmsgbuf, "  data ports range = [%d-%d]\n", pasvport_min, pasvport_max) ;
	append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
     }

#ifdef CERTIFICATE_AUTH
   if (accept_pass_only)
     {
	sprintf(dmsgbuf, "  The server only accepts USER/PASS\n",);
	append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
     }
   if (accept_certs_only)
     {
	sprintf(dmsgbuf, "  The server only accepts certificates\n",logmessage);
	append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
     }
#endif
#ifndef PRIVATE_AUTH
   if (server_config->force_encoding)
     sprintf(dmsgbuf, "  The server requires encrypted login\n");
   else
     sprintf(dmsgbuf, "  The server allows non-encrypted login\n");
   append_to_logmessage (logmessage, sizeof(logmessage), dmsgbuf);
#endif

   reply(MSG_OK,logmessage);
   exit(0);
}

static int get_peer_and_sock_names (int sock)
{
#ifdef HAVE_SOCKLEN_T
    socklen_t     addrlen ;
#else
    int           addrlen ;
#endif

   /* Get the remote address -- note that addrlen must be uninitialized */
   addrlen = sizeof(his_addr);
   if (-1 == getpeername(sock, (struct sockaddr *) &his_addr, &addrlen))
     {
	bbftpd_log(BBFTPD_ERR, "getpeername : %s", strerror(errno));
	return -1;
     }

   addrlen = sizeof(ctrl_addr);
   if (-1 == getsockname(sock, (struct sockaddr *) &ctrl_addr, &addrlen))
     {
	bbftpd_log(BBFTPD_ERR, "getsockname : %s", strerror(errno));
	return -1;
     }
   bbftpd_log(BBFTPD_INFO,"Getting new bbftp connexion from : %s",inet_ntoa(his_addr.sin_addr)) ;
   return 0;
}

/* Not used for SSH mode */
static int perform_login (Server_Config_Type *server_config)
{
   struct message msg ;

   /*
    ** Send the encryption supported 
    */
   sendcrypt() ;

   while (1)
     {
        state = S_CONN ;
	if (-1 == bbftpd_msgread_msg (&msg))
	  {
	     /* error or time-out expired */
	     bbftpd_log(BBFTPD_ERR,"Error reading MSG_LOG") ;
	     return -1;
	  }

	switch (msg.code)
	  {
	   default:
	     bbftpd_log(BBFTPD_ERR,"Unkown message in connected state : %d",msg.code) ;
	     reply(MSG_BAD,"Unkown message in connected state") ;
	     return -1;

	   case MSG_SERVER_STATUS:
	     send_server_status (server_config);
	     exit (0);

#ifndef PRIVATE_AUTH
# ifdef CERTIFICATE_AUTH
	   case MSG_CERT_LOG:
	     if (server_config->accept_pass_only)
	       {
		  bbftpd_log (BBFTPD_ERR,"%s", "The server only accepts USER/PASS");
		  reply(MSG_BAD_NO_RETRY, "The server only accepts USER/PASS");
		  return -1;
	       }
	     status = bbftpd_cert_receive_connection(msg.msglen);
	     if ( retval < 0 ) /* The login failed, do not wait for a new one */
	       return -1;

	     /* bbftpd_cert_receive_connection could return 1 to indicate that /tmp will
	      * be used as the home directory.  Assume that the login was successful
	      */
	     bbftpd_msg_reply (MSG_OK, "bbftpd version %s : OK", VERSION);
	     return 0;
# endif				       /* CERTIFICATE_AUTH */

	   case MSG_LOG:
# ifdef CERTIFICATE_AUTH
	     if (server_config->accept_certs_only)
	       {
		  bbftpd_log(BBFTPD_ERR, "%s", "The server only accepts certificates") ;
		  reply (MSG_BAD_NO_RETRY, "The server only accepts certificates");
		  break;
	       }
# endif				       /* CERTIFICATE_AUTH */
	     /*
	      ** It seems that it is the message we were waiting
	      ** so lets decrypt 
	      */
	     if (-1 == bbftpd_loginsequence(&msg)) /* The login failed, do not wait for a new one */
	       return -1;

	     state = S_PROTWAIT ;
	     bbftpd_msg_reply (MSG_OK, "bbftpd version %s : OK", VERSION);
	     return 0;

	   case MSG_CRYPT:
# ifdef CERTIFICATE_AUTH
	     if (server_config->accept_certs_only)
	       {
		  reply(MSG_BAD_NO_RETRY, "The server only accepts certificates");
		  return -1;
	       }
# endif
	     if (server_config->force_encoding)
	       {
		  /*
		   **  The client can't encode and ask if it can send uncrypted
		   **  login information
		   */
		  reply (MSG_BAD_NO_RETRY, "The server requires encrypted login");
		  return -1;
	       }

	     reply(MSG_OK, "Uncrypted login dialog accepted");
	     /* continue with the loop */
	     break;
#else /* PRIVATE_AUTH */
	   case MSG_PRIV_LOG:
	     if ( bbftpd_private_receive_connection(msg.msglen) < 0 )
		return -1; /* The login failed, do not wait for a new one */

	     state = S_PROTWAIT ;
	     bbftpd_msg_reply (MSG_OK, "bbftpd version %s : OK", VERSION);
	     return 0;
#endif				       /* PRIVATE_AUTH */
	  }
     }
}



static int initialize_bbftpd_mode_daemon (Server_Config_Type *server_config)
{
   int s;

#ifdef CERTIFICATE_AUTH
   if ((server_config->accept_pass_only == 0)
       && (-1 == bbftpd_cert_get_gwf_cred ()))
     return -1;
#endif

   s = do_daemon(server_config->argc, server_config->argv, server_config->control_port);
   if (s == -1) return -1;

   incontrolsock = outcontrolsock = s;

   /* At this point,  we are the forked child from the parent process and a connection has been
    * accepted.  The incontrolsock/outcontrolsock values have changed but are still equal.
    */
   fatherpid = getpid() ;	       /* Update from previous value */

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
   bbftpd_rfio_enable_debug (server_config->debug);
#endif

   if (-1 == get_peer_and_sock_names (incontrolsock))
     return -1;

   if (-1 == perform_login (server_config))
     return -1;

   return 0;
}

static int initialize_bbftpd_mode_ssh (Server_Config_Type *server_config)
{
   (void) server_config;

  /*
   ** Get the username 
   */
   struct passwd *result ;
   if ( (result = getpwuid(getuid())) == NULL ) {
   bbftpd_log(BBFTPD_WARNING,"Error getting username") ;
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
   if (-1 == checkfromwhere (server_config->ask_remote_address, server_config->control_port))
     {
	reply (MSG_BAD, "Server exiting");
	bbftpd_log(BBFTPD_INFO,"User %s disconnected", currentusername);
	return -1;
     }

   bbftpd_log(BBFTPD_INFO,"bbftpd started by : %s from %s",currentusername,inet_ntoa(his_addr.sin_addr)) ;
   return 0;
}


/* Started from inetd */
static int initialize_bbftpd_mode_inetd (Server_Config_Type *server_config)
{
#ifdef CERTIFICATE_AUTH
   if ((server_config->accept_pass_only == 0)
       && (-1 == bbftpd_cert_get_gwf_cred ()))
     return -1;
#endif

   if (-1 == bbftpd_crypt_init_random ())
     return -1;

   incontrolsock = 0  ;
   outcontrolsock = incontrolsock ;
   state = S_PROTWAIT ;

   if (-1 == get_peer_and_sock_names (incontrolsock))
     return -1;

   if (-1 == perform_login (server_config))
     return -1;

   return 0;
}

int main (int argc, char **argv)
{
   Server_Config_Type server_config;
   struct  message msg ;
   char *logfile = NULL;
   int use_syslog = 1, log_level;

   if (is_option_present (argc, argv, 'v'))
     {
	print_version_info ();
	exit (0);
     }

   if (is_option_present (argc, argv, 's'))
     use_syslog = 0;

   if (is_option_present (argc, argv, 'L'))
     {
	logfile = optarg;
	use_syslog = 0;
     }

   log_level = BBFTPD_DEFAULT;
   if (is_option_present (argc, argv, 'l'))
     {
	int new_level = bbftpd_log_get_level (optarg);
	if (new_level != -1)
	  log_level = new_level;
     }

   bbftpd_log_open (use_syslog, log_level, logfile);

    /*
    ** Set the log mask to BBFTPD_EMERG (0) 
    */

    /*
    ** Initialize variables
    */
    protocolversion = 0 ;
#ifdef PORT_RANGE
    sscanf(PORT_RANGE,"%d:%d",&pasvport_min, &pasvport_max) ;
#endif

   if (-1 == check_libs ())
     exit (1);

   bbftpd_log(BBFTPD_DEBUG,"Starting bbftpd") ;

   if ((-1 == init_default_server_config (argc, argv, &server_config))
       || (-1 == process_cmdline_options (argc, argv, &server_config))
       || (-1 == process_bbftpd_rcfile (&server_config)))
     {
	bbftpd_log_close ();
	exit (1);
     }

   fatherpid = getpid() ;

   switch (server_config.server_mode)
     {
      case 0:
	if (-1 == initialize_bbftpd_mode_inetd (&server_config))
	  exit (1);
	break;

      case 1:
	if (-1 == initialize_bbftpd_mode_ssh (&server_config))
	  exit (1);
	break;

      case 2:
	if (-1 == initialize_bbftpd_mode_daemon (&server_config))
	  exit (1);
	break;
     }

   /*
    ** At this stage we are in the S_PROTWAIT state.
    * Determine the protocol version
    */
   state = S_PROTWAIT;
   if ((bbftpd_msg_pending (10) <= 0)
       || (-1 == bbftpd_msgread_msg (&msg)))
     exit (1);

   if (msg.code != MSG_PROT)
     {
#ifdef BBFTPD_V1_SUPPORT
	protocolversion = 1;
#else
	bbftpd_log (BBFTPD_ERR, "%s\n", "Protocol version 1 is not supported");
	bbftpd_log (MSG_BAD_NO_RETRY, "%s\n", "Protocol version 1 is not supported");
#endif
     }
   else	if (-1 == checkprotocol (&protocolversion))
     exit (1);

   bbftpd_log(BBFTPD_INFO,"Using bbftp protocol version %d", protocolversion) ;
   state = S_LOGGED ;

   switch (protocolversion)
     {
#ifdef BBFTPD_V1_SUPPORT
      case 1:
	(void) bbftp_run_protocol_1 (&msg);
	break;
#endif
      case 2:
	(void) bbftp_run_protocol_2 ();
	break;

      case 3:
	(void) bbftp_run_protocol_3 ();
	break;

      default:
	bbftpd_log(BBFTPD_ERR,"Unsupported protocol version %d", protocolversion) ;
	exit (1);
     }

   clean_child() ;
   exit_clean ();

   return 0;
}

void clean_child (void)
{
    int    *pidfree ;
    int    i ;

    if ( protocolversion == 0 ) return ;
    if ( protocolversion == 1 ) {
        if ( killdone == 0 ) {
            killdone = 1 ;
            for ( i=0 ; i<MAXPORT ; i++) {
                if ( pid_child[i] != 0 ) {
                    bbftpd_log(BBFTPD_DEBUG,"Killing child %d",pid_child[i]) ;
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
                    bbftpd_log(BBFTPD_DEBUG,"Killing child %d",*pidfree) ;
                    kill(*pidfree,SIGKILL) ;
                }
                pidfree++ ;
            }
        }
        return ;
    }
}

void exit_clean (void)
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
            bbftpd_log(BBFTPD_INFO,"User %s disconnected",currentusername) ;
            return ;
        }
        default :{
            return ;
        }
    }
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

