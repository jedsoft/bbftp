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

#endif
