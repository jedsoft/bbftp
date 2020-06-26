/*
 * bbftpc/tbbftp_turl.c
 * Copyright (C) 1999, 2000, 2001, 2002 IN2P3, CNRS
 * bbftp@in2p3.fr
 * http://doc.in2p3.fr/bbftp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */ 


/****************************************************************************

 bbftp_turl.c	v 2.2.1	2013/02/01	- Move structure and prototype definition to bbftp_turl.h
					  to avoid compilation issues
  
*****************************************************************************/

#include <bbftp.h>
#include <bbftp_turl.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

extern  int     timestamp;
extern  int     globaltrymax;
extern  int     myexitcode;
extern  int     verbose ;
extern  char    *curfilename ;
extern  char    *realfilename ;
extern  int     nbport ;
extern  int     state ;

int decodeTURL(char *oTURL, struct decodedTURL **dTURL) {
    char    *turlbuffer  = NULL;
    char    *host = NULL;
    char    *port = NULL;
    int     rfio=0;
    int     i=0;
    
    if ( oTURL == NULL ) return -1;
    if ( (turlbuffer = (char *) malloc (strlen(oTURL)+1) ) == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","turlbuffer",strerror(errno)) ;
    }
    strcpy (turlbuffer, oTURL);
    while (*turlbuffer == ' ') turlbuffer++ ;
    if ( *turlbuffer == '\0' ) return -1;
    /*
    ** action is now pointing to the file directory or TURL
    */
    if (!strncmp(turlbuffer,"bbftp://",8) )  {
        turlbuffer = turlbuffer+8 ;
        if ( *turlbuffer == '\0' ) return -1;
        if (index(turlbuffer, '/') == NULL) return -1;
        if ( (host = (char *) malloc (strlen(turlbuffer)+1) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","host",strerror(errno)) ;
        }
        strcpy( host, turlbuffer);
        /* length of server name: strcspn(host, '/') */
        /*host[strcspn(host, "/")] = '\0';*/
        while (host[i++] != '/') ;
        host[i-1] = '\0';
        /*printf("host=%s\n", host);*/
        if (index(host, ':') != NULL) { /* a port has been specified */
    	    if ( (port = (char *) malloc (strlen(host)+1)) == NULL ) return -1;
    	    strcpy(port, host);
    	    /*host[strcspn(host, ':')] = '\0';*/
    	    i = 0;
                while (host[i++] != ':') ;
                host[i-1] = '\0';
    	    while ( *port != ':' ) port++;
    	    port++;
                /*printf("port=%s\n", port);*/
        }
        /*printf("host=%s\n", host);*/
        /* host and port OK */
        /* Now: filename */
        while ( *turlbuffer != '/' ) turlbuffer++;
        turlbuffer++;
    }
    /* is the filename a rfio turl ? */
    if (!strncmp(turlbuffer,"rfio://",7) ) {
            rfio = 1;
            turlbuffer = turlbuffer+7;
    } else if (!strncmp(turlbuffer,"file://",7) ) {
            turlbuffer = turlbuffer+7;
    }
    if ( ((*dTURL) = (struct decodedTURL *) malloc (sizeof(struct decodedTURL)) ) == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","dTURL",strerror(errno)) ;
	return -1;
    }
    if (host != NULL) {
        strcpy((*dTURL)->hostname, host);
    } else {
	(*dTURL)->hostname[0] = '\0';
    }
    if (port != NULL) {
	strcpy((*dTURL)->port, port);
    } else {
	(*dTURL)->port[0] = '\0';
    }
    strcpy((*dTURL)->filename, turlbuffer);
    (*dTURL)->rfio = rfio;
    /*printf("hostname=%s;port=%s;filename=%s;rfio=%d\n", (*dTURL)->hostname, (*dTURL)->port, (*dTURL)->filename, (*dTURL)->rfio);*/
    return 0;
}
    

int translatecommand(char *buffercmd, char **translatedcmd, char **host, int *port, int *remoterfio, int *localrfio)
{
    char    *mybuffercmd ;
    char    *dupbuffercmd ;
    char    *action ;
    char    *startfn ;
    char    *remotefilename ;
    int     j ;
    int     nbtry ;
    int     retcode ;
    int     errcode ;
    int     nooption ;
    int     alluse ;
    char    *localfilename ;
    struct decodedTURL *dLocalTurl = NULL;
    struct decodedTURL *dRemoteTurl = NULL;

    /**remoterfio = *localrfio = 0;*/

    if ( (dupbuffercmd = (char *) malloc (strlen(buffercmd)+1) ) == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","dupbuffercmd",strerror(errno)) ;
    }
    strcpy (dupbuffercmd, buffercmd);
    /*
    ** Strip leading blank
    */
    mybuffercmd = dupbuffercmd ;
    j = 0 ;
    while ( *mybuffercmd == ' ' && j < strlen(dupbuffercmd) ) {
        j++ ;
        *mybuffercmd++ ;
    }
        
        /*
    if ( *mybuffercmd == '!' ) {
        ** This is a comment
        free(dupbuffercmd) ;
        return 0 ;
    }
        */
    /*
    ** Strip trailing blank 
    */
    j = strlen(mybuffercmd) - 1 ;
    while ( mybuffercmd[j] == ' ' && j >= 0 ) {
        j-- ;
    }
    mybuffercmd[j+1] = 0 ;
        /*
    if ( strlen(mybuffercmd) == 0 ) {
        ** Null command ignore it
        free(dupbuffercmd) ;
        return 0 ;
    }
        */
    /*
     * By default, command is not translated
     */
    /*
    if ( (*translatedcmd = (char *) malloc (strlen(buffercmd)+1) ) == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","translatedcmd",strerror(errno)) ;
        return -1 ;
    }
    strcpy (*translatedcmd, buffercmd);
      */
    (*translatedcmd) = strdup(buffercmd);  
    action = mybuffercmd ;
    action = (char *) strchr (action, ' ');
    /*
    ** Decode the action
    */
    if (!strncmp(mybuffercmd,"cd",2) ) {
/*******************************************************************************
** cd                                                                          *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 2;
	if (decodeTURL(mybuffercmd, &dRemoteTurl) == -1) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        } 
	if (dRemoteTurl == NULL) goto end;
    /*printf("hostname=%s\nport=%s\nfilename=%s\nrfio=%d\n", dRemoteTurl->hostname, dRemoteTurl->port, dRemoteTurl->filename, dRemoteTurl->rfio);*/
	   
	if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	if (dRemoteTurl->port[0] != '\0') {
	    sscanf(dRemoteTurl->port, "%d", port);
	}
        if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+3) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","*tempbuf",strerror(errno)) ;
            return -1 ;
        }
	strcpy(tempbuf, "cd ");
	strcat(tempbuf, dRemoteTurl->filename);
        *translatedcmd = tempbuf;
	*remoterfio = dRemoteTurl->rfio;
    } else if (!strncmp(mybuffercmd,"dir",3) ) {
/*******************************************************************************
** cd                                                                          *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 3;
	if (decodeTURL(mybuffercmd, &dRemoteTurl) == -1) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        } 
	if (dRemoteTurl == NULL) goto end;
	   
	if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	if (dRemoteTurl->port[0] != '\0') {
	    sscanf(dRemoteTurl->port, "%d", port);
	}
        if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+4) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","*tempbuf",strerror(errno)) ;
            return -1 ;
        }
	strcpy(tempbuf, "dir ");
	strcat(tempbuf, dRemoteTurl->filename);
        *translatedcmd = tempbuf;
	*remoterfio = dRemoteTurl->rfio;
    } else if (  !strncmp(mybuffercmd,"get",3) ) {
/*******************************************************************************
** get                                                                         *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 3;
        if ( action == NULL ) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        while (*action == ' ') action++ ;
        if ( *action == '\0' ) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        startfn = action ;
        startfn = (char *) strchr (startfn, ' ');
        if ( startfn == NULL ) {
            /*
            ** Only one name
            */
	    if (decodeTURL(action, &dRemoteTurl) == -1) {
                printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                myexitcode = 26 ;
                free(dupbuffercmd) ;
                return -1 ;
            } 
	    if (dRemoteTurl == NULL) goto end;
	    if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	    if (dRemoteTurl->port[0] != '\0') {
	      sscanf(dRemoteTurl->port, "%d", port);
	    }
            if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+4) ) == NULL ) {
                printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                return -1 ;
            }
	    strcpy(tempbuf, "get ");
	    strcat(tempbuf, dRemoteTurl->filename);
            *translatedcmd = tempbuf;
	    *remoterfio = dRemoteTurl->rfio;
        } else {
            while ( *startfn == ' ' ) startfn++ ;
            if ( *startfn == '\0' ) {
                /*
                ** Only one name
                */
	        if (decodeTURL(action, &dRemoteTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dRemoteTurl == NULL) goto end;
    	        if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	        if (dRemoteTurl->port[0] != '\0') {
	          sscanf(dRemoteTurl->port, "%d", port);
	        }
                if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+4) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "get ");
	        strcat(tempbuf, dRemoteTurl->filename);
                *translatedcmd = tempbuf;
	        *remoterfio = dRemoteTurl->rfio;
            } else {
                /*
                ** two names
                */
		char *localfile;
		char *remotefile;
		if ( (remotefile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","remotefile",strerror(errno)) ;
                    return -1 ;
		}
		if ( (localfile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","localfile",strerror(errno)) ;
                    return -1 ;
		}
                sscanf(action,"%s %s", remotefile, localfile);
	        if (decodeTURL(localfile, &dLocalTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dLocalTurl == NULL) goto end;
		if (dLocalTurl->hostname[0] != '\0') {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
		}
	        if (decodeTURL(remotefile, &dRemoteTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dRemoteTurl == NULL) goto end;
    	        if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	        if (dRemoteTurl->port[0] != '\0') {
	          sscanf(dRemoteTurl->port, "%d", port);
	        }
                if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+strlen(dLocalTurl->filename)+1+5) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "get ");
	        strcat(tempbuf, dRemoteTurl->filename);
		strcat(tempbuf, " ");
	        strcat(tempbuf, dLocalTurl->filename);
                *translatedcmd = tempbuf;
	        *localrfio = dLocalTurl->rfio;
	        *remoterfio = dRemoteTurl->rfio;
            }
        }
    } else if (  !strncmp(mybuffercmd,"mget",4) ) {
/*******************************************************************************
** mget                                                                        *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 4;
        if ( action == NULL ) {
        /*
        ** get needs one or two parameters
        */
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        while (*action == ' ') action++ ;
        if ( *action == '\0' ) {
            /*
            ** get needs one or two parameters
            */
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        startfn = action ;
        startfn = (char *) strchr (startfn, ' ');
        if ( startfn == NULL ) {
            /*
            ** Only one name
            */
	    if (decodeTURL(action, &dRemoteTurl) == -1) {
                printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                myexitcode = 26 ;
                free(dupbuffercmd) ;
                return -1 ;
            } 
	    if (dRemoteTurl == NULL) goto end;
	    if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	    if (dRemoteTurl->port[0] != '\0') {
	      sscanf(dRemoteTurl->port, "%d", port);
	    }
            if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+5) ) == NULL ) {
                printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                return -1 ;
            }
	    strcpy(tempbuf, "mget ");
	    strcat(tempbuf, dRemoteTurl->filename);
            *translatedcmd = tempbuf;
	    *remoterfio = dRemoteTurl->rfio;
        } else {
            while ( *startfn == ' ' ) startfn++ ;
            if ( *startfn == '\0' ) {
                /*
                ** Only one name
                */
	        if (decodeTURL(action, &dRemoteTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dRemoteTurl == NULL) goto end;
    	        if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	        if (dRemoteTurl->port[0] != '\0') {
	          sscanf(dRemoteTurl->port, "%d", port);
	        }
                if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+5) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "mget ");
	        strcat(tempbuf, dRemoteTurl->filename);
                *translatedcmd = tempbuf;
	        *remoterfio = dRemoteTurl->rfio;
            } else {
                /*
                ** two names
                */
		char *localfile;
		char *remotefile;
		if ( (remotefile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","remotefile",strerror(errno)) ;
                    return -1 ;
		}
		if ( (localfile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","localfile",strerror(errno)) ;
                    return -1 ;
		}
                sscanf(action,"%s %s", remotefile, localfile);
	        if (decodeTURL(localfile, &dLocalTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dLocalTurl == NULL) goto end;
		/*if (dLocalTurl->hostname[0] != '\0') {*/
		if (dLocalTurl->hostname[0] != '\0') {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
		}
	        if (decodeTURL(remotefile, &dRemoteTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dRemoteTurl == NULL) goto end;
    	        if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	        if (dRemoteTurl->port[0] != '\0') {
	          sscanf(dRemoteTurl->port, "%d", port);
	        }
                if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+strlen(dLocalTurl->filename)+1+6) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "mget ");
	        strcat(tempbuf, dRemoteTurl->filename);
		strcat(tempbuf, " ");
	        strcat(tempbuf, dLocalTurl->filename);
                *translatedcmd = tempbuf;
	        *localrfio = dLocalTurl->rfio;
	        *remoterfio = dRemoteTurl->rfio;
            }
	}
    } else if (  !strncmp(mybuffercmd,"mkdir",5) ) {
/*******************************************************************************
** mkdir                                                                       *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 5;
	if (decodeTURL(mybuffercmd, &dRemoteTurl) == -1) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        } 
	if (dRemoteTurl == NULL) goto end;
    /*printf("hostname=%s\nport=%s\nfilename=%s\nrfio=%d\n", dRemoteTurl->hostname, dRemoteTurl->port, dRemoteTurl->filename, dRemoteTurl->rfio);*/
	   
	if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	if (dRemoteTurl->port[0] != '\0') {
	    sscanf(dRemoteTurl->port, "%d", port);
	}
        if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+6) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","tempbuf",strerror(errno)) ;
            return -1 ;
        }
	strcpy(tempbuf, "mkdir ");
	strcat(tempbuf, dRemoteTurl->filename);
        *translatedcmd = tempbuf;
	*remoterfio = dRemoteTurl->rfio;
    } else if (  !strncmp(mybuffercmd,"mput",4) ) {
/*******************************************************************************
** mput                                                                        *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 4;
        if ( action == NULL ) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        while (*action == ' ') action++ ;
        if ( *action == '\0' ) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        startfn = action ;
        startfn = (char *) strchr (startfn, ' ');
        if ( startfn == NULL ) {
	    if (decodeTURL(action, &dLocalTurl) == -1) {
                printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                myexitcode = 26 ;
                free(dupbuffercmd) ;
                return -1 ;
            } 
	    if (dLocalTurl == NULL) goto end;
	    if (dLocalTurl->hostname[0] != '\0') {
                printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                myexitcode = 26 ;
                free(dupbuffercmd) ;
                return -1 ;
	    }
            if ( (tempbuf = (char *) malloc (strlen(dLocalTurl->filename)+1+5) ) == NULL ) {
                printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                return -1 ;
            }
	    strcpy(tempbuf, "mput ");
	    strcat(tempbuf, dLocalTurl->filename);
            *translatedcmd = tempbuf;
	    *localrfio = dLocalTurl->rfio;
        } else {
            while ( *startfn == ' ' ) startfn++ ;
            if ( *startfn == '\0' ) {
	        if (decodeTURL(action, &dLocalTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dLocalTurl == NULL) goto end;
	        if (dLocalTurl->hostname[0] != '\0') {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
	        }
                if ( (tempbuf = (char *) malloc (strlen(dLocalTurl->filename)+1+5) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "mput ");
	        strcat(tempbuf, dLocalTurl->filename);
                *translatedcmd = tempbuf;
	        *localrfio = dLocalTurl->rfio;
            } else {
		char *localfile;
		char *remotefile;
		if ( (remotefile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","remotefile",strerror(errno)) ;
                    return -1 ;
		}
		if ( (localfile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","localfile",strerror(errno)) ;
                    return -1 ;
		}
                sscanf(action,"%s %s", localfile, remotefile);
	        if (decodeTURL(localfile, &dLocalTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dLocalTurl == NULL) goto end;
		if (dLocalTurl->hostname[0] != '\0') {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
		}
	        if (decodeTURL(remotefile, &dRemoteTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dRemoteTurl == NULL) goto end;
    	        if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	        if (dRemoteTurl->port[0] != '\0') {
	          sscanf(dRemoteTurl->port, "%d", port);
	        }
                if ( (tempbuf = (char *) malloc (strlen(dLocalTurl->filename)+strlen(dRemoteTurl->filename)+1+6) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "mput ");
	        strcat(tempbuf, dLocalTurl->filename);
		strcat(tempbuf, " ");
	        strcat(tempbuf, dRemoteTurl->filename);
                *translatedcmd = tempbuf;
	        *localrfio = dLocalTurl->rfio;
	        *remoterfio = dRemoteTurl->rfio;
            }
        }
    } else if (  !strncmp(mybuffercmd,"put",3) ) {
/*******************************************************************************
** put                                                                         *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 3;
        if ( action == NULL ) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        while (*action == ' ') action++ ;
        if ( *action == '\0' ) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        }
        startfn = action ;
        startfn = (char *) strchr (startfn, ' ');
        if ( startfn == NULL ) {
	    if (decodeTURL(action, &dLocalTurl) == -1) {
                printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                myexitcode = 26 ;
                free(dupbuffercmd) ;
                return -1 ;
            } 
	    if (dLocalTurl == NULL) goto end;
	    if (dLocalTurl->hostname[0] != '\0') {
                printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                myexitcode = 26 ;
                free(dupbuffercmd) ;
                return -1 ;
	    }
            if ( (tempbuf = (char *) malloc (strlen(dLocalTurl->filename)+1+4) ) == NULL ) {
                printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                return -1 ;
            }
	    strcpy(tempbuf, "put ");
	    strcat(tempbuf, dLocalTurl->filename);
            *translatedcmd = tempbuf;
	    *localrfio = dLocalTurl->rfio;
        } else {
            while ( *startfn == ' ' ) startfn++ ;
            if ( *startfn == '\0' ) {
                /*
                ** Only one name
                */
	        if (decodeTURL(action, &dLocalTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dLocalTurl == NULL) goto end;
	        if (dLocalTurl->hostname[0] != '\0') {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
	        }
                if ( (tempbuf = (char *) malloc (strlen(dLocalTurl->filename)+1+4) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "put ");
	        strcat(tempbuf, dLocalTurl->filename);
                *translatedcmd = tempbuf;
	        *localrfio = dLocalTurl->rfio;
            } else {
                /*
                ** two names
                */
		char *localfile;
		char *remotefile;
		if ( (remotefile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","remotefile",strerror(errno)) ;
                    return -1 ;
		}
		if ( (localfile = (char *) malloc (strlen(action)+1)) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","localfile",strerror(errno)) ;
                    return -1 ;
		}
                sscanf(action,"%s %s", localfile, remotefile);
	        if (decodeTURL(localfile, &dLocalTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dLocalTurl == NULL) goto end;
		if (dLocalTurl->hostname[0] != '\0') {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
		}
	        if (decodeTURL(remotefile, &dRemoteTurl) == -1) {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",action) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                } 
        	if (dRemoteTurl == NULL) goto end;
    	        if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	        if (dRemoteTurl->port[0] != '\0') {
	          sscanf(dRemoteTurl->port, "%d", port);
	        }
                if ( (tempbuf = (char *) malloc (strlen(dLocalTurl->filename)+strlen(dRemoteTurl->filename)+1+5) ) == NULL ) {
                    printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s: %s\n","tempbuf",strerror(errno)) ;
                    return -1 ;
                }
	        strcpy(tempbuf, "put ");
	        strcat(tempbuf, dLocalTurl->filename);
		strcat(tempbuf, " ");
	        strcat(tempbuf, dRemoteTurl->filename);
                *translatedcmd = tempbuf;
	        *localrfio = dLocalTurl->rfio;
	        *remoterfio = dRemoteTurl->rfio;
            }
        }
    } else if (  !strncmp(mybuffercmd,"rm",2) ) {
/*******************************************************************************
** rm                                                                       *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 2;
	if (decodeTURL(mybuffercmd, &dRemoteTurl) == -1) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        } 
	if (dRemoteTurl == NULL) goto end;
	   
	if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	if (dRemoteTurl->port[0] != '\0') {
	    sscanf(dRemoteTurl->port, "%d", port);
	}
        if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+3) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","tempbuf",strerror(errno)) ;
            return -1 ;
        }
	strcpy(tempbuf, "rm ");
	strcat(tempbuf, dRemoteTurl->filename);
        *translatedcmd = tempbuf;
	*remoterfio = dRemoteTurl->rfio;
    } else if (  !strncmp(mybuffercmd,"stat",4) ) {
/*******************************************************************************
** stat                                                                       *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 4;
	if (decodeTURL(mybuffercmd, &dRemoteTurl) == -1) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        } 
	if (dRemoteTurl == NULL) goto end;
	   
	if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	if (dRemoteTurl->port[0] != '\0') {
	    sscanf(dRemoteTurl->port, "%d", port);
	}
        if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+5) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","tempbuf",strerror(errno)) ;
            return -1 ;
        }
	strcpy(tempbuf, "stat ");
	strcat(tempbuf, dRemoteTurl->filename);
        *translatedcmd = tempbuf;
	*remoterfio = dRemoteTurl->rfio;
    } else if (  !strncmp(mybuffercmd,"df",2) ) {
/*******************************************************************************
** df                                                                       *
*******************************************************************************/
	char *tempbuf;
	mybuffercmd += 2;
	if (decodeTURL(mybuffercmd, &dRemoteTurl) == -1) {
            printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
            myexitcode = 26 ;
            free(dupbuffercmd) ;
            return -1 ;
        } 
	if (dRemoteTurl == NULL) goto end;
	   
	if (dRemoteTurl->hostname[0] != '\0') *host = dRemoteTurl->hostname;
	if (dRemoteTurl->port[0] != '\0') {
	    sscanf(dRemoteTurl->port, "%d", port);
	}
        if ( (tempbuf = (char *) malloc (strlen(dRemoteTurl->filename)+1+3) ) == NULL ) {
            printmessage(stderr,CASE_FATAL_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","tempbuf",strerror(errno)) ;
            return -1 ;
        }
	strcpy(tempbuf, "df ");
	strcat(tempbuf, dRemoteTurl->filename);
        *translatedcmd = tempbuf;
	*remoterfio = dRemoteTurl->rfio;
    }
end:
    /*
     * At this point the command is not a TURL so send as it is
     */
    free(dupbuffercmd);
    return 0 ;
}
