/*
 * Copyright 2001 by Lionel Schwarz, Computing Center IN2P3
 */

#include <stdio.h>
#include <errno.h>

/*#ifdef HAVE_UNISTD_H*/
#include <unistd.h>
/*#endif*/

#include <syslog.h>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "gfw-misc.h"
#include "gfw.h"

#include <globus_common.h>
#include <globus_gsi_cert_utils.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern char *malloc();
#endif

FILE *display_file;
/*int verbose = 1;*/

static void display_status_1
	 (char *m, OM_uint32 code, int type) ;

int write_all(int fildes, char *buf, unsigned int nbyte)
{
     int ret;
     char *ptr;

     for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {
	  ret = write(fildes, ptr, nbyte);
	  if (ret < 0) {
	       if (errno == EINTR)
		    continue;
	       return(ret);
	  } else if (ret == 0) {
	       return(ptr-buf);
	  }
     }

     return(ptr-buf);
}

int read_all(int fildes, char *buf, unsigned int nbyte)
{
     int ret;
     char *ptr;

     for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {
	  ret = read(fildes, ptr, nbyte);
	  if (ret < 0) {
	       if (errno == EINTR)
		    continue;
	       return(ret);
	  } else if (ret == 0) {
	       return(ptr-buf);
	  }
     }

     return(ptr-buf);
}

/*
 * Function: send_token
 *
 * Purpose: Writes a token to a file descriptor.
 *
 * Arguments:
 *
 * 	s		(r) an open file descriptor
 * 	tok		(r) the token to write
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * send_token writes the token length (as a network long) and then the
 * token data to the file descriptor s.  It returns 0 on success, and
 * -1 if an error occurs or if it could not write all the data.
 */
int send_token(s, tok)
     int s;
     gss_buffer_t tok;
{
	int ret;
	long len;

#ifndef WORDS_BIGENDIAN
	len = htonl(tok->length);
#else
	len = tok->length;
#endif

	ret = write_all(s, (char *) &len, sizeof(len));
	if (ret < 0) {
		perror("sending token length");
		return -1;
	} else if (ret != sizeof(len)) {
/*	 if (display_file)
	     fprintf(display_file, 
		     "sending token length: %d of %d bytes written\n", 
		     ret, sizeof(len));*/
		return -1;
	}

	ret = write_all(s, tok->value, tok->length);
	if (ret < 0) {
		perror("sending token data");
		return -1;
	} else if (ret != tok->length) {
/*	 if (display_file)
	     fprintf(display_file, 
		     "sending token data: %d of %d bytes written\n", 
		     ret, tok->length);*/
		return -1;
	}
	return 0;
}

/*
 * Function: recv_token
 *
 * Purpose: Reads a token from a file descriptor.
 *
 * Arguments:
 *
 * 	s		(r) an open file descriptor
 * 	tok		(w) the read token
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * recv_token reads the token length (as a network long), allocates
 * memory to hold the data, and then reads the token data from the
 * file descriptor s.  It blocks to read the length and data, if
 * necessary.  On a successful return, the token should be freed with
 * gss_release_buffer.  It returns 0 on success, and -1 if an error
 * occurs or if it could not read all the data.
 */
int recv_token(s, tok)
     int s;
     gss_buffer_t tok;
{
	int ret;

	ret = read_all(s, (char *) &tok->length, sizeof(tok->length));
	if (ret < 0) {
		perror("reading token length");
		return -1;
	} else if (ret != sizeof(tok->length)) {
/*	 if (display_file)
	     fprintf(display_file, 
		     "reading token length: %d of %d bytes read\n", 
		     ret, sizeof(tok->length));*/
		return -1;
	}

#ifndef WORDS_BIGENDIAN
	tok->length = ntohl(tok->length);
#endif

	tok->value = (char *) malloc(tok->length);
	if (tok->value == NULL) {
	 /*if (display_file)
	     fprintf(display_file, 
		     "Out of memory allocating token data\n");*/
		return -1;
	}

	ret = read_all(s, (char *) tok->value, tok->length);
	if (ret < 0) {
		perror("reading token data");
		free(tok->value);
		return -1;
	} else if (ret != tok->length) {
	  /*fprintf(stderr, "sending token data: %d of %d bytes written\n", 
		  ret, tok->length);*/
		free(tok->value);
		return -1;
	}

	return 0;
}

static void display_status_1(char *m, OM_uint32 code, int type)
{
     OM_uint32 maj_stat, min_stat;
     gss_buffer_desc msg;
     OM_uint32 msg_ctx;
     
     msg_ctx = 0;
     while (1) {
	  maj_stat = gss_display_status(&min_stat, code,
				       type, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
	  if (display_file)
	      fprintf(display_file, "GSS-API Error : %s (%s)\n", (char *)msg.value, m); 
/*	  syslog(LOG_ERR,"GSS-API Error : %s\n", (char *)msg.value);*/
	  (void) gss_release_buffer(&min_stat, &msg);
	  
	  if (!msg_ctx)
	       break;
     }
}

void status_to_string(stat, logmsg)
     OM_uint32 stat;
     char *logmsg;
{
     OM_uint32 maj_stat, min_stat;
     gss_buffer_desc msg;
     OM_uint32 msg_ctx;
     
     msg_ctx = 0;
     maj_stat = gss_display_status(&min_stat, stat,
				       GSS_C_GSS_CODE, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
	 strcpy(logmsg, (char *)msg.value);
	  (void) gss_release_buffer(&min_stat, &msg);
}
	
void gfw_status_to_string(stat, logmsg)
     OM_uint32 stat;
     char **logmsg;
{
     OM_uint32 maj_stat, min_stat;
     gss_buffer_desc msg;
     OM_uint32 msg_ctx;
     
     msg_ctx = 0;
     maj_stat = gss_display_status(&min_stat, stat,
				       GSS_C_GSS_CODE, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
         /*strcpy(logmsg, (char *)msg.value);*/
      *logmsg = (char *)msg.value;
	  /*(void) gss_release_buffer(&min_stat, &msg);*/
}

void gfw_status_to_strings(major_stat, minor_stat, msgs)
     OM_uint32 major_stat;
     OM_uint32 minor_stat;
     gfw_msgs_list **msgs;
{
     OM_uint32 maj_stat, min_stat;
     gss_buffer_desc msg;
     OM_uint32 msg_ctx;
     gfw_msgs_list *new_msg;
     gfw_msgs_list *first_item;
     gfw_msgs_list *used_item;
     
     msg_ctx = 0;
     first_item = NULL;
     while (1) {
         maj_stat = gss_display_status(&min_stat, major_stat,
				       GSS_C_GSS_CODE, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
         if ((new_msg = (gfw_msgs_list*)malloc(sizeof(gfw_msgs_list))) == NULL) {
             printf("ERROR malloc struct\n");
         }
         if ((new_msg->msg = (char *) malloc( (int)msg.length + 1)) == NULL) {
             printf("ERROR malloc char*\n");
         }
         sprintf(new_msg->msg,"%s",(char *)msg.value) ;
         new_msg->next = NULL ;
         if ( first_item == NULL ) {
             first_item = new_msg ;
         } else {
             used_item = first_item ;
             while ( used_item->next != NULL ) used_item = used_item->next ;
             used_item->next = new_msg ;
         }
         if (!msg_ctx)
	       break;
     }
     msg_ctx = 0;
     while (1) {
         maj_stat = gss_display_status(&min_stat, minor_stat,
				       GSS_C_MECH_CODE, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
         if ((new_msg = (gfw_msgs_list*)malloc(sizeof(gfw_msgs_list))) == NULL) {
             printf("ERROR malloc struct\n");
         }
         if ((new_msg->msg = (char *) malloc( (int)msg.length + 1)) == NULL) {
             printf("ERROR malloc char*\n");
         }
         sprintf(new_msg->msg,"%s",(char *)msg.value) ;
         new_msg->next = NULL ;
         if ( first_item == NULL ) {
             first_item = new_msg ;
         } else {
             used_item = first_item ;
             while ( used_item->next != NULL ) used_item = used_item->next ;
             used_item->next = new_msg ;
         }
         if (!msg_ctx)
	       break;
     }
     *msgs = first_item;
}
/*
 * Function: display_status
 *
 * Purpose: displays GSS-API messages
 *
 * Arguments:
 *
 * 	msg		a string to be displayed with the message
 * 	maj_stat	the GSS-API major status code
 * 	min_stat	the GSS-API minor status code
 *
 * Effects:
 *
 * The GSS-API messages associated with maj_stat and min_stat are
 * displayed on stderr, each preceeded by "GSS-API error <msg>: " and
 * followed by a newline.
 */
void display_status(msg, maj_stat, min_stat)
     char *msg;
     OM_uint32 maj_stat;
     OM_uint32 min_stat;
{
    display_status_1(msg, maj_stat, GSS_C_GSS_CODE);
    display_status_1(msg, min_stat, GSS_C_MECH_CODE);
}
/*
 * 
char *gfw_get_globus_modules_versions()
{
     globus_version_t                   version;
     extern globus_module_descriptor_t	globus_i_common_module,
     					globus_i_gsi_cert_utils_module;
     char printstring[30];
     char versionsstring[500];
     globus_module_get_version(&globus_i_common_module, &version);
     sprintf(printstring, "%s-%d.%d; ", globus_i_common_module.module_name, version.major, version.minor);
     strcpy(versionsstring, printstring);
     globus_module_get_version(&globus_i_gsi_cert_utils_module, &version);
     sprintf(printstring, "%s-%d.%d", globus_i_gsi_cert_utils_module.module_name, version.major, version.minor);
     strcat(versionsstring, printstring);
     return versionsstring;
}
*/
