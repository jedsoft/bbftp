#ifndef BBFTPD_PRIVATE_H_
#define BBFTPD_PRIVATE_H_ 1

#include "../includes/structures.h"

extern void clean_child (void);
extern void do_daemon (int argc, char **argv);
extern void sendcrypt (void);
extern int loginsequence (void);
extern int set_signals_v1 (void);
extern int readcontrol (int msgcode, int msglen);

extern int bbftpd_rm(int sock,int msglen);
extern int bbftpd_stat(int sock,int msglen);
extern int bbftpd_statfs(int sock,int msglen);
extern int bbftpd_createreceivesocket(int portnumber,char *logmessage);
extern int bbftpd_getdatasock(int nbsock);
extern int discardmessage(int sock,int msglen,int to);

extern int sendlist (int code, int msglen);

extern void strip_trailing_slashes (char *path);
extern void free_all_var (void);
extern int changetodir (int code, int msglen);
extern void reply(int n, char *str);
extern int readmessage(int sock,char *buffer,int msglen,int to);
extern int createadir(int code, int msglen);
extern int createreceivesock(int port, int socknum, char *logmessage);
extern int readcontrol(int msgcode,int msglen);
extern int sendafile(int code);
extern int storeafile(int code);

extern int checkendchild (int status);
extern void in_sigchld_v1( int sig);
extern void in_sighup_v1(int sig);
extern void in_sigterm_v1(int sig);
extern int set_signals_v1(void);

extern int flagsighup ;
extern int msgsock ;
extern char currentfilename[MAXLENFILE];
extern int childendinerror ;
extern int pid_child[MAXPORT] ;
extern int state ;
extern int unlinkfile ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;
extern	int	datato ;


extern int state ;
extern int msgsock ;
extern int unlinkfile ;
extern int pid_child[MAXPORT] ;
extern	int	recvcontrolto ;


extern struct sockaddr_in his_addr;        /* Remote adresse */
extern struct sockaddr_in ctrl_addr;    /* Local adresse */
extern int  fixeddataport ;

extern int msgsock ;
extern int     recvcontrolto ;

extern int outcontrolsock ;
extern	int	sendcontrolto ;

extern int  sendwinsize ;
extern int  recvwinsize ;
extern struct sockaddr_in his_addr;        /* Remote adresse */
extern struct sockaddr_in ctrl_addr;    /* Local adresse */
extern int  fixeddataport ;
extern int     *myports ;
extern int     *mysockets ;
extern int  transferoption ;
extern int  pasvport_min ;
extern int  pasvport_max ;


extern int transferoption ;
extern int recvcontrolto ;


#ifndef HAVE_NTOHLL
extern my64_t ntohll (my64_t v);
#endif

extern int writemessage(int sock,char *buffer,int msglen,int to);
extern int decodersapass(char *buffer, char *username, char *password);

extern int loginsequence (void);
extern int decodersapass(char *buffer, char *username, char *password);

extern int incontrolsock ;
extern	int	recvcontrolto ;
extern char currentusername[MAXLEN] ;

#ifdef USE_PAM
extern char daemonchar[50] ;
#endif

extern int  outcontrolsock ;
extern	int	sendcontrolto ;


extern int  transferoption ;
extern my64_t  filesize ;
extern int  requestedstreamnumber ;
extern int  buffersizeperstream ;
extern int  maxstreams ;
extern int  filemode ;
extern int  *myports ;
extern char *readbuffer ;
extern char *compbuffer ;
extern int  *mychildren ;
extern int  *mysockets ;
extern int  nbpidchild ;
extern int  unlinkfile ;
extern int  incontrolsock ;
extern int  outcontrolsock ;
extern	int	datato ;
extern	int	ackto ;
extern int  state ;
extern int  childendinerror ;
extern int  flagsighup ;
extern char lastaccess[9] ;
extern char lastmodif[9] ;
extern struct  timeval  tstart ;
extern int  protocolversion ;

extern char currentfilename[MAXLENFILE];
extern char *curfilename;
extern char *realfilename;
extern int curfilenamelen;


/*
** For V1 Protocol
*/
extern int  pid_child[MAXPORT] ;
extern char currentfilename[MAXLENFILE];
/*
** For V1 and V2 Protocol
*/
extern int  flagsighup;
extern int  childendinerror ;
extern int  state ;
extern int  killdone ;
extern int  unlinkfile ;
extern pid_t    fatherpid ;
/*
** For V2 protocol
*/
extern char *realfilename ;
extern char *curfilename ;
extern int  transferoption ;
extern int  *mychildren ;
extern int  nbpidchild ;
extern char lastaccess[9] ;
extern char lastmodif[9] ;
extern int  filemode ;

#endif
