/*
 * Copyright 2001 by Lionel Schwarz, Computing Center IN2P3
 */

/*#ifdef HAVE_UNISTD_H*/
#include <unistd.h>
/*#endif*/
#include <syslog.h>
#include <errno.h>

#include "gfw-misc.h"
#include "gfw.h"

#ifndef gss_nt_service_name
#define gss_nt_service_name GSS_C_NT_HOSTBASED_SERVICE
#endif

/*#ifdef USE_STRING_H*/
#include <string.h>
/*#else
#include <strings.h>
#endif*/

FILE *log;
int debug_gfw=0;

/*extern int verbose;*/

/*
 * Function: server_acquire_creds
 *
 * Purpose: imports a service name and acquires credentials for it
 *
 * Arguments:
 *
 * 	service_name	(r) the ASCII service name
 * 	server_creds	(w) the GSS-API service credentials
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * The service name is imported with gss_import_name, and service
 * credentials are acquired with gss_acquire_cred.  If either opertion
 * fails, an error message is displayed and -1 is returned; otherwise,
 * 0 is returned.
 */
int server_acquire_creds(service_name, server_creds)
     char *service_name;
     gss_cred_id_t *server_creds;
{
     gss_buffer_desc name_buf;
     gss_name_t server_name = GSS_C_NO_NAME;
     OM_uint32 maj_stat, min_stat;

	 if (service_name) {
if(debug_gfw) fprintf(display_file,"service_name=%s\n",service_name) ;
		 name_buf.value = service_name;
		 name_buf.length = strlen(name_buf.value) + 1;
		 maj_stat = gss_import_name(&min_stat, &name_buf, 
				(gss_OID) gss_nt_service_name, &server_name);
		if (maj_stat != GSS_S_COMPLETE) {
			display_status("importing name", maj_stat, min_stat);
fflush(display_file);
	  		return -1;
		}
	 }

     maj_stat = gss_acquire_cred(&min_stat, server_name, 0,
				 GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
				 server_creds, NULL, NULL);
     if (maj_stat != GSS_S_COMPLETE) {
	  display_status("acquiring credentials", maj_stat, min_stat);
fflush(display_file);
	  return -1;
     }

     (void) gss_release_name(&min_stat, &server_name);

     return 0;
}

/*
 * Function: server_establish_context
 *
 * Purpose: establishses a GSS-API context as a specified service with
 * an incoming client, and returns the context handle and associated
 * client name
 *
 * Arguments:
 *
 * 	is		(r) input stream socket
 *	os		(r) output stream socket
 * 	service_creds	(r) server credentials, from gss_acquire_cred
 * 	context		(w) the established GSS-API context
 * 	client_name	(w) the client's ASCII name
 *	ret_flags	(w)	the flags returned
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Any valid client request is accepted.  If a context is established,
 * its handle is returned in context and the client name is returned
 * in client_name and 0 is returned.  If unsuccessful, an error
 * message is displayed and -1 is returned.
 */
int server_establish_context(is, os, server_creds, context, client_name, ret_flags)
     int is;
	 int os;
     gss_cred_id_t server_creds;
     gss_ctx_id_t *context;
     gss_buffer_t client_name;
     OM_uint32 *ret_flags;

{
     gss_buffer_desc send_tok, recv_tok;
     gss_name_t client;
     gss_OID doid;
     OM_uint32 maj_stat, min_stat;
     gss_buffer_desc	oid_name;

     *context = GSS_C_NO_CONTEXT;
     
if (debug_gfw) fprintf(display_file, "looping...\n");
     do {
if (debug_gfw) fprintf(display_file, "receiving token...\n");
if (debug_gfw) fflush(display_file);
	  if (recv_token(is, &recv_tok) < 0)
	       return -1;

if (debug_gfw) fprintf(display_file, "Received token (size=%d): \n", recv_tok.length);

if(debug_gfw) fprintf(display_file, "calling gss_accept_sec_context...\n");
	  maj_stat =
	       gss_accept_sec_context(&min_stat,
				      context,
				      server_creds,
				      &recv_tok,
				      GSS_C_NO_CHANNEL_BINDINGS,
				      &client,
				      &doid,
				      &send_tok,
				      ret_flags,
				      NULL, 	/* ignore time_rec */
				      NULL); 	/* ignore del_cred_handle */

	  if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
	       display_status("accepting context", maj_stat, min_stat);
	       (void) gss_release_buffer(&min_stat, &recv_tok);
	       return -1;
	  }

	  (void) gss_release_buffer(&min_stat, &recv_tok);

	  if (send_tok.length != 0) {
if (debug_gfw) fprintf(display_file, "Sending accept_sec_context token (size=%d):\n", send_tok.length);
	       if (send_token(os, &send_tok) < 0) {
		    fprintf(display_file, "failure sending token\n");
		    return -1;
	       }
if (debug_gfw) fprintf(display_file, "token sent\n");

	       (void) gss_release_buffer(&min_stat, &send_tok);
	  }

     } while (maj_stat & GSS_S_CONTINUE_NEEDED);
if (debug_gfw) fprintf(display_file, "COMPLETE\n");
     /* display the flags */
/*     display_ctx_flags(*ret_flags);

*/
     maj_stat = gss_display_name(&min_stat, client, client_name, &doid);
     if (maj_stat != GSS_S_COMPLETE) {
	  display_status("displaying name", maj_stat, min_stat);
	  return -1;
     }
     maj_stat = gss_release_name(&min_stat, &client);
     if (maj_stat != GSS_S_COMPLETE) {
	  display_status("releasing name", maj_stat, min_stat);
	  return -1;
     }

     return 0;

}


/*
 * Function: sign_server
 *
 * Purpose: Performs the "sign" service.
 *
 * Arguments:
 *
 * 	is		(r) input stream socket
 *	os		(r) output stream socket
 * 	service_creds	(r) server credentials
 * 
 * Returns: -1 on error
 *
 * Effects:
 *
 * sign_server establishes a context
 *
 * If any error occurs, -1 is returned.
 */
int sign_server(is, os, server_creds)
     int is;
	 int os;
     gss_cred_id_t server_creds;
{
     gss_buffer_desc client_name, logon_buf, out_buf;
     gss_ctx_id_t context;
     OM_uint32 maj_stat, min_stat;
     int i;
     int conf_state = 0;
     int ret_flags = 0;
     char	*cp;
     gss_name_t		src_name, targ_name;
     gss_buffer_desc	sname, tname;
     OM_uint32		lifetime;
     gss_OID		mechanism, name_type;
     int		is_local;
     OM_uint32		context_flags;
     int		is_open;
	 char 		*userid;
	int state;
     
     /* Establish a context with the client */
if (debug_gfw) fprintf(display_file, "establishing context...\n");
fflush(display_file);
     if (server_establish_context(is, os, server_creds, &context,
				  &client_name, &ret_flags) < 0)
	return(-1);
	  
     /* Get context information */
     maj_stat = gss_inquire_context(&min_stat, context,
				    &src_name, &targ_name, &lifetime,
				    &mechanism, &context_flags,
				    &is_local,
				    &is_open);
     if (maj_stat != GSS_S_COMPLETE) {
	 display_status("inquiring context", maj_stat, min_stat);
	 return -1;
     }

     maj_stat = gss_display_name(&min_stat, src_name, &sname,
				 &name_type);
     if (maj_stat != GSS_S_COMPLETE) {
	 display_status("displaying source name", maj_stat, min_stat);
	 return -1;
     }
     maj_stat = gss_display_name(&min_stat, targ_name, &tname,
				 (gss_OID *) NULL);
     if (maj_stat != GSS_S_COMPLETE) {
	 display_status("displaying target name", maj_stat, min_stat);
	 return -1;
     }
/*     fprintf(stderr, "\"%.*s\" to \"%.*s\", lifetime %d, flags %x, %s, %s\n",
	     (int) sname.length, (char *) sname.value,
	     (int) tname.length, (char *) tname.value, lifetime,
	     context_flags,
	     (is_local) ? "locally initiated" : "remotely initiated",
	     (is_open) ? "open" : "closed");*/

	if (globus_gss_assist_gridmap((char *) sname.value, &userid) != 0) {
		(void) gss_release_name(&min_stat, &src_name);
		(void) gss_release_name(&min_stat, &targ_name);
		(void) gss_release_buffer(&min_stat, &sname);
		(void) gss_release_buffer(&min_stat, &tname);

if (debug_gfw) fprintf(display_file,"Accepted connection: \"%.*s\"\n",
	    (int) client_name.length, (char *) client_name.value);
		(void) gss_release_buffer(&min_stat, &client_name);

     /* Delete context */
     	maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
		if (maj_stat != GSS_S_COMPLETE) {
			display_status("deleting context", maj_stat, min_stat);
			return(-1);
		}
		return(-1);
	}

	 
     (void) gss_release_name(&min_stat, &src_name);
     (void) gss_release_name(&min_stat, &targ_name);
     (void) gss_release_buffer(&min_stat, &sname);
     (void) gss_release_buffer(&min_stat, &tname);

if (debug_gfw) fprintf(display_file,"Accepted connection: \"%.*s\"\n",
	    (int) client_name.length, (char *) client_name.value);
     (void) gss_release_buffer(&min_stat, &client_name);

     /* Delete context */
     maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
     if (maj_stat != GSS_S_COMPLETE) {
	display_status("deleting context", maj_stat, min_stat);
	return(-1);
     }

     /*fflush(log);*/

     return(0);
}

/*
 * Function: gfw_server_auth
 *
 * Purpose: Performs the authentication on a specified service
 *			to allow a client connected by the TCP connection
 *			to authenticate to this server for this service
 *
 * Arguments:
 *
 * 	is		(r) input stream socket
 *	os		(r) output stream socket
 * 	service_name	(r) the ASCII service name
 *	logmessage		(w) the message written int	case of failure
 * 
 * Returns: -1 on error, 0 else
 */
int gfw_server_auth(is, os, service_name, logmessage)
     int is;
     int os;
	 char *service_name;
	 char *logmessage;
{
     gss_cred_id_t server_creds;
     OM_uint32 min_stat;
	 char *logfilename;
     log = stdout;
/*     display_file = stdout;*/
/*     sprintf(logfilename,"/tmp/gfw-server.log") ;*/
if (debug_gfw) display_file = fopen("/tmp/gfw-server.log","w") ;
	if (server_acquire_creds(service_name, &server_creds) < 0) {
if (debug_gfw) fprintf(display_file,"server_acquire_creds():%s\n",strerror(errno)) ;
	logmessage = strerror(errno);
	 return -1;
	}
	
	if (sign_server(is, os, server_creds) < 0) {
if (debug_gfw) fprintf(display_file,"sign_server():%s\n", strerror(errno));
		logmessage = strerror(errno);
		return -1;
	}

	(void) gss_release_cred(&min_stat, &server_creds);

	return 0;
}
