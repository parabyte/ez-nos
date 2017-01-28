/* ARCNET trace routines
 * Copyright 1990 Russ Nelson
 */
/* Tracing to sockets, mods April '95 by Johan. K. Reinalda, WG7J */
#include "global.h"
#ifdef ARCNET
#include "mbuf.h"
#include "arcnet.h"
#include "trace.h"
#include "socket.h"  

void
arc_dump(s,bpp,check)
int s;
struct mbuf **bpp;
int check;  /* Not used */
{
    struct arc ahdr;
    char src[20],d[20];
  
    ntoharc(&ahdr,bpp);
    parc(src,ahdr.source);
    parc(d,ahdr.dest);
    usprintf(s,"Arcnet: len %u %s->%s",ARCLEN + len_p(*bpp),src,d);
  
    switch(uchar(ahdr.type)){
        case ARC_IP:
            usprintf(s," type IP\n");
            ip_dump(s,bpp,1);
            break;
        case ARC_ARP:
            usprintf(s," type ARP\n");
            arp_dump(s,bpp);
            break;
        default:
            usprintf(s," type 0x%x\n",ahdr.type);
            break;
    }
}
int
arc_forus(iface,bp)
struct iface *iface;
struct mbuf *bp;
{
    return 1;
}
#endif /* ARCNET */
  
