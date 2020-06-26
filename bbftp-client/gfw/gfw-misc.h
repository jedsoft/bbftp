/*
 * Copyright 2001 by Lionel Schwarz, Computing Center IN2P3
 */

#ifndef _GSSMISC_H_
#define _GSSMISC_H_

#include <gssapi.h>
#include <stdio.h>

extern FILE *display_file;

int send_token
	 (int s, gss_buffer_t tok) ;
int recv_token
	 (int s, gss_buffer_t tok) ;
void display_status
	 (char *msg, OM_uint32 maj_stat, OM_uint32 min_stat) ;
/*void display_ctx_flags
	 (OM_uint32 flags) ;*/
void print_token
	 (gss_buffer_t tok) ;

#endif
