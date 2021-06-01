#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>

#include "bbftpd.h"
#include "_bbftpd.h"
#include "../includes/version.h"
#include "../includes/daemon.h"

static FILE *_pBBftp_Log_Fp;
static int Log_Mask = 0;
static int Log_Level = 0;
static int Use_Syslog = 0;
static char *Log_File;

typedef struct
{
   const char *name;
   int level;
}
Log_Level_Map_Type;

static Log_Level_Map_Type Log_Level_Map[] =
{
   {"EMERGENCY", BBFTPD_EMERG},
   {"ALERT", BBFTPD_ALERT},
   {"CRITICAL", BBFTPD_CRIT},
   {"ERROR", BBFTPD_ERR},
   {"WARNING", BBFTPD_WARNING},
   {"NOTICE", BBFTPD_NOTICE},
   {"INFORMATION", BBFTPD_INFO},
   {"DEBUG", BBFTPD_DEBUG},
   {NULL, -1},
};

const char *bbftpd_log_level_string (int level)
{
   Log_Level_Map_Type *map;

   map = Log_Level_Map;
   while (map->name != NULL)
     {
	if (map->level == level) return map->name;
	map++;
     }
   return "UNKNOWN";
}

int bbftpd_log_get_level (const char *name)
{
   Log_Level_Map_Type *map;

   if (name == NULL) return Log_Level;

   map = Log_Level_Map;
   while (map->name != NULL)
     {
	if (0 == strcmp (name, map->name))
	  return map->level;
	map++;
     }
   return -1;
}


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
     {
	Log_Mask = LOG_UPTO(log_level);
	Log_Level = log_level;
     }

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

static void write_to_log (int priority, char *msg)
{
   FILE *fp;

   if (Use_Syslog)
     {
	syslog (priority, "%s", msg);
	return;
     }

   if (NULL != (fp = _pBBftp_Log_Fp))
     {
	char timestamp[64];
	struct tm tm;
	time_t now;

	timestamp[0] = 0;
	if ((-1 != time (&now))
	    && (NULL != gmtime_r (&now, &tm)))
	  (void) strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm);

	(void) fprintf (fp, "%s\t%d\t%s\t", timestamp, getpid(), bbftpd_log_level_string(priority));
	(void) fputs (msg, fp);
	(void) fputs ("\n", fp);
	(void) fflush (fp);
     }
}

void bbftpd_log (int priority, const char *format, ...)
{
   char msg[4096];
   va_list ap;
   int save_errno = errno;

   if (0 == (Log_Mask & (LOG_MASK(priority))))
     return;

   va_start(ap, format);
   (void) vsnprintf (msg, sizeof(msg), format, ap);
   va_end(ap);

   write_to_log (priority, msg);
   errno = save_errno;
}

/* Log to files and stderr */
void bbftpd_log_stderr (int priority, const char *format, ...)
{
   char msg[4096];
   va_list ap;
   int save_errno = errno;

   va_start(ap, format);
   (void) vsnprintf (msg, sizeof(msg), format, ap);
   va_end(ap);

   if (Log_Mask & (LOG_MASK(priority)))
     write_to_log (priority, msg);

   (void) fputs (msg, stderr);
   if (*msg && (msg[strlen(msg)-1] != '\n'))
     (void) fputs ("\n", stderr);

   errno = save_errno;
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
