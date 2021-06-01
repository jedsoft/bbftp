#ifndef BBFTPD_PRIVATE_H_
#define BBFTPD_PRIVATE_H_ 1

#define BBFTPD_DEFAULT_UMASK (022)

#ifdef __GNUC__
# define ATTRIBUTE_(x) __attribute__ (x)
#else
# define ATTRIBUTE_(x)
#endif
#define ATTRIBUTE_PRINTF(a,b) ATTRIBUTE_((format(printf,a,b)))


#include "../includes/structures.h"

extern void clean_child (void);
extern int do_daemon (int argc, char **argv, int ctrlport);
extern void sendcrypt (void);
extern int set_signals_v1 (void);

extern int bbftpd_rm(int sock,int msglen);
extern int bbftpd_stat(struct mess_dir *msg_file);
extern int bbftpd_statfs(struct mess_dir *msg_file);
extern int bbftpd_createreceivesocket(int portnumber,char *msgbuf, size_t msgbuf_size);
extern int bbftpd_getdatasock(int nbsock);
extern int bbftpd_crypt_init_random (void);
extern int checkfromwhere (int ask_remote, int ctrlport);

extern void *bbftpd_malloc (size_t);
extern int discardmessage(int sock,int msglen,int to);
extern void reply(int n, const char *str) ;
extern int writemessage(int sock, const char *buffer,int msglen,int to) ;

extern int sendlist (int code, int msglen);

extern void strip_trailing_slashes (char *path);
extern void free_all_var (void);
extern int changetodir (int code, int msglen);
extern int readmessage(int sock,char *buffer,int msglen,int to);
extern int createadir(int code, int msglen);
extern int createreceivesock(int port, int socknum, char *logmessage);
extern int sendafile(int code);
extern int storeafile(int code);

extern int checkendchild (int status);
extern void in_sigchld_v1( int sig);
extern void in_sighup_v1(int sig);
extern void in_sigterm_v1(int sig);
extern int set_signals_v1(void);

extern int flagsighup ;
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

extern int decodersapass(struct mess_rsa *msg_rsa, char *username, char *password);
extern int bbftpd_loginsequence (struct message *);

extern	int	recvcontrolto ;
extern char currentusername[MAXLEN] ;

#ifdef USE_PAM
/* extern char daemonchar[50] ; */
#endif

extern	int	sendcontrolto ;


extern int  transferoption ;
extern my64_t  filesize ;
extern int  requestedstreamnumber ;
extern int  buffersizeperstream ;
extern int  maxstreams ;
extern int  *myports ;
extern char *readbuffer ;
extern char *compbuffer ;
extern int  *mychildren ;
extern int  *mysockets ;
extern int  nbpidchild ;
extern int  unlinkfile ;
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

#ifdef BBFTPD_V1_SUPPORT
extern int msgsock ;		       /* protocol 1 */
#endif

extern int incontrolsock ;	       /* protocol 2 */
extern int outcontrolsock ;	       /* protocol 2 */

extern int checkstdinto;
extern int protocolmin;
extern int protocolmax;

extern int bbftpd_log_open (int use_syslog, int log_level, const char *logfile);
extern int bbftpd_log_reopen (void);
extern void bbftpd_log (int priority, const char *format, ...) ATTRIBUTE_PRINTF(2,3);
extern void bbftpd_log_stderr (int priority, const char *format, ...) ATTRIBUTE_PRINTF(2,3);

extern void bbftpd_log_close (void);
extern const char *bbftpd_log_level_string (int level);
extern int bbftpd_log_get_level (const char *name);

extern char *bbftpd_strdup (const char *in);
extern char *bbftpd_strcat (const char *a, const char *b);
extern char *bbftpd_read_file (const char *file, size_t *sizep);

extern int bbftpd_input_pending (int fd, int timeout);
extern int bbftpd_fd_msgwrite_int32 (int fd, int code, int i);
extern int bbftpd_fd_msgwrite_int32_2 (int fd, int code, int i, int j);
extern int bbftpd_fd_msgwrite_len (int fd, int code, int len);
extern int bbftpd_fd_msgread_msg (int fd, struct message *msg, int timeout);
extern int bbftpd_fd_msgread_int32 (int32_t *valp);

/* Like above but use outcontrolsock */
extern int bbftpd_msg_pending (int timeout);
extern int bbftpd_msgwrite_int32 (int code, int i);
extern int bbftpd_msgwrite_int32_2 (int code, int i, int j);
extern int bbftpd_msgwrite_len (int code, int len);
extern int bbftpd_msgread_msg (struct message *msg);
extern int bbftpd_msgread_int32 (int32_t *valp);
extern int bbftpd_msgread_int32_array (int32_t *a, int num);
extern int bbftpd_msgread_bytes (char *bytes, int num);
extern int bbftpd_msgwrite_msg_bytes (int code, char *bytes, int len);
extern int bbftpd_msgwrite_bytes (char *bytes, int len);
extern int _bbftpd_write_int32_array (int32_t *a, int len);

extern void bbftpd_msg_reply (int code, const char *format, ...) ATTRIBUTE_PRINTF(2,3);

extern int bbftp_run_protocol_1 (struct message *msg);
extern int bbftp_run_protocol_2 (void);
extern int bbftp_run_protocol_3 (void);

extern int bbftp_store_process_transfer (char *rfile, char *cfile, int unlink_rfile_upon_fail);


extern int  state ;
extern int  incontrolsock ;
extern int  outcontrolsock ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;
extern char *curfilename ;
extern char *realfilename ;
extern int  curfilenamelen ;
extern int  transferoption ;
extern int  filemode ;
extern char lastaccess[9] ;
extern char lastmodif[9] ;
extern int  sendwinsize ;        
extern int  recvwinsize ;        
extern int  buffersizeperstream ;
extern int  requestedstreamnumber ;
extern my64_t  filesize ;
extern int  *myports ;
extern int  *mychildren ;
extern int  *mysockets ;
extern int  myumask ;
extern char *readbuffer ;
extern char *compbuffer ;
extern int  protocolversion ;
extern  char            currentusername[MAXLEN] ;

#endif
