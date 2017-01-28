/*
 * Center for Information Technology Integration
 *           The University of Michigan
 *                    Ann Arbor
 *
 * Dedicated to the public domain.
 * Send questions to info@citi.umich.edu
 *
 * BOOTP is documented in RFC 951 and RFC 1048
 * Delinted, ANSIfied and reformatted - 5/30/91 P. Karn
 *
 * KPK, DOS Solutions, updated for compliance to RFC1048 (2007)
 * tags 0(padding), 1(subnet), 3(gateway), 6(DNS), 12(hostname),
 * 15(Domain), 28(Broadcast), 255(end) implemented.
 */
  
  
#include <time.h>
#include "global.h"
#ifdef BOOTPCLIENT
#include "mbuf.h"
#include "socket.h"
#include "netuser.h"
#include "udp.h"
#include "iface.h"
#include "ip.h"
#include "internet.h"
#include "domain.h"
#include "rip.h"
#include "cmdparse.h"
#include "bootp.h"
#include "commands.h"

/*kpk, I put these here, instead of bootp.h, because bootpd.c uses bootp.h to compile as well */
/*kpk, Probably the only  extra tags from DHCP, that we could use on NOS networks */
#define BOOTP_DOMAIN	15 /* kpk */
#define BOOTP_BCAST		28 /* kpk */

static int bootp_rx __ARGS((struct iface *ifp,struct mbuf *bp));
static void ntoh_bootp __ARGS((struct mbuf **bpp,struct bootp *bootpp));
  
#define BOOTP_TIMEOUT   30      /* Time limit for booting       */
#define BOOTP_RETRANS   5       /* The inteval between sendings */

static char Cookie[5] = {99, 130, 83, 99, 255}; /* kpk, rfc951 vend */
  
int WantBootp = 0;

static int SilentStartup = 0;
  
int
dobootp(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct iface *ifp = NULLIF;
    struct socket lsock, fsock;
    struct mbuf *bp;
	struct udp_cb *bootp_cb;
	struct route *rp; /* kpk, needed to open a broadcast on client iface */
	char *cp;
    time_t now,   /* The current time (seconds)   */
    starttime,    /* The start time of sending BOOTP */
    lastsendtime; /* The last time of sending BOOTP  */
    int i;
	int32 broadcast; /* kpk */

    if(argc < 2)            /* default to the first interface */
        ifp = Ifaces;
    else {
        for(i = 1; i != argc; ++i){
  
            if((ifp = if_lookup(argv[i])) != NULLIF)
                continue;
            else if(strncmp(argv[i], "silent", strlen(argv[i])) == 0)
                SilentStartup = 1;
            else if(strncmp(argv[i], "noisy", strlen(argv[i])) == 0)
                SilentStartup = 0;
            else {
                tprintf("bootp [iface_name] [silent|noisy]\n");
                return 1;
            }
        }
    }
  
    if(ifp == NULLIF)
		return 0;

	/* Add broadcast, on interface(ifp->eth0[en0]), kpk */
	if(ifp != NULLIF && ifp->name != NULLCHAR) {
		broadcast = ifp->addr | ~(ifp->netmask); /* all-ones */ 
     	rp = rt_blookup(ifp->broadcast,32);
     	if(rp != NULLROUTE && rp->iface == ifp)
     	rt_drop(ifp->broadcast,32);
     	ifp->broadcast = broadcast;
		rt_add(ifp->broadcast,32,0L,ifp,1L,0L,0);       
	} else {
		tprintf("No broadcast route on: %s:\n", ifp->name);
	}
    
    lsock.address = ifp->addr;
    lsock.port = IPPORT_BOOTPC;
  
   	if ((bootp_cb = open_udp (&lsock, NULLVFP ((struct iface *, struct udp_cb *, int16)))) == NULLUDP)
		return 0;

	fsock.address = ifp->broadcast = 0xffffffffL; /* kpk, actually need 255.255.255.255 */
    fsock.port = IPPORT_BOOTPS;
  
    /* Get boot starting time */
	(void)time(&starttime);
    lastsendtime = 0;
  
    /* Send the bootp request packet until a response is received or time
       out */
    for(;;){
  
        /* Allow bootp packets should be passed through iproute. */
        WantBootp = 1;
  
        /* Get the current time */
        time(&now);
  
        /* Stop, if time out */
        if(now - starttime >= BOOTP_TIMEOUT){
            tprintf("bootp: timed out, values not set\n");
            break;
        }
  
        /* Don't flood the network, send in intervals */
        if(now - lastsendtime > BOOTP_RETRANS){
			if(!SilentStartup) tprintf("Bootp Request, Broadcasted on: %s\n", ifp->name);
  
            /* Allocate BOOTP packet and fill it in */
            if((bp = alloc_mbuf(sizeof(struct bootp))) == NULLBUF)
                break;
  
			cp = bp->data;      /* names per the RFC1048: */
            *cp++ = BOOTREQUEST;        	/* op */
            *cp++ = ifp->iftype->type;  	/* htype, 1 ethernet */
			*cp++ = ifp->iftype->hwalen;    /* hlen */
            *cp++ = 0;          			/* hops */
            cp = put32(cp,(int32) now); 	/* xid */
			cp = put16(cp, now - starttime);/* secs */
			cp = put16(cp, VF_PCBOOT);  	/* unused flags, 0 or 1 bit for broadcast */
			cp = put32(cp, 0L); 			/* ciaddr, set to 0, kpk, ifp->addr */
			cp = put32(cp, 0L); 			/* yiaddr */
			cp = put32(cp, 0L); 			/* siaddr */
			cp = put32(cp, 0L); 			/* giaddr */
				memmove(cp, ifp->hwaddr, ifp->iftype->hwalen);
			cp += 16; 						/* chaddr, client hardware address */
			memset(cp, 0, 64); 				/* sname, set to 0 */
			cp += 64; 
			memset(cp, 0, 128); 			/* file, set to 0 */
			cp += 128; 
				memcpy(cp, Cookie, strlen(Cookie));
			cp += 64;						/* vend, magic cookie & end tag(255) */
											/* kpk, need to end request, stops
											 * non-rfc1048 response, from DHCP server
											 */
			bp->cnt = cp - bp->data;
            /* assert(bp->cnt == BOOTP_LEN) */
  
            /* Send out one BOOTP Request packet as a broadcast */
            (void)send_udp(&lsock, &fsock,0,0,bp,bp->cnt,0,0); /* kpk, void */
  
            lastsendtime = now;
        }
  
        /* Give other tasks a chance to run. */
		pwait(NULL);
  
        /* Test for and process any replies */
        if(recv_udp(bootp_cb, &fsock, &bp) > -1){
            if(bootp_rx(ifp,bp))
                break;
        } else if(Net_error != WOULDBLK){
            tprintf("bootp: Net_error %d, no values set\n",
            Net_error);
            break;
        }
    }
  
    WantBootp = 0;
    del_udp(bootp_cb);
    return 0;
}
  
/* Process BOOTP input received from 'interface'. */
static int
bootp_rx(ifp,bp)
struct iface *ifp;
struct mbuf *bp;
{
    int         ch;
    int         count;
    int32       gateway = 0;
    int32       nameserver = 0;
    int32       broadcast, netmask;
    struct route    *rp;
    struct bootp    reply;
	char    *cp;
	char *suffix;
	
    if(len_p(bp) != sizeof(struct bootp)){
        free_p(bp);
        return 0;
    }
    ntoh_bootp(&bp, &reply);
    free_p(bp);
  
    if(reply.op != BOOTREPLY)
        return 0;

    if(!SilentStartup)
        tprintf("Network %s configured:\n", ifp->name);
  
    if(ifp->addr == 0L){
        Ip_addr = ifp->addr = (int32) reply.yiaddr.s_addr;    /* yiaddr */
        if(!SilentStartup)
			tprintf("\tIP address: %s\n",
            inet_ntoa(ifp->addr));
    }
  
  
    /* now process the vendor-specific block, check for cookie first. */
	cp = (char *) reply.vend; /* kpk, char added) */

	/* kpk, changed from 0x63825363L, now in bootp.h */
	if(get32(cp) != VM_RFC1048){
        tprintf("Invalid magic cookie.\n");
        return(0);
    }
  
    cp += 4;
	while(((ch = *cp) != BOOTP_END) && (++cp < (reply.vend + 64)))
    switch(ch){
		case BOOTP_PAD:     /* They're just padding (rfc1048 tag 0) */
            continue;
		case BOOTP_SUBNET:  /* fixed length, 4 octets (rfc1048 tag 1) */
            cp++;       	/* moved past length */
  
            /* Set the netmask */
                /* Remove old entry if it exists */
            netmask = get32(cp);
            cp += 4;
  
            rp = rt_blookup(ifp->addr & ifp->netmask,mask2width(ifp->netmask));
            if(rp != NULLROUTE)
                rt_drop(rp->target,rp->bits);
            ifp->netmask = netmask;
			rt_add(ifp->addr,mask2width(ifp->netmask),0L,ifp,0L,0L,0);

            if(!SilentStartup)
                tprintf("\tSubnet mask: %s\n", inet_ntoa(netmask));
  
            /* Set the broadcast */
			broadcast = ifp->addr | ~(ifp->netmask);
            rp = rt_blookup(ifp->broadcast,32);
            if(rp != NULLROUTE && rp->iface == ifp)
                rt_drop(ifp->broadcast,32);
            ifp->broadcast = broadcast;
			rt_add(ifp->broadcast,32,0L,ifp,1L,0L,1);
  
            if(!SilentStartup)
                tprintf("\tBroadcast: %s\n", inet_ntoa(broadcast));
  
			break;
		case BOOTP_GATEWAY: /* (rfc1048 tag 3) */
            count = (int) *cp;
            cp++;
  
            gateway = get32(cp);

            /* Add the gateway as the default */
            rt_add(0,0,gateway,ifp,1,0,0);
  
            if(!SilentStartup)
                tprintf("\tDefault gateway: %s\n", inet_ntoa(gateway));
            cp += count;
			break;
		case BOOTP_DNS: /* (rfc1048 tag 6) */
            count = (int) *cp;
            cp++;
  
            while(count){
                nameserver = get32(cp);
                add_nameserver(nameserver,0);
                if(!SilentStartup)
                    tprintf("\tNameserver: %s\n", inet_ntoa(nameserver));
                cp += 4;
                count -= 4;
            }
			break;
		case BOOTP_HOSTNAME: /* (rfc1048 tag 12) */
            count = (int) *cp;
            cp++;
  
            if(Hostname != NULLCHAR)
                free(Hostname);
            Hostname = callocw(count+1,1);   /* KD4DTS: accommodate non-NUL-terminated hostnames */
            strncpy(Hostname, cp, count);
            cp += count;
  
            if(!SilentStartup)
                tprintf("\tHostname: %s\n", Hostname);
            break;
		case BOOTP_DOMAIN:  /*kpk, (rfc1048 tag 15) */
			count = (int) *cp;
			cp++;
			
			suffix = callocw(count+1,1);
			strncpy(suffix, cp, count);
			strcat(suffix, ".");
			/* domainsuffix(suffix); */

			cp += count;

			if(!SilentStartup)
				tprintf("\tDomain via DHCP server: %s\n", suffix);
			break;
		case BOOTP_BCAST: /* kpk rfc1048 tag 28 */
			count = (int) *cp;
            cp++;
  
			broadcast = get32(cp);

			rp = rt_blookup(ifp->broadcast,32);
            if(rp != NULLROUTE && rp->iface == ifp)
                rt_drop(ifp->broadcast,32);
            ifp->broadcast = broadcast;
			rt_add(ifp->broadcast,32,0L,ifp,1L,0L,1);

			if(!SilentStartup)
				tprintf("\tBroadcast via DHCP server: %s\n", inet_ntoa(broadcast));

			cp += count;
			break;
        default:        /* variable field we don't know about */
            count = (int) *cp;
            cp++;
  
            cp += count;
            break;
    }
  
    rt_add(ifp->addr,mask2width(ifp->netmask),0L,ifp,1,0,0);
  
    return(1);
}
  
/* kpk, added (char, long, void) */  
static void
ntoh_bootp(bpp, bootpp)
struct mbuf **bpp;
struct bootp *bootpp;
{
	bootpp->op = (char) pullchar(bpp);          /* op */
    bootpp->htype = (char) pullchar(bpp);       /* htype */
    bootpp->hlen = (char) pullchar(bpp);        /* hlen */
    bootpp->hops = (char) pullchar(bpp);        /* hops */
    bootpp->xid = (long) pull32(bpp);          	/* xid */
    bootpp->secs = pull16(bpp);         		/* secs */
    bootpp->unused = pull16(bpp);           	/* unused */
    bootpp->ciaddr.s_addr = pull32(bpp);        /* ciaddr */
    bootpp->yiaddr.s_addr = pull32(bpp);        /* ciaddr */
    bootpp->siaddr.s_addr = pull32(bpp);        /* siaddr */
    bootpp->giaddr.s_addr = pull32(bpp);        /* giaddr */
	(void) pullup(bpp, (char *) bootpp->chaddr, 16);	/* chaddr */
	(void) pullup(bpp, (char *) bootpp->sname, 64);    	/* sname */
	(void) pullup(bpp, (char *) bootpp->file, 128);    	/* file name */
	(void) pullup(bpp, (char *) bootpp->vend, 64);     	/* vendor */
}
  
int
bootp_validPacket(ip, bpp)
struct ip *ip;
struct mbuf **bpp;
{
    struct udp udp;
    struct pseudo_header ph;
    int status;
  
  
    /* Must be a udp packet */
    if(ip->protocol !=  UDP_PTCL)
        return 0;
  
    /* Invalid if packet is not the right size */
    if(len_p(*bpp) != (sizeof(struct udp) + sizeof(struct bootp)))
        return 0;
  
    /* Invalid if not a udp bootp packet */
    ntohudp(&udp, bpp);
  
    status = (udp.dest == IPPORT_BOOTPC) ? 1 : 0;
  
    /* Restore packet, data hasn't changed */
        /* Create pseudo-header and verify checksum */
    ph.source = ip->source;
    ph.dest = ip->dest;
    ph.protocol = ip->protocol;
    ph.length = ip->length - IPLEN - ip->optlen;
  
    *bpp = htonudp(&udp, *bpp, &ph);
  
    return status;
}
#endif /* BOOTPCLIENT */
