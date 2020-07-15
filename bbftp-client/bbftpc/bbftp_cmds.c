#include "bbftp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "_bbftp.h"
#include <client.h>
#include <client_proto.h>
#include <structures.h>

static char *skip_whitepace (char *s)
{
   while (isspace(*s)) s++;
   return s;
}

static char *skip_non_whitepace (char *s)
{
   while (*s && (isspace(*s) == 0))
     s++;

   return s;
}

static void free_argv (char **argv, int argc)
{
   int i;

   for (i = 0; i < argc; i++)
     {
	if (argv[i] != NULL) free (argv[i]);
     }
   free (argv);
}

static char **tokenize (char *cmdstr, int *argcp)
{
   char *s;
   char **argv;
   int argc = 0;

   s = cmdstr;
   while (1)
     {
	s = skip_whitepace (s);
	if (*s == 0) break;
	s = skip_non_whitepace (s);
	argc++;
     }

   if (NULL == (argv = (char **)malloc((argc+1)*sizeof(char *))))
     {
	BBftp_Myexitcode = 35;
	printmessage(stderr,CASE_ERROR,BBftp_Myexitcode, "Error allocating memory for %s : %s\n", cmdstr, strerror(errno)) ;
	return NULL;
     }

   s = cmdstr;
   argc = 0;
   while (1)
     {
	char *e;
	s = skip_whitepace (s);
	if (*s == 0) break;
	e = skip_non_whitepace (s);
	if (NULL == (argv[argc] = strndup (s, (e-s))))
	  {
	     BBftp_Myexitcode = 35;
	     printmessage(stderr,CASE_ERROR,BBftp_Myexitcode, "Error allocating memory for %s : %s\n", cmdstr, strerror(errno)) ;
	     free_argv (argv, argc);
	     return NULL;
	  }
	s = e;
	argc++;
     }
   argv[argc] = NULL;

   *argcp = argc;
   return argv;
}

typedef struct
{
   const char *cmd;
   int min_num_args, max_num_args;
   int (*func)(char **, int, int *, int *);
}
Cmd_Type;

static int do_cd (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_cd (argv[1], errcodep);
   return 0;
}

static int do_lcd (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_lcd (argv[1], errcodep);
   return 0;
}

static int do_dir (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_dir (argv[1], errcodep);
   return 0;
}

static char *get_basename (char *file)
{
   char *f;

   f = file + strlen (file);
   while (f > file)
     {
	f--;
	if (*f == '/') return f+1;
     }
   return f;
}

static char *allocate_filename (size_t len)
{
   char *file = (char *)malloc(len+1);
   if (file == NULL)
     {
	BBftp_Myexitcode = 35;
	printmessage(stderr,CASE_ERROR, BBftp_Myexitcode,
		     "Error allocating memory for filename : %s\n", strerror(errno));
     }
   return file;
}

/* This function allocates 2 file names *aoutp and *boutp
 * and sets them as follows:
 *
 *  If a ends in / then
      *aoutp = a/basename(ref)
 *   else
 *    *aoutp = a
 * if (boutp != NULL) then
 *  *boutp = *aoutp + extra_len bytes allocated
 */
static int allocate_curr_and_real_filenames (char *a, char *ref,
					     unsigned int extra_len,
					     char **aoutp, char **boutp)
{
   char *base, *aout, *bout;
   unsigned int len, lena;

   base = get_basename (ref);
   len = strlen (a);
   lena = len + strlen(base);

   aout = allocate_filename (lena);
   if (aout == NULL) return -1;

   strcpy (aout, a);
   if (a[len-1] == '/') strcpy (aout+len, base);
   *aoutp = aout;

   if (boutp == NULL)
     return 0;

   if (NULL == (bout = allocate_filename (lena + extra_len)))
     {
	*aoutp = NULL;
	free (aout);
	return -1;
     }

   strcpy (bout, aout);
   *boutp = bout;
   return 0;
}

static int do_get (char **argv, int argc, int *retcodep, int *errcodep)
{
   char hostname[64];
   char *remfile, *localfile, *curr_file, *real_file;
   unsigned int extra_len;

   remfile = argv[1];

   if (argc == 2)
     localfile = "./";
   else
     localfile = argv[2];

   extra_len = sizeof(hostname)+64;
   if (-1 == allocate_curr_and_real_filenames (localfile, remfile, extra_len, &curr_file, &real_file))
     return -1;

   if (((BBftp_Transferoption & TROPT_TMP) == TROPT_TMP)
#ifdef CASTOR
       && (0 == ((BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O))
#endif
      )
     {
	if (-1 == gethostname (hostname, sizeof(hostname)))
	  *hostname = 0;
	(void) snprintf(real_file + strlen(real_file), extra_len, ".bbftp.tmp.%s.%d", hostname, getpid());
     }

   BBftp_Curfilename = curr_file;
   BBftp_Realfilename = real_file;

   *retcodep = bbftp_get (remfile, errcodep) ;
   BBftp_State = 0;

   return 0;
}

/* put local remote
 * put local remote/ ==> put local remote/basename(local)
 * put local ==> put local basename(local)
 */
static int do_put (char **argv, int argc, int *retcodep, int *errcodep)
{
   char *remfile, *localfile, *curr_file, *real_file;
   unsigned int len;

   localfile = argv[1];

   if (argc == 3)
     remfile = argv[2];
   else
     remfile = get_basename (localfile);

   if (-1 == allocate_curr_and_real_filenames (remfile, localfile, 0, &remfile, NULL))
     return -1;

   len = strlen (localfile);
   curr_file = allocate_filename (len);
   if (curr_file == NULL)
     {
	free (remfile);
	return -1;
     }
   real_file = allocate_filename (len);
   if (real_file == NULL)
     {
	free (curr_file);
	free (remfile);
	return -1;
     }
   strcpy (curr_file, localfile);
   strcpy (real_file, localfile);

   BBftp_Curfilename = curr_file;
   BBftp_Realfilename = real_file;

   *retcodep = bbftp_put (remfile, errcodep) ;
   free (remfile);

   return 0;
}

static int do_mget (char **argv, int argc, int *retcodep, int *errcodep)
{
   char *remfile, *localfile;

   remfile = argv[1];

   if (argc == 2)
     localfile = "./";
   else
     localfile = argv[2];

   *retcodep = bbftp_mget (remfile, localfile, errcodep) ;

   return 0;
}

static int do_mput (char **argv, int argc, int *retcodep, int *errcodep)
{
   char *remfile, *localfile;

   localfile = argv[1];

   if (argc == 2)
     remfile = "./";
   else
     remfile = argv[2];

   *retcodep = bbftp_mput (localfile, remfile, errcodep) ;

   return 0;
}

static int do_mkdir (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_mkdir (argv[1], errcodep);

   return 0;
}

static int do_rm (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_rm (argv[1], errcodep);

   return 0;
}

static int do_stat (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_stat (argv[1], errcodep);

   return 0;
}

static int do_df (char **argv, int argc, int *retcodep, int *errcodep)
{
   (void) argc;
   *retcodep = bbftp_statfs (argv[1], errcodep);

   return 0;
}

static int do_set_xxx (char **argv, int argc, int *retcodep, int *errcodep, int base, int *valp, int doverbose)
{
   char *basestr;
   int ret, val;
   unsigned int uval;

   (void) argc;

   switch (base)
     {
      case 8:
	ret = sscanf (argv[1], "%o", &uval);
	val = (int)uval;
	basestr = "octal";
	break;

      case -10:
      case 10:
      default:
	ret = sscanf (argv[1], "%d", &val);
	basestr = "decimal";
	break;
     }

   if ((ret != 1) || ((ret <= 0) && (base > 0)))
     {
	BBftp_Myexitcode = 26 ;
	printmessage(stderr,CASE_ERROR,BBftp_Myexitcode, "Incorrect command : %s %s (value must be positive %s)\n",
		     argv[0], argv[1], basestr);
	*retcodep = -1;
	*errcodep = BBftp_Myexitcode;
	return -1;
     }

   *valp = val;

   if (doverbose && BBftp_Verbose)
     {
	printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s %s\n", argv[0], argv[1]) ;
	printmessage(stdout,CASE_NORMAL,0, "%s", "<< OK\n") ;
     }

   *retcodep = 0;
   *errcodep = 0;
   return 0;
}

static int do_setbuffersize (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Buffersizeperstream, 1);
}

static int do_setlocalumask (char **argv, int argc, int *retcodep, int *errcodep)
{
   int ret = do_set_xxx (argv, argc, retcodep, errcodep, 8, &BBftp_Localumask, 1);
   if (ret == 0)
     umask (BBftp_Localumask);
   return ret;
}

static int do_setnbstream (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Nbport, 1);
}

static int do_setremotecos (char **argv, int argc, int *retcodep, int *errcodep)
{
   int val;

   if (-1 == do_set_xxx (argv, argc, retcodep, errcodep, -10, &val, 0))
     return -1;
   if (BB_RET_OK == (*retcodep = bbftp_setremotecos (val, errcodep)))
     BBftp_Remotecos = val;
   return 0;
}

static int do_setremoteumask (char **argv, int argc, int *retcodep, int *errcodep)
{
   int val;

   if (-1 == do_set_xxx (argv, argc, retcodep, errcodep, 8, &val, 0))
     return -1;

   *retcodep = bbftp_setremoteumask (val, errcodep);
   if (*retcodep == BB_RET_OK) BBftp_remoteumask = val;
   return 0;
}

static int do_setrecvwinsize (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Recvwinsize, 1);
}

static int do_setsendwinsize (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Sendwinsize, 1);
}

static int do_setackto (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Ackto, 1);
}

static int do_setrecvcontrolto (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Recvcontrolto, 1);
}

static int do_setsendcontrolto (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Sendcontrolto, 1);
}

static int do_setdatato (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 10, &BBftp_Datato, 1);
}

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
static int do_setlocalcos (char **argv, int argc, int *retcodep, int *errcodep)
{
   return do_set_xxx (argv, argc, retcodep, errcodep, 8, &BBftp_Localcos, 1);
}
#endif

static int do_setoption (char **argv, int argc, int *retcodep, int *errcodep)
{
   char *option;
   int set, flag;

   (void) argc;
   option = argv[1];
   set = strncmp (option, "no", 2);
   if (set == 0) option += 2;

   flag = 0;
   if (0 == strcmp (option, "createdir")) flag = TROPT_DIR;
   if (0 == strcmp (option, "tmpfile")) flag = TROPT_TMP;
   if (0 == strcmp (option, "remoterfio")) flag = TROPT_RFIO;
   if (0 == strcmp (option, "localrfio")) flag = TROPT_RFIO_O;
   if (0 == strcmp (option, "keepmode")) flag = TROPT_MODE;
   if (0 == strcmp (option, "keepaccess")) flag = TROPT_ACC;
   if (0 == strcmp (option, "gzip")) flag = TROPT_GZIP;
   if (0 == strcmp (option, "qbss")) flag = TROPT_QBSS;

   if (flag == 0)
     {
	BBftp_Myexitcode = 26 ;
	printmessage(stderr,CASE_ERROR, BBftp_Myexitcode, "Incorrect command : %s %s\n", argv[0], argv[1]) ;
	*retcodep = -1;
	return -1;
     }
   if (set == 0)
     BBftp_Transferoption &= ~flag;
   else
     {
#if !defined(WITH_RFIO) && !defined(WITH_RFIO64)
	if (flag == TROPT_RFIO_O)
	  {
	     BBftp_Myexitcode = 26 ;
	     printmessage(stderr,CASE_ERROR,BBftp_Myexitcode, "Incorrect command : %s %s (RFIO not supported)\n",
			  argv[0], argv[1]);
	     *retcodep = -1;
	     return -1 ;
	  }
#endif
	BBftp_Transferoption |= flag;
     }

   if ( BBftp_Verbose)
     {
	printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s %s\n", argv[0], argv[1]) ;
	printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
     }

   *errcodep = 0;
   *retcodep = 0;
   return 0;
}

static Cmd_Type Cmd_Table[] =
{
   {"cd", 1, 1, do_cd},
   {"dir", 1, 1, do_dir},
   {"get", 1, 2, do_get},
   {"lcd", 1, 1, do_lcd},
   {"mget", 1, 2, do_mget},
   {"mkdir", 1, 1, do_mkdir},
   {"put", 1, 2, do_put},
   {"stat", 1, 1, do_stat},
   {"rm", 1, 1, do_rm},
   {"mput", 1, 2, do_mput},
   {"df", 1, 1, do_df},
   {"setbuffersize", 1, 1, do_setbuffersize},
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
   {"setlocalcos", 1, 1, do_setlocalcos},
#endif
   {"setlocalumask", 1, 1, do_setlocalumask},
   {"setnbstream", 1, 1, do_setnbstream},
   {"setoption", 1, 1, do_setoption},
   {"setremotecos", 1, 1, do_setremotecos},
   {"setremoteumask", 1, 1, do_setremoteumask},
   {"setrecvwinsize", 1, 1, do_setrecvwinsize},
   {"setsendwinsize", 1, 1, do_setsendwinsize},
   {"setrecvcontrolto", 1, 1, do_setrecvcontrolto},
   {"setsendcontrolto", 1, 1, do_setsendcontrolto},
   {"setackto", 1, 1, do_setackto},
   {"setdatato", 1, 1, do_setdatato},

   {NULL, 0, 0, NULL}
};

static Cmd_Type *find_cmd (const char *cmd)
{
   Cmd_Type *c;

   c = Cmd_Table;
   while (c->cmd != NULL)
     {
	if (0 == strcmp (cmd, c->cmd))
	  return c;
	c++;
     }

   return NULL;
}

int treatcommand (char *cmdstr)
{
   Cmd_Type *cmd;
   char **argv;
   int argc;
   int num_tries, errcode = 0, retcode = 0;

   cmdstr = skip_whitepace (cmdstr);
   if ((*cmdstr == '!') || (*cmdstr == '#') || (*cmdstr == 0))
     return 0;

   if (NULL == (argv = tokenize (cmdstr, &argc)))
     return -1;

   if (NULL == (cmd = find_cmd (argv[0])))
     {
        printmessage(stderr,CASE_ERROR, 25, "Unkown command : %s\n", argv[0]);
	free_argv (argv, argc);
	return -1;
     }

   if (((argc-1) < cmd->min_num_args) || ((argc-1) > cmd->max_num_args))   /* argc = 1 + num_args */
     {
	BBftp_Myexitcode = 26 ;
	printmessage (stderr, CASE_ERROR, BBftp_Myexitcode, "Incorrect command : %s\n", cmdstr) ;
	free_argv (argv, argc);
	return -1;
     }

   num_tries = BBftp_Globaltrymax;
   while (num_tries)
     {
	num_tries--;

	/*
	 ** check if the last try was not disconnected
	 */
	if ( BBftp_Connectionisbroken == 1 )
	  {
	     reconnecttoserver() ;
	     BBftp_Connectionisbroken = 0 ;
	  }

	if (-1 == (*cmd->func)(argv, argc, &retcode, &errcode))
	  break;

	switch (retcode)
	  {
	   case BB_RET_FT_NR:
	     /*
	      ** Fatal error no retry and connection is not broken
	      */
	     num_tries = 0;
	     break ;

	   case BB_RET_FT_NR_CONN_BROKEN:
	     /*
	      ** Fatal error no retry and connection is broken
	      */
	     BBftp_Connectionisbroken = 1 ;
	     num_tries = 0;
	     break ;

	   case BB_RET_OK:
	     /*
	      ** Success , no retry
	      */
	     num_tries = 0;
	     break;

	   case BB_RET_CONN_BROKEN:
	     /*
	      ** Retry needed with a new connection
	      */
	     BBftp_Connectionisbroken = 1;

	     /* fall through */
	   default:
	     /*
	      ** retcode > 0 means retry on this transfer
	      */
	     if (num_tries)
	       {
		  if ( BBftp_Verbose ) printmessage(stdout,CASE_NORMAL,0, "Retrying command waiting %d s\n",WAITRETRYTIME) ;
		  sleep(WAITRETRYTIME) ;
	       }
	  }
     }

   free_argv (argv, argc);

   if (retcode != 0)
     {
	BBftp_Myexitcode = errcode;
	return -1;
     }
   return 0;
}
