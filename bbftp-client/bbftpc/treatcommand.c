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
#include <sys/stat.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

int treatcommand(char *buffercmd)
{
    char    *mybuffercmd ;
    char    *dupbuffercmd ;
    char    *action ;
    char    *startfn ;
    char    *remotefilename ;
    unsigned int uj;
    int     nbtry ;
    int     retcode ;
    int     errcode ;
    int     nooption ;
    int     alluse ;
   unsigned int ualluse;
    char    *localfilename ;
    
    if ( (dupbuffercmd = (char *) malloc (strlen(buffercmd)+1) ) == NULL ) {
        printmessage(stderr,CASE_FATAL_ERROR,35, "Error allocating memory for %s : %s\n","dupbuffercmd",strerror(errno)) ;
    }
    strcpy (dupbuffercmd, buffercmd);
    /*
    ** Strip leading blank
    */
    mybuffercmd = dupbuffercmd ;
    while (*mybuffercmd == ' ') mybuffercmd++;

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
    uj = strlen(mybuffercmd);
   while ((uj > 0) && (mybuffercmd[uj-1] == ' '))
     {
	uj--;
	mybuffercmd[uj] = 0;
     }

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
        for ( nbtry = 1 ; nbtry <= BBftp_Globaltrymax ; nbtry++ ) {
            /*
            ** check if the last try was not disconnected
            */
            if ( BBftp_Connectionisbroken == 1 ) {
                reconnecttoserver() ;
                BBftp_Connectionisbroken = 0 ;
            }
            if (!strncmp(mybuffercmd,"cd",2) ) {
/*******************************************************************************
** cd                                                                          *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** cd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** cd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** cd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
		char lhostname[10 + 1];
		if (gethostname(lhostname, sizeof(lhostname)) < 0) {
			lhostname[0] = '\0';
		} else {
			lhostname[sizeof(lhostname) - 1] = '\0';
		}
                if ( action == NULL ) {
                    /*
                    ** get needs one or two parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** get needs one or two parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (BBftp_Curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        return -1 ;
                    }
                    if ( (BBftp_Realfilename = (char *) malloc (strlen(action)+30)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        free(BBftp_Curfilename) ;
                        BBftp_Curfilename=NULL ;
                       return -1 ;
                    }
                    /*
                    ** We usesscanf  in order to avoid white space
                    */
                    sscanf(action,"%s",BBftp_Curfilename) ;
                    sscanf(action,"%s",remotefilename) ;
                    if ( (BBftp_Transferoption & TROPT_TMP) == TROPT_TMP ) {
#ifdef CASTOR
                        if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                            sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                        } else {
                            sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
                        }
#else                         
                        sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
#endif
                    } else {
                        sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                    }
                } else {
                    while ( *startfn == ' ' ) startfn++ ;
                    if ( *startfn == '\0' ) {
                        /*
                        ** Only one name
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (BBftp_Curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (BBftp_Realfilename = (char *) malloc (strlen(action)+30)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            free(BBftp_Curfilename) ;
                            BBftp_Curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",BBftp_Curfilename) ;
                        sscanf(action,"%s",remotefilename) ;
                        if ( (BBftp_Transferoption & TROPT_TMP) == TROPT_TMP ) {
#ifdef CASTOR
                            if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                                sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                            } else {
                                sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
                            }
#else                         
                            sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
#endif
                        } else {
                            sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                        }
                    } else {
                        /*
                        ** two names
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (BBftp_Curfilename = (char *) malloc (strlen(startfn)+strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (BBftp_Realfilename = (char *) malloc (strlen(startfn)+strlen(action)+30)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            free(BBftp_Curfilename) ;
                            BBftp_Curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",remotefilename) ;
                        sscanf(startfn,"%s",BBftp_Curfilename) ;
                        /* If BBftp_Curfilename ends with '/', add remotefilename without path */
                        if (*(BBftp_Curfilename+(strlen(BBftp_Curfilename)-1)) == '/') {
                            if (rindex(remotefilename, '/') != NULL) {
                                sprintf(BBftp_Curfilename,"%s%s",startfn,rindex(remotefilename,'/'));
                            } else {
                                sprintf(BBftp_Curfilename,"%s%s",startfn,remotefilename);
                            }
                        }
                        if ( (BBftp_Transferoption & TROPT_TMP) == TROPT_TMP ) {
#ifdef CASTOR
                            if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                                sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                            } else {
                                sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
                            }
#else                         
                            sprintf(BBftp_Realfilename,"%s.bbftp.tmp.%s.%d",BBftp_Curfilename,lhostname,getpid()) ;
#endif
                        } else {
                            sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                        }
                    }
                }
                retcode = bbftp_get(remotefilename,&errcode) ;
                BBftp_State = 0 ;
                free(remotefilename) ;
            } else if (  !strncmp(mybuffercmd,"lcd",3) ) {
/*******************************************************************************
** lcd                                                                         *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** lcd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** lcd needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** get needs one or two parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (localfilename = (char *) malloc (2+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
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
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (localfilename = (char *) malloc (2+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
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
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (localfilename = (char *) malloc (strlen(startfn)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** get needs one or two parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (remotefilename = (char *) malloc (2+1)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
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
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (remotefilename = (char *) malloc (2+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
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
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (remotefilename = (char *) malloc (strlen(startfn)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","localfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** put needs one or two parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        return -1 ;
                    }
                    if ( (BBftp_Curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        return -1 ;
                    }
                    if ( (BBftp_Realfilename = (char *) malloc (strlen(action)+11)) == NULL ) {
                        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                        BBftp_Myexitcode = 35 ;
                        free(dupbuffercmd) ;
                        free(remotefilename) ;
                        free(BBftp_Curfilename) ;
                        BBftp_Curfilename=NULL ;
                       return -1 ;
                    }
                    /*
                    ** We usesscanf  in order to avoid white space
                    */
                    sscanf(action,"%s",BBftp_Curfilename) ;
                    sscanf(action,"%s",remotefilename) ;
                    sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                } else {
                    while ( *startfn == ' ' ) startfn++ ;
                    if ( *startfn == '\0' ) {
                        /*
                        ** Only one name
                        */
                        if ( (remotefilename = (char *) malloc (strlen(action)+1)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (BBftp_Curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            return -1 ;
                        }
                        if ( (BBftp_Realfilename = (char *) malloc (strlen(action)+11)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename) ;
                            free(BBftp_Curfilename) ;
                            BBftp_Curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",BBftp_Curfilename) ;
                        sscanf(action,"%s",remotefilename) ;
                        sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                    } else {
                        /*
                        ** two names
                        */
                        if ( (remotefilename = (char *) malloc (strlen(startfn)+strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","remotefilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            return -1 ;
                        }
                        if ( (BBftp_Curfilename = (char *) malloc (strlen(action)+1) ) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Curfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(remotefilename);
                            return -1 ;
                        }
                        if ( (BBftp_Realfilename = (char *) malloc (strlen(action)+11)) == NULL ) {
                            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Realfilename",strerror(errno)) ;
                            BBftp_Myexitcode = 35 ;
                            free(dupbuffercmd) ;
                            free(BBftp_Curfilename) ;
                            free(remotefilename);
                            BBftp_Curfilename=NULL ;
                            return -1 ;
                        }
                        sscanf(action,"%s",BBftp_Curfilename) ;
                        sprintf(BBftp_Realfilename,"%s",BBftp_Curfilename) ;
                        sscanf(startfn,"%s",remotefilename) ;
                        /* If remote file name ends with '/', add BBftp_Curfilename */
                        if (*(remotefilename+(strlen(remotefilename)-1)) == '/') {
                            if (rindex(BBftp_Curfilename, '/') != NULL) {
                                sprintf(remotefilename,"%s%s",startfn,rindex(BBftp_Curfilename,'/'));
                            } else {
                                sprintf(remotefilename,"%s%s",startfn,BBftp_Curfilename);
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** rm needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** mkdir needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setbuffersize needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (remoteumask must be numeric octal)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Buffersizeperstream = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setlocalcos needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%o",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (localcos must be numeric octal)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Localcos = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setlocalumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%o",&ualluse) ;
                if ( retcode != 1) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (localumask must be octal numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                umask(ualluse) ;
                BBftp_Localumask = ualluse ;
                free(dupbuffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
                return 0 ;
            } else if (  !strncmp(mybuffercmd,"setnbstream",11) ) {
/*******************************************************************************
** setnbstream                                                                 *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setnbstream needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setnbstream needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (number of stream must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                BBftp_Nbport = alluse ;
                free(dupbuffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
                return 0 ;
           } else if (  !strncmp(mybuffercmd,"setoption",9) ) {
/*******************************************************************************
** setoption                                                                   *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setoption needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setoption needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
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
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_DIR ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_DIR ;
                   }
                } else if ( !strncmp(action,"tmpfile",7) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_TMP ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_TMP ;
                   }
                } else if ( !strncmp(action,"remoterfio",10) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO ;
                    }
                } else if ( !strncmp(action,"localrfio",9) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_RFIO_O ;
                    } else {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_RFIO_O ;
#else
                        printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (RFIO not supported)\n",buffercmd) ;
                        BBftp_Myexitcode = 26 ;
                        free(dupbuffercmd) ;
                        return -1 ;
#endif
                    }
                } else if ( !strncmp(action,"keepmode",8) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_MODE ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_MODE ;
                    }
                } else if ( !strncmp(action,"keepaccess",10) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_ACC ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_ACC ;
                    }
                } else if ( !strncmp(action,"gzip",4) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_GZIP ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_GZIP ;
                    }
                } else if ( !strncmp(action,"qbss",4) ) {
                    if ( nooption ) {
                        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_QBSS ;
                    } else {
                        BBftp_Transferoption = BBftp_Transferoption | TROPT_QBSS ;
                    }
                } else {
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setremotecos needs one parameter
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 /*|| alluse < 0*/ ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (remotecos must be numeric octal)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = bbftp_setremotecos(alluse,&errcode) ;
                if ( retcode == BB_RET_OK ) BBftp_Remotecos = alluse ;
            } else if (  !strncmp(mybuffercmd,"setremoteumask",14) ) {
/*******************************************************************************
** setremoteumask                                                              *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setremoteumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setremoteumask needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%o",&ualluse) ;
                if ( retcode != 1) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (remoteumask must be numeric octal)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = bbftp_setremoteumask(ualluse,&errcode) ;
                if ( retcode == BB_RET_OK ) BBftp_remoteumask = ualluse ;
            } else if (  !strncmp(mybuffercmd,"setrecvwinsize",14) ) {
/*******************************************************************************
** setrecvwinsize                                                              *
*******************************************************************************/
                if ( action == NULL ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (recvwinsize must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Recvwinsize = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setsendwinsize needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (sendwinsize must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Sendwinsize = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setackto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (ackto must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Ackto = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setrecvcontrolto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (recvcontrolto must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Recvcontrolto = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setsendcontrolto needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (sendcontrolto must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Sendcontrolto = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                while (*action == ' ') action++ ;
                if ( *action == '\0' ) {
                    /*
                    ** setdatato needs one parameters
                    */
                    printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                retcode = sscanf(action,"%d",&alluse) ;
                if ( retcode != 1 || alluse <= 0 ) {
                     printmessage(stderr,CASE_ERROR,26, "Incorrect command : %s (datato must be numeric)\n",buffercmd) ;
                    BBftp_Myexitcode = 26 ;
                    free(dupbuffercmd) ;
                    return -1 ;
                }
                BBftp_Datato = alluse ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : %s\n",buffercmd) ;
                if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
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
                BBftp_Connectionisbroken = 1 ;
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
                if ( nbtry != BBftp_Globaltrymax ) {
                    sleep(WAITRETRYTIME) ;
                    reconnecttoserver() ;
                } else {
                    /*
                    ** In case of broken connection and max retry reached
                    ** indicate to the next transfer that the connection
                    ** is broken
                    */
                    BBftp_Connectionisbroken = 1 ;
                }
            } else {
                /*
                ** retcode > 0 means retry on this transfer
                */
                if ( nbtry != BBftp_Globaltrymax ) {
                    if ( BBftp_Verbose ) printmessage(stdout,CASE_NORMAL,0, "Retrying command waiting %d s\n",WAITRETRYTIME) ;
                    sleep(WAITRETRYTIME) ;
                }
            }
        }
        if ( retcode != 0 ) {
            BBftp_Myexitcode = errcode ;
            free(dupbuffercmd) ;
            return -1 ;
        } else {
		    free(dupbuffercmd) ;
            return 0 ;
        }
    } else {
        printmessage(stderr,CASE_ERROR,25, "Unkown command : %s\n",buffercmd) ;
        free(dupbuffercmd) ;
        return -1 ;
    }
}
