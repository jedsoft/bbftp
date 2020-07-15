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

#include "_bbftp.h"

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

int     BBftp_State   = SETTOZERO ;
/*
** BBftp_Timestamp:
**      Every message on standart error and standart output are
**      timestamped
*/
int     BBftp_Timestamp = SETTOZERO ;
/*
** BBftp_Protocolmin:
**      Minimum BBftp_Protocol supportted
*/
int     BBftp_Protocolmin = 2 ; 
int     BBftp_Protocolmax = 3 ; 
int     BBftp_Protocol ;
/*
** BBftp_Debug:
**      Set to one to print more debugging information
*/
int     BBftp_Debug = SETTOZERO ;
/*
** BBftp_Verbose:
**      Set to one to print  information
*/
int     BBftp_Verbose = SETTOZERO ;
/*
** BBftp_Warning:
**      Set to one to print warning to stderr
*/
int     BBftp_Warning = SETTOZERO ;
/*
** BBftp_Statoutput:
**      Set to one for special output
*/
int     BBftp_Statoutput = SETTOZERO ;
/*
** BBftp_Globaltrymax:
**      Number of try in case or recoverable error
*/
int     BBftp_Globaltrymax = NBTRYMAX ;
/*
** BBftp_Newcontrolport:
**      Control port to be used 
*/
int     BBftp_Newcontrolport = CONTROLPORT ;
/*
** BBftp_Use_SSH:
**      Set to one when using ssh to start the remote daemon
*/
int     BBftp_Use_SSH = SETTOZERO ;
/*
** BBftp_SSHbatchmode:
**      This is set to non-zero if running in batch mode (that is, password
**      and passphrase queries are not allowed). 
*/
int     BBftp_SSHbatchmode  = SETTOZERO ;
/*
** BBftp_SSH_Childpid:
**      To keep the ssh child pid
*/
int     BBftp_SSH_Childpid   = SETTOZERO ;
/*
** For ssh
*/
char    *BBftp_SSHidentityfile = NULL ;
char    *BBftp_SSHremotecmd = NULL ;
char    *BBftp_SSHcmd = NULL ;
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
** BBftp_His_Ctladdr:
**      the remote address
*/
struct sockaddr_in BBftp_His_Ctladdr ;
/* 
** BBftp_My_Ctladdr:
**      the local address
*/
struct sockaddr_in BBftp_My_Ctladdr ;
/*
** bbftprc:
**      Where to store the bbftprc file
*/
char    *bbftprc = NULL ;
/*
** Variable defining the local options :
** 
** BBftp_Localcos:
**      Value of the local class of service (in case of RFIO ability)
**
** BBftp_Localumask:
**      Local umask taken by the umask command at start and
**      modified by the setlocalumask command
**
** localrfio:
**      set to one when using rfio for local files
**
*/
int     BBftp_Localumask ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
int     BBftp_Localcos  = SETTOZERO ;
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
int     BBftp_Sendwinsize     = 256 ;
int     BBftp_Recvwinsize     = 256 ;
int     BBftp_Nbport = 1 ;
int		BBftp_Ackto			= ACKTO;
int		BBftp_Recvcontrolto	= CONTROLSOCKTO;
int		BBftp_Sendcontrolto	= SENDCONTROLTO;
int		BBftp_Datato			= DATASOCKTO;

/*
** Variables remote side options :
**
** remoterfio:
**      Set to one when rfio for remote file
**
** BBftp_remoteumask:
**      if set to zero do not set remote umask
**
** BBftp_Remotecos:
**      if not set to zero do set remote cos
**
** BBftp_Remotedir :
**      if not set to NULL change dir after connection
**
*/
int     BBftp_remoteumask = SETTOZERO ;
int     BBftp_Remotecos   = -1 ;
char    *BBftp_Remotedir = NULL ;
/*
** BBftp_Incontrolsock :
**      Define the control socket for reading
** BBftp_Outcontrolsock :
**      Define the control socket for writing
**
**      For normal use : BBftp_Incontrolsock = BBftp_Outcontrolsock
*/
int     BBftp_Incontrolsock ;
int     BBftp_Outcontrolsock ;
/*
** BBftp_Myexitcode :
**      Contains the first error code that has to be return when program
**      is ended
*/
int     BBftp_Myexitcode = SETTOZERO ;
char    *BBftp_Hostname   = NULL ;
struct hostent  *BBftp_Hostent = NULL ;
char    *BBftp_Username   = NULL ;
char    *BBftp_Password   = NULL ;
#ifdef CERTIFICATE_AUTH
char    *BBftp_Service    = NULL ;
#endif
/*
** BBftp_Mychildren :
**      Pointer to the first pid of children
*/
int     *BBftp_Mychildren = NULL ;
/*
** nbpidchid :
**      Number of pid pointed by BBftp_Mychildren 
*/
int     nbpidchild ;
/*
** BBftp_Cast_Fd:
**      CASTOR file descriptor
*/
#ifdef CASTOR
int     BBftp_Cast_Fd = -1 ;
char    *BBftp_Cast_Filename = NULL ;
#endif

/*
** Parameters describing the transfer ********************
*/
int     BBftp_Transferoption = TROPT_TMP | TROPT_DIR | TROPT_MODE | TROPT_ACC; 
int     BBftp_Filemode ;
char    BBftp_Lastaccess[9] ;
char    BBftp_Lastmodif[9] ;
int     BBftp_Buffersizeperstream = 256 ;
int     BBftp_Requestedstreamnumber ;
my64_t  BBftp_Filesize ;
/*
** BBftp_Curfilename :
**      Define the pointer to the current file
*/
char    *BBftp_Curfilename = NULL ;
/*
** BBftp_Realfilename :
**      Define the pointer to the real file (= BBftp_Curfilename if TROPT_TMP not
**      set)
*/
char    *BBftp_Realfilename   = NULL ;
int     *BBftp_Myports        = NULL ;
int     *BBftp_Mysockets      = NULL ;
char    *BBftp_Readbuffer     = NULL ;
char    *BBftp_Compbuffer     = NULL ; 
/*
** Simulation mode (option -n)
*/
int		BBftp_Simulation_Mode = SETTOZERO;
/*
**
*/
int BBftp_Connectionisbroken = SETTOZERO ;

/*
 * Range for the ephemeral ports for data connections
 */
int     BBftp_Pasvport_Min = 0 ;
int     BBftp_Pasvport_Max = 0 ;

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
           printmessage(stderr,CASE_FATAL_ERROR,23, "Unable to malloc memory for command list : %s\n",strerror(errno)) ;
    }
    if ((newCommand->cmd = (char *) malloc( strlen(lnewcmd) + 1)) == NULL) {
	   printmessage(stderr,CASE_FATAL_ERROR,23, "Unable to malloc memory for command : %s\n",strerror(errno)) ;
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
** For BBftp_Hostname
*/  
    int     hosttype = 0 ;
    char    *calchostname ;
/*
** For local user
*/
    struct  passwd  *mypasswd ;
    char    *bbftprcfile = NULL, *bbftprcfile_static = NULL;
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
    BBftp_Localumask = umask(0) ;
/*
** and reset it to the correct value
*/
    umask(BBftp_Localumask) ;
/*
** First check for timestamp in order to have a common output
*/
    opterr = 0 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 't' :{
                BBftp_Timestamp = SETTOONE ;
                break ;
            }
        }
    }

#ifdef PRIVATE_AUTH
    useprivate = SETTOONE ;
    BBftp_Use_SSH = SETTOZERO ;
#endif

/*
** Check for -v option
*/
    opterr = 0 ;
    optind = 1 ;
    while ((j = getopt(argc, argv, OPTIONS)) != -1) {
        switch (j) {
            case 'v' :{
                printmessage(stdout,CASE_NORMAL,0, "bbftp version %s\n",VERSION) ;
                printmessage(stdout,CASE_NORMAL,0, "Compiled with  :   default port %d\n",CONTROLPORT) ;
#ifdef PORT_RANGE
                printmessage(stdout,CASE_NORMAL,0, "                   data ports range = %s \n", PORT_RANGE) ;
#endif
#ifdef WITH_GZIP
                printmessage(stdout,CASE_NORMAL,0, "                   compression with Zlib-%s\n", zlibVersion()) ;
#endif
#ifdef WITH_SSL
                printmessage(stdout,CASE_NORMAL,0, "                   encryption with %s \n",SSLeay_version(SSLEAY_VERSION)) ;
#endif
#ifdef WITH_RFIO
# ifdef CASTOR
                printmessage(stdout,CASE_NORMAL,0, "                   CASTOR support (RFIO)\n") ;
# else
                printmessage(stdout,CASE_NORMAL,0, "                   HPSS support (RFIO)\n") ;
# endif
#endif
#ifdef WITH_RFIO64
# ifdef CASTOR
                printmessage(stdout,CASE_NORMAL,0, "                   CASTOR support (RFIO64)\n") ;
# else
                printmessage(stdout,CASE_NORMAL,0, "                   HPSS support (RFIO64)\n") ;
# endif
#endif
#ifdef AFS
                printmessage(stdout,CASE_NORMAL,0, "                   AFS authentication \n") ;
#endif
#ifdef PRIVATE_AUTH
                printmessage(stdout,CASE_NORMAL,0, "                   private authentication \n") ;
#else
                printmessage(stdout,CASE_NORMAL,0, "                   default ssh command = %s \n",SSHCMD) ;
                printmessage(stdout,CASE_NORMAL,0, "                   default ssh remote command = %s \n",SSHREMOTECMD) ;
# ifdef CERTIFICATE_AUTH
                printmessage(stdout,CASE_NORMAL,0, "                   GSI authentication\n") ;
# endif
#endif
                printmessage(stdout,CASE_NORMAL,0, "                   default number of tries = %d  \n",NBTRYMAX) ;
                printmessage(stdout,CASE_NORMAL,0, "                   default sendwinsize = %d Kbytes\n",BBftp_Sendwinsize) ;
                printmessage(stdout,CASE_NORMAL,0, "                   default recvwinsize = %d Kbytes\n",BBftp_Recvwinsize) ;
                printmessage(stdout,CASE_NORMAL,0, "                   default number of stream = %d \n",BBftp_Nbport) ;
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
		if ( (stderrfd = open(errorfile,O_CREAT|O_WRONLY|O_TRUNC,0777)) < 0 )
#else
        if ( (stderrfd = open(errorfile,O_CREAT|O_WRONLY|O_SYNC|O_TRUNC,0777)) < 0 )
#endif
	   {
     		printmessage(stderr,CASE_FATAL_ERROR,10, "Error openning error file (%s) : %s\n",errorfile,strerror(errno)) ;
        }
        close(STDERR_FILENO);
        if ( fcntl(stderrfd,F_DUPFD,STDERR_FILENO) != STDERR_FILENO ) {
            printmessage(stderr,CASE_FATAL_ERROR,11, "Error dup on error file (%s) : %s\n",errorfile,strerror(errno)) ;
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
       if ( (stdoutfd = open(outputfile,O_CREAT|O_WRONLY|O_TRUNC,0777)) < 0 )
#else
        if ( (stdoutfd = open(outputfile,O_CREAT|O_WRONLY|O_SYNC|O_TRUNC,0777)) < 0 )
#endif
	   {
     		printmessage(stderr,CASE_FATAL_ERROR,12, "Error openning output file (%s) : %s\n",outputfile,strerror(errno)) ;
        }
        close(STDOUT_FILENO);
        if ( fcntl(stdoutfd,F_DUPFD,STDOUT_FILENO) != STDOUT_FILENO ) {
            printmessage(stderr,CASE_FATAL_ERROR,13, "Error dup on output file (%s) : %s\n",outputfile,strerror(errno)) ;
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
                BBftp_Transferoption = BBftp_Transferoption | TROPT_GZIP ;
#else
                printmessage(stderr,CASE_FATAL_ERROR,7, "option -c is not available: bbftp was built without compression utility\n") ;
#endif                
                break ;
            }
            case 'd' :{
                BBftp_Debug = 1 ;
                break ;
            }
            case 'D' :{         
                if (optarg) {
                    if ((sscanf(optarg,"%d:%d",&i, &k) == 2) && (i < k)) {
                        BBftp_Pasvport_Min = i; BBftp_Pasvport_Max = k;
                    } else {
                        printmessage(stderr,CASE_FATAL_ERROR,4, "Invalid port range : %s\n",optarg) ;
                    }
                } else {
#ifdef PORT_RANGE
                     sscanf(PORT_RANGE,"%d:%d",&BBftp_Pasvport_Min, &BBftp_Pasvport_Max) ;
#endif
                     if (0 == BBftp_Pasvport_Max) {
                         BBftp_Pasvport_Min = 0;
                         BBftp_Pasvport_Max = 1;
                     }
                }
		BBftp_Protocolmax = 2 ;
                break ;
            }
            case 'e' : {
                bbftpcmd = optarg ;
                break ;
            }
            case 'E' : {
                BBftp_SSHremotecmd = optarg ;
                BBftp_Use_SSH = 1 ;
                break ;
            }
            case 'f' :{
                errorfile = optarg ;
                break ;
            }
#ifdef CERTIFICATE_AUTH
            case 'g' :{
                BBftp_Service = optarg ;
                break ;
            }
#endif         
            case 'i' :{
                inputfile = optarg ;
                if ( stat (inputfile,&statbuf) < 0 ) {
                    printmessage(stderr,CASE_FATAL_ERROR,7, "Input file (%s) cannot be stated\n",inputfile) ;
                }
                if ( (resultfile = (char *) malloc (strlen(inputfile) + 5 )) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,1, "Cannot malloc space for result file name\n") ;
                }
                strcpy(resultfile,inputfile) ;
                strcat(resultfile,".res") ;
                break ;
            }
            case 'I' :{
                BBftp_SSHidentityfile = optarg ;
                BBftp_Use_SSH = 1 ;
                /*
                ** Check if file exists
                */
                if ( stat (BBftp_SSHidentityfile,&statbuf) < 0 ) {
                    printmessage(stderr,CASE_FATAL_ERROR,5, "SSH identity file (%s) cannot be stated\n",BBftp_SSHidentityfile) ;
                }
                break ;
            }
            case 'L' : {
                BBftp_SSHcmd = optarg ;
                BBftp_Use_SSH = 1 ;
                break ;
            }
            case 'm' :{
                BBftp_Statoutput = SETTOONE ;
                break ;
            }
            case 'n':{
                BBftp_Simulation_Mode = SETTOONE ;
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
                BBftp_Transferoption = BBftp_Transferoption | TROPT_QBSS ;
				break ;
			}
            case 'p' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse < 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,3, "Number of streams must be numeric and > 0\n") ;
                }
                BBftp_Nbport = alluse ;
                break ;
            }
            case 'r' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,4, "Number of tries must be numeric > 0\n") ;
                }
                BBftp_Globaltrymax = alluse ;
                break ;
            }
            case 'R' :{
                bbftprcfile_static = optarg ;
                break ;
            }
            case 's' :{
                BBftp_Use_SSH = SETTOONE ;
                break ;
            }
            case 'S' :{
                BBftp_Use_SSH = SETTOONE ;
                BBftp_SSHbatchmode = SETTOONE ;
                break ;
            }
            case 't' :{
                BBftp_Timestamp = SETTOONE ;
                break ;
            }
            case 'u' :{
                BBftp_Username = optarg ;
                break ;
            }
            case 'V':{
                BBftp_Verbose = SETTOONE ;
                break ;
            }
            case 'w' :{
                retcode = sscanf(optarg,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                    printmessage(stderr,CASE_FATAL_ERROR,4, "Control port must be numeric\n") ;
                }
                BBftp_Newcontrolport = alluse ;
                break ;
            }
            case 'W':{
                BBftp_Warning = SETTOONE ;
                break ;
            }
            default : {
                Usage() ;
                printmessage(stderr,CASE_FATAL_ERROR,6, "Error on command line (unsupported option -%c)\n",optopt) ;
            }
        }
    }
    /*
    ** Get 5K for BBftp_Cast_Filename in order to work with CASTOR
    ** software (even if we are not using it)
    */
#ifdef CASTOR
    if ( (BBftp_Cast_Filename = (char *) malloc (5000)) == NULL ) {
        /*
        ** Starting badly if we are unable to malloc 5K
        */
        printmessage(stderr,CASE_FATAL_ERROR,10, "No memory for CASTOR : %s\n",strerror(errno)) ;
    }
#endif

    /*
    ** Reset all outputs variable if BBftp_Statoutput is set to one
    */
    if ( BBftp_Statoutput ) {
        BBftp_Debug     = SETTOZERO ;
        BBftp_Verbose   = SETTOZERO ;
        BBftp_Warning   = SETTOZERO ;
        BBftp_Timestamp = SETTOZERO ;
    }
/*
#ifdef PRIVATE_AUTH
    useprivate = SETTOONE ;
    BBftp_Use_SSH = SETTOZERO ;
#endif

#ifdef CERTIFICATE_AUTH
	usecert = SETTOONE ;
    useprivate = SETTOZERO ;
    BBftp_Use_SSH = SETTOZERO ;
#endif
*/	        
 
/*
** Check all ssh stuff
*/
    if ( BBftp_Use_SSH) {
        if ( BBftp_SSHremotecmd == NULL ) {
            /*
            ** Malloc space and set the default
            */
            if ( (BBftp_SSHremotecmd = (char *) malloc (strlen(SSHREMOTECMD)+1) ) == NULL ) {
                printmessage(stderr,CASE_FATAL_ERROR,8, "Cannot malloc space for ssh remote cmd\n") ;
            }
            strcpy(BBftp_SSHremotecmd,SSHREMOTECMD) ;
        } else {
            /*
            ** Verify if -s is present if not add it (in order to fit v 1.9.4-tja1
            ** behaviour)
            */
            if ( strstr(BBftp_SSHremotecmd," -s") == NULL ) {
                if ( ( tmpsshremotecmd = (char *) malloc (strlen(BBftp_SSHremotecmd) + 4) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,8, "Cannot malloc space for ssh remote cmd\n") ;
                }
                sprintf(tmpsshremotecmd,"%s -s",BBftp_SSHremotecmd) ;
                BBftp_SSHremotecmd = tmpsshremotecmd ;
            }
        }
        if ( BBftp_SSHcmd == NULL ) {
            if ( (BBftp_SSHcmd = (char *) malloc (strlen(SSHCMD)+1) ) == NULL ) {
                  printmessage(stderr,CASE_FATAL_ERROR,9, "Cannot malloc space for ssh cmd\n") ;
            }
            strcpy(BBftp_SSHcmd,SSHCMD) ;
        }
    }
/*
** Check for the local user in order to find the .bbftprc file
*/
       bbftprcfile = bbftprcfile_static;

    if ( bbftprcfile == NULL ) {
        /*
        ** look for the local user in order to find the .bbftprc file
        */
        if ( (mypasswd = getpwuid(getuid())) == NULL ) {
            if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,6, "Unable to get passwd entry, using %s\n", BBFTP_CONF) ;
            if ( (bbftprcfile = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
	      strcpy(bbftprcfile,BBFTP_CONF);
	    }
        } else if ( mypasswd->pw_dir == NULL ) {
            if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,7, "No home directory, using %s\n", BBFTP_CONF) ;
            if ( (bbftprcfile = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
	      strcpy(bbftprcfile,BBFTP_CONF);
	    }
        } else if ( (bbftprcfile = (char *) malloc (strlen(mypasswd->pw_dir)+10) ) == NULL ) {
            if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,8, "Error allocationg space for bbftprc file name, .bbftprc will not be used\n") ;
        } else {
            strcpy(bbftprcfile,mypasswd->pw_dir) ;
            strcat(bbftprcfile,"/.bbftprc") ;
            if ( stat(bbftprcfile,&statbuf) < 0  ) {
	       free (bbftprcfile);
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
                if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,9, "Error stating bbftprc file (%s)\n",bbftprcfile) ;
            } else if ( statbuf.st_size == 0 ) {
                /*
                ** do nothing 
                */
            } else if ( (bbftprc = (char *) malloc (statbuf.st_size + 1 ) ) == NULL ) {
                if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,10, "Error allocation memory for bbftprc, .bbftprc will not be used\n") ;
            } else if ( ( fd  = open (bbftprcfile,O_RDONLY) )  < 0 ) {
                if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,11, "Error openning .bbftprc file (%s) : %s \n",bbftprcfile,strerror(errno)) ;
            } else if ( ( j = read( fd, bbftprc , statbuf.st_size )) != statbuf.st_size ) {
                if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,12, "Error reading .bbftprc file (%s)\n",bbftprcfile) ;
            } else {
                bbftprc[j] = '\0' ;
            }
        }
       if (bbftprcfile != bbftprcfile_static) free (bbftprcfile);
       bbftprcfile = NULL;
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
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,5, "Buffersize in bbftprc file must be numeric > 0\n") ;
                } else {
                    BBftp_Buffersizeperstream = alluse ;
                }
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
            } else if ( !strncmp(startcmd,"setlocalcos",11)) {
                retcode = sscanf(startcmd,"setlocalcos %d",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,14, "Local COS in bbftprc file must be numeric\n") ;
                } else {
                    BBftp_Localcos = alluse ;
                }
#endif
            } else if (!strncmp(startcmd,"setremotecos",12)) {
                retcode = sscanf(startcmd,"setremotecos %d",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,15, "Remote COS in bbftprc file must be numeric\n") ;
                }else {
                    BBftp_Remotecos = alluse ;
                }
            } else if (!strncmp(startcmd,"setlocalumask",13)) {
                retcode = sscanf(startcmd,"setlocalumask %o",&ualluse) ;
                if ( retcode != 1) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,16, "Local umask in bbftprc file must be octal numeric\n") ;
                } else {
                    BBftp_Localumask = ualluse ;
                    umask(BBftp_Localumask) ;
                }
            } else if (!strncmp(startcmd,"setremoteumask",14)) {
                retcode = sscanf(startcmd,"setremoteumask %o",&ualluse) ;
                if ( retcode != 1) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,17, "Remote umask in bbftprc file must be octal numeric\n") ;
                }else {
                    BBftp_remoteumask = ualluse ;
                    umask(BBftp_Localumask) ;
                }
            } else if (!strncmp(startcmd,"setsendwinsize",14)) {
                retcode = sscanf(startcmd,"setsendwinsize %d",&alluse) ;
                if ( retcode != 1 || alluse < 0 ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,18, "Send window size in bbftprc file must be numeric\n") ;
                } else {
                    BBftp_Sendwinsize = alluse ;
                }
            } else if (!strncmp(startcmd,"setrecvwinsize",14)) {
                retcode = sscanf(startcmd,"setrecvwinsize %d",&alluse) ;
                if ( retcode != 1 || alluse < 0  ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,19, "Receive window size in bbftprc file must be numeric\n") ;
                }else {
                    BBftp_Recvwinsize = alluse ;
                }
            } else if (!strncmp(startcmd,"setnbstream",11)) {
                retcode = sscanf(startcmd,"setnbstream %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,19, "Number of streams in bbftprc file must be numeric and > 0\n") ;
                } else {
                    BBftp_Nbport = alluse ;
                }
            } else if (!strncmp(startcmd,"setackto",8)) {
                retcode = sscanf(startcmd,"setackto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,19, "Acknowledge timeout must be numeric and > 0\n") ;
                } else {
                    BBftp_Ackto = alluse ;
                }
            } else if (!strncmp(startcmd,"setrecvcontrolto",16)) {
                retcode = sscanf(startcmd,"setrecvcontrolto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,19, "Input control timeout must be numeric and > 0\n") ;
                } else {
                    BBftp_Recvcontrolto = alluse ;
                }
            } else if (!strncmp(startcmd,"setsendcontrolto",16)) {
                retcode = sscanf(startcmd,"setsendcontrolto %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,19, "Output control timeout must be numeric and > 0\n") ;
                } else {
                    BBftp_Sendcontrolto = alluse ;
                }
            } else if (!strncmp(startcmd,"setdatato",9)) {
                retcode = sscanf(startcmd,"setdatato %d",&alluse) ;
                if ( retcode != 1  || alluse <= 0 ) {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,19, "Data timeout must be numeric and > 0\n") ;
                } else {
                    BBftp_Datato = alluse ;
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
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_DIR ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_DIR ;
                   }
                } else if ( !strncmp(startcmd,"tmpfile",7) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_TMP ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_TMP ;
                   }
                } else if ( !strncmp(startcmd,"remoterfio",10) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO ;
			if ( BBftp_Remotecos == -1 ) BBftp_Remotecos = 0;
                    }
                } else if ( !strncmp(startcmd,"localrfio",9) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO_O ;
                    } else {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO_O ;
#else
                        if (BBftp_Warning) printmessage(stderr,CASE_WARNING,20, "%s", "Incorrect command : setoption localrfio (RFIO not supported)\n");
#endif
                    }
                } else if ( !strncmp(startcmd,"keepmode",8) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_MODE ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_MODE ;
                    }
                } else if ( !strncmp(startcmd,"keepaccess",10) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_ACC ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_ACC ;
                    }
                } else if ( !strncmp(startcmd,"gzip",4) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_GZIP ;
                    } else {
#ifdef WITH_GZIP
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_GZIP ;
#else
                        printmessage(stderr,CASE_FATAL_ERROR,7, "gzip option is not available: bbftp was built without compression support\n") ;
#endif
                    }
                } else if ( !strncmp(startcmd,"qbss",4) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_QBSS ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_QBSS ;
                    }
                } else {
                    if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,20, "Unkown option in .bbftprc file (%s)\n",startcmd) ;
                }
            } else {
                if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,13, "Unkown command in .bbftprc file (%s)\n",startcmd) ;
            }
            carret++ ;
        }
    }
/*
** Check for input file or command line
*/
    if ( inputfile == NULL && bbftpcmd == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,20, "Error on command line (-i or -e option are mandatory)\n") ;
    }
/*
** Open the input file if needed
*/
    if ( inputfile != NULL ) {
        if ( (infd = open(inputfile,O_RDONLY)) < 0 ) {
            printmessage(stderr,CASE_FATAL_ERROR,21, "Error opening input file (%s) : %s\n",inputfile,strerror(errno)) ;
        }
       if (-1 == bbftp_res_open (resultfile))
	 {
	    printmessage(stderr,CASE_FATAL_ERROR,22, "Error opening result file (%s) : %s\n",resultfile,strerror(errno)) ;
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
            printmessage(stderr,CASE_FATAL_ERROR,24, "Error reading input file (%s) : %s\n",inputfile,strerror(errno)) ;
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
            printmessage(stderr,CASE_FATAL_ERROR,23, "Unable to malloc memory for command : %s\n",strerror(errno)) ;
        }
    } else {
        if ( (buffercmd = (char *) malloc (strlen(bbftpcmd)+1)) == NULL ) {
             printmessage(stderr,CASE_FATAL_ERROR,23, "Unable to malloc memory for command : %s\n",strerror(errno)) ;
         }
    }

/*
** Check hostname
*/          
    if ( optind == argc-1 ) {
        BBftp_Hostname= argv[optind] ;
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
                    printmessage(stderr,CASE_FATAL_ERROR,24, "Error reading input file (%s) : %s\n",inputfile,strerror(errno)) ;
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
            if ( translatecommand(buffercmd, &newcmd, &BBftp_Hostname, &BBftp_Newcontrolport, &remoterfio, &localrfio) == 0 ) {
		    if (localrfio == 1 && ((BBftp_Transferoption & TROPT_RFIO_O)!=TROPT_RFIO_O) ) {
	                addCommand("setoption localrfio");
			BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO_O ;
	            } else if (localrfio == 0 && ((BBftp_Transferoption & TROPT_RFIO_O)==TROPT_RFIO_O) ) {
			addCommand("setoption nolocalrfio");
			BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO_O ;
		    }
		    if (remoterfio == 1 && ((BBftp_Transferoption & TROPT_RFIO)!=TROPT_RFIO) ) {
	                addCommand("setoption remoterfio");
			if ( BBftp_Remotecos == -1 ) BBftp_Remotecos = 0 ;
			BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO ;
	            } else if (remoterfio == 0 && ((BBftp_Transferoption & TROPT_RFIO)==TROPT_RFIO) ) {
			addCommand("setoption noremoterfio");
			BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO ;
		    }
		    addCommand(newcmd);
            } else {
		    printmessage(stderr,CASE_FATAL_ERROR,24, "Unable to parse command  : %s\n",buffercmd) ;
            }
	}
	commandList = first;
    }
    if (BBftp_Hostname == NULL || strlen(BBftp_Hostname) == 0) {
        Usage() ;
        printmessage(stderr,CASE_FATAL_ERROR,14, "No hostname on command line\n") ;
    }    
/*
** Check if hostname is in numeric format 
*/
    for (uj=0 ; uj < strlen(BBftp_Hostname) ; uj++) {
        if ( isalpha(BBftp_Hostname[uj]) ) {
            /*
            ** One alpha caractere means no numeric form
            */
            hosttype = 1 ;
            break ;
        } else if ( isdigit(BBftp_Hostname[uj]) ) {
        } else if ( BBftp_Hostname[uj] == '.' ) {
        } else {
            printmessage(stderr,CASE_FATAL_ERROR,15, "Invalid hostname (%s)\n",BBftp_Hostname) ;
        }
    }
    if ( hosttype == 0 ) {
       /*
       ** Numeric format 
       */
        BBftp_His_Ctladdr.sin_addr.s_addr = 0 ;
        BBftp_His_Ctladdr.sin_addr.s_addr = inet_addr(BBftp_Hostname) ;
        if (BBftp_His_Ctladdr.sin_addr.s_addr == INADDR_NONE ) {
            printmessage(stderr,CASE_FATAL_ERROR,16, "Invalid IP address (%s)\n",BBftp_Hostname) ;
        }
        calchostname = (char *)inet_ntoa(BBftp_His_Ctladdr.sin_addr) ;
        if ( strcmp(BBftp_Hostname,calchostname) ) {
            printmessage(stderr,CASE_FATAL_ERROR,16, "Invalid IP address (%s)\n",BBftp_Hostname) ;
        }
    } else {
       /*
       ** Alpha format 
       */
        if ( (BBftp_Hostent = gethostbyname((char *)BBftp_Hostname) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,17, "BBftp_Hostname no found (%s)\n",BBftp_Hostname) ;
        } else {
            if (BBftp_Hostent->h_length > (int)sizeof(BBftp_His_Ctladdr.sin_addr)) {
                BBftp_Hostent->h_length = sizeof(BBftp_His_Ctladdr.sin_addr);
            }
            memcpy(&BBftp_His_Ctladdr.sin_addr, BBftp_Hostent->h_addr_list[0], BBftp_Hostent->h_length) ;
        }
    }
/*
** Check BBftp_Username if not in	certificate authentication mode
*/          
    if ( BBftp_Username == NULL ) {
#ifdef CERTIFICATE_AUTH
        if (BBftp_Use_SSH) {
/*            Usage() ;
            printmessage(stderr,CASE_FATAL_ERROR,18, "No username given\n") ;*/
        } else {
            usecert = SETTOONE;
        }
#else        
	if (!BBftp_Use_SSH) {
            Usage() ;
            printmessage(stderr,CASE_FATAL_ERROR,18, "No username given\n") ;
	}
#endif
    }
    if ( BBftp_Debug ) {
        if (BBftp_Simulation_Mode) {
            printmessage(stdout,CASE_NORMAL,0, "** SIMULATION MODE: No data written **\n") ;
        }
        printmessage(stdout,CASE_NORMAL,0, "Starting parameters -----------------\n") ;
        printmessage(stdout,CASE_NORMAL,0, "number of tries   = %d\n",BBftp_Globaltrymax) ;
        printmessage(stdout,CASE_NORMAL,0, "number of streams = %d\n",BBftp_Nbport) ;
        printmessage(stdout,CASE_NORMAL,0, "localumask        = %03o\n",BBftp_Localumask) ;
        printmessage(stdout,CASE_NORMAL,0, "remoteumask       = %03o\n",BBftp_remoteumask) ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
        printmessage(stdout,CASE_NORMAL,0, "localcos          = %d\n",BBftp_Localcos) ;
#endif
        printmessage(stdout,CASE_NORMAL,0, "remotecos         = %d\n",BBftp_Remotecos) ;
        printmessage(stdout,CASE_NORMAL,0, "buffersize        = %d KB\n",BBftp_Buffersizeperstream) ;
        printmessage(stdout,CASE_NORMAL,0, "sendwinsize       = %d KB\n",BBftp_Sendwinsize) ;
        printmessage(stdout,CASE_NORMAL,0, "recvwinsize       = %d KB\n",BBftp_Recvwinsize) ;
        printmessage(stdout,CASE_NORMAL,0, "ackto			   = %d s\n",BBftp_Ackto) ;
        printmessage(stdout,CASE_NORMAL,0, "recvcontrolto     = %d s\n",BBftp_Recvcontrolto) ;
        printmessage(stdout,CASE_NORMAL,0, "sendcontrolto     = %d s\n",BBftp_Sendcontrolto) ;
        printmessage(stdout,CASE_NORMAL,0, "datato     = %d s\n",BBftp_Datato) ;
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_DIR ) == TROPT_DIR) ? "createdir" : "nocreatedir") ;
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_TMP ) == TROPT_TMP) ? "tmpfile" : "notmpfile") ;
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_RFIO ) == TROPT_RFIO) ? "remoterfio" : "noremoterfio") ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_RFIO_O ) == TROPT_RFIO_O) ? "localrfio" : "nolocalrfio") ;
#endif
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_MODE ) == TROPT_MODE) ? "keepmode" : "nokeepmode") ;
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_ACC ) == TROPT_ACC) ? "keepaccess" : "nokeepaccess") ;
#ifdef WITH_GZIP
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP) ? "gzip" : "nogzip") ;
#endif        
		printmessage(stdout,CASE_NORMAL,0, "option %s\n", ((BBftp_Transferoption & TROPT_QBSS ) == TROPT_QBSS) ? "qbss" : "noqbss") ;
        printmessage(stdout,CASE_NORMAL,0, "-------------------------------------\n") ;
        printmessage(stdout,CASE_NORMAL,0, "Connection mode ---------------------\n") ;
        if ( usecert ) {
/*			if ( BBftp_Username == NULL ) {*/
				printmessage(stdout,CASE_NORMAL,0, "Using certificate authentication\n") ;
/*			} else {
				printmessage(stdout,CASE_NORMAL,0, "Using standard bbftp mode\n") ;
			}*/
        } else if ( useprivate ) {
            printmessage(stdout,CASE_NORMAL,0, "Using private authentication\n") ;
        } else if ( BBftp_Use_SSH) {
            printmessage(stdout,CASE_NORMAL,0, "Using ssh mode\n") ;
            printmessage(stdout,CASE_NORMAL,0, "     ssh command        = %s \n",BBftp_SSHcmd) ;
            printmessage(stdout,CASE_NORMAL,0, "     ssh remote command = %s \n",BBftp_SSHremotecmd) ;
        } else {
            printmessage(stdout,CASE_NORMAL,0, "Using standard bbftp mode (0%o)\n",BBftp_Localumask) ;
        }
        printmessage(stdout,CASE_NORMAL,0, "-------------------------------------\n") ;
   }

/*
** Ask for password if not in ssh mode
*/
#ifdef PRIVATE_AUTH
    if ( bbftp_private_getargs(logmessage) < 0 ) {
        printmessage(stderr,CASE_FATAL_ERROR,19, "Error while private authentication : %s\n",logmessage) ;
    }     
#else
    if ((!BBftp_Use_SSH && !usecert)/* || (usecert && BBftp_Username)*/) {
# ifdef AFS
        char *reasonString, passwordBuffer[1024] ;
        long code ;
        if ( (code = ka_UserReadPassword("Password: ", passwordBuffer, sizeof(passwordBuffer), &reasonString)) ) {
            printmessage(stderr,CASE_FATAL_ERROR,19, "Error while entering password\n") ;
        }
        if ( ( BBftp_Password = (char *) malloc (strlen(passwordBuffer) + 1) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,54, "Unable to store password : malloc failed (%s)\n",strerror(errno)) ;
            return -1 ;
        }
        strcpy(BBftp_Password, passwordBuffer);
# else
#  ifdef USE_GETPASSPHRASE
        BBftp_Password = (char *) getpassphrase("Password: ") ;
        if ( strlen(BBftp_Password) == 0 ) {
            printmessage(stderr,CASE_FATAL_ERROR,19, "Password is mandatory\n") ;
        }
#  else
        printstdout("%s", "Password: ") ;
        BBftp_Password = (char *) getpass("") ;
#  endif /* USE_GETPASSPHRASE */
# endif /* AFS */
    }
#endif
/*
** Set in background if required
*/
    if ( background ) {
        retcode = fork() ; 
        if ( retcode < 0 ) printmessage(stderr,CASE_FATAL_ERROR,31, "Error forking while setting in background\n") ;
        if ( retcode > 0 ) exit(0) ;
        setsid() ;
        if ( BBftp_Verbose ) printmessage(stdout,CASE_NORMAL,0, "Starting under pid %d\n",getpid());
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
                printmessage(stderr,CASE_FATAL_ERROR,24, "Error reading input file (%s) : %s\n",inputfile,strerror(errno)) ;
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
	    if ( BBftp_Remotecos == -1 ) BBftp_Remotecos = 0;
	}
      }
      commandList = first;
    }
/*
** Now we've got all informations to make the connection
*/

    if ( BBftp_Debug )
        printmessage(stdout,CASE_NORMAL,0, "%s", "Connecting to server ----------------\n") ;
    reconnecttoserver() ;
    if ( BBftp_Debug )
         printmessage(stdout,CASE_NORMAL,0, "%s", "Connecting end ---------------------\n");
   
/*
** Start the infinite loop
*/
   {
      cmd_list *literator;
      literator = commandList;
      while (literator != NULL)
	{
	   char *passfail = "OK";

	   if (0 != treatcommand(literator->cmd))
	     passfail = "FAILED";

	   (void) bbftp_res_printf ("%s %s\n", literator->cmd, passfail);
	   literator = literator->next;
	}
   }
    if ( inputfile != NULL ) {
        close(infd) ;
       bbftp_res_close ();
    }
    msg = (struct message *)minbuffer ;
    msg->code = MSG_CLOSE_CONN ;
    msg->msglen = 0 ;
    /*
    ** We do not care of the result because this routine is called
    ** only at the end of the client
    */
    writemessage(BBftp_Outcontrolsock,minbuffer,MINMESSLEN,BBftp_Sendcontrolto,0) ;
    sleep(1) ;
    bbftp_close_control() ;
    exit(BBftp_Myexitcode) ;
}
