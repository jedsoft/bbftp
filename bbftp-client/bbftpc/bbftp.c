/*
 * bbftpc/bbftp.c
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


  
  
 bbftp.c  v 2.0.0  2001/03/01   - Complete rewriting for version 2.0.0
          v 2.0.1  2001/04/19   - Correct indentation 
                                - Verify if -s is present on shhremotecmd
                                  and add it if not (in order to fit v 1.9.4-tja1
                                  behaviour)
          v 2.1.0 2001/05/21    - Add debug
                                - Add -m option to have special output
                                - Set -v before all options
                                - Correct case where the last line in control
                                  file has no CR
          v 2.1.2 2001/11/19    - Fix COS 0 case
          v 2.2.0 2001/10/03    - Add certificate authentication mode
	  v 2.2.1 2013/02/01	- Fix implicit declaration issues in 64 bits 

*****************************************************************************/
#include <bbftp.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h> 
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
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

#include <getopt.h>

#include <client.h>
#include <client_proto.h>
#include <bbftp_turl.h>
#include <common.h>
#include <config.h>
#include <structures.h>
#include <version.h>

#ifdef WITH_SSL
#include <openssl/rsa.h>
#endif

#ifdef WITH_GZIP
# include <zlib.h>
#endif

#define SETTOZERO    0
#define SETTOONE     1

#define SSHREMOTECMD "bbftpd -s"
#define SSHCMD "ssh -q"

#ifdef PRIVATE_AUTH
#define OPTIONS "qbcde:f:i:l:mno:p:P:r:R:tu:vVw:WD::"
#else
#define OPTIONS "qbcde:E:f:g:i:I:l:L:mno:p:r:R:sStu:vVw:WD::"
#endif
/*
#endif
*/

int     state   = SETTOZERO ;
/*
** timestamp:
**      Every message on standart error and standart output are
**      timestamped
*/
int     timestamp = SETTOZERO ;
/*
** protocolmin:
**      Minimum protocol supportted
*/
int     protocolmin = 2 ; 
int     protocolmax = 3 ; 
int     protocol ;
/*
** debug:
**      Set to one to print more debugging information
*/
int     debug = SETTOZERO ;
/*
** verbose:
**      Set to one to print  information
*/
int     verbose = SETTOZERO ;
/*
** warning:
**      Set to one to print warning to stderr
*/
int     warning = SETTOZERO ;
/*
** statoutput:
**      Set to one for special output
*/
int     statoutput = SETTOZERO ;
/*
** globaltrymax:
**      Number of try in case or recoverable error
*/
int     globaltrymax = NBTRYMAX ;
/*
** newcontrolport:
**      Control port to be used 
*/
int     newcontrolport = CONTROLPORT ;
/*
** usessh:
**      Set to one when using ssh to start the remote daemon
*/
int     usessh = SETTOZERO ;
/*
** sshbatchmode:
**      This is set to non-zero if running in batch mode (that is, password
**      and passphrase queries are not allowed). 
*/
int     sshbatchmode  = SETTOZERO ;
/*
** sshchildpid:
**      To keep the ssh child pid
*/
int     sshchildpid   = SETTOZERO ;
/*
** For ssh
*/
char    *sshidentityfile = NULL ;
char    *sshremotecmd = NULL ;
char    *sshcmd = NULL ;
/*
** usecert:
**		Set to one if using certificate authentifaction
*/
int		usecert = SETTOZERO ;
/*
** useprivate:
**      Set to one if using private authentication
*/
int     useprivate = SETTOZERO ;
/*
** privatestr:
**      Pointer to a private string used for private authentication
**
*/
char    *privatestr = NULL ;
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
/*
** bbftprc:
**      Where to store the bbftprc file
*/
char    *bbftprc = NULL ;
/*
** Variable defining the local options :
** 
** localcos:
**      Value of the local class of service (in case of RFIO ability)
**
** localumask:
**      Local umask taken by the umask command at start and
**      modified by the setlocalumask command
**
** localrfio:
**      set to one when using rfio for local files
**
*/
int     localumask ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
int     localcos  = SETTOZERO ;
#endif

char *newcmd = NULL;
int remoterfio;
int localrfio;
/*
** Variables defining both side options :
**
** usetmpfile:
**      Set to one when using tmpname for file creation
**
** usegzipcompress:
**      Set to one when using tmpname for file creation
**
** keepaccess:
**      Set to one when keeping access time and modification
**
** keepmode:
**      Set to one when keeping file mode
**
** creatdir:
**      Set to one when automatic directory creation is needed
*/
int     sendwinsize     = 256 ;
int     recvwinsize     = 256 ;
int     nbport = 1 ;
int		ackto			= ACKTO;
int		recvcontrolto	= CONTROLSOCKTO;
int		sendcontrolto	= SENDCONTROLTO;
int		datato			= DATASOCKTO;

/*
** Variables remote side options :
**
** remoterfio:
**      Set to one when rfio for remote file
**
** remoteumask:
**      if set to zero do not set remote umask
**
** remotecos:
**      if not set to zero do set remote cos
**
** remotedir :
**      if not set to NULL change dir after connection
**
*/
int     remoteumask = SETTOZERO ;
int     remotecos   = -1 ;
char    *remotedir = NULL ;
/*
** incontrolsock :
**      Define the control socket for reading
** outcontrolsock :
**      Define the control socket for writing
**
**      For normal use : incontrolsock = outcontrolsock
*/
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
#ifdef CERTIFICATE_AUTH
char    *service    = NULL ;
#endif
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
** castfd:
**      CASTOR file descriptor
*/
#ifdef CASTOR
int     castfd = -1 ;
char    *castfilename = NULL ;
#endif

/*
** Parameters describing the transfer ********************
*/
int     transferoption = TROPT_TMP | TROPT_DIR | TROPT_MODE | TROPT_ACC; 
int     filemode ;
char    lastaccess[9] ;
char    lastmodif[9] ;
int     buffersizeperstream = 256 ;
int     requestedstreamnumber ;
my64_t  filesize ;
/*
** curfilename :
**      Define the pointer to the current file
*/
char    *curfilename = NULL ;
/*
** realfilename :
**      Define the pointer to the real file (= curfilename if TROPT_TMP not
**      set)
*/
char    *realfilename   = NULL ;
int     *myports        = NULL ;
int     *mysockets      = NULL ;
char    *readbuffer     = NULL ;
char    *compbuffer     = NULL ; 
int     resfd = -1 ;
/*
** Simulation mode (option -n)
*/
int		simulation_mode = SETTOZERO;
/*
**
*/
int connectionisbroken = SETTOZERO ;

/*
 * Range for the ephemeral ports for data connections
 */
int     pasvport_min = 0 ;
int     pasvport_max = 0 ;

typedef struct cmd_list_st {
	char *cmd;
	struct cmd_list_st *next;
} cmd_list;
cmd_list *commandList = NULL;
cmd_list *first = NULL;
cmd_list *iterator = NULL;

static void addCommand(char *lnewcmd) {
    cmd_list *newCommand = NULL;
    if ((newCommand  = (cmd_list *)malloc(sizeof(cmd_list))) == NULL) {
           printmessage(stderr,CASE_FATAL_ERROR,23,timestamp,"Unable to malloc memory for command list : %s\n",strerror(errno)) ;
    }
    if ((newCommand->cmd = (char *) malloc( strlen(lnewcmd) + 1)) == NULL) {
	   printmessage(stderr,CASE_FATAL_ERROR,23,timestamp,"Unable to malloc memory for command : %s\n",strerror(errno)) ;
    }
    strcpy(newCommand->cmd, lnewcmd);
    newCommand->next = NULL;
    if (iterator == NULL) {
	    iterator = newCommand;
	    first = newCommand;
    } else {
	    iterator->next = newCommand;
	    iterator = iterator->next;
    }
}

int main(int argc, char **argv)
{
    /* extern char *optarg; */
    /* extern int optind, opterr, optopt; */
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
   unsigned int uj;
    int     stderrfd ;
    int     stdoutfd ;
    int     infd ;
    char    calcmaxline[1] ;
   unsigned int     maxlen ;
    int     lengthread ;
    char    *buffercmd ;
    int     alluse ;
    unsigned int ualluse ;
    char    *tmpsshremotecmd ;
#ifdef PRIVATE_AUTH
    char    logmessage[1024] ;
#endif
    char    minbuffer[MINMESSLEN] ;
    struct  message *msg ;
/*
** Get local umask 
*/
    localumask = umask(0) ;
/*
** and reset it to the correct value
*/
    umask(localumask) ;
/*
** First check for timestamp in order to have a common output
*/
    opterr = 0 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 't' :{
                timestamp = SETTOONE ;
                break ;
            }
        }
    }

#ifdef PRIVATE_AUTH
    useprivate = SETTOONE ;
    usessh = SETTOZERO ;
#endif

/*
** Check for -v option
*/
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'v' :{
                printmessage(stdout,CASE_NORMAL,0,timestamp,"bbftp version %s\n",VERSION) ;
                printmessage(stdout,CASE_NORMAL,0,timestamp,"Compiled with  :   default port %d\n",CONTROLPORT) ;
#ifdef PORT_RANGE
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   data ports range = %s \n", PORT_RANGE) ;
#endif
#ifdef WITH_GZIP
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   compression with Zlib-%s\n", zlibVersion()) ;
#endif
#ifdef WITH_SSL
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   encryption with %s \n",SSLeay_version(SSLEAY_VERSION)) ;
#endif
#ifdef WITH_RFIO
# ifdef CASTOR
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   CASTOR support (RFIO)\n") ;
# else
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   HPSS support (RFIO)\n") ;
# endif
#endif
#ifdef WITH_RFIO64
# ifdef CASTOR
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   CASTOR support (RFIO64)\n") ;
# else
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   HPSS support (RFIO64)\n") ;
# endif
#endif
#ifdef AFS
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   AFS authentication \n") ;
#endif
#ifdef PRIVATE_AUTH
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   private authentication \n") ;
#else
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   default ssh command = %s \n",SSHCMD) ;
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   default ssh remote command = %s \n",SSHREMOTECMD) ;
# ifdef CERTIFICATE_AUTH
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   GSI authentication\n") ;
# endif
#endif
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   default number of tries = %d  \n",NBTRYMAX) ;
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   default sendwinsize = %d Kbytes\n",sendwinsize) ;
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   default recvwinsize = %d Kbytes\n",recvwinsize) ;
                printmessage(stdout,CASE_NORMAL,0,timestamp,"                   default number of stream = %d \n",nbport) ;
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
            case 'f' :{
                errorfile = optarg ;
                break ;
            }
        }
    }
    if ( errorfile != NULL ) {
#ifdef DARWIN
		if ( (stderrfd = open(errorfile,O_CREAT|O_WRONLY|O_TRUNC,0777)) < 0 ) {
#else
        if ( (stderrfd = open(errorfile,O_CREAT|O_WRONLY|O_SYNC|O_TRUNC,0777)) < 0 ) {
#endif
     		printmessage(stderr,CASE_FATAL_ERROR,10,timestamp,"Error openning error file (%s) : %s\n",errorfile,strerror(errno)) ;
        }
        close(STDERR_FILENO);
        if ( fcntl(stderrfd,F_DUPFD,STDERR_FILENO) != STDERR_FILENO ) {
            printmessage(stderr,CASE_FATAL_ERROR,11,timestamp,"Error dup on error file (%s) : %s\n",errorfile,strerror(errno)) ;
        }
    }
/*
** Check for stdout replacement
*/
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'o' :{
                outputfile = optarg ;
                break ;
            }
        }
    }
    if ( outputfile != NULL ) {
#ifdef DARWIN
		if ( (stdoutfd = open(outputfile,O_CREAT|O_WRONLY|O_TRUNC,0777)) < 0 ) {
#else
        if ( (stdoutfd = open(outputfile,O_CREAT|O_WRONLY|O_SYNC|O_TRUNC,0777)) < 0 ) {
#endif
     		printmessage(stderr,CASE_FATAL_ERROR,12,timestamp,"Error openning output file (%s) : %s\n",outputfile,strerror(errno)) ;
        }
        close(STDOUT_FILENO);
        if ( fcntl(stdoutfd,F_DUPFD,STDOUT_FILENO) != STDOUT_FILENO ) {
            printmessage(stderr,CASE_FATAL_ERROR,13,timestamp,"Error dup on output file (%s) : %s\n",outputfile,strerror(errno)) ;
        }
    }

/*
** Block all signals , the routine will exit in case of error
*/
    blockallsignals() ;

/*
** Now all the others
*/
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'b' :{
                background = SETTOONE ;
                break ;
            }
            case 'c' :{
#ifdef WITH_GZIP                
                transferoption = transferoption | TROPT_GZIP ;
#else
                printmessage(stderr,CASE_FATAL_ERROR,7,timestamp,"option -c is not available: bbftp was built without compression utility\n") ;
#endif                
                break ;
            }
            case 'd' :{
                debug = 1 ;
                break ;
            }
            case 'D' :{         
                if (optarg) {
                    if ((sscanf(optarg,"%d:%d",&i, &k) == 2) && (i < k)) {
                        pasvport_min = i; pasvport_max = k;
                    } else {
                        printmessage(stderr,CASE_FATAL_ERROR,4,timestamp,"Invalid port range : %s\n",optarg) ;
                    }
                } else {
#ifdef PORT_RANGE
                     sscanf(PORT_RANGE,"%d:%d",&pasvport_min, &pasvport_max) ;
#endif
                     if (0 == pasvport_max) {
                         pasvport_min = 0;
                         pasvport_max = 1;
                     }
                }
		protocolmax = 2 ;
                break ;
            }
            case 'e' : {
                bbftpcmd = optarg ;
                break ;
            }
            case 'E' : {
                sshremotecmd = optarg ;
                usessh = 1 ;
                break ;
            }
            case 'f' :{
                errorfile = optarg ;
                break ;
            }
#ifdef CERTIFICATE_AUTH
            case 'g' :{
                service = optarg ;
                break ;
            }
#endif         
            case 'i' :{
                inputfile = optarg ;
                if ( stat (inputfile,&statbuf) < 0 ) {
                    printmessage(stderr,CASE_FATAL_ERROR,7,timestamp,"Input file (%s) cannot be stated\n",inputfile) ;
                }
                if ( (resultfile = (char *) malloc (strlen(inputfile) + 5 )) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,1,timestamp,"Cannot malloc space for result file name\n") ;
                }
                strcpy(resultfile,inputfile) ;
                strcat(resultfile,".res") ;
                break ;
            }
            case 'I' :{
                sshidentityfile = optarg ;
                usessh = 1 ;
                /*
                ** Check if file exists
                */
                if ( stat (sshidentityfile,&statbuf) < 0 ) {
                    printmessage(stderr,CASE_FATAL_ERROR,5,timestamp,"SSH identity file (%s) cannot be stated\n",sshidentityfile) ;
                }
                break ;
            }
            case 'L' : {
                sshcmd = optarg ;
                usessh = 1 ;
                break ;
            }
            case 'm' :{
                statoutput = SETTOONE ;
                break ;
            }
            case 'n':{
                simulation_mode = SETTOONE ;
                break ;
            }
            case 'o' :{
                outputfile = optarg ;
                break ;
            }
            case 'P' :{
                privatestr = optarg ;
                break ;
            }
            case 'q' :{
                transferoption = transferoption | TROPT_QBSS ;
				break ;
			}
            case 'p' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse < 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,3,timestamp,"Number of streams must be numeric and > 0\n") ;
                }
                nbport = alluse ;
                break ;
            }
            case 'r' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,4,timestamp,"Number of tries must be numeric > 0\n") ;
                }
                globaltrymax = alluse ;
                break ;
            }
            case 'R' :{
                bbftprcfile = optarg ;
                break ;
            }
            case 's' :{
                usessh = SETTOONE ;
                break ;
            }
            case 'S' :{
                usessh = SETTOONE ;
                sshbatchmode = SETTOONE ;
                break ;
            }
            case 't' :{
                timestamp = SETTOONE ;
                break ;
            }
            case 'u' :{
                username = optarg ;
                break ;
            }
            case 'V':{
                verbose = SETTOONE ;
                break ;
            }
            case 'w' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,4,timestamp,"Control port must be numeric\n") ;
                }
                newcontrolport = alluse ;
                break ;
            }
            case 'W':{
                warning = SETTOONE ;
                break ;
            }
            default : {
                Usage() ;
                printmessage(stderr,CASE_FATAL_ERROR,6,timestamp,"Error on command line (unsupported option -%c)\n",optopt) ;
            }
        }
    }
    /*
    ** Get 5K for castfilename in order to work with CASTOR
    ** software (even if we are not using it)
    */
#ifdef CASTOR
    if ( (castfilename = (char *) malloc (5000)) == NULL ) {
        /*
        ** Starting badly if we are unable to malloc 5K
        */
        printmessage(stderr,CASE_FATAL_ERROR,10,timestamp,"No memory for CASTOR : %s\n",strerror(errno)) ;
    }
#endif

    /*
    ** Reset all outputs variable if statoutput is set to one
    */
    if ( statoutput ) {
        debug     = SETTOZERO ;
        verbose   = SETTOZERO ;
        warning   = SETTOZERO ;
        timestamp = SETTOZERO ;
    }
/*
#ifdef PRIVATE_AUTH
    useprivate = SETTOONE ;
    usessh = SETTOZERO ;
#endif

#ifdef CERTIFICATE_AUTH
	usecert = SETTOONE ;
    useprivate = SETTOZERO ;
    usessh = SETTOZERO ;
#endif
*/	        
 
/*
** Check all ssh stuff
*/
    if ( usessh) {
        if ( sshremotecmd == NULL ) {
            /*
            ** Malloc space and set the default
            */
            if ( (sshremotecmd = (char *) malloc (strlen(SSHREMOTECMD)+1) ) == NULL ) {
                printmessage(stderr,CASE_FATAL_ERROR,8,timestamp,"Cannot malloc space for ssh remote cmd\n") ;
            }
            strcpy(sshremotecmd,SSHREMOTECMD) ;
        } else {
            /*
            ** Verify if -s is present if not add it (in order to fit v 1.9.4-tja1
            ** behaviour)
            */
            if ( strstr(sshremotecmd," -s") == NULL ) {
                if ( ( tmpsshremotecmd = (char *) malloc (strlen(sshremotecmd) + 4) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,8,timestamp,"Cannot malloc space for ssh remote cmd\n") ;
                }
                sprintf(tmpsshremotecmd,"%s -s",sshremotecmd) ;
                sshremotecmd = tmpsshremotecmd ;
            }
        }
        if ( sshcmd == NULL ) {
            if ( (sshcmd = (char *) malloc (strlen(SSHCMD)+1) ) == NULL ) {
                  printmessage(stderr,CASE_FATAL_ERROR,9,timestamp,"Cannot malloc space for ssh cmd\n") ;
            }
            strcpy(sshcmd,SSHCMD) ;
        }
    }
/*
** Check for the local user in order to find the .bbftprc file
*/
    if ( bbftprcfile == NULL ) {
        /*
        ** look for the local user in order to find the .bbftprc file
        */
        if ( (mypasswd = getpwuid(getuid())) == NULL ) {
            if ( warning ) printmessage(stderr,CASE_WARNING,6,timestamp,"Unable to get passwd entry, using %s\n", BBFTP_CONF) ;
            if ( (bbftprcfile = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
	      strcpy(bbftprcfile,BBFTP_CONF);
	    }
        } else if ( mypasswd->pw_dir == NULL ) {
            if ( warning ) printmessage(stderr,CASE_WARNING,7,timestamp,"No home directory, using %s\n", BBFTP_CONF) ;
            if ( (bbftprcfile = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
	      strcpy(bbftprcfile,BBFTP_CONF);
	    }
        } else if ( (bbftprcfile = (char *) malloc (strlen(mypasswd->pw_dir)+10) ) == NULL ) {
            if ( warning ) printmessage(stderr,CASE_WARNING,8,timestamp,"Error allocationg space for bbftprc file name, .bbftprc will not be used\n") ;
        } else {
            strcpy(bbftprcfile,mypasswd->pw_dir) ;
            strcat(bbftprcfile,"/.bbftprc") ;
            if ( stat(bbftprcfile,&statbuf) < 0  ) {
                bbftprcfile = NULL;
                if ( (bbftprcfile = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
	          strcpy(bbftprcfile,BBFTP_CONF);
	        }
	    }
        }
    }
    if ( bbftprcfile != NULL ) {
        if ( strncmp(bbftprcfile,"none",4) != 0 ) {
            /*
            ** Stat the file in order to get the length
            */
            if ( stat(bbftprcfile,&statbuf) < 0  ) {
                if ( warning ) printmessage(stderr,CASE_WARNING,9,timestamp,"Error stating bbftprc file (%s)\n",bbftprcfile) ;
            } else if ( statbuf.st_size == 0 ) {
                /*
                ** do nothing 
                */
            } else if ( (bbftprc = (char *) malloc (statbuf.st_size + 1 ) ) == NULL ) {
                if ( warning ) printmessage(stderr,CASE_WARNING,10,timestamp,"Error allocation memory for bbftprc, .bbftprc will not be used\n") ;
            } else if ( ( fd  = open (bbftprcfile,O_RDONLY) )  < 0 ) {
                if ( warning ) printmessage(stderr,CASE_WARNING,11,timestamp,"Error openning .bbftprc file (%s) : %s \n",bbftprcfile,strerror(errno)) ;
            } else if ( ( j = read( fd, bbftprc , statbuf.st_size )) != statbuf.st_size ) {
                if ( warning ) printmessage(stderr,CASE_WARNING,12,timestamp,"Error reading .bbftprc file (%s)\n",bbftprcfile) ;
            } else {
                bbftprc[j] = '\0' ;
            }
        }
    }
/*
** Analyse the bbftprc command in order to supress forbiden command
** Allowed commands are :
**          setbuffersize %d
**          setlocalcos %d
**          setremotecos %d
**          setlocalumask %ooo
**          setremoteumask %ooo
**          setsendwinsize %d
**          setrecvwinsize %d
**          setnbstream %d
**          setoption [ [no]createdir | [no]tmpfile | [no]remoterfio | [no]localrfio | [no]keepmode |[no]keepaccess |[no]gzip]
**
*/
    if ( bbftprc != NULL ) {
        carret = bbftprc ;
        startcmd = bbftprc ;
        /*
        ** Strip starting CR
        */
        while (1) {
            while ( *carret == 10 || *carret == ' ' ) carret++ ;
            startcmd = carret ;
            carret = (char *) strchr (carret, 10);
            if ( carret == NULL ) break ;
            *carret = '\0' ;
            if ( !strncmp(startcmd,"setbuffersize",13)) {
                retcode = sscanf(startcmd,"setbuffersize %d",&alluse) ;
                if ( retcode != 1 || alluse < 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,5,timestamp,"Buffersize in bbftprc file must be numeric > 0\n") ;
                } else {
                    buffersizeperstream = alluse ;
                }
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
            } else if ( !strncmp(startcmd,"setlocalcos",11)) {
                retcode = sscanf(startcmd,"setlocalcos %d",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,14,timestamp,"Local COS in bbftprc file must be numeric\n") ;
                } else {
                    localcos = alluse ;
                }
#endif
            } else if (!strncmp(startcmd,"setremotecos",12)) {
                retcode = sscanf(startcmd,"setremotecos %d",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,15,timestamp,"Remote COS in bbftprc file must be numeric\n") ;
                }else {
                    remotecos = alluse ;
                }
            } else if (!strncmp(startcmd,"setlocalumask",13)) {
                retcode = sscanf(startcmd,"setlocalumask %o",&ualluse) ;
                if ( retcode != 1) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,16,timestamp,"Local umask in bbftprc file must be octal numeric\n") ;
                } else {
                    localumask = ualluse ;
                    umask(localumask) ;
                }
            } else if (!strncmp(startcmd,"setremoteumask",14)) {
                retcode = sscanf(startcmd,"setremoteumask %o",&ualluse) ;
                if ( retcode != 1) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,17,timestamp,"Remote umask in bbftprc file must be octal numeric\n") ;
                }else {
                    remoteumask = ualluse ;
                    umask(localumask) ;
                }
            } else if (!strncmp(startcmd,"setsendwinsize",14)) {
                retcode = sscanf(startcmd,"setsendwinsize %d",&alluse) ;
                if ( retcode != 1 || alluse < 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,18,timestamp,"Send window size in bbftprc file must be numeric\n") ;
                } else {
                    sendwinsize = alluse ;
                }
            } else if (!strncmp(startcmd,"setrecvwinsize",14)) {
                retcode = sscanf(startcmd,"setrecvwinsize %d",&alluse) ;
                if ( retcode != 1 || alluse < 0  ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,19,timestamp,"Receive window size in bbftprc file must be numeric\n") ;
                }else {
                    recvwinsize = alluse ;
                }
            } else if (!strncmp(startcmd,"setnbstream",11)) {
                retcode = sscanf(startcmd,"setnbstream %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,19,timestamp,"Number of streams in bbftprc file must be numeric and > 0\n") ;
                } else {
                    nbport = alluse ;
                }
            } else if (!strncmp(startcmd,"setackto",8)) {
                retcode = sscanf(startcmd,"setackto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,19,timestamp,"Acknowledge timeout must be numeric and > 0\n") ;
                } else {
                    ackto = alluse ;
                }
            } else if (!strncmp(startcmd,"setrecvcontrolto",16)) {
                retcode = sscanf(startcmd,"setrecvcontrolto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,19,timestamp,"Input control timeout must be numeric and > 0\n") ;
                } else {
                    recvcontrolto = alluse ;
                }
            } else if (!strncmp(startcmd,"setsendcontrolto",16)) {
                retcode = sscanf(startcmd,"setsendcontrolto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,19,timestamp,"Output control timeout must be numeric and > 0\n") ;
                } else {
                    sendcontrolto = alluse ;
                }
            } else if (!strncmp(startcmd,"setdatato",9)) {
                retcode = sscanf(startcmd,"setdatato %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( warning ) printmessage(stderr,CASE_WARNING,19,timestamp,"Data timeout must be numeric and > 0\n") ;
                } else {
                    datato = alluse ;
                }
            } else if (!strncmp(startcmd,"setoption",9)) {
                /*
                ** look for the option
                */
                startcmd = startcmd + 9 ;
                while (*startcmd == ' ' && *startcmd != '\0' ) startcmd++ ;
                if ( !strncmp(startcmd,"no",2) ) {
                    nooption = SETTOONE ;
                    startcmd = startcmd+2 ;
                } else {
                    nooption = SETTOZERO ;
                }
                if ( !strncmp(startcmd,"createdir",9) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_DIR ;
                    } else {
                        transferoption = transferoption | TROPT_DIR ;
                   }
                } else if ( !strncmp(startcmd,"tmpfile",7) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_TMP ;
                    } else {
                        transferoption = transferoption | TROPT_TMP ;
                   }
                } else if ( !strncmp(startcmd,"remoterfio",10) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_RFIO ;
                    } else {
                        transferoption = transferoption | TROPT_RFIO ;
			if ( remotecos == -1 ) remotecos = 0;
                    }
                } else if ( !strncmp(startcmd,"localrfio",9) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_RFIO_O ;
                    } else {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
                        transferoption = transferoption | TROPT_RFIO_O ;
#else
                        if (warning) printmessage(stderr,CASE_WARNING,20,timestamp,"Incorrect command : setoption localrfio (RFIO not supported)\n", "");
#endif
                    }
                } else if ( !strncmp(startcmd,"keepmode",8) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_MODE ;
                    } else {
                        transferoption = transferoption | TROPT_MODE ;
                    }
                } else if ( !strncmp(startcmd,"keepaccess",10) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_ACC ;
                    } else {
                        transferoption = transferoption | TROPT_ACC ;
                    }
                } else if ( !strncmp(startcmd,"gzip",4) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_GZIP ;
                    } else {
#ifdef WITH_GZIP
                        transferoption = transferoption | TROPT_GZIP ;
#else
                        printmessage(stderr,CASE_FATAL_ERROR,7,timestamp,"gzip option is not available: bbftp was built without compression support\n") ;
#endif
                    }
                } else if ( !strncmp(startcmd,"qbss",4) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_QBSS ;
                    } else {
                        transferoption = transferoption | TROPT_QBSS ;
                    }
                } else {
                    if ( warning ) printmessage(stderr,CASE_WARNING,20,timestamp,"Unkown option in .bbftprc file (%s)\n",startcmd) ;
                }
            } else {
                if ( warning ) printmessage(stderr,CASE_WARNING,13,timestamp,"Unkown command in .bbftprc file (%s)\n",startcmd) ;
            }
            carret++ ;
        }
    }
/*
** Check for input file or command line
*/
    if ( inputfile == NULL && bbftpcmd == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,20,timestamp,"Error on command line (-i or -e option are mandatory)\n") ;
    }
/*
** Open the input file if needed
*/
    if ( inputfile != NULL ) {
        if ( (infd = open(inputfile,O_RDONLY)) < 0 ) {
            printmessage(stderr,CASE_FATAL_ERROR,21,timestamp,"Error opening input file (%s) : %s\n",inputfile,strerror(errno)) ;
        }
#ifdef DARWIN
		if ( (resfd = open(resultfile,O_CREAT|O_WRONLY|O_TRUNC,0777)) < 0 )
#else
        if ( (resfd = open(resultfile,O_CREAT|O_WRONLY|O_SYNC|O_TRUNC,0777)) < 0 )
#endif
	   {
     		printmessage(stderr,CASE_FATAL_ERROR,22,timestamp,"Error opening result file (%s) : %s\n",resultfile,strerror(errno)) ;
        }
        /*
        ** Now calc the max line of the input file in order to malloc 
        ** the buffer
        */
        maxlen = 0 ;
        uj = 0 ;
        while ( (lengthread = read (infd,calcmaxline,1)) == 1 ) {
            if ( calcmaxline[0] == 10 ) {
                if ( uj > maxlen ) {
                    maxlen = uj  ;
                }
                uj = 0 ;
            } else {
                uj++ ;
            }
        }
        if ( lengthread < 0 ) {
            printmessage(stderr,CASE_FATAL_ERROR,24,timestamp,"Error reading input file (%s) : %s\n",inputfile,strerror(errno)) ;
        } else if ( lengthread == 0 ) {
            if ( uj > maxlen ) {
                maxlen = uj  ;
            }
        }
       /*
        ** Reset the file at start position
        */
        lseek(infd,0,SEEK_SET) ;
        if ( (buffercmd = (char *) malloc (maxlen+2) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,23,timestamp,"Unable to malloc memory for command : %s\n",strerror(errno)) ;
        }
    } else {
        if ( (buffercmd = (char *) malloc (strlen(bbftpcmd)+1)) == NULL ) {
             printmessage(stderr,CASE_FATAL_ERROR,23,timestamp,"Unable to malloc memory for command : %s\n",strerror(errno)) ;
         }
    }

/*
** Check hostname
*/          
    if ( optind == argc-1 ) {
        hostname= argv[optind] ;
    } else {
	/*
	 * Maybe a TURL format: get hostname from command
	 */
        maxlen = 0 ;
        retcode = 0 ;
        while (retcode == 0 ) {
            if ( inputfile != NULL ) {
                j = 0 ;
                while ( (lengthread = read (infd,&buffercmd[j],1)) == 1 ) {
                    if ( buffercmd[j] == 10 ) {
                        buffercmd[j] = 0 ;
                        break ;
                    } else {
                        j++ ;
                    }
                }
                if ( lengthread < 0 ) {
                    printmessage(stderr,CASE_FATAL_ERROR,24,timestamp,"Error reading input file (%s) : %s\n",inputfile,strerror(errno)) ;
                } else if ( lengthread == 0 ) {
                    if ( j == 0 ) {
                        retcode = 1 ;
                        break ;
                    } else {
                        buffercmd[j+1] = 0 ;
                    }
                }
            } else {
                /*
                ** the command may be under the form cmd ; cmd ; cmd....
                */
                j = 0 ;
                while ((maxlen < strlen(bbftpcmd)) && (bbftpcmd[maxlen] == ' ')) maxlen++ ;
                while ( (maxlen < strlen(bbftpcmd)) && (bbftpcmd[maxlen] != ';') ) {
                    buffercmd[j] =  bbftpcmd[maxlen] ;
                    j++ ;
                    maxlen++ ;
                }
                if ( maxlen == strlen(bbftpcmd) ) retcode = 1 ;
                maxlen++ ;
                buffercmd[j] = 0 ;
	    }
            if ( translatecommand(buffercmd, &newcmd, &hostname, &newcontrolport, &remoterfio, &localrfio) == 0 ) {
		    if (localrfio == 1 && ((transferoption & TROPT_RFIO_O)!=TROPT_RFIO_O) ) {
	                addCommand("setoption localrfio");
			transferoption = transferoption | TROPT_RFIO_O ;
	            } else if (localrfio == 0 && ((transferoption & TROPT_RFIO_O)==TROPT_RFIO_O) ) {
			addCommand("setoption nolocalrfio");
			transferoption = transferoption & ~TROPT_RFIO_O ;
		    }
		    if (remoterfio == 1 && ((transferoption & TROPT_RFIO)!=TROPT_RFIO) ) {
	                addCommand("setoption remoterfio");
			if ( remotecos == -1 ) remotecos = 0 ;
			transferoption = transferoption | TROPT_RFIO ;
	            } else if (remoterfio == 0 && ((transferoption & TROPT_RFIO)==TROPT_RFIO) ) {
			addCommand("setoption noremoterfio");
			transferoption = transferoption & ~TROPT_RFIO ;
		    }
		    addCommand(newcmd);
            } else {
		    printmessage(stderr,CASE_FATAL_ERROR,24,timestamp,"Unable to parse command  : %s\n",buffercmd) ;
            }
	}
	commandList = first;
    }
    if (hostname == NULL || strlen(hostname) == 0) {
        Usage() ;
        printmessage(stderr,CASE_FATAL_ERROR,14,timestamp,"No hostname on command line\n") ;
    }    
/*
** Check if hostname is in numeric format 
*/
    for (uj=0 ; uj < strlen(hostname) ; uj++) {
        if ( isalpha(hostname[uj]) ) {
            /*
            ** One alpha caractere means no numeric form
            */
            hosttype = 1 ;
            break ;
        } else if ( isdigit(hostname[uj]) ) {
        } else if ( hostname[uj] == '.' ) {
        } else {
            printmessage(stderr,CASE_FATAL_ERROR,15,timestamp,"Invalid hostname (%s)\n",hostname) ;
        }
    }
    if ( hosttype == 0 ) {
       /*
       ** Numeric format 
       */
        hisctladdr.sin_addr.s_addr = 0 ;
        hisctladdr.sin_addr.s_addr = inet_addr(hostname) ;
        if (hisctladdr.sin_addr.s_addr == INADDR_NONE ) {
            printmessage(stderr,CASE_FATAL_ERROR,16,timestamp,"Invalid IP address (%s)\n",hostname) ;
        }
        calchostname = (char *)inet_ntoa(hisctladdr.sin_addr) ;
        if ( strcmp(hostname,calchostname) ) {
            printmessage(stderr,CASE_FATAL_ERROR,16,timestamp,"Invalid IP address (%s)\n",hostname) ;
        }
    } else {
       /*
       ** Alpha format 
       */
        if ( (hp = gethostbyname((char *)hostname) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,17,timestamp,"Hostname no found (%s)\n",hostname) ;
        } else {
            if (hp->h_length > (int)sizeof(hisctladdr.sin_addr)) {
                hp->h_length = sizeof(hisctladdr.sin_addr);
            }
            memcpy(&hisctladdr.sin_addr, hp->h_addr_list[0], hp->h_length) ;
        }
    }
/*
** Check username if not in	certificate authentication mode
*/          
    if ( username == NULL ) {
#ifdef CERTIFICATE_AUTH
        if (usessh) {
/*            Usage() ;
            printmessage(stderr,CASE_FATAL_ERROR,18,timestamp,"No username given\n") ;*/
        } else {
            usecert = SETTOONE;
        }
#else        
	if (!usessh) {
            Usage() ;
            printmessage(stderr,CASE_FATAL_ERROR,18,timestamp,"No username given\n") ;
	}
#endif
    }
    if ( debug ) {
        if (simulation_mode) {
            printmessage(stdout,CASE_NORMAL,0,timestamp,"** SIMULATION MODE: No data written **\n") ;
        }
        printmessage(stdout,CASE_NORMAL,0,timestamp,"Starting parameters -----------------\n") ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"number of tries   = %d\n",globaltrymax) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"number of streams = %d\n",nbport) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"localumask        = %03o\n",localumask) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"remoteumask       = %03o\n",remoteumask) ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        printmessage(stdout,CASE_NORMAL,0,timestamp,"localcos          = %d\n",localcos) ;
#endif
        printmessage(stdout,CASE_NORMAL,0,timestamp,"remotecos         = %d\n",remotecos) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"buffersize        = %d KB\n",buffersizeperstream) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"sendwinsize       = %d KB\n",sendwinsize) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"recvwinsize       = %d KB\n",recvwinsize) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"ackto			   = %d s\n",ackto) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"recvcontrolto     = %d s\n",recvcontrolto) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"sendcontrolto     = %d s\n",sendcontrolto) ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"datato     = %d s\n",datato) ;
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_DIR ) == TROPT_DIR) ? "createdir" : "nocreatedir") ;
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_TMP ) == TROPT_TMP) ? "tmpfile" : "notmpfile") ;
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_RFIO ) == TROPT_RFIO) ? "remoterfio" : "noremoterfio") ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_RFIO_O ) == TROPT_RFIO_O) ? "localrfio" : "nolocalrfio") ;
#endif
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_MODE ) == TROPT_MODE) ? "keepmode" : "nokeepmode") ;
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_ACC ) == TROPT_ACC) ? "keepaccess" : "nokeepaccess") ;
#ifdef WITH_GZIP
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_GZIP ) == TROPT_GZIP) ? "gzip" : "nogzip") ;
#endif        
		printmessage(stdout,CASE_NORMAL,0,timestamp,"option %s\n", ((transferoption & TROPT_QBSS ) == TROPT_QBSS) ? "qbss" : "noqbss") ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"-------------------------------------\n") ;
        printmessage(stdout,CASE_NORMAL,0,timestamp,"Connection mode ---------------------\n") ;
        if ( usecert ) {
/*			if ( username == NULL ) {*/
				printmessage(stdout,CASE_NORMAL,0,timestamp,"Using certificate authentication\n") ;
/*			} else {
				printmessage(stdout,CASE_NORMAL,0,timestamp,"Using standard bbftp mode\n") ;
			}*/
        } else if ( useprivate ) {
            printmessage(stdout,CASE_NORMAL,0,timestamp,"Using private authentication\n") ;
        } else if ( usessh) {
            printmessage(stdout,CASE_NORMAL,0,timestamp,"Using ssh mode\n") ;
            printmessage(stdout,CASE_NORMAL,0,timestamp,"     ssh command        = %s \n",sshcmd) ;
            printmessage(stdout,CASE_NORMAL,0,timestamp,"     ssh remote command = %s \n",sshremotecmd) ;
        } else {
            printmessage(stdout,CASE_NORMAL,0,timestamp,"Using standard bbftp mode\n",localumask) ;
        }
        printmessage(stdout,CASE_NORMAL,0,timestamp,"-------------------------------------\n") ;
   }

/*
** Ask for password if not in ssh mode
*/
#ifdef PRIVATE_AUTH
    if ( bbftp_private_getargs(logmessage) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,19,timestamp,"Error while private authentication : %s\n",logmessage) ;
    }     
#else
    if ((!usessh && !usecert)/* || (usecert && username)*/) {
# ifdef AFS
        char *reasonString, passwordBuffer[1024] ;
        long code ;
        if ( (code = ka_UserReadPassword("Password: ", passwordBuffer, sizeof(passwordBuffer), &reasonString)) ) {
            printmessage(stderr,CASE_FATAL_ERROR,19,timestamp,"Error while entering password\n") ;
        }
        if ( ( password = (char *) malloc (strlen(passwordBuffer) + 1) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,54,timestamp,"Unable to store password : malloc failed (%s)\n",strerror(errno)) ;
            return -1 ;
        }
        strcpy(password, passwordBuffer);
# else
#  ifdef USE_GETPASSPHRASE
        password = (char *) getpassphrase("Password: ") ;
        if ( strlen(password) == 0 ) {
            printmessage(stderr,CASE_FATAL_ERROR,19,timestamp,"Password is mandatory\n") ;
        }
#  else
        printmessage(stdout,CASE_NORMAL,0,0,"Password: ") ;
        password = (char *) getpass("") ;
#  endif /* USE_GETPASSPHRASE */
# endif /* AFS */
    }
#endif
/*
** Set in background if required
*/
    if ( background ) {
        retcode = fork() ; 
        if ( retcode < 0 ) printmessage(stderr,CASE_FATAL_ERROR,31,timestamp,"Error forking while setting in background\n") ;
        if ( retcode > 0 ) exit(0) ;
        setsid() ;
        if ( verbose ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Starting under pid %d\n",getpid());
    }

/*
** Set the signals
*/
    bbftp_setsignals() ;

/*
 * Analyze command list or command file to populate commandList
 */
    if (commandList == NULL) {
      maxlen = 0 ;
      retcode = 0 ;
      while (retcode == 0 ) {
        if ( inputfile != NULL ) {
            j = 0 ;
            while ( (lengthread = read (infd,&buffercmd[j],1)) == 1 ) {
                if ( buffercmd[j] == 10 ) {
                    buffercmd[j] = 0 ;
                    break ;
                } else {
                    j++ ;
                }
            }
            if ( lengthread < 0 ) {
                printmessage(stderr,CASE_FATAL_ERROR,24,timestamp,"Error reading input file (%s) : %s\n",inputfile,strerror(errno)) ;
            } else if ( lengthread == 0 ) {
                if ( j == 0 ) {
                    retcode = 1 ;
                    break ;
                } else {
                    buffercmd[j+1] = 0 ;
                }
            }
        } else {
            /*
            ** the command may be under the form cmd ; cmd ; cmd....
            */
            j = 0 ;
            while ((maxlen < strlen(bbftpcmd)) && (bbftpcmd[maxlen] == ' ')) maxlen++ ;
            while ( (maxlen < strlen(bbftpcmd)) && (bbftpcmd[maxlen] != ';') ) {
                buffercmd[j] =  bbftpcmd[maxlen] ;
                j++ ;
                maxlen++ ;
            }
            if ( maxlen == strlen(bbftpcmd) ) retcode = 1 ;
            buffercmd[j] = 0 ;
            maxlen++ ;
        }
	addCommand(buffercmd);
	if ( !strncmp(buffercmd,"setoption remoterfio",20) ) {
	    if ( remotecos == -1 ) remotecos = 0;
	}
      }
      commandList = first;
    }
/*
** Now we've got all informations to make the connection
*/

    if ( debug )
        printmessage(stdout,CASE_NORMAL,0,timestamp,"Connecting to server ----------------\n",localumask) ;
    reconnecttoserver() ;
    if ( debug )
         printmessage(stdout,CASE_NORMAL,0,timestamp,"Connecting end ---------------------\n",localumask) ;
   
/*
** Start the infinite loop
*/
    {
      cmd_list *literator;
      literator = commandList;
      while (literator != NULL) {
        if ( treatcommand(literator->cmd) == 0 ) {
            if ( inputfile != NULL ) {
                write(resfd,literator->cmd,strlen(literator->cmd)) ;
                write(resfd," OK\n",4) ;
            } else {
                if (!verbose && !statoutput) printmessage(stdout,CASE_NORMAL,0,timestamp,"%s OK\n",literator->cmd);
            }
        } else {
            if ( inputfile != NULL ) {
                write(resfd,literator->cmd,strlen(literator->cmd)) ;
                write(resfd," FAILED\n",8) ;
            } else {
                if (!verbose && !statoutput) printmessage(stdout,CASE_NORMAL,0,timestamp,"%s FAILED\n",literator->cmd);
            }
        }
	 literator = literator->next;
      }
    }
    if ( inputfile != NULL ) {
        close(infd) ;
        close(resfd) ;
    }
    msg = (struct message *)minbuffer ;
    msg->code = MSG_CLOSE_CONN ;
    msg->msglen = 0 ;
    /*
    ** We do not care of the result because this routine is called
    ** only at the end of the client
    */
    writemessage(outcontrolsock,minbuffer,MINMESSLEN,sendcontrolto,0) ;
    sleep(1) ;
    bbftp_close_control() ;
    exit(myexitcode) ;
}
