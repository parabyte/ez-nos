/* code to allow nos to talk directly to bpqcode
 *
 * by paul h launspach n5faz
 * (c) 1993,1994 FazCom
 * 4-13-93
 */
/* $Id: bpqcmd.c 1.5 1994/03/23 06:10:54 fz-phl Exp $ */
/* $Log: bpqcmd.c $
 * Revision 1.5  1994/03/23  06:10:54  fz-phl
 *     cleaned up the ifdefs.
 *    dobpqstat - removed display of values only used by bpq socket code.
 *   added dobpqtrace to trace unattached bpq ports
 *   dobpq now uses command mux
 *   Further modified by N5KNX for Jnos 1.11*
 *
 * Revision 1.4  1994/03/11  15:25:06  fz-phl
 * added ifdef BPQJNOS for printf -> tprintf macro and
 * changed display of ifp-rawsndcnt to a %lu in dobpqstat.
 *
 * Revision 1.3  1994/03/11  15:13:45  Johan
 * changed printf's to tprintf's
 *
 * Revision 1.2  1994/03/11  15:04:33  fz-phl
 * added ifdef BPQSOCK and other stuff for jnos.
 *
 * Revision 1.1  1994/03/11  02:25:10  fz-phl
 * Initial revision
 *
 */
#include "global.h"
#include "config.h"
#ifdef BPQ
#include "cmdparse.h"
#include "socket.h"
#include "session.h"
#include "usock.h"
#include "bpq.h"
#include "trace.h"

#ifdef BPQJNOS          /* change printf to tprintf for jnos etc */
#define printf tprintf
#endif

int dobpqstat (int,char *argv[],void *);
#ifdef TRACE
static int dobpqtrace (int,char *argv[],void *);
#endif
#ifdef BPQSOCK
static int dobpqconn (int,char *argv[],void *);
static int dobpqreset (int,char *argv[],void *);
static int dobpqwindow (int,char *argv[],void *);
#endif
 
char *Bpqstates[] = {
    "Disconn",
    "Connect",
    "Listen",
    "Lis>Con"
};

char *Bpqreasons[] = {
	"Normal",
	"By Peer",
	"Timeout",
	"Reset",
	"Refused"
} ;

static struct cmds Bpqcmds[] = {
    "status",         dobpqstat,  0, 0, NULLCHAR,
#ifdef TRACE
#ifdef BPQJNOS
    "trace",        dobpqtrace,  0,  1,  "bpq trace <port> <flags>",
#else
    "trace",        dobpqtrace,  512,  1,  "bpq trace <port> <flags>",
#endif
#endif /* trace */
#ifdef BPQSOCK
    "connect",      dobpqconn,  1024, 0, NULLCHAR,
    "reset",        dobpqreset, 0, 2, "bpq reset <bpqcb>",
    "window",       dobpqwindow, 0, 1, "bpq window <frames>",
#endif /* BPQSOCK */
    NULLCHAR,
};

/* Multiplexer for top-level bpq command */
int
dobpq(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    if(!Bpqinfo.vec){
        printf("Bpq must be initialized with: attach bpq init <vec> <stream>\n");
        return 1;
    }
    if(argc < 2)
        return dobpqstat(argc,argv,p);
    return subcmd(Bpqcmds,argc,argv,p);
}

/* display bpq info stats */
static int
dobpqstat(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int i;
    struct iface *ifp;
#ifdef BPQSOCK
    struct bpq_cb *cb;
#endif
    char buf[11];


#ifdef BPQSOCK
    printf("Vec Stream Ports Window Conn: Tot Max Curr Next\n");
    printf(" %2x     %2u    %2u     %2u       %3u %3u  %3u  %3u\n",
        Bpqinfo.vec,Bpqinfo.monstream,Bpqinfo.maxport,Bpqinfo.window,
        Bpqinfo.maxstream-Bpqinfo.monstream + 1,
        Bpqinfo.maxconn,Bpqinfo.curr,Bpqinfo.nextconn+1);
    printf("Recvd  Sent ");
#else
    printf("Vec Stream Ports\n");
    printf(" %2x     %2u    %2u\n",Bpqinfo.vec,
        Bpqinfo.monstream,Bpqinfo.maxport);
#endif
        
    printf("   Rx    Tx   Rxint   Txint  Ctlint Free Min\n");
#ifdef BPQSOCK
    printf("%5lu %5lu ",Bpqinfo.recv,Bpqinfo.send);
#endif
    printf("%5lu %5lu %7lu %7lu %7lu",
        Bpqinfo.recvcnt,Bpqinfo.sendcnt,Bpqinfo.rxcnt,
        Bpqinfo.txcnt,Bpqinfo.contcnt);
        i = bpq_int(BPQ_BUF,1,0,NULLCHAR);
        Bpqinfo.minfree = min(i,Bpqinfo.minfree);
        printf("  %3u %3u\n",i,Bpqinfo.minfree);

    /* print any iface info */
    if(Bpqinfo.maxport){
        printf("\nPort Label      Call      IP-Address      Mtu    Rx    Tx\n");
        for(i=0; i < BPQMAXPORT ; i++)
            if((ifp = Bpqinfo.ports[i]) != NULLIF)
               printf("%4u %-10.10s %-9.9s %-15.15s %3u %5lu %5lu\n",
                 ifp->dev,ifp->name,pax25(buf,ifp->hwaddr),
                 inet_ntoa(ifp->addr),ifp->mtu,ifp->rawrecvcnt,
                 ifp->rawsndcnt);
    }

    /* print any connection status */
#ifdef BPQSOCK
    if(Bpqinfo.cb != NULLBPQ){
        printf("\n&BPQCB   Owner            Sock St Num Recv Sent  Rxq  Txq Appl Tot Curr\n");
        for(cb = Bpqinfo.cb; cb != NULLBPQ ; cb = cb->next)
            st_bpq_cb(cb);
    }
#endif
    /* print the all stream status */
    /* whether we are using it or something else is */
    printf("\n St Num State       Type/Pt User      Recv Unak Maxf Pacl\n");
    for(i=1; i <= BPQMAXSTREAM ; i++)
            st_bpq(i);

    return 0;
}

#ifdef BPQSOCK
void
st_bpq_cb(cb)
struct bpq_cb *cb;
{
    struct usock *up;

    up = itop(cb->user);
    printf("%-8lx %-16.16s %4u %2u %3u %4u %4u %4u %4u",
      ptol(cb),up->owner->name,cb->user,cb->stream,cb->conn,
      cb->recv,cb->send,len_p(cb->rxq),len_q(cb->txq));
    if(cb->serv != NULLBPQS)
        if(cb->state == BPQ_ST_LIS)
            printf("   %2u %3u  %3u",cb->serv->appl,
              cb->serv->users,cb->serv->curr);
        else
            printf("           %3u",cb->sconn);
    printf("\n");
}
#endif

void
st_bpq(i)
int i;
{
#ifdef BPQSOCK
    struct bpq_cb *cb;
#endif
    char buf[11];
    uint16 st;
    uint16 tp;

        /* dont ack the state call. we want who ever is */
        /* really using this stream to see any state changes  */
        /* only display  active streams */

#ifdef BPQSOCK
    cb = find_bpq(i);
    st = bpq_int(BPQ_STATE,i,0,NULLCHAR);
    if(!st && cb == NULLBPQ && i != Bpqinfo.monstream)
        return;
#else
    if(!(st = bpq_int(BPQ_STATE,i,0,NULLCHAR)) && i != Bpqinfo.monstream)
        return;
#endif

#ifdef BPQSOCK
    if(cb != NULLBPQ){
        st |= cb->state;
        printf("%s%2u %3u",i == Bpqinfo.monstream ? "*" : " ",
        cb->stream,cb->conn);
    } else
#endif
        printf("%s%2u    ",i == Bpqinfo.monstream ? "*" : " ",i);

    printf(" %-7s%s",Bpqstates[st & ~BPQ_ST_CHG],
      st & BPQ_ST_CHG ? "+" : " ");

    if(st & BPQ_ST_CON){
        tp = bpq_int(BPQ_USERCALL,i,0,buf);
        if(tp & BPQ_HOST)
            printf("    Host");
        else if(tp & BPQ_SESSION)
            printf(" Circuit");
        else if(tp & BPQ_UPLINK)
            printf("  Uplink");
        else if(tp & BPQ_DOWNLK)
            printf(" Downlnk");
        else if(tp)
            printf(" Unk: %2u",tp);
        else printf("        ");
        if(tp & BPQ_L2)
            printf(" %2u",lobyte(tp));
        else printf("   ");
#ifdef BPQSOCK
        if(cb != NULLBPQ)
            printf(" %9.9s  %3u  %3u  %3u  %3u",buf,
              bpq_int(BPQ_RECVQUE,i,0,NULLCHAR),cb->unack,
              cb->maxframe,cb->paclen);
        else
#endif
            printf(" %9.9s  %3u  %3u  %3u  %3u",buf,
              bpq_int(BPQ_RECVQUE,i,0,NULLCHAR),
              bpq_int(BPQ_UNACK,i,0,NULLCHAR),
              bpq_int(BPQ_MAXFR,i,0,buf),
              bpq_int(BPQ_PACLEN,i,0,buf));
    }
    printf("\n");
}

#ifdef TRACE
/* trace an unattached bpq port */
static int
dobpqtrace(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    char bpq[]="bpq ";
    char hex[]="123456789abcdef";
    struct iface *ifp;
    struct tracecmd *tp;
#ifdef BPQJNOS
    extern int stdoutSock;
#else
    struct session *sp;
#endif
    int i;
    int port;

    if(argc < 2){
        for(i=0;i <= BPQMAXPORT;i++)
            if(Bpqinfo.tport[i] != NULLIF)
                showtrace(Bpqinfo.tport[i]);
        return 0;
    }
    port = htoi(argv[1]);
    if(port < 1 || port > BPQMAXPORT){
        printf("Port must be between 1 and %d.\n",BPQMAXPORT);
        return -1;
    }
    port--;
    bpq[3] = hex[port];
    if(Bpqinfo.ports[port] != NULLIF){
        printf("Port %x is attached!\nUse 'Trace %s <flags>' to trace.\n",
          port+1, Bpqinfo.ports[port]->name);
        return -1;
    }
    if(argc == 2){
        if(Bpqinfo.tport[port] != NULLIF)
            showtrace(Bpqinfo.tport[port]);
        else printf("%s: tracing off\n",bpq);
        return 0;
    }
    if(argc >= 3){
        if(Bpqinfo.tport[port] == NULLIF){
            ifp = (struct iface *)callocw(1,sizeof(struct iface));
            ifp->name = strdup(bpq);
            ifp->iftype = &Bpqinfo.ift;
#ifdef BPQJNOS
            ifp->type = Bpqinfo.ift.type;
#endif
            Bpqinfo.tport[port] = ifp;
            Bpqinfo.maxtrace++;
        } else ifp = Bpqinfo.tport[port];

        for(tp = Tracecmd;tp->name != NULLCHAR;tp++)
            if(strncmp(tp->name,argv[2],strlen(argv[2])) == 0)
                break;
        if(tp->name != NULLCHAR)
            ifp->trace = (ifp->trace & ~tp->mask) | tp->val;
        else
            ifp->trace = htoi(argv[2]);
    }
    if(!Bpqinfo.maxport && ifp->trace != 0)
        bpq_int(BPQ_SET,Bpqinfo.monstream,BPQ_MONITOR,NULLCHAR);

#ifdef BPQJNOS
    /* we steal code from dotrace() in trace.c */
    /* Always default to stdout unless trace file is given */
    if(ifp->trsock != stdoutSock)
        close_s(ifp->trsock);
    ifp->trsock = stdoutSock;
    if(ifp->trfile != NULLCHAR)
        free(ifp->trfile);
    ifp->trfile = NULLCHAR;

    if(argc >= 4){
        if((ifp->trsock = sockfopen(argv[3],APPEND_TEXT)) == -1){
            tprintf("Can't open socket for %s\n",argv[3]);
            ifp->trsock = stdoutSock;
        } else {
            ifp->trfile = strdup(argv[3]);
        }
    } else {
        /* If not from the console, trace to the current output socket! */
        if(Curproc->input != Command->input) {
            /* this comes from a remote connection, ie a user in
             * 'sysop' mode on the bbs or maybe in a TRACESESSION
             */
            ifp->trsock = Curproc->output;
            /* make sure stopping trace doesn't kill connection */
            usesock(ifp->trsock);
        }
    }
    showtrace(ifp);

#else /* BPQJNOS */
	if(ifp->trfp != NULLFILE){
		/* Close existing trace file */
		fclose(ifp->trfp);
		ifp->trfp = NULLFILE;
	}
	if(argc >= 4){
		if((ifp->trfp = fopen(argv[3],APPEND_TEXT)) == NULLFILE){
			printf("Can't write to %s\n",argv[3]);
		}
	} else if(ifp->trace != 0){
		/* Create trace session */
		sp = newsession(Cmdline,ITRACE,1);
		sp->cb.p = NULL;
		sp->proc = sp->proc1 = sp->proc2 = NULLPROC;
		ifp->trfp = sp->output;
		showtrace(ifp);
		getchar();	/* Wait for the user to hit something */
		ifp->trace = 0;
		ifp->trfp = NULLFILE;
		freesession(sp);
	}
#endif /* BPQJNOS */

    if(ifp->trace == 0){
        free(ifp->name);
        free((char *)ifp);
        Bpqinfo.tport[port] = NULLIF;
        Bpqinfo.maxtrace--;
    }
    if(!Bpqinfo.maxtrace && !Bpqinfo.maxport)
        bpq_int(BPQ_SET,Bpqinfo.monstream,0,NULLCHAR);

    return 0;
}

void
bpqshuttrace()
{
    int i;
    struct iface *ifp;

    if(Bpqinfo.vec){ /* must be init'ed */
        for(i=0;i <= BPQMAXPORT; i++)
            if((ifp=Bpqinfo.tport[i]) != NULLIF){
                free(ifp->name);
                free((char *)ifp);
                Bpqinfo.tport[i] = NULLIF;
                Bpqinfo.maxtrace--;
            }
        if(!Bpqinfo.maxtrace && !Bpqinfo.maxport)
            bpq_int(BPQ_SET,Bpqinfo.monstream,0,NULLCHAR);
    }
    return;
}
#endif /* TRACE */

#ifdef BPQSOCK
static int
dobpqconn(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct sockaddr_bq fsocket;
    struct session *sp;
    int s;

    if((sp = newsession(Cmdline,BPQSESSION,1)) == NULLSESSION){
        printf(TooManySessions);
		return 1;
	}
    if((s = socket(AF_BPQ,SOCK_STREAM,0)) == -1){
        printf("Can't create socket\n");
		freesession(sp);
		keywait(NULLCHAR,1);
		return 1;
	}
    fsocket.bpq_family = AF_BPQ;
    memcpy(fsocket.bpq_addr,"SWITCH   \0",AXBUF);
	sp->network = fdopen(s,"r+t");
	setvbuf(sp->network,NULLCHAR,_IOLBF,BUFSIZ);
    return tel_connect(sp, (char *)&fsocket, sizeof(struct sockaddr_bq));
}

static int
dobpqreset(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct bpq_cb *cb;

    cb = (struct bpq_cb *)ltop(htol(argv[1]));
    if(!bpqval(cb)){
        printf(Notval);
		return 1;
	}
    disc_bpq(cb,1);
	return 0;
}

static int
dobpqwindow(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setshort(&Bpqinfo.window,"Bpq window (frames)",argc,argv);
}
#endif /* BPQSOCK */

#endif /* BPQ */

