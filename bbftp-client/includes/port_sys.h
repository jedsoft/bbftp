#ifdef _SX
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/statfs.h>
#define HAVE_STRUCT_STATFS
#endif

#ifdef IRIX
#include <arpa/inet.h>
#endif
