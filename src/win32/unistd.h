#if !defined TL_UNISTD_H_INCLUDED

#define TL_UNISTD_H_INCLUDED

void sleep(unsigned int Sec);

void srandom(unsigned int Seed);
unsigned int random(void);

char *StrError(unsigned int ErrNo);

int getpid(void);

#define IPTOS_TOS(x) (x & 0x1e)
#define IPTOS_PREC(x) (x & 0xe0)

#endif
