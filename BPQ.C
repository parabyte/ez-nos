/* code to allow nos to talk directly to bpqcode
 *
 * by paul h launspach n5faz
 * (c) 1993,1994 FazCom
 * 4-13-93
 */
/* $Id: bpq.c 1.5 1994/03/23 06:07:23 fz-phl Exp $ */
/* $Log: bpq.c $
 * Revision 1.5  1994/03/23  06:07:23  fz-phl
 *     fixed compiles with new karn.
 *     cleaned up the ifdefs.
 *     added macro for DUMP and NET_ROUTE to bpq.c.
 *     bpq_raw now checks for free bufs in bpq before sending frames.
 *
 * Revision 1.4  1994/03/11  14:46:16  fz-phl
 * added ifdef BPQJNOS for printf -> tprintf macro and
 * for different dump and net_route calls.
 *
 * Revision 1.3  1994/03/11  14:36:32  Johan
 * changed printf's to tprintf's
 *
 * Revision 1.2  1994/03/11  14:29:02  fz-phl
 * added ifdef BPQSOCK and changed dump and netroute
 * calls for jnos.
 *
 * Revision 1.1  1994/03/11  02:24:15  fz-phl
 * Initial revision
 *
 */

#include <stdio.h>
#include <dos.h>
#include "global.h"
#include "mbuf.h"
#include "proc.h"
#include "iface.h"
#include "ax25.h"
#include "trace.h"
#include "pktdrvr.h"
#include "config.h"
#ifdef BPQ
#include "bpq.h"

#ifdef BPQJNOS          /* change printf to tprintf for jnos etc */
#define printf tprintf
#define DUMP(a,b,c) dump((a),(b),CL_AX25,(c))
#define NET_ROUTE(a,b) net_route((a),CL_AX25,(b))
#else
#define DUMP dump
#define NET_ROUTE net_route
#endif

static int bpq_stop (struct iface *iface);
static int bpq_raw (struct iface *iface, struct mbuf *bp);


struct bpqinfo Bpqinfo = {0};
extern int32 Ip_addr;

/* attach an interface to bpqcode
 *
 * init bpq:
 * argv[0] = bpq
 * argv[1] = init
 * argv[2] = bpq_host intrupt vec in hex
 * argv[3] = bpq_host stream number
 * argv[4] = number of outgoing bpq_host sessions optional
 *
 * attach an iface:
 * argv[0] = bpq
 * argv[1] = bpq radio port number
 * argv[2] = lable
 * argv[3] = mtu optional
 * argv[4] = callsign optional
 *
 */
int
bpq_attach(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    char vec;
    char streem;
    int port;
    int users;
    uint16 mtu;
    char hwaddr[AXALEN];
    struct iface *ifp;
    char *cp;
    char buffer[6];
    static char g8bpq[] = "G8BPQ";

    if(strncmp(argv[1],"init",strlen(argv[1])) == 0){
        if(argc < 4){
#ifdef BPQSOCK
            printf("Usage: attach bpq init <vec> <stream> [<sessions>]\n");
#else
            printf("Usage: attach bpq init <vec> <stream>\n");
#endif
            return -1;
        }
        vec = htoi(argv[2]);
        streem = atoi(argv[3]);
#ifdef BPQSOCK
        users = argc > 4 ? atoi(argv[4]) : 0;
#endif
        if(!Bpqinfo.vec){
            /* check that bpqcode is loaded */
            long vector = (long)getvect(vec);
            movblock(FP_OFF(vector)-7, FP_SEG(vector),
             FP_OFF(buffer),FP_SEG(buffer),strlen(g8bpq));
            if(strncmp(buffer,g8bpq,strlen(g8bpq))){
                printf("Bpqcode not found !\n");
                return -1;
            }
            if(streem < 1 || streem > BPQMAXSTREAM){
                printf("Stream must be between 1 and %d.\n",BPQMAXSTREAM);
                return -1;
            }
#ifdef BPQSOCK
            if(users < 0 || streem + users >= BPQMAXSTREAM){
                printf("Stream plus sessions must be less than %u.\n",
                  BPQMAXSTREAM);
                return -1;
            }
#endif
            Bpqinfo.vec = vec;
            Bpqinfo.monstream = streem;
            Bpqinfo.minfree = bpq_int(BPQ_BUF,1,0,NULLCHAR);
#ifdef BPQSOCK
            Bpqinfo.maxstream = streem + users;
            Bpqinfo.window = BPQWINDOW;
            Bpqinfo.rxproc = newproc("Bpq receive",256,bpqrxp,0,NULL,NULL,0);
#endif
#ifdef TRACE
            Bpqinfo.ift.type = CL_AX25;
#ifndef BPQJNOS
            Bpqinfo.ift.trace = ax25_dump;
            Bpqinfo.ift.addrtest = ax_forus;
#endif
#endif /* trace */
        } else {
            printf("Bpq already initalized");
            return -1;
        }
        return 0;
    }

    if(!Bpqinfo.vec){
        printf("Bpq must be initilized with: attach bpq init <vec> <stream>\n");
        return 1;
    }
    port = atoi(argv[1]);
    mtu = (argc >3) ? max(256,atoi(argv[3])) : Paclen;
    if(port < 1 || port > BPQMAXPORT){
        printf("Port must be between 1 and %d.\n",BPQMAXPORT);
        return -1;
    }
    if(Bpqinfo.ports[--port] != NULLIF){
        printf("Port already used");
        return -1;
    }
    if(if_lookup(argv[2]) != NULLIF){
        printf("Interface %s already exists\n",argv[2]);
		return -1;
	}
    if(Bpqinfo.tport[port] != NULLIF){
        ifp = Bpqinfo.tport[port];
        free(ifp->name);
        Bpqinfo.tport[port] = NULLIF;
        Bpqinfo.maxtrace--;
    } else ifp = (struct iface *)callocw(1,sizeof(struct iface));
    ifp->name = strdup(argv[2]);
    ifp->addr = Ip_addr;
    ifp->mtu = mtu;
    ifp->dev = port+1;
    ifp->raw = bpq_raw;
    ifp->stop = bpq_stop;
    setencap(ifp,"AX25");
    ifp->hwaddr = mallocw(AXALEN);
    if(argc > 4 && setcall(hwaddr,argv[4]) == 0)
        memcpy(ifp->hwaddr,hwaddr,AXALEN);
	else
		memcpy(ifp->hwaddr,Mycall,AXALEN);
    Bpqinfo.ports[port] = ifp;
    ifp->next = Ifaces;
    Ifaces = ifp;
#ifndef BPQJNOS
    cp = if_name(ifp," tx");
    ifp->txproc = newproc(cp,768,if_tx,0,ifp,NULL,0);
#endif
    /* tell bpqcode to send us any received frames */
    if(!Bpqinfo.maxport++
#ifdef TRACE
     && !Bpqinfo.maxtrace
#endif
    )   bpq_int(BPQ_SET,Bpqinfo.monstream,BPQ_MONITOR,NULLCHAR);
    return 0;
}

/* detach a bpq interface
 */
static int
bpq_stop(iface)
struct iface *iface;
{
    int port = iface->dev;
    Bpqinfo.ports[port-1] = NULLIF;
    if(--Bpqinfo.maxport == 0
#ifdef TRACE
    && !Bpqinfo.maxtrace
#endif
    )   /* stop bpqcode sending us frames */
        bpq_int(BPQ_SET,Bpqinfo.monstream,0,NULLCHAR);

    return 0;
}

/* raw send
 */
static int
bpq_raw(iface,bp)
struct iface *iface;
struct mbuf *bp;
{
    uint16 i;

    if(iface == NULLIF || bp == NULLBUF)
        return -1;

    if(bp->next != NULLBUF){
        /* Copy to contiguous buffer */
        struct mbuf *bp1 = copy_p(bp,len_p(bp));
		free_p(bp);
        if((bp = bp1) == NULLBUF)
			return -1;
	}
    i = bpq_int(BPQ_BUF,1,0,NULLCHAR);
    Bpqinfo.minfree = min(i,Bpqinfo.minfree);
    if(i>20){       /* make sure there are free buf in bpqcode */
        Bpqinfo.sendcnt++;
        iface->rawsndcnt++;
        iface->lastsent = secclock();
        bpq_int(BPQ_TX,iface->dev,bp->cnt,bp->data);
    }
    free_p(bp);
    return 0;
}

/* bpq raw receive handler
 * called every tick
 */
void
bpqtimer(void)
{
    char port;
    int txf;

    /* no bpq ports in use  */
    if(!Bpqinfo.maxport)
        return;

    for(;;){
        /* get a mbuf to put received frame in  */
        if(Bpqinfo.buffer == NULLBUF)
            if((Bpqinfo.buffer = alloc_mbuf(BPQSIZE +
              sizeof(struct iface))) == NULLBUF)
                return;
            else
                Bpqinfo.buffer->data += sizeof(struct iface);

        /* no more frames at this time so return */
        if((Bpqinfo.buffer->cnt = bpq_int(BPQ_RX,Bpqinfo.monstream,0,
                Bpqinfo.buffer->data)) == 0)
            return;

        port = Bpqinfo.buffer->data[2]; /* get port number from bpqheader */

        Bpqinfo.buffer->data += BPQHEADER;  /* move past the 5 byte */
        Bpqinfo.buffer->cnt -= BPQHEADER;  /* header bpq sends us */

        txf = port & 0x80;   /* this is a transmited frame */
        port &= 0x7f;        /* mask off is tx frame bit */

        if(port-- > BPQMAXPORT)
            continue;

#ifdef TRACE
        /* trace any unattached bpq ports */
        if(Bpqinfo.tport[port] != NULLIF){
            DUMP(Bpqinfo.tport[port],
               txf ? IF_TRACE_OUT : IF_TRACE_IN,Bpqinfo.buffer);
            continue;
        }
#endif /* TRACE */

        /* received frame not on a port we want so get another */
        if(Bpqinfo.ports[port] == NULLIF)
            continue;

        /* send tracing done here so we can also see frames
         * sent by other apps and bpq itself. */
        if(txf){
            DUMP(Bpqinfo.ports[port],IF_TRACE_OUT,Bpqinfo.buffer);
            continue;
        }

        NET_ROUTE(Bpqinfo.ports[port],Bpqinfo.buffer);
        Bpqinfo.buffer = NULLBUF;
        Bpqinfo.recvcnt++;
    }
}


/* common interupt routine for all bpq_host calls
 */
uint16
bpq_int(command,stream,flag,bp)
char command;
char stream;
int flag;
char *bp;
{
    union REGS regs;
    struct SREGS sregs;
    uint16 rval;

    segread(&sregs);
    regs.h.ah = command;
    regs.h.al =  stream;
    switch(command){
        case BPQ_SET:
        case BPQ_CONNECT:
                regs.h.ch = regs.h.dh = 0;
                regs.h.dl = hibyte(flag);   /* appl mask */
                regs.h.cl = lobyte(flag);   /* appl flags */
                Bpqinfo.contcnt++;
                break;
        case BPQ_SEND:
        case BPQ_TX:
                regs.x.cx = flag;
                sregs.es = FP_SEG(bp);
                regs.x.si = FP_OFF(bp);
                Bpqinfo.txcnt++;
                break;
        case BPQ_RECV:
        case BPQ_RX:
                regs.x.di = FP_OFF(bp);
                sregs.es = FP_SEG(bp);
                Bpqinfo.rxcnt++;
                break;
        case BPQ_BUF:
        case BPQ_UNACK:
        case BPQ_RECVQUE:
                regs.h.ah = BPQ_BUF;
                Bpqinfo.contcnt++;
                break;
        case BPQ_USERCALL:
        case BPQ_PACLEN:
        case BPQ_MAXFR:
                regs.h.ah = BPQ_USERCALL;
                sregs.es = FP_SEG(bp);
                regs.x.di = FP_OFF(bp);
                Bpqinfo.contcnt++;
                break;
        case BPQ_BEACON:
                regs.x.dx = 1;
                sregs.es = FP_SEG(bp);
                regs.x.si = FP_OFF(bp);
                regs.x.cx = flag;
        case BPQ_STATE:
        case BPQ_ACKSTATE:
        case BPQ_TIME:
                Bpqinfo.contcnt++;
                break;
        default:
            return -1;
    }
    int86x(Bpqinfo.vec,&regs,&regs,&sregs);
    switch(command){
        case BPQ_USERCALL:
            bp[10] = '\0';
        case BPQ_TIME:
            rval = regs.x.ax;
            break;
        case BPQ_PACLEN:
        case BPQ_RECVQUE:
            rval = regs.x.bx;
            break;
        case BPQ_RECV:
        case BPQ_RX:
        case BPQ_UNACK:
            rval = regs.x.cx;
            break;
        case BPQ_BUF:
            rval = regs.x.dx;
            break;
        case BPQ_MAXFR:
            rval = max(regs.x.cx,regs.x.dx);
            break;
        case BPQ_STATE:
            rval  = (regs.x.dx << 8) | regs.x.cx;
            break;
        default:
            rval = 0;
    }
    return rval;
}

#endif /* BPQ */

