#ifndef BBFTP_PRIVATE_H_
#define BBFTP_PRIVATE_H_ 1

/*
** Simulation mode (option -n)
*/
extern int BBftp_Simulation_Mode;

extern int bbftp_rm(char *filename, int *errcode);
extern int bbftp_stat(char *filename,int *errcode);
extern int bbftp_statfs(char *filename,int  *errcode);
extern int bbftp_dir(char *remotefile, int  *errcode);

#ifndef HAVE_NTOHLL
extern my64_t ntohll (my64_t v);
#endif

extern char		*BBftp_Compbuffer ;
extern char		*BBftp_Curfilename ;
extern char		*BBftp_Realfilename ;
extern char		*BBftp_Hostname ;
extern int		*BBftp_Mychildren;
extern int		*BBftp_Myports ;
extern int		*BBftp_Mysockets;
extern char		*BBftp_Password ;
extern char		*BBftp_Readbuffer ;
extern char		*BBftp_Remotedir ;
extern char		*BBftp_Service ;
extern char		*BBftp_SSHcmd ;
extern char		*BBftp_SSHidentityfile ;
extern char		*BBftp_SSHremotecmd  ;
extern char		*BBftp_Username ;
extern int		BBftp_Ackto;
extern int		BBftp_Buffersizeperstream ;
extern int		BBftp_Connectionisbroken ;
extern int		BBftp_Datato ;
extern int		BBftp_Debug ;
extern int		BBftp_Filemode ;
extern my64_t		BBftp_Filesize ;
extern int		BBftp_Globaltrymax;
extern struct		hostent  *BBftp_Hostent ;
extern int		BBftp_Incontrolsock ;
extern int		BBftp_Outcontrolsock ;
extern char		BBftp_Lastaccess[9] ;
extern char		BBftp_Lastmodif[9] ;
extern int		BBftp_Localumask ;
extern int		BBftp_Myexitcode;
extern int		BBftp_Nbport ;
extern int		BBftp_Pasvport_Min ;
extern int		BBftp_Pasvport_Max ;
extern int		BBftp_Protocol ;
extern int		BBftp_Protocolmax;
extern int		BBftp_Protocolmin;
extern int		BBftp_Recvcontrolto;
extern int		BBftp_Recvwinsize ;
extern int		BBftp_Remotecos ;
extern int		BBftp_remoteumask ;
extern int		BBftp_Requestedstreamnumber ;
extern int		BBftp_Resfd ;
extern int		BBftp_Sendcontrolto;
extern int		BBftp_Sendwinsize ;
extern int		BBftp_Simulation_Mode ;
extern struct		sockaddr_in BBftp_His_Ctladdr ;
extern struct		sockaddr_in BBftp_My_Ctladdr ;
extern int		BBftp_SSHbatchmode ;
extern int		BBftp_SSH_Childpid ;
extern int		BBftp_State;
extern int		BBftp_Statoutput ;
extern int		BBftp_Timestamp;
extern int		BBftp_Transferoption ;
extern int		BBftp_Use_SSH ;
extern int		BBftp_Verbose ;
extern int		BBftp_Warning ;
extern int		BBftp_Newcontrolport;

#ifdef CASTOR
extern int		BBftp_Cast_Fd ;
extern char		*BBftp_Cast_Filename ;
#endif

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
extern int		BBftp_Localcos;
#endif

#endif
