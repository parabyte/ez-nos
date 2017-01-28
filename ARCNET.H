/* Generic ARCnet constants and templates */
  
#ifndef _ARCNET_H
#define _ARCNET_H
  
#ifndef _GLOBAL_H
#include "global.h"
#endif
  
#ifndef _MBUF_H
#include "mbuf.h"
#endif
  
#ifndef _IFACE_H
#include "iface.h"
#endif
  
#define AADDR_LEN   1
/* Format of an ARCnet header */
struct arc {
    char source[AADDR_LEN];
    char dest[AADDR_LEN];
    char type;
};
#define ARCLEN  3
  
/* ARCnet broadcast address */
extern char ARC_bdcst[];
  
/* ARCnet type fields */
#define ARC_IP      0xf0    /* Type field for IP */
#define ARC_ARP     0xf1    /* Type field for ARP */
  
/* In file arcnet.c: */
struct mbuf *htonarc __ARGS((struct arc *arc,struct mbuf *data));
int ntoharc __ARGS((struct arc *arc,struct mbuf **bpp));
char *parc __ARGS((char *out,char *addr));
int garc __ARGS((char *out,char *cp));
int anet_send __ARGS((struct mbuf *bp,struct iface *iface,int32 gateway,int prec,
int del,int tput,int rel));
int anet_output __ARGS((struct iface *iface,char dest[],char source[],int16 type,
struct mbuf *data));
void aproc __ARGS((struct iface *iface,struct mbuf *bp));
  
#endif  /* _ARCNET_H */
  
