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


/*
 * Function: gfw_accept_sec_context
 *
 * Purpose: accept a context initiated by a client
 *
 * Arguments:
 *
 *  minor_status    (w) the second status
 * 	is              (r) the input stream socket
 * 	os              (r) the output stream socket
 *  server_creds    (r) the credential for the context to be accepted
 * 	client_name     (w) the name (certificate subject) of the client who wants to
 *                      establish a context
 *
 * Returns: major status
 *
 * Effects:
 * 
 * gfw_accept_sec_context establishes a GSS-API context required by a client 
 * over the connection designated by the two sockets. 
 */
OM_uint32 gfw_accept_sec_context(
OM_uint32 *         minor_status,
int                 is, 
int                 os, 
gss_cred_id_t       server_creds, 
gss_buffer_t        client_name)
{
    gss_buffer_desc send_tok, recv_tok;
    gss_name_t client;
    gss_OID doid;
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc	oid_name;
    gss_ctx_id_t   context;
     
    context = GSS_C_NO_CONTEXT;
     
    do {
#ifdef DEBUG 
        fprintf(display_file, "receiving token...\n");
#endif
        if (recv_token(is, &recv_tok) < 0)
            return -1;

#ifdef DEBUG 
        fprintf(display_file, "Received token (size=%d): \n", recv_tok.length);
#endif

        maj_stat = gss_accept_sec_context(minor_status,
                                            &context,
                                            server_creds,
                                            &recv_tok,
                                            GSS_C_NO_CHANNEL_BINDINGS,
                                            &client,
                                            &doid,
                                            &send_tok,
                                            NULL,
                                            NULL, 	/* ignore time_rec */
                                            NULL); 	/* ignore del_cred_handle */

        if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
            display_status("accepting context", maj_stat, *minor_status);
            (void) gss_release_buffer(&min_stat, &recv_tok);
            /* send a void token to close conversation */
            (void) gss_release_buffer(&min_stat, &send_tok);
            if (send_token(os, &send_tok) < 0) {
                fprintf(display_file, "failure sending token\n");
                return -1;
            }
            return maj_stat;
        }

        (void) gss_release_buffer(&min_stat, &recv_tok);

        if (send_tok.length != 0) {
#ifdef DEBUG 
            fprintf(display_file, "Sending accept_sec_context token (size=%d):\n", send_tok.length);
#endif
            if (send_token(os, &send_tok) < 0) {
                fprintf(display_file, "failure sending token\n");
                return -1;
            }
#ifdef DEBUG 
            fprintf(display_file, "token sent\n");
#endif

            (void) gss_release_buffer(&min_stat, &send_tok);
        }

    } while (maj_stat & GSS_S_CONTINUE_NEEDED);
#ifdef DEBUG 
    fprintf(display_file, "COMPLETE\n");
#endif

    maj_stat = gss_display_name(minor_status, client, client_name, &doid);
    if (maj_stat != GSS_S_COMPLETE) {
/*        *minor_status = min_stat;*/
        display_status("displaying name", maj_stat, *minor_status);
        return maj_stat;
    }
    (void) gss_release_name(&min_stat, &client);

     /* Delete context */
    (void) gss_delete_sec_context(&min_stat, &context, NULL);

    return maj_stat;

}


