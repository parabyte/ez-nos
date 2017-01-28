/* Stuff generic to all ARCnet controllers
 * Copyright 1990 Russ Nelson
 */
#include "global.h"
#ifdef ARCNET
#include "mbuf.h"
#include "iface.h"
#include "timer.h"
#include "arp.h"
#include "ip.h"
#include "arcnet.h"
  
char ARC_bdcst[] = { 0 };
  
/* Convert ARCnet header in host form to network mbuf */
struct mbuf *
htonarc(arc,data)
struct arc *arc;
struct mbuf *data;
{
    struct mbuf *bp;
    register char *cp;
  
    bp = pushdown(data,ARCLEN);
  
    cp = bp->data;
  
    memcpy(cp,arc->source,AADDR_LEN);
    cp += AADDR_LEN;
    memcpy(cp,arc->dest,AADDR_LEN);
    cp += AADDR_LEN;
    *cp++ = arc->type;
  
    return bp;
}
/* Extract ARCnet header */
int
ntoharc(arc,bpp)
struct arc *arc;
struct mbuf **bpp;
{
    pullup(bpp,arc->source,AADDR_LEN);
    pullup(bpp,arc->dest,AADDR_LEN);
    arc->type = PULLCHAR(bpp);
    return ARCLEN;
}
  
/* Format an ARCnet address into a printable ascii string */
char *
parc(out,addr)
char *out,*addr;
{
    sprintf(out,"%02x", uchar(addr[0]));
    return  out;
}
  
/* Convert an ARCnet address from Hex/ASCII to binary */
int
garc(out,cp)
register char *out;
register char *cp;
{
    *out = htoi(cp);
    return 0;
}
/* Send an IP datagram on ARCnet */
int
anet_send(bp,iface,gateway,prec,del,tput,rel)
struct mbuf *bp;    /* Buffer to send */
struct iface *iface;    /* Pointer to interface control block */
int32 gateway;      /* IP address of next hop */
int prec;
int del;
int tput;
int rel;
{
    char *agate;
  
    agate = res_arp(iface,ARP_ARCNET,gateway,bp);
    if(agate != NULLCHAR)
        return (*iface->output)(iface,agate,iface->hwaddr,ARC_IP,bp);
    return 0;
}
/* Send a packet with ARCnet header */
int
anet_output(iface,dest,source,type,data)
struct iface *iface;    /* Pointer to interface control block */
char *dest;     /* Destination ARCnet address */
char *source;       /* Source ARCnet address */
int16 type;     /* Type field */
struct mbuf *data;  /* Data field */
{
    struct arc ap;
    struct mbuf *bp;
  
    memcpy(ap.dest,dest,AADDR_LEN);
    memcpy(ap.source,source,AADDR_LEN);
    ap.type = type;
    if((bp = htonarc(&ap,data)) == NULLBUF){
        free_p(data);
        return -1;
    }
    return (*iface->raw)(iface,bp);
}
/* Process incoming ARCnet packets. Shared by all ARCnet drivers. */
void
aproc(iface,bp)
struct iface *iface;
struct mbuf *bp;
{
    struct arc hdr;
  
    /* Remove ARCnet header and kick packet upstairs */
    ntoharc(&hdr,&bp);
    switch(uchar(hdr.type)){
        case ARC_ARP:
            arp_input(iface,bp);
            break;
        case ARC_IP:
            ip_route(iface,bp,0);
            break;
        default:
            free_p(bp);
            break;
    }
}
#endif /* ARCNET */
  
