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

/*
 * Function: gfw_init_sec_context
 *
 * Purpose: initiate a context
 *
 * Arguments:
 *
 *  minor_status    (w) the second status
 * 	is              (r) the input stream socket
 * 	os              (r) the output stream socket
 * 	service_name    (r) the GSS-API service name to authenticate to	
 *
 * Returns: major status
 *
 * Effects:
 * 
 * gfw_init_sec_context establishes a GSS-API context with service_name 
 * over the connection designated by the two sockets. 
 */

OM_uint32 gfw_init_sec_context(
OM_uint32 * minor_status, 
int         is, 
int         os, 
char *      service_name)
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

    /*
     * Import the name into target_name.  Use send_tok to save
     * local variable space.
     */
    if (service_name) {
        send_tok.value = service_name;
        send_tok.length = strlen(service_name) + 1;
        maj_stat = gss_import_name(minor_status, 
                                    &send_tok,
                                    (gss_OID) gss_nt_service_name, 
                                    &target_name);
        if (maj_stat != GSS_S_COMPLETE) {
            return maj_stat;
        }
    }
     
     /*
      * Perform the context-establishement loop.
      *
      */
     
    token_ptr = GSS_C_NO_BUFFER;
    gss_context = GSS_C_NO_CONTEXT;

    do {
        maj_stat = gss_init_sec_context(minor_status,
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
            /*(void) gss_release_name(&min_stat, &target_name);*/
            return maj_stat;
        }

        if (send_tok.length != 0) {
#ifdef DEBUG 
            fprintf(stdout, "sending token...\n");
#endif
            if (send_token(os, &send_tok) < 0) {
                (void) gss_release_buffer(&min_stat, &send_tok);
                (void) gss_release_name(&min_stat, &target_name);
                return maj_stat;
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
                (void) gss_release_name(&min_stat, &target_name);
                return maj_stat;
            }
            /* if token is void, conversation has been closed by the server */
            token_ptr = &recv_tok;
            if (recv_tok.length > 0) {
#ifdef DEBUG 
                fprintf(stdout, "token received (size=%d)\n",recv_tok.length);
#endif
            } else {
#ifdef DEBUG 
                fprintf(stdout, "void token received\n");
#endif
                minor_status = NULL ;
                return -1 ;
            }
        }
    } while (maj_stat & GSS_S_CONTINUE_NEEDED);

#ifdef DEBUG 
    fprintf(stdout, "COMPLETE\n");
#endif
    (void) gss_release_name(&min_stat, &target_name);

    (void) gss_delete_sec_context(&min_stat, &gss_context, GSS_C_NO_BUFFER);

    return maj_stat;
}


