#if !defined TL_SYS_SOCKET_H_INCLUDED

#define TL_SYS_SOCKET_H_INCLUDED

#define MSG_NOSIGNAL 0
#define SHUT_RDWR SD_BOTH

void WinSockPError(char *);
char *StrError(unsigned int ErrNo);
#endif
