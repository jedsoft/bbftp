#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>

#include "bbftpd.h"
#include "_bbftpd.h"
#include "../includes/version.h"
#include "../includes/daemon.h"

static FILE *_pBBftp_Log_Fp;
static int Log_Mask = 0;
static int Use_Syslog = 0;
static char *Log_File;

static void open_syslog (void)
{
   static char ident[64];

   /* openlog function requires that this be static */
   if (ident[0] == 0)
     (void) snprintf (ident, sizeof(ident), "bbftpd v%s", VERSION) ;

   openlog (ident, LOG_PID | LOG_NDELAY, BBFTPD_FACILITY);
   setlogmask (Log_Mask);
}


int bbftpd_log_open (int use_syslog, int log_level, const char *logfile)
{
   if (log_level != -1)
     Log_Mask = LOG_UPTO(log_level);

   if ((logfile != NULL) && (logfile != Log_File))
     {
	if (Log_File != NULL) free (Log_File);

	if (NULL == (Log_File = bbftpd_strdup (logfile)))
	  {
	     (void) fprintf (stderr, "Unable to allocate space for the logfile name\n");
	     return -1;
	  }
	if (NULL == (_pBBftp_Log_Fp = fopen (Log_File, "a")))
	  {
	     (void) fprintf (stderr, "Unable to open logfile %s: %s\n", Log_File, strerror(errno));
	     return -1;
	  }
	return 0;
     }

   if (use_syslog)
     {
	open_syslog ();
     }

   return 0;
}

void bbftpd_log (int priority, const char *format, ...)
{
   char msg[4096];
   va_list ap;
   FILE *fp;

   if (0 == (Log_Mask & (LOG_MASK(priority))))
     return;

   va_start(ap, format);
   (void) vsnprintf (msg, sizeof(msg), format, ap);
   va_end(ap);

   if (Use_Syslog)
     {
	syslog (priority, "%s", msg);
	return;
     }

   if (NULL != (fp = _pBBftp_Log_Fp))
     {
	(void) fputs (msg, _pBBftp_Log_Fp);
	(void) fputs ("\n", _pBBftp_Log_Fp);
	(void) fflush (_pBBftp_Log_Fp);
     }
}

void bbftpd_log_close (void)
{
   if (Use_Syslog) closelog ();
   if (_pBBftp_Log_Fp != NULL)
     {
	(void) fclose (_pBBftp_Log_Fp);
	_pBBftp_Log_Fp = NULL;
     }
}

int bbftpd_log_reopen (void)
{
   return bbftpd_log_open (Use_Syslog, -1, Log_File);
}
