#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "bbftp.h"
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
