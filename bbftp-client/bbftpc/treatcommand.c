/*
 * bbftpc/treatcommand.c
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

  
  
 treatcommand.c v 2.0.0 2001/03/01  - Creation of the routine
                v 2.0.1 2001/04/17  - Correct put bug
                                    - Correct indentation 
                v 2.0.2 2001/05/07  - Correct include for RFIO
				v 2.2.0 2001/11/21	- Allow negative COS


*****************************************************************************/
#include <bbftp.h>

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
extern  int     connectionisbroken ;
extern  int     verbose ;
extern  int     transferoption ;
extern  int     sendwinsize ;
extern  int     recvwinsize ;
extern  int     localumask ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
extern  int     localcos ;
#endif
extern  int     remoteumask ;
extern  int     remotecos ;
extern	int		ackto;
extern	int		recvcontrolto;
extern	int		sendcontrolto;
extern	int		datato;
extern  char    *curfilename ;
extern  char    *realfilename ;
extern  int     nbport ;
extern  int     state ;
extern  int     buffersizeperstream ;

int treatcommand(char *buffercmd)
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
        
    if ( *mybuffercmd == '!' ) {
        /*
        ** This is a comment
        */
        free(dupbuffercmd) ;
        return 0 ;
    }
    /*
    ** Strip trailing blank 
    */
    j = strlen(mybuffercmd) - 1 ;
    while ( mybuffercmd[j] == ' ' && j >= 0 ) {
        j-- ;
    }
    mybuffercmd[j+1] = 0 ;
    if ( strlen(mybuffercmd) == 0 ) {
        /*
        ** Null command ignore it
        */
        free(dupbuffercmd) ;
        return 0 ;
    }
        
    action = mybuffercmd ;
    action = (char *) strchr (action, ' ');
    /*
    ** Decode the action
    */
    if ( !strncmp(mybuffercmd,"cd",2) || 
         !strncmp(mybuffercmd,"get",3) || 
         !strncmp(mybuffercmd,"lcd",3) ||
         !strncmp(mybuffercmd,"mget",4)  ||
         !strncmp(mybuffercmd,"mkdir",5)  ||
         !strncmp(mybuffercmd,"rm",2)  ||
         !strncmp(mybuffercmd,"mput",4)  ||
         !strncmp(mybuffercmd,"put",3) ||
	 !strncmp(mybuffercmd,"stat",4) ||
	 !strncmp(mybuffercmd,"df",2) ||
	 !strncmp(mybuffercmd,"dir",3) ||
         !strncmp(mybuffercmd,"setbuffersize",13) ||
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
         !strncmp(mybuffercmd,"setlocalcos",11) ||
#endif
         !strncmp(mybuffercmd,"setlocalumask",13) ||
         !strncmp(mybuffercmd,"setnbstream",11) ||
         !strncmp(mybuffercmd,"setoption",9) ||
         !strncmp(mybuffercmd,"setremotecos",12) ||
         !strncmp(mybuffercmd,"setremoteumask",14) ||
         !strncmp(mybuffercmd,"setrecvwinsize",14) ||
         !strncmp(mybuffercmd,"setsendwinsize",14) ||
         !strncmp(mybuffercmd,"setrecvcontrolto",16) ||
         !strncmp(mybuffercmd,"setsendcontrolto",16) ||
         !strncmp(mybuffercmd,"setackto",8) ||
         !strncmp(mybuffercmd,"setdatato",9) ) {
        for ( nbtry = 1 ; nbtry <= globaltrymax ; nbtry++ ) {
            /*
            ** check if the last try was not disconnected
            */
            if ( connectionisbroken == 1 ) {
                reconnecttoserver() ;
                connectionisbroken = 0 ;
            }
            if (!strncmp(mybuffercmd,"cd",2) ) {
/*******************************************************************************
** cd                                                                          *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** cd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** cd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the file directory
                */
                retcode = bbftp_cd(action,&errcode) ;
	    } else if (!strncmp(mybuffercmd,"dir",3) ) {
/*******************************************************************************
** dir                                                                         *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** dir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** cd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the file directory
                */
                retcode = bbftp_dir(action,&errcode) ;
            } else if (  !strncmp(mybuffercmd,"get",3) ) {
/*******************************************************************************
** get                                                                         *
*******************************************************************************/
		char hostname[10 + 1];
		if (gethostname(hostname, sizeof(hostname)) < 0) {
			hostname[0] = '\0';
		} else {
			hostname[sizeof(hostname) - 1] = '\0';
		}
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
                    if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        return -1 ;
                    }
                    if ( (realfilename = (char *) malloc (strlen(action)+30)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        free(curfilename) ;
                        curfilename=NULL ;
                       return -1 ;
                    }
                    /*
                    ** We usesscanf  in order to avoid white space
                    */
                    sscanf(action,"%s",curfilename) ;
                    sscanf(action,"%s",remotefilename) ;
                    if ( (transferoption & TROPT_TMP) == TROPT_TMP ) {
#ifdef CASTOR
                        if ( (transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                            sprintf(realfilename,"%s",curfilename) ;
                        } else {
                            sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
                        }
#else                         
                        sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
#endif
                    } else {
                        sprintf(realfilename,"%s",curfilename) ;
                    }
                } else {
                    while ( *startfn == ' ' ) startfn++ ;
                    if ( *startfn == '\0' ) {
                        /*
                        ** Only one name
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (realfilename = (char *) malloc (strlen(action)+30)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            free(curfilename) ;
                            curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",curfilename) ;
                        sscanf(action,"%s",remotefilename) ;
                        if ( (transferoption & TROPT_TMP) == TROPT_TMP ) {
#ifdef CASTOR
                            if ( (transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                                sprintf(realfilename,"%s",curfilename) ;
                            } else {
                                sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
                            }
#else                         
                            sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
#endif
                        } else {
                            sprintf(realfilename,"%s",curfilename) ;
                        }
                    } else {
                        /*
                        ** two names
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (curfilename = (char *) malloc (strlen(startfn)+strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (realfilename = (char *) malloc (strlen(startfn)+strlen(action)+30)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            free(curfilename) ;
                            curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",remotefilename) ;
                        sscanf(startfn,"%s",curfilename) ;
                        /* If curfilename ends with '/', add remotefilename without path */
                        if (*(curfilename+(strlen(curfilename)-1)) == '/') {
                            if (rindex(remotefilename, '/') != NULL) {
                                sprintf(curfilename,"%s%s",startfn,rindex(remotefilename,'/'));
                            } else {
                                sprintf(curfilename,"%s%s",startfn,remotefilename);
                            }
                        }
                        if ( (transferoption & TROPT_TMP) == TROPT_TMP ) {
#ifdef CASTOR
                            if ( (transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                                sprintf(realfilename,"%s",curfilename) ;
                            } else {
                                sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
                            }
#else                         
                            sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
#endif
                        } else {
                            sprintf(realfilename,"%s",curfilename) ;
                        }
                    }
                }
                retcode = bbftp_get(remotefilename,&errcode) ;
                state = 0 ;
                free(remotefilename) ;
            } else if (  !strncmp(mybuffercmd,"lcd",3) ) {
/*******************************************************************************
** lcd                                                                         *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** lcd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** lcd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the directory
                */
                retcode = bbftp_lcd(action,&errcode) ;
            } else if (  !strncmp(mybuffercmd,"mget",4) ) {
/*******************************************************************************
** mget                                                                        *
*******************************************************************************/
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
                    if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (localfilename = (char *) malloc (2+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        return -1 ;
                    }
                    /*
                    ** We usesscanf  in order to avoid white space
                    */
                    sprintf(localfilename,"./") ;
                    sscanf(action,"%s",remotefilename) ;
                } else {
                    while ( *startfn == ' ' ) startfn++ ;
                    if ( *startfn == '\0' ) {
                        /*
                        ** Only one name
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (localfilename = (char *) malloc (2+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        sprintf(localfilename,"./") ;
                        sscanf(action,"%s",remotefilename) ;
                    } else {
                        /*
                        ** two names
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (localfilename = (char *) malloc (strlen(startfn)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        sscanf(action,"%s",remotefilename) ;
                        sscanf(startfn,"%s",localfilename) ;
                    }
                }
                retcode = bbftp_mget(remotefilename,localfilename,&errcode) ;
                free(remotefilename) ;
                free(localfilename) ;
            } else if (  !strncmp(mybuffercmd,"mkdir",5) ) {
/*******************************************************************************
** mkdir                                                                       *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the directory
                */
                retcode = bbftp_mkdir(action,&errcode) ;
            } else if (  !strncmp(mybuffercmd,"mput",4) ) {
/*******************************************************************************
** mput                                                                        *
*******************************************************************************/
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
                    if ( (localfilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (remotefilename = (char *) malloc (2+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(localfilename) ;
                        return -1 ;
                    }
                    /*
                    ** We usesscanf  in order to avoid white space
                    */
                    sprintf(remotefilename,"./") ;
                    sscanf(action,"%s",localfilename) ;
            } else {
                    while ( *startfn == ' ' ) startfn++ ;
                    if ( *startfn == '\0' ) {
                        /*
                        ** Only one name
                        */
                        if ( (localfilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (remotefilename = (char *) malloc (2+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(localfilename) ;
                            return -1 ;
                        }
                        sprintf(remotefilename,"./") ;
                        sscanf(action,"%s",localfilename) ;
                    } else {
                        /*
                        ** two names
                        */
                        if ( (localfilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (remotefilename = (char *) malloc (strlen(startfn)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(localfilename) ;
                            return -1 ;
                        }
                        sscanf(action,"%s",localfilename) ;
                        sscanf(startfn,"%s",remotefilename) ;
                    }
                }
                retcode = bbftp_mput(localfilename,remotefilename,&errcode) ;
                free(remotefilename) ;
                free(localfilename) ;
           } else if (  !strncmp(mybuffercmd,"put",3) ) {
/*******************************************************************************
** put                                                                         *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** put needs one or two parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** put needs one or two parameters
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
                    if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        return -1 ;
                    }
                    if ( (realfilename = (char *) malloc (strlen(action)+11)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                        myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        free(curfilename) ;
                        curfilename=NULL ;
                       return -1 ;
                    }
                    /*
                    ** We usesscanf  in order to avoid white space
                    */
                    sscanf(action,"%s",curfilename) ;
                    sscanf(action,"%s",remotefilename) ;
                    sprintf(realfilename,"%s",curfilename) ;
                } else {
                    while ( *startfn == ' ' ) startfn++ ;
                    if ( *startfn == '\0' ) {
                        /*
                        ** Only one name
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (realfilename = (char *) malloc (strlen(action)+11)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            free(curfilename) ;
                            curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",curfilename) ;
                        sscanf(action,"%s",remotefilename) ;
                        sprintf(realfilename,"%s",curfilename) ;
                    } else {
                        /*
                        ** two names
                        */
                        if ( (remotefilename = (char *) malloc (strlen(startfn)+strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","curfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename);
                            return -1 ;
                        }
                        if ( (realfilename = (char *) malloc (strlen(action)+11)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","realfilename",strerror(errno)) ;
                            myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(curfilename) ;
                            free(remotefilename);
                            curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",curfilename) ;
                        sprintf(realfilename,"%s",curfilename) ;
                        sscanf(startfn,"%s",remotefilename) ;
                        /* If remote file name ends with '/', add curfilename */
                        if (*(remotefilename+(strlen(remotefilename)-1)) == '/') {
                            if (rindex(curfilename, '/') != NULL) {
                                sprintf(remotefilename,"%s%s",startfn,rindex(curfilename,'/'));
                            } else {
                                sprintf(remotefilename,"%s%s",startfn,curfilename);
                            }
                        }
                    }
                }
                retcode = bbftp_put(remotefilename,&errcode) ;
                free(remotefilename) ;
            } else if (  !strncmp(mybuffercmd,"rm",2) ) {
/*******************************************************************************
** rm                                                                       *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** rm needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** rm needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the file
                */
                retcode = bbftp_rm(action,&errcode) ;
            } else if (  !strncmp(mybuffercmd,"stat",4) ) {
/*******************************************************************************
** stat                                                                       *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** stat needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the directory
                */
                retcode = bbftp_stat(action,&errcode) ;
            } else if (  !strncmp(mybuffercmd,"df",2) ) {
/*******************************************************************************
** df                                                                       *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** stat needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                /*
                ** action is now pointing to the directory
                */
                retcode = bbftp_statfs(action,&errcode) ;
            } else if (  !strncmp(mybuffercmd,"setbuffersize",13) ) {
/*******************************************************************************
** setbuffersize                                                               *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setbuffersize needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setbuffersize needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (remoteumask must be numeric octal)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                buffersizeperstream = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
            } else if (  !strncmp(mybuffercmd,"setlocalcos",11) ) {
/*******************************************************************************
** setlocalcos                                                                 *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setlocalcos needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setlocalcos needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%o",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (localcos must be numeric octal)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                localcos = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
#endif
            } else if (  !strncmp(mybuffercmd,"setlocalumask",13) ) {
/*******************************************************************************
** setlocalumask                                                               *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setlocalumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setlocalumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%o",&alluse) ;
                if ( retcode != 1  || alluse < 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (localumask must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                umask(alluse) ;
                localumask = alluse ;
                free(dupbuffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setnbstream",11) ) {
/*******************************************************************************
** setnbstream                                                                 *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setnbstream needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setnbstream needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (number of stream must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                nbport = alluse ;
                free(dupbuffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                return 0 ;
           } else if (  !strncmp(mybuffercmd,"setoption",9) ) {
/*******************************************************************************
** setoption                                                                   *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setoption needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setoption needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( !strncmp(action,"no",2) ) {
                    nooption = 1 ;
                    action = action+2 ;
                } else {
                    nooption = 0 ;
                }
                if ( !strncmp(action,"createdir",9) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_DIR ;
                    } else {
                        transferoption = transferoption | TROPT_DIR ;
                   }
                } else if ( !strncmp(action,"tmpfile",7) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_TMP ;
                    } else {
                        transferoption = transferoption | TROPT_TMP ;
                   }
                } else if ( !strncmp(action,"remoterfio",10) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_RFIO ;
                    } else {
                        transferoption = transferoption | TROPT_RFIO ;
                    }
                } else if ( !strncmp(action,"localrfio",9) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_RFIO_O ;
                    } else {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
                        transferoption = transferoption | TROPT_RFIO_O ;
#else
                        printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (RFIO not supported)\n",buffercmd) ;
                        myexitcode = 26 ;
                        free(dupbuffercmd) ;
                        return -1 ;
#endif
                    }
                } else if ( !strncmp(action,"keepmode",8) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_MODE ;
                    } else {
                        transferoption = transferoption | TROPT_MODE ;
                    }
                } else if ( !strncmp(action,"keepaccess",10) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_ACC ;
                    } else {
                        transferoption = transferoption | TROPT_ACC ;
                    }
                } else if ( !strncmp(action,"gzip",4) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_GZIP ;
                    } else {
                        transferoption = transferoption | TROPT_GZIP ;
                    }
                } else if ( !strncmp(action,"qbss",4) ) {
                    if ( nooption ) {
                        transferoption = transferoption & ~TROPT_QBSS ;
                    } else {
                        transferoption = transferoption | TROPT_QBSS ;
                    }
                } else {
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setremotecos",12) ) {
/*******************************************************************************
** setremotecos                                                                *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setremotecos needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setremotecos needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/ ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (remotecos must be numeric octal)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = bbftp_setremotecos(alluse,&errcode) ;
                if ( retcode == BB_RET_OK ) remotecos = alluse ;
            } else if (  !strncmp(mybuffercmd,"setremoteumask",14) ) {
/*******************************************************************************
** setremoteumask                                                              *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setremoteumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setremoteumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%o",&alluse) ;
                if ( retcode != 1 || alluse < 0) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (remoteumask must be numeric octal)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = bbftp_setremoteumask(alluse,&errcode) ;
                if ( retcode == BB_RET_OK ) remoteumask = alluse ;
            } else if (  !strncmp(mybuffercmd,"setrecvwinsize",14) ) {         
/*******************************************************************************
** setrecvwinsize                                                              *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (recvwinsize must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                recvwinsize = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setsendwinsize",14) ) {
/*******************************************************************************
** setsendwinsize                                                              *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (sendwinsize must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                sendwinsize = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setackto",8) ) {
/*******************************************************************************
** setackto                                                                    *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setackto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setackto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (ackto must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                ackto = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setrecvcontrolto",16) ) {
/*******************************************************************************
** setrecvcontrolto                                                            *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setrecvcontrolto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setrecvcontrolto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (recvcontrolto must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                recvcontrolto = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setsendcontrolto",16) ) {
/*******************************************************************************
** setsendcontrolto                                                            *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setsendcontrolto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setsendcontrolto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (sendcontrolto must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                sendcontrolto = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setdatato",9) ) {
/*******************************************************************************
** setdatato                                                            *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setdatato needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setdatato needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26,timestamp,"Incorrect command : %s (datato must be numeric)\n",buffercmd) ;
                    myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                datato = alluse ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : %s\n",buffercmd) ;
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                free(dupbuffercmd) ;
                return 0 ;
            }
            /*
            ** Now analyse the return code 
            */
            if ( retcode == BB_RET_FT_NR ) {
                /*
                ** Fatal error no retry and connection is not broken
                */
                break ;
            } else if ( retcode == BB_RET_FT_NR_CONN_BROKEN ) {
                /*
                ** Fatal error no retry and connection is broken
                */
                connectionisbroken = 1 ;
                break ;
            } else if ( retcode == BB_RET_OK ) {
                /*
                ** Success , no retry
                */                            
                break ;
            } else if ( retcode == BB_RET_CONN_BROKEN ) {
                /*
                ** Retry needed with a new connection
                */
                if ( nbtry != globaltrymax ) {
                    sleep(WAITRETRYTIME) ;
                    reconnecttoserver() ;
                } else {
                    /*
                    ** In case of broken connection and max retry reached
                    ** indicate to the next transfer that the connection
                    ** is broken
                    */
                    connectionisbroken = 1 ;
                }
            } else {
                /*
                ** retcode > 0 means retry on this transfer
                */
                if ( nbtry != globaltrymax ) {
                    if ( verbose ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Retrying command waiting %d s\n",WAITRETRYTIME) ;
                    sleep(WAITRETRYTIME) ;
                }
            }
        }
        if ( retcode != 0 ) {
            myexitcode = errcode ;
            free(dupbuffercmd) ;
            return -1 ;
        } else {
		    free(dupbuffercmd) ;
            return 0 ;
        }
    } else {
        printmessage(stderr,CASE_ERROR,25,timestamp,"Unkown command : %s\n",buffercmd) ;
        free(dupbuffercmd) ;
        return -1 ;
    }
}
