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
 * Function: gfw_acquire_cred
 *
 * Purpose: acquire a credential
 *
 * Arguments:
 *
 *  minor_status    (w) the second status
 * 	service_name    (r) the GSS-API service name to authenticate to
 *  server_creds    (w) the credential acquired
 *
 * Returns: major status
 *
 */
OM_uint32 gfw_acquire_cred(
OM_uint32 *    minor_status,
char *          service_name, 
gss_cred_id_t * server_creds)
{
    gss_buffer_desc name_buf;
    gss_name_t server_name = GSS_C_NO_NAME;
    OM_uint32 maj_stat, min_stat;

#ifdef DEBUG 
    display_file = fopen("/tmp/gfw-server.log","w") ;
#endif
    if (service_name) {
#ifdef DEBUG 
        fprintf(display_file,"service_name=%s\n",service_name) ;
#endif
        name_buf.value = service_name;
        name_buf.length = strlen(name_buf.value) + 1;
        maj_stat = gss_import_name(minor_status, 
                                    &name_buf, 
                                    (gss_OID) gss_nt_service_name, 
                                    &server_name);
        if (maj_stat != GSS_S_COMPLETE) {
            display_status("importing name", maj_stat, *minor_status);
            return maj_stat;
        }
    }

    maj_stat = gss_acquire_cred(minor_status, 
                                server_name, 
                                GSS_C_INDEFINITE,
                                GSS_C_NULL_OID_SET, 
                                GSS_C_ACCEPT,
                                server_creds, 
                                NULL, 
                                NULL);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("acquiring credentials", maj_stat, *minor_status);
        return maj_stat;
    }

    if (service_name) {
        (void) gss_release_name(&min_stat, &server_name);
    }

    return maj_stat;
}


