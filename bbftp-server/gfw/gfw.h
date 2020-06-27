/**************************************************************************
* definitions for the GSS FrameWork (gfw) package                         *
*                                                                         *
* This package allows to use in a simple way the GSS-API                  *
***************************************************************************/
#include "gfw-config.h"

/**************************************************************************
* Type definitions                                                        *
***************************************************************************/
typedef struct gfw_msgs_list_st {
    char    *msg;
    struct  gfw_msgs_list_st *next;
} gfw_msgs_list;

/**************************************************************************
* Functions prototypes                                                    *
***************************************************************************/

/*>>>>>>>>>>>>>>>>>>>> CREDENTIALS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/

/*------------------------------------------------------------------------*/
/*
* gfw_acquire_cred
*       acquire credentials
*/
OM_uint32 gfw_acquire_cred(
OM_uint32 * ,           /* (w) minor status */
char * ,                /* (r) service required */ 
gss_cred_id_t *         /* (w) outout credentials */
);


/*>>>>>>>>>>>>>>>>>>>> CONTEXT <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/

/*------------------------------------------------------------------------*/
/*
* gfw_init_sec_context
*       client side: initiate a context
*/
OM_uint32 gfw_init_sec_context(
OM_uint32 * ,           /* (w) minor status */
int ,                   /* (r) input stream socket */
int ,                   /* (r) output stream socket */
char *                  /* (r) service required */
);

/*------------------------------------------------------------------------*/
/*
* gfw_accept_sec_context
*       server side: accept a context initiated by a client
*/
OM_uint32 gfw_accept_sec_context(
OM_uint32 * ,           /* (w) minor status */
int ,                   /* (r) input stream socket */
int ,                   /* (r) output stream socket */
gss_cred_id_t ,         /* (r) credentials */
gss_buffer_t            /* (w) name of the client which initiated the context */
);


/*>>>>>>>>>>>>>>>>>>>> TOOLS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/

/*------------------------------------------------------------------------*/
/*
* gfw_status_to_string
*       extract the main error message of a status into a string
*/
void gfw_status_to_string(
OM_uint32 ,             /* (r) status from which to extract the main message */
char **                 /* (w) container of the message */
);

/*------------------------------------------------------------------------*/
/*
* gfw_status_to_strings
*       extract all error message of both major and minor status
*/
void gfw_status_to_strings(
OM_uint32 ,             /* (r) major status */
OM_uint32 ,             /* (r) minor status */
gfw_msgs_list **        /* (w) container of the message list */
);

/*------------------------------------------------------------------------*/
/*
 * gfw_get_globus_modules_versions
 * 	get all globus modules with version
 */
/*
char *gfw_get_globus_modules_versions(
);
*/
