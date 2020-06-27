/*
 * Copyright 2001 by Lionel Schwarz, Computing Center IN2P3
 * 
 */

#include <stdio.h>
#include "gfw-misc.h"
#include "gfw.h"

#ifndef gss_nt_service_name
#define gss_nt_service_name GSS_C_NT_HOSTBASED_SERVICE
#endif

/*extern int verbose;*/
/*
 * Function: gfw_client_auth
 *
 * Purpose: authenticate a client on a remote server for a specific service
 *
 * Arguments:
 *
 * 	is		(r) the input stream socket
 * 	os		(r) the 
 * 	service_name	(r) the GSS-API service name to authenticate to	
 * 	logmessage		(w) the message written int	case of failure
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 * 
 * gfw_client_auth establishes a GSS-API context with service_name 
 * over the connection designated by the two sockets. 
 * -1 is returned if any step fails, otherwise 0 is returned.
 */
int gfw_client_auth(int is, int os, char *service_name, char *logmessage)
{
     gss_ctx_id_t gss_context;
     OM_uint32 ret_flags;
     OM_uint32 maj_stat, min_stat;
     size_t	i;
     gss_buffer_desc send_tok, recv_tok, *token_ptr;
     gss_name_t target_name;

     OM_uint32 deleg_flag=0;
     gss_OID oid = GSS_C_NULL_OID;

     gss_name_t		src_name, targ_name;
     gss_buffer_desc	sname, tname;
     gss_buffer_desc client_name, xmit_buf, logon_buf;
     OM_uint32		lifetime;
     gss_OID		mechanism, name_type;
     int		is_local;
     OM_uint32		context_flags;
     int		is_open;
	 char 		*userid;
	 int		conf_state=0;
     display_file = stdout;

#ifdef DEBUG
    printf("Entering gfw-client...\n");
#endif
     /*
      * Import the name into target_name.  Use send_tok to save
      * local variable space.
      */
     send_tok.value = service_name;
     send_tok.length = strlen(service_name) + 1;
     maj_stat = gss_import_name(&min_stat, &send_tok,
				(gss_OID) gss_nt_service_name, &target_name);
     if (maj_stat != GSS_S_COMPLETE) {
		  status_to_string(maj_stat, logmessage);
		  display_status("parsing name", maj_stat, min_stat);
		  return -1;
     }
     
     /*
      * Perform the context-establishement loop.
      *
      */
     
     token_ptr = GSS_C_NO_BUFFER;
     gss_context = GSS_C_NO_CONTEXT;

     do {
	  maj_stat =
	       gss_init_sec_context(&min_stat,
				    GSS_C_NO_CREDENTIAL,
				    &gss_context,
				    target_name,
				    oid,
				    GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG |	deleg_flag,
				    0,
				    NULL,	/* no channel bindings */
				    token_ptr,
				    NULL,	/* ignore mech type */
				    &send_tok,
				    &ret_flags,
				    NULL);	/* ignore time_rec */

	  if (token_ptr != GSS_C_NO_BUFFER)
	       (void) gss_release_buffer(&min_stat, &recv_tok);

	  if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
		  status_to_string(maj_stat, logmessage);
	       display_status("initializing context", maj_stat, min_stat);
	       (void) gss_release_name(&min_stat, &target_name);
	       return -1;
	  }

	  if (send_tok.length != 0) {
#ifdef DEBUG 
    fprintf(stdout, "sending token...\n");
#endif
	       if (send_token(os, &send_tok) < 0) {
			  strcpy(logmessage, "Error while sending token");
		    (void) gss_release_buffer(&min_stat, &send_tok);
		    (void) gss_release_name(&min_stat, &target_name);
		    return -1;
	       }
#ifdef DEBUG 
    fprintf(stdout, "token sent (size=%d)\n",send_tok.length);
#endif
	  }
	  (void) gss_release_buffer(&min_stat, &send_tok);
	  
	  if (maj_stat == GSS_S_CONTINUE_NEEDED) {
#ifdef DEBUG 
    fprintf(stdout, "receiving token...\n");
#endif
	       if (recv_token(is, &recv_tok) < 0) {
			  strcpy(logmessage, "Error while receiving token");
		    (void) gss_release_name(&min_stat, &target_name);
		    return -1;
	       }
	       token_ptr = &recv_tok;
#ifdef DEBUG 
    fprintf(stdout, "token received (size=%d)\n",recv_tok.length);
#endif
	  }
     } while (maj_stat & GSS_S_CONTINUE_NEEDED);

#ifdef DEBUG 
    fprintf(stdout, "COMPLETE\n");
#endif
     (void) gss_release_name(&min_stat, &target_name);

	 (void) gss_delete_sec_context(&min_stat, &gss_context, GSS_C_NO_BUFFER);

     return 0;
}


