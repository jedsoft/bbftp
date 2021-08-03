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
 *
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
 *
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
# include <openssl/rsa.h>
#endif

#ifdef WITH_GZIP
# include <zlib.h>
#endif

#define SETTOZERO    0
#define SETTOONE     1

#define SSHREMOTECMD "bbftpd -s"
#define SSHCMD "ssh -q"

#ifdef PRIVATE_AUTH
# define OPTIONS "qbcde:f:i:l:mno:p:P:r:R:tu:vVw:WD::"
#else
# define OPTIONS "qbcde:E:f:g:i:I:l:L:mno:p:r:R:sStu:vVw:WD::"
#endif
/*
 #endif
 */

int BBftp_PID = 0;

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

typedef struct Cmd_List_Item_Type
{
   char *cmd;
   struct Cmd_List_Item_Type *next;
}
Cmd_List_Item_Type;

typedef struct
{
   Cmd_List_Item_Type *head;
   Cmd_List_Item_Type *tail;
}
Cmd_List_Type;

static void free_command_list (Cmd_List_Type *cmd_list)
{
   Cmd_List_Item_Type *cmd;

   if (cmd_list == NULL) return;
   cmd = cmd_list->head;
   while (cmd != NULL)
     {
	Cmd_List_Item_Type *next = cmd->next;
	if (cmd->cmd != NULL)
	  free (cmd->cmd);
	free (cmd);
	cmd = next;
     }
   free (cmd_list);
}

static Cmd_List_Type *new_command_list (void)
{
   Cmd_List_Type *cmd_list;

   if (NULL == (cmd_list = (Cmd_List_Type *) malloc (sizeof(Cmd_List_Type))))
     {
	printmessage(stderr,CASE_FATAL_ERROR,23, "Unable to malloc memory for command list : %s\n",strerror(errno)) ;
	return NULL;
     }
   memset (cmd_list, 0, sizeof(Cmd_List_Type));
   return cmd_list;
}

/* This function returns -1 upon failure, 0 upon success. Unlike the original version,
 * this function does not exit upon failure.
 */
static int add_command (Cmd_List_Type *cmd_list, char *lnewcmd)
{
   Cmd_List_Item_Type *item;

   if (NULL == (item = (Cmd_List_Item_Type *)malloc (sizeof(Cmd_List_Item_Type))))
     return -1;

   if (NULL == (item->cmd = (char *)malloc(strlen(lnewcmd) + 1)))
     {
	free (item);
	return -1;
     }

   strcpy (item->cmd, lnewcmd);
   item->next = NULL;
   if (cmd_list->tail == NULL)
     cmd_list->head = item;
   else
     cmd_list->tail->next = item;

   cmd_list->tail = item;
   return 0;
}

static void print_version (void)
{
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
}

static int redirect_to_file (int stdio_fd, const char *file)
{
   int fd;
   int flags = O_CREAT|O_WRONLY|O_TRUNC;

#ifdef O_SYNC
   flags |= O_SYNC;
#endif

   if (-1 == (fd = open (file,O_CREAT|O_WRONLY|O_TRUNC,0777)))
     {
	printmessage(stderr,CASE_FATAL_ERROR,10, "Error openning file (%s) : %s\n", file, strerror(errno)) ;
        return -1;
     }

   close (stdio_fd);

   if (stdio_fd != fcntl (fd, F_DUPFD, stdio_fd))
     {
	printmessage(stderr,CASE_FATAL_ERROR,11, "Error dup on file (%s) : %s\n", file, strerror(errno)) ;
	return -1;
     }
   return 0;
}

static int check_redirect_option (int opt, int stdio_fd, int argc, char **argv)
{
   const char *file = NULL;
   int j;

   opterr = 0 ;
   optind = 1 ;
   while ((j = getopt(argc, argv, OPTIONS)) != -1)
     {
	if (j == opt) file = optarg;   /* get the such option in the list */
     }

   if (file == NULL) return 0;

   return redirect_to_file (stdio_fd, file);
}

static int process_command_list (Cmd_List_Type *cmd_list)
{
   Cmd_List_Item_Type *cmds = cmd_list->head;

   while (cmds != NULL)
     {
	const char *passfail = "OK";
	char *cmd = cmds->cmd;

	if (0 != treatcommand (cmd))
	  passfail = "FAILED";

	(void) bbftp_res_printf ("%s %s\n", cmd, passfail);

	cmds = cmds->next;
     }
   return 0;
}

static int init_ssh_vars (void)
{
   if ( BBftp_SSHremotecmd == NULL )
     {
	/*
	 ** Malloc space and set the default
	 */
	if (NULL == (BBftp_SSHremotecmd = (char *) malloc (strlen(SSHREMOTECMD)+1)))
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,8, "Cannot malloc space for ssh remote cmd\n") ;
	     return -1;
	  }
	strcpy (BBftp_SSHremotecmd, SSHREMOTECMD) ;
     }
   else if (NULL == strstr(BBftp_SSHremotecmd," -s"))
     {
	/*
	 ** Verify if -s is present if not add it (in order to fit v 1.9.4-tja1
	 ** behaviour)
	 */
	char *tmpcmd;

	if (NULL == (tmpcmd = (char *) malloc (strlen(BBftp_SSHremotecmd) + 4)))
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,8, "Cannot malloc space for ssh remote cmd\n") ;
	     return -1;
	  }
	(void) sprintf (tmpcmd,"%s -s", BBftp_SSHremotecmd);
	BBftp_SSHremotecmd = tmpcmd;
     }

   if ( BBftp_SSHcmd == NULL )
     {
	if (NULL == (BBftp_SSHcmd = (char *) malloc (strlen(SSHCMD)+1)))
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,9, "Cannot malloc space for ssh cmd\n") ;
	     return -1;
	  }
	strcpy(BBftp_SSHcmd,SSHCMD) ;
     }

   return 0;
}

static int read_file_contents (const char *file, int must_exist, char **bufp, ssize_t *np)
{
   ssize_t n;
   struct stat st;
   char *lines;
   int fd;

   *bufp = NULL;

   if (-1 == stat (file, &st))
     {
	if (must_exist)
	  {
	     printmessage (stderr, CASE_NORMAL, 72, "Error stating file %s : %s\n", file, strerror(errno));
	     return -1;
	  }
	return 0;
     }

   if (NULL == (lines = (char *) malloc (st.st_size + 1)))
     {
	printmessage (stderr, CASE_NORMAL, 35, "Error allocating memory for %s : %s\n", file, strerror(errno));
	return -1;
     }

   if (-1 == (fd = open (file, O_RDONLY)))
     {
	printmessage (stderr, CASE_NORMAL, 200, "Error opening file %s : %s\n", file, strerror(errno));
	free (lines);
	return -1;
     }

   n = 0;
   while (n < st.st_size)
     {
	ssize_t dn = read (fd, lines + n, st.st_size-n);
	if (dn <= 0)
	  {
	     if ((dn == -1) && (errno == EINTR)) continue;

	     free (lines);
	     close (fd);

	     printmessage (stderr, CASE_NORMAL, 201, "Error reading file %s : %s\n", file, strerror(errno));
	     return -1;
	  }

	n += dn;
     }
   close (fd);
   lines[n] = 0;
   *np = n;
   *bufp = lines;
   return 0;
}

static int read_bbftprc_file (char *file, char **linesp, ssize_t *np)
{
   if (-1 == read_file_contents (file, 0, linesp, np))
     {
	printmessage (stderr, CASE_WARNING, 12, "Error reading .bbftprc file (%s) : %s\n", file, strerror(errno));
	return -1;
     }
   return 0;
}

typedef struct
{
   const char *name;
   int *valuep;
#define BBFTPRC_SETUMASK 0x01
#define BBFTPRC_OCTAL    0x02
#define BBFTPRC_NORANGE_CHECK 0x04
   int flags;
   int warning_code;
   const char *usage;
}
BBftprc_Cmd_Type;

static BBftprc_Cmd_Type BBftprc_Cmd_Table[] =
{
   {"setbuffersize", &BBftp_Buffersizeperstream, 0, 5, "Buffersize in bbftprc file must be numeric > 0\n"},
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
   {"setlocalcos", &BBftp_Localcos, BBFTPRC_NORANGE_CHECK, 14, "Local COS in bbftprc file must be numeric\n"},
#endif
   {"setremotecos", &BBftp_Remotecos, BBFTPRC_NORANGE_CHECK, 15, "Remote COS in bbftprc file must be numeric\n"},
   {"setlocalumask", &BBftp_Localumask, BBFTPRC_OCTAL|BBFTPRC_SETUMASK, 16, "Local umask in bbftprc file must be octal numeric\n"},
   {"setremoteumask", &BBftp_remoteumask, BBFTPRC_OCTAL|BBFTPRC_SETUMASK, 17, "Remote umask in bbftprc file must be octal numeric\n"},
   {"setsendwinsize", &BBftp_Sendwinsize, 0, 18, "Send window size in bbftprc file must be numeric\n"},
   {"setrecvwinsize", &BBftp_Recvwinsize, 0, 19, "Receive window size in bbftprc file must be numeric\n"},
   {"setnbstream", &BBftp_Nbport, 0, 19, "Number of streams in bbftprc file must be numeric and > 0\n"},
   {"setackto", &BBftp_Ackto, 0, 19, "Acknowledge timeout must be numeric and > 0\n"},
   {"setrecvcontrolto", &BBftp_Recvcontrolto, 0, 19, "Input control timeout must be numeric and > 0\n"},
   {"setsendcontrolto", &BBftp_Sendcontrolto, 0, 19, "Output control timeout must be numeric and > 0\n"},
   {"setdatato", &BBftp_Datato, 0, 19, "Data timeout must be numeric and > 0\n"},
   {NULL, NULL, 0, 0, NULL},
};

/* Returns 1 if set, -1 with errors, 0 if not found */
static int try_set_bbftprc_cmd (const char *cmdline)
{
   BBftprc_Cmd_Type *cmd_info = BBftprc_Cmd_Table;

   while (cmd_info->name != NULL)
     {
	int status = 0;;
	unsigned int len = strlen (cmd_info->name);

	if ((0 != strncmp (cmd_info->name, cmdline, len))
	    || ((cmdline[len] != ' ') && (cmdline[len] != '\t')))
	  {
	     cmd_info++;
	     continue;
	  }

	if ((cmd_info->flags == 0) || (cmd_info->flags == BBFTPRC_NORANGE_CHECK))
	  {
	     int val;
	     if ((1 != sscanf (cmdline + len, "%d", &val))
		 || ((cmd_info->flags == BBFTPRC_NORANGE_CHECK) && (val <= 0)))
	       status = -1;
	     else
	       *cmd_info->valuep = val;
	  }
	else if (cmd_info->flags & BBFTPRC_OCTAL)
	  {
	     unsigned int val;
	     if (1 != sscanf (cmdline + len, "%o", &val))
	       status = -1;
	     else
	       *cmd_info->valuep = (int) val;
	  }
	if (status == -1)
	  {
	     if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING, cmd_info->warning_code, "%s", cmd_info->usage);
	     return status;
	  }

	if (cmd_info->flags & BBFTPRC_SETUMASK)
	  umask(BBftp_Localumask);

	return 1;
     }

   return 0;			       /* not found */
}

typedef struct
{
   char *optname;
   int flag;
   int *flagvar;
   int warning_code;
   char *warning_mesg;
}
BBftprc_Option_Type;

static BBftprc_Option_Type BBftprc_Option_Table[] =
{
   {"createdir", TROPT_DIR, &BBftp_Transferoption, 0, NULL},
   {"tmpfile", TROPT_TMP, &BBftp_Transferoption, 0, NULL},
   {"remoterfio", TROPT_RFIO, &BBftp_Transferoption, 0, NULL},
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
   {"localrfio", TROPT_RFIO_O, &BBftp_Transferoption, 0, NULL},
#else
   {"localrfio", TROPT_RFIO_O, &BBftp_Transferoption, 20, "Incorrect command : setoption localrfio (RFIO not supported)\n"},
#endif
   {"keepmode", TROPT_MODE, &BBftp_Transferoption, 0, NULL},
   {"keepaccess", TROPT_ACC, &BBftp_Transferoption, 0, NULL},
#ifdef WITH_GZIP
   {"gzip", TROPT_GZIP, &BBftp_Transferoption, 0, NULL},
#else
   {"gzip", TROPT_GZIP, &BBftp_Transferoption, 20, "gzip option is not available: bbftp was built without compression support\n"},
#endif
   {"qbss", TROPT_QBSS, &BBftp_Transferoption, 0, NULL},
   {NULL, 0, NULL, 0, NULL},
};


static int process_bbftprc_option (const char *name)
{
   BBftprc_Option_Type *opt_info;
   unsigned int len;
   int noption = 0;

   if (0 == strncmp(name, "no", 2))
     {
	noption = 1;
	name += 2;
     }

   len = strlen (name);
   opt_info = BBftprc_Option_Table;
   while (opt_info->optname != NULL)
     {
	if ((0 != strncmp (opt_info->optname, name, len))
	    || ((name[len] != 0) && (name[len] != ' ') && (name[len] != '\t')))
	  {
	     opt_info++;
	     continue;
	  }

	if (noption)
	  *opt_info->flagvar &= ~opt_info->flag;
	else
	  {
	     if (opt_info->warning_mesg != NULL)
	       {
		  if (BBftp_Warning) printmessage (stderr,CASE_WARNING, opt_info->warning_code, "%s", opt_info->warning_mesg);
	       }
	     else
	       *opt_info->flagvar |= opt_info->flag;
	  }
	return 0;
     }

   if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING, 13, "Unknown option in .bbftprc file (%s)\n", name) ;
   return 0;
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

static char *strchr_limited (char *s, char *smax, char ch)
{
   while ((s < smax) && (*s != ch))
     s++;

   if (s == smax) return NULL;
   return s;
}

static int analyze_bbftprc (char *bbftprc, ssize_t nbytes)
{
   char *carret, *carret_max;
   int status;

   carret = bbftprc ;
   carret_max = carret + nbytes;

   status = 0;
   /*
    ** Strip starting CR
    */
   while (carret < carret_max)
     {
	char *cmdline;
	int this_status;

	while ((carret < carret_max)
	       && ((*carret == '\n') || (*carret == ' ') || (*carret == '\t')))
	  carret++ ;

	if (carret == carret_max) break;

	if (*carret == '#')
	  {
	     /* Allow '#' to denote a comment */
	     if (NULL == (carret = strchr_limited (carret, carret_max, '\n')))
	       break;

	     continue;
	  }

	cmdline = carret;
	carret = strchr_limited (carret, carret_max, '\n');
	if (carret == NULL) carret = carret_max;
	/* It is ok to set *carret_max to 0 since an extra byte was allocated for this */
	*carret++ = 0;

	this_status = try_set_bbftprc_cmd (cmdline);
	if (this_status && BBftp_Debug)
	  {
	     printmessage (stdout, CASE_NORMAL, 0, "bbftprc: %s %s\n",
			   cmdline, (this_status == -1 ? "FAILED" : "OK"));
	  }

	if (this_status == 1)
	  continue;

	if (this_status == -1)
	  {
	     status = -1;
	     continue;
	  }

	/* Otherwise, the command must be an option */
	if ((0 != strncmp (cmdline,"setoption",9))
	    || ((cmdline[9] != ' ') && (cmdline[9] != '\t')))
	  {
	     if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,13, "Skipping Unknown command in .bbftprc file (%s)\n", cmdline) ;
	     if (BBftp_Debug)
	       printmessage (stdout, CASE_NORMAL, 0, "bbftprc: Unknown command: %s\n", cmdline);

	     continue;
	  }

	cmdline += 9;
	while (*cmdline && ((*cmdline == ' ') || (*cmdline == '\t')))
	  cmdline++;

	if (-1 == (this_status = process_bbftprc_option (cmdline)))
	  status = -1;

	if (BBftp_Debug)
	  printmessage (stdout, CASE_NORMAL, 0, "bbftprc: setoption %s %s\n",
			cmdline, (this_status == -1 ? "FAILED" : "OK"));
     }

   return status;
}

static int handle_bbftprc_file (char *bbftprc_file)
{
   struct  passwd  *mypasswd ;
   ssize_t nbytes;
   char *bbftprc;
   int status;

   if (bbftprc_file != NULL)
     {
	if (0 == strncmp (bbftprc_file, "none", 4))
	  return 0;

	if (-1 == read_bbftprc_file (bbftprc_file, &bbftprc, &nbytes))
	  return -1;
	if (bbftprc == NULL) return -1;/* for a user-specified file, make this an error */

	status = analyze_bbftprc (bbftprc, nbytes);
	free (bbftprc);
	return status;
     }

   /* None specified, so look for one */

   if ( (mypasswd = getpwuid(getuid())) == NULL )
     {
	if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,6, "Unable to get passwd entry, using %s\n", BBFTP_CONF) ;
	if ( (bbftprc_file = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL )
	  strcpy(bbftprc_file,BBFTP_CONF);
     }
   else if ( mypasswd->pw_dir == NULL )
     {
	if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,7, "No home directory, using %s\n", BBFTP_CONF) ;
	if ( (bbftprc_file = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
	   strcpy(bbftprc_file,BBFTP_CONF);
	}
     }
   else if ( (bbftprc_file = (char *) malloc (strlen(mypasswd->pw_dir)+10) ) == NULL )
     {
	if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,8, "Error allocationg space for bbftprc file name, .bbftprc will not be used\n") ;
	return 0;
     }
   else
     {
	struct stat st;

	strcpy(bbftprc_file,mypasswd->pw_dir) ;
	strcat(bbftprc_file,"/.bbftprc") ;
	if ( stat(bbftprc_file, &st) < 0  )
	  {
	     free (bbftprc_file);
	     bbftprc_file = NULL;
	     if ( (bbftprc_file = (char *) malloc (strlen(BBFTP_CONF)+1) ) != NULL ) {
		strcpy(bbftprc_file,BBFTP_CONF);
	     }
	  }
     }

   if (bbftprc_file == NULL) return 0;

   status = read_bbftprc_file (bbftprc_file, &bbftprc, &nbytes);
   free (bbftprc_file);

   if (status == -1)
     return -1;

   if (bbftprc == NULL) return 0;

   status = analyze_bbftprc (bbftprc, nbytes);
   free (bbftprc);
   return status;
}

static void print_parameters (int usecert, int useprivate)
{
   if (BBftp_Simulation_Mode)
     printmessage(stdout,CASE_NORMAL,0, "** SIMULATION MODE: No data written **\n") ;

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

static int gather_input_cmds (char *lines, int delim, int turl_mode, Cmd_List_Type *cmd_list)
{
   char *p, *newp;

   newp = NULL;
   p = lines;
   while (p != NULL)
     {
	char *next_line;
	int remote_rfio, local_rfio;

	next_line = strchr (p, delim);
	if (next_line != NULL)
	  *next_line++ = 0;

	if (turl_mode == 0)
	  {
	     if (-1 == add_command (cmd_list, p))
	       break;

	     if ((0 == strncmp(p,"setoption remoterfio", 20))
		 && (BBftp_Remotecos == -1))
	       BBftp_Remotecos = 0;

	     p = next_line;
	     continue;
	  }

	if (0 != translatecommand (p, &newp, &BBftp_Hostname, &BBftp_Newcontrolport, &remote_rfio, &local_rfio))
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,24, "Unable to parse command  : %s\n", p) ;
	     break;
	  }

	if ((local_rfio == 1)
	    && ((BBftp_Transferoption & TROPT_RFIO_O) != TROPT_RFIO_O))
	  {
	     if (-1 == add_command (cmd_list, "setoption localrfio"))
	       break;

	     BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO_O ;
	  }
	else if ((local_rfio == 0)
		 && ((BBftp_Transferoption & TROPT_RFIO_O)==TROPT_RFIO_O))
	  {
	     if (-1 == add_command (cmd_list, "setoption nolocalrfio"))
	       break;

	     BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO_O ;
	  }

	if ((remote_rfio == 1)
	    && ((BBftp_Transferoption & TROPT_RFIO)!=TROPT_RFIO))
	  {
	     if (-1 == add_command (cmd_list, "setoption remoterfio"))
	       break;

	     if ( BBftp_Remotecos == -1 ) BBftp_Remotecos = 0 ;
	     BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO ;
	  }
	else if ((remote_rfio == 0)
		 && ((BBftp_Transferoption & TROPT_RFIO)==TROPT_RFIO))
	  {
	     if (-1 == add_command (cmd_list, "setoption noremoterfio"))
	       break;

	     BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO ;
	  }

	if (-1 == add_command (cmd_list, newp))
	  break;

	free (newp); newp = NULL;
	p = next_line;
     }

   if (newp != NULL) free (newp);
   if (p != NULL)
     return -1;

   return 0;
}

static int gather_input_file_cmds (char *inputfile, int turl_mode, Cmd_List_Type *cmd_list)
{
   ssize_t nbytes;
   char *lines;
   int status;

   if (-1 == read_file_contents (inputfile, 1, &lines, &nbytes))
     return -1;

   status = gather_input_cmds (lines, '\n', turl_mode, cmd_list);
   free (lines);
   return status;
}

static int gather_bbftpcmd_cmds (char *cmds, int turl_mode, Cmd_List_Type *cmd_list)
{
   return gather_input_cmds (cmds, ';', turl_mode, cmd_list);
}

static int get_password (int usecert)
{
#ifdef PRIVATE_AUTH
   if ( bbftp_private_getargs(logmessage) < 0 )
     {
	printmessage (stderr,CASE_FATAL_ERROR,19, "Error while private authentication : %s\n",logmessage);
	return -1;
     }
   return 0;

#else
   if (usecert || BBftp_Use_SSH)
     return 0;

# ifdef AFS
     {
	char *reasonString, passwordBuffer[1024] ;
	long code ;

	if ( (code = ka_UserReadPassword("Password: ", passwordBuffer, sizeof(passwordBuffer), &reasonString)) )
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,19, "Error while entering password\n") ;
	     return -1;
	  }
	if ( ( BBftp_Password = (char *) malloc (strlen(passwordBuffer) + 1) ) == NULL )
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,54, "Unable to store password : malloc failed (%s)\n",strerror(errno)) ;
	     return -1 ;
	  }
	strcpy(BBftp_Password, passwordBuffer);
     }
# else
#  ifdef USE_GETPASSPHRASE
   BBftp_Password = (char *) getpassphrase("Password: ") ;
   if ( strlen(BBftp_Password) == 0 )
     {
	printmessage(stderr,CASE_FATAL_ERROR,19, "Password is mandatory\n") ;
	return -1;
     }
#  else
   printstdout("%s", "Password: ") ;
   BBftp_Password = (char *) getpass("") ;
#  endif /* USE_GETPASSPHRASE */
# endif /* AFS */

   return 0;
#endif 				       /* PRIVATE_AUTH */
}

static int init_hostname_vars (const char *hostname)
{
   const char *h;
   int is_numeric = 1;
   char ch;

   h = hostname;
   while ((ch = *h++) != 0)
     {
	if (isalpha (ch))
	  {
	     is_numeric = 0;
	     break;
	  }

	if (isdigit(ch) || (ch == '.'))
	  continue;

	printmessage(stderr,CASE_FATAL_ERROR,15, "Invalid hostname (%s)\n", hostname) ;
	return -1;
     }

   if (is_numeric)
     {
	BBftp_His_Ctladdr.sin_addr.s_addr = 0 ;
	BBftp_His_Ctladdr.sin_addr.s_addr = inet_addr (hostname);
	if (BBftp_His_Ctladdr.sin_addr.s_addr == INADDR_NONE )
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,16, "Invalid IP address (%s)\n", hostname) ;
	     return -1;
	  }
	h = inet_ntoa (BBftp_His_Ctladdr.sin_addr) ;
	if (0 != strcmp (h, hostname))
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,16, "Invalid IP address (%s)\n", hostname) ;
	     return -1;
	  }
	return 0;
     }

   /*
    ** Alpha format
    */
   if (NULL == (BBftp_Hostent = gethostbyname(hostname)))
     {
	printmessage(stderr,CASE_FATAL_ERROR,17, "BBftp_Hostname no found (%s)\n", hostname);
	return -1;
     }
   if (BBftp_Hostent->h_length > (int)sizeof(BBftp_His_Ctladdr.sin_addr))
     BBftp_Hostent->h_length = sizeof(BBftp_His_Ctladdr.sin_addr);

   memcpy(&BBftp_His_Ctladdr.sin_addr, BBftp_Hostent->h_addr_list[0], BBftp_Hostent->h_length) ;
   return 0;
}

int main(int argc, char **argv)
{
   Cmd_List_Type *cmd_list = NULL;
   /*
    ** Variable set by options
    */
   char    *inputfile  = NULL ;
   char    *resultfile = NULL ;
   char    *bbftpcmd   = NULL ;
   int     background  = SETTOZERO ;
   /*
    ** For BBftp_Hostname
    */
   /*
    ** For local user
    */
   char    *bbftprcfile_static = NULL;

   struct  stat    statbuf ;
   int     retcode ;
   int     i, j, k ;
   int     alluse ;
#ifdef PRIVATE_AUTH
   char    logmessage[1024] ;
#endif
   char    minbuffer[MINMESSLEN] ;
   struct  message *msg ;
   int usecert = 0, useprivate = 0;
   int status;

   BBftp_PID = getpid ();

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
   optind = 1;
   while ((j = getopt(argc, argv, OPTIONS)) != -1)
     {
	if (j == 't')
	  {
	     BBftp_Timestamp = SETTOONE ;
	     break ;
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
   while ((j = getopt(argc, argv, OPTIONS)) != -1)
     {
	if (j != 'v') continue;
	print_version ();
	exit (0);
     }

   /*
    ** Check for stderr replacement
    */
   (void) check_redirect_option ('f', STDERR_FILENO, argc, argv);

   /*
    ** Check for stdout replacement
    */
   (void) check_redirect_option ('o', STDOUT_FILENO, argc, argv);

   /*
    ** Block all signals , the routine will exit in case of error
    */
   blockallsignals() ;

   /*
    ** Now all the others
    */
   opterr = 0 ;
   optind = 1 ;
   while ((j = getopt(argc, argv, OPTIONS)) != -1)
     {
	switch (j)
	  {
	   case 'b':
	     background = SETTOONE ;
	     break ;

	   case 'c':
#ifdef WITH_GZIP
	     BBftp_Transferoption = BBftp_Transferoption | TROPT_GZIP ;
#else
	     printmessage(stderr,CASE_FATAL_ERROR,7, "option -c is not available: bbftp was built without compression utility\n") ;
	     goto return_error;
#endif
	     break ;

	   case 'd':
	     BBftp_Debug = 1 ;
	     break;

	   case 'D' :
	     if (optarg)
	       {
		  if ((2 != sscanf(optarg,"%d:%d",&i, &k)) || (i > k))
		    {
		       printmessage(stderr,CASE_FATAL_ERROR,4, "Invalid port range : %s\n",optarg) ;
		       goto return_error;
		    }
		  BBftp_Pasvport_Min = i;
		  BBftp_Pasvport_Max = k;
	       }
	     else
	       {
#ifdef PORT_RANGE
		  sscanf(PORT_RANGE,"%d:%d",&BBftp_Pasvport_Min, &BBftp_Pasvport_Max) ;
#endif
		  if (0 == BBftp_Pasvport_Max)
		    {
		       BBftp_Pasvport_Min = 0;
		       BBftp_Pasvport_Max = 1;
		    }
	       }
	     BBftp_Protocolmax = 2 ;
	     break ;

	   case 'e' :
	     bbftpcmd = optarg ;
	     break ;

	   case 'E' :
	     BBftp_SSHremotecmd = optarg ;
	     BBftp_Use_SSH = 1 ;
	     break ;

	   case 'f' :
	     /* All ready handled */
	     break ;

#ifdef CERTIFICATE_AUTH
	   case 'g':
	     BBftp_Service = optarg ;
	     break ;
#endif
	   case 'i' :
	     inputfile = optarg ;
	     if ( stat (inputfile,&statbuf) < 0 )
	       {
		  printmessage(stderr,CASE_FATAL_ERROR,7, "Input file (%s) cannot be stated\n",inputfile) ;
		  goto return_error;
	       }
	     if ( (resultfile = (char *) malloc (strlen(inputfile) + 5 )) == NULL )
	       {
		  printmessage(stderr,CASE_FATAL_ERROR,1, "Cannot malloc space for result file name\n") ;
		  goto return_error;
	       }
	     strcpy(resultfile,inputfile) ;
	     strcat(resultfile,".res") ;
	     break ;

	   case 'I' :
	     BBftp_SSHidentityfile = optarg ;
	     BBftp_Use_SSH = 1 ;
	     /*
	      ** Check if file exists
	      */
	     if ( stat (BBftp_SSHidentityfile,&statbuf) < 0 )
	       {
		  printmessage(stderr,CASE_FATAL_ERROR,5, "SSH identity file (%s) cannot be stated\n",BBftp_SSHidentityfile) ;
		  goto return_error;
	       }
	     break ;

	   case 'L' :
	     BBftp_SSHcmd = optarg ;
	     BBftp_Use_SSH = 1 ;
	     break ;

	   case 'm' :
	     BBftp_Statoutput = SETTOONE ;
	     break ;

	   case 'n':
	     BBftp_Simulation_Mode = SETTOONE ;
	     break ;

	   case 'o' :
	     /* Already handled */
	     break ;

	   case 'P' :
	     privatestr = optarg ;
	     break ;

	   case 'q' :
	     BBftp_Transferoption = BBftp_Transferoption | TROPT_QBSS ;
	     break ;

	   case 'p' :
	     retcode = sscanf(optarg,"%d",&alluse) ;
	     if ( retcode != 1 || alluse < 0)
	       {
		  printmessage(stderr,CASE_FATAL_ERROR,3, "Number of streams must be numeric and > 0\n") ;
		  goto return_error;
	       }
	     BBftp_Nbport = alluse ;
	     break ;

	   case 'r' :
	     retcode = sscanf(optarg,"%d",&alluse) ;
	     if ( retcode != 1 || alluse <= 0)
	       {
		  printmessage(stderr,CASE_FATAL_ERROR,4, "Number of tries must be numeric > 0\n") ;
		  goto return_error;
	       }
	     BBftp_Globaltrymax = alluse ;
	     break ;

	   case 'R' :
	     bbftprcfile_static = optarg ;
	     break ;

	   case 's' :
	     BBftp_Use_SSH = SETTOONE ;
	     break ;

	   case 'S' :
	     BBftp_Use_SSH = SETTOONE ;
	     BBftp_SSHbatchmode = SETTOONE ;
	     break;

	   case 't':		       /* already handled */
	     break;

	   case 'u' :
	     BBftp_Username = optarg ;
	     break ;

	   case 'V':
	     BBftp_Verbose = SETTOONE ;
	     break ;

	   case 'w' :
	     retcode = sscanf(optarg,"%d",&alluse) ;
	     if ( retcode != 1 || alluse <= 0)
	       {
		  printmessage(stderr,CASE_FATAL_ERROR,4, "Control port must be numeric\n") ;
		  goto return_error;
	       }
	     BBftp_Newcontrolport = alluse ;
	     break ;

	   case 'W':
	     BBftp_Warning = SETTOONE ;
	     break ;

	   default :
	     Usage() ;
	     printmessage(stderr,CASE_FATAL_ERROR,6, "Error on command line (unsupported option -%c)\n", j) ;
	     goto return_error;
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
      /* unreached -- Does not return */
   }
#endif

   /*
    ** Reset all outputs variable if BBftp_Statoutput is set to one
    */
   if ( BBftp_Statoutput )
     {
        BBftp_Debug     = SETTOZERO ;
        BBftp_Verbose   = SETTOZERO ;
        BBftp_Warning   = SETTOZERO ;
        BBftp_Timestamp = SETTOZERO ;
     }

   /*
    ** Check all ssh stuff
    */
   if (BBftp_Use_SSH
       && (-1 == init_ssh_vars ()))
     goto return_error;

   if (-1 == handle_bbftprc_file (bbftprcfile_static))
     goto return_error;

   /*
    ** Check for input file or command line
    */
   if ((inputfile == NULL) && (bbftpcmd == NULL))
     {
        printmessage(stderr,CASE_FATAL_ERROR,20, "Error on command line (-i or -e option are mandatory)\n") ;
	return EXIT_FAILURE;
     }
   if ((inputfile != NULL) && (bbftpcmd != NULL))
     {
        printmessage(stderr,CASE_FATAL_ERROR,20, "Error on command line (Only one of -i or -e option are permitted)\n") ;
	return EXIT_FAILURE;
     }

   /*
    ** Check hostname
    */
   if ( optind == argc-1 )
     BBftp_Hostname = argv[optind];

   /* Process the command list now in case the TURL mode is used */
   if (NULL == (cmd_list = new_command_list ()))
     {
	printmessage(stderr,CASE_FATAL_ERROR,23, "Unable to malloc memory for commands : %s\n",strerror(errno)) ;
	return EXIT_FAILURE;
     }

   if (inputfile != NULL)
     {
	if (-1 == bbftp_res_open (resultfile))
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,22, "Error opening result file (%s) : %s\n",resultfile,strerror(errno)) ;
	     goto return_error;
	  }
	status = gather_input_file_cmds (inputfile, (BBftp_Hostname == NULL), cmd_list);
     }
   else
     status = gather_bbftpcmd_cmds (bbftpcmd, (BBftp_Hostname == NULL), cmd_list);

   if (status == -1)
     goto return_error;

   if (BBftp_Hostname == NULL || strlen(BBftp_Hostname) == 0)
     {
        Usage() ;
        printmessage(stderr,CASE_FATAL_ERROR,14, "No hostname on command line\n") ;
	goto return_error;
     }

   if (-1 == init_hostname_vars (BBftp_Hostname))
     goto return_error;

   /*
    ** Check BBftp_Username if not in	certificate authentication mode
    */
   if ((BBftp_Username == NULL) && (BBftp_Use_SSH == 0))
     {
#ifdef CERTIFICATE_AUTH
	usecert = 1;
#else
	Usage() ;
	printmessage(stderr,CASE_FATAL_ERROR,18, "No username given\n") ;
#endif
     }

   if (BBftp_Debug)
     print_parameters (usecert, useprivate);

   /*
    ** Ask for password if not in ssh mode
    */
   if (-1 == get_password (usecert))
     goto return_error;

   /*
    ** Set in background if required
    */
   if ( background )
     {
        int pid = fork();
	if (pid == -1)
	  {
	     printmessage(stderr,CASE_FATAL_ERROR,31, "Error forking while setting in background\n") ;
	     return EXIT_FAILURE;
	  }
	if (pid != 0)
	  return EXIT_SUCCESS;

	/* child process */
        (void) setsid() ;
        if ( BBftp_Verbose ) printmessage(stdout,CASE_NORMAL,0, "Starting under pid %d\n", pid);
	BBftp_PID = pid;
     }

   /*
    ** Set the signals
    */
   bbftp_setsignals() ;

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
   (void) process_command_list (cmd_list);
   free_command_list (cmd_list);
   cmd_list = NULL;

   bbftp_res_close ();

   msg = (struct message *)minbuffer ;
   msg->code = MSG_CLOSE_CONN ;
   msg->msglen = 0 ;
   /*
    ** We do not care of the result because this routine is called
    ** only at the end of the client
    */
   writemessage(BBftp_Outcontrolsock,minbuffer,MINMESSLEN,BBftp_Sendcontrolto) ;
   sleep(1) ;
   bbftp_close_control() ;
   if (resultfile != NULL) free (resultfile);
   return BBftp_Myexitcode;

return_error:
   if (resultfile != NULL) free (resultfile);
   if (cmd_list != NULL) free_command_list (cmd_list);
   return EXIT_FAILURE;
}
