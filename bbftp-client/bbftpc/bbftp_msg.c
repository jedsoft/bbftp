#include "bbftp.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>

static int print_timestamp (FILE *strm)
{
   time_t  logtime;
   char    clogtime[64];

   if (BBftp_Timestamp == 0) return 0;

   /*
    ** Get time
    */
   if (-1 == time(&logtime))
     return -1;

   if ( strftime(clogtime,sizeof(clogtime),"%a %b %d %H:%M:%S (%Z)",localtime(&logtime)) == 0 )
     return -1;

   (void) fprintf(strm,"%s : ",clogtime) ;
   return 0;
}

void printmessage (FILE *strm , int flag, int errcode, char *fmt, ...)
{
   va_list ap;

   (void) print_timestamp (strm);

   switch (flag)
     {
      case CASE_ERROR:
      case CASE_FATAL_ERROR:
        (void) fprintf(strm,"BBFTP-ERROR-%05d : ",errcode) ;
	break;

      case CASE_WARNING:
        (void) fprintf(strm,"BBFTP-WARNING-%05d : ",errcode) ;

      default:
	break;
    }
    /*
    ** And print the requested string 
    */
   va_start(ap,fmt);
   (void) vfprintf(strm, fmt, ap);
   va_end(ap);
   fflush(strm) ;

   if (flag == CASE_FATAL_ERROR)
     exit (errcode);
}

void printstdout (const char *fmt, ...)
{
   va_list ap;
   va_start(ap,fmt);
   (void) vfprintf(stdout, fmt, ap);
   va_end(ap);
   fflush(stdout) ;
}

#ifdef DARWIN
# define OPEN_FLAGS (O_CREAT|O_WRONLY|O_TRUNC)
#else
# define OPEN_FLAGS (O_CREAT|O_WRONLY|O_SYNC|O_TRUNC)
#endif

static int Res_FD = -1;

int bbftp_res_open (const char *file)
{
   int fd;

   while (-1 == (fd = open (file, OPEN_FLAGS, 0777)))
     {
	if (errno == EINTR)
	  continue;

	printmessage(stderr, CASE_FATAL_ERROR,22, "Error opening result file (%s) : %s\n", file, strerror(errno));
	return -1;
     }

   Res_FD = fd;
   return 0;
}

void bbftp_res_close (void)
{
   if (Res_FD != -1)
     {
	(void) close (Res_FD);
	Res_FD = -1;
     }
}

int bbftp_res_write (const char *str)
{
   int fd;
   size_t len;

   if (-1 == (fd = Res_FD))
     {
	fd = fileno (stdout);
	if (fd == -1) return -1;
     }

   len = strlen (str);
   while (len > 0)
     {
	ssize_t n = write (fd, str, len);
	if (n == -1)
	  {
	     if (errno == EINTR)
	       continue;

	     printmessage(stderr,CASE_ERROR,79, "Error writing to to result file: %s\n", strerror(errno));
	     return -1;
	  }
	len -= n;
	str += n;
     }
   return 0;
}

int bbftp_res_printf (const char *fmt, ...)
{
   va_list ap;
   char buf[1024];

   va_start(ap,fmt);
   (void) vsnprintf (buf, sizeof(buf), fmt, ap);
   va_end(ap);

   return bbftp_res_write (buf);
}
