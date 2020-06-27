#ifndef BBFTP_PRIVATE_H_
#define BBFTP_PRIVATE_H_ 1

/*
** Simulation mode (option -n)
*/
extern int simulation_mode;

extern int bbftp_rm(char *filename, int *errcode);
extern int bbftp_stat(char *filename,int *errcode);
extern int bbftp_statfs(char *filename,int  *errcode);
extern int bbftp_dir(char *remotefile, int  *errcode);

#ifndef HAVE_NTOHLL
extern my64_t ntohll (my64_t v);
#endif

extern char		*compbuffer ;
extern char		*curfilename ;
extern char		*hostname ;
extern int		*mychildren;
extern int		*myports ;
extern int		*mysockets;
extern char		*password ;
extern char		*readbuffer ;
extern char		*realfilename ;
extern char		*remotedir ;
extern char		*service ;
extern char		*sshcmd ;
extern char		*sshidentityfile ;
extern char		*sshremotecmd  ;
extern int		ackto;
extern int		buffersizeperstream ;
extern int		connectionisbroken ;
extern int		datato ;
extern int		debug ;
extern int		filemode ;
extern my64_t		filesize ;
extern int		globaltrymax;
extern struct		hostent  *hp ;
extern int		incontrolsock ;
extern char		lastaccess[9] ;
extern char		lastmodif[9] ;
extern int		localcos ;
extern int		localumask ;
extern int		myexitcode;
extern int		nbport ;
extern int		outcontrolsock ;
extern int		pasvport_min ;
extern int		pasvport_max ;
extern int		protocol ;
extern int		protocolmax;
extern int		protocolmin;
extern int		recvcontrolto;
extern int		recvwinsize ;
extern int		remotecos ;
extern int		remoteumask ;
extern int		requestedstreamnumber ;
extern int		resfd ;
extern int		sendcontrolto;
extern int		sendwinsize ;
extern int		simulation_mode ;
extern struct		sockaddr_in hisctladdr ;
extern struct		sockaddr_in myctladdr ;
extern int		sshbatchmode ;
extern int		sshchildpid ;
extern int		state;
extern int		statoutput ;
extern int		timestamp;
extern int		transferoption ;
extern int		usessh ;
extern char		*username ;
extern int		verbose ;
extern int		warning ;
extern int		newcontrolport;

#ifdef CASTOR
extern int		castfd ;
extern char		*castfilename ;
#endif

#if defined(WITH_RFIO) || defined(WITH_RFIO64)
extern int		localcos;
#endif

#endif
