#if !defined TL_ARPA_INET_H_INCLUDED

#define TL_ARPA_INET_H_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef interface

int inet_aton(char *cp, struct in_addr *addr);
int inet_pton(int af, char *src, void *dst);
char *inet_ntop(int af, void *src, char *dst, int size);

#endif
