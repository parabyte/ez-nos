/* Header for Bpqcode to nos
 * by paul h launspach n5faz
 * (c) 1993,1994 FazCom
 * 4-13-93
 */
/* $Id: bpq.h 1.5 1994/03/23 06:02:48 fz-phl Exp $ */

/* $Log: bpq.h $
 * Revision 1.5  1994/03/23  06:02:48  fz-phl
 * cleaned up ifdefs.
 * added varibles to bpqinfo for bpq trace command.
 * increases BPQSIZE of received mbuf.
 *
 * Revision 1.4  1994/03/11  14:20:00  fz-phl
 * added BPQJNOS to detect 911229 based versions.
 *
 * Revision 1.3  1994/03/11  13:50:49  Johan
 * changed uint16 typedef to #define
 *
 * Revision 1.2  1994/03/11  13:36:08  fz-phl
 * added typedef uint16 and ifdef BPQSOCK for JNOS
 *
 * Revision 1.1  1994/03/11  02:23:37  fz-phl
 * Initial revision
 *
 */

#ifndef _BPQ_H
#define _BPQ_H

#ifndef	_MBUF_H
#include "mbuf.h"
#endif

#ifndef	_IFACE_H
#include "iface.h"
#endif

#define BPQSOCK 1          /* include bpqhost socket code */
                            /* only works with new karncode  */

#define BPQJNOS 1           /* set if we are using a 911229 version */
#ifdef BPQJNOS
#define uint16 int16        /* typedef-ed in new karn global.h */
#undef BPQSOCK
#endif

#define BPQHEADER 5         /* size of header added by bpqcode */
#define BPQMAXPORT 8        /* max number of bpq radio ports supported */
#define BPQMAXSTREAM 64
#define BPQSIZE 440         /* size of the alloc receive mbuf  */
#define BPQDSIZE 256
#define BPQWINDOW 8

#ifndef AXBUF
#define AXBUF 10
#endif

#ifdef BPQSOCK
struct sockaddr_bq {
    short bpq_family;
    char bpq_addr[AXBUF];
    char bpq_appl;
    char bpq_flag;
    char zero[2];
};

struct bpq_cb {
    struct bpq_cb *next;

    char stream;          /* stream number of this connection */
    char appl;           /* application number */
    char applflag;       /* application flags */
    int mode;
    uint16 state;

    struct mbuf *txq;
    struct mbuf *rxq;

    char local[AXBUF];
    char remote[AXBUF];

    uint16 unack;           /* Number of unacked frames */
    uint16 maxframe;        /* Transmit flow control level, frames */
    uint16 paclen;          /* Maximum outbound packet size, bytes */
    int recv;               /* total packets recvd */
    int send;               /* total packets sent  */

    struct bpq_serv *serv;
    int conn;
    int sconn;

    void (*r_upcall)  (struct bpq_cb *, int);
    void (*t_upcall)  (struct bpq_cb *, int);
    void (*s_upcall)  (struct bpq_cb *, int, int);
	int user;			/* User pointer */

};
#define NULLBPQ (struct bpq_cb *)0

struct bpq_servers {
    char *name;                         /* name */
    void (*task)(int,void *,void *);    /* process to run */
    int stack;                          /* stack size */
    char flag;                          /* bpq appl flags */
    int file;                           /* 1 = open network file */
};                                      /* before starting proc */

struct bpq_serv {
    struct bpq_serv *next;
    struct bpq_servers *serv;
    int users;                  /* total users */
    int curr;                   /* current users */
    int s;                      /* listeners socket */
    int appl;                   /* bpq application number */
};
#define NULLBPQS (struct bpq_serv *)0

extern struct bpq_servers Bpqserv[];
#endif /* BPQSOCK */

struct bpqinfo {
    char vec;               /* bpq_host interupt number  */
    char monstream;         /* bpq_host stream number used by nos */
    int maxport;            /* number of bpq ports in use */
    struct iface *ports[BPQMAXPORT];  /* stash for iface structs */
#ifdef TRACE
    struct iface *tport[BPQMAXPORT];
    struct iftype ift;
    int maxtrace;
#endif
    struct mbuf *buffer;    /* mbuf for next received packet */
    int32 recvcnt;          /* total l2 frames received from bpq */
    int32 sendcnt;          /* total l2 frames sent */
    int32 contcnt;          /* number of control calls to bpq */
    int32 txcnt;            /* number of transmit calls to bpq */
    int32 rxcnt;            /* number of receive calls to bpq */
    int minfree;            /* min free buffers  */
#ifdef BPQSOCK
    char maxstream;         /* last stream nos can use */
    struct bpq_cb *cb;      /* list of active connections  */
    struct bpq_serv *serv;  /* list of active servers  */
    struct proc *rxproc;  /* bpq receive process */
    int32 recv;             /* packets recv from bpq_host */
    int32 send;             /* packets sent to bpq_host  */
    int maxconn;            /* total number of connections */
    int nextconn;           /* next connection number */
    int curr;               /* current number of connections */
    uint16 window;             /* packets to send before blocking tx */
#endif
};

extern struct bpqinfo Bpqinfo;
extern char *Bpqstates[];
extern char *Bpqreasons[];

/* bpq_host commands */
#define BPQ_VERSION  0  /* not supported */
#define BPQ_SET      1
#define BPQ_SEND     2
#define BPQ_RECV     3
#define BPQ_STATE    4
#define BPQ_ACKSTATE 5
#define BPQ_CONNECT  6
#define BPQ_BUF      7
#define BPQ_UNACK   17
#define BPQ_RECVQUE 27
#define BPQ_USERCALL 8
#define BPQ_PACLEN  18
#define BPQ_MAXFR   28
#define BPQ_CALLS    9  /* not supported */
#define BPQ_TX      10
#define BPQ_RX      11
#define BPQ_BEACON  12
#define BPQ_TIME    15

/* bpq application flags  used by SET command */
#define BPQ_MONITOR 0x80    /* monitor */
#define BPQ_CMSG_U  0x02    /* Connected to xxx sent to user  */
#define BPQ_CMSG_AP 0x04    /* Connected to xxx sent to application */
#define BPQ_CMD_AP  0x01    /* user command sent to application */

/* bpq application masks  used by SET command  */
#define BPQ_NONE    0x0
#define BPQ_NODE    0x0
#define BPQ_BBS     0x01
#define BPQ_CON     0x02
#define BPQ_SYSOP   0x04
#define BPQ_AP4     0x08
#define BPQ_AP5     0x10
#define BPQ_AP6     0x20
#define BPQ_AP7     0x40
#define BPQ_AP8     0x80

/* bpq CONNECT command flags  */
#define BPQ_CONNODE 0x0     /* outgoing connect using node call */
#define BPQ_CONNBBS 0x0100  /* outgoing connect using bbs call */
#define BPQ_CONN    0x01    /* appl mask == 1 ? bbs call : node call */
#define BPQ_DISC    0x02    /* disconnect */
#define BPQ_RET     0x03    /* return user to node */

/* bpq USERCALL command return value masks */
/* bpq radio port of connection returned in lowbyte if level 2 */
/* session type returned in hibyte as */
#define BPQ_L2      0x0100
#define BPQ_SESSION 0x0200    /* level 4 connection */
#define BPQ_UPLINK  0x0400
#define BPQ_DOWNLK  0x0800
#define BPQ_HOST    0x2000    /* outgoing appl connection */

/* bpq STATE command return value masks */
#define BPQ_ST_DIS  0x0     /* disconnected */
#define BPQ_ST_CON  0x01    /* connected */
#define BPQ_ST_LIS  0x02    /* Listen */
#define BPQ_ST_CHG  0x0100  /* state has changed sence last reported */

/* in bpq.c */
uint16 bpq_int (char, char, int, char *);

/* bpqcmd.c */
void st_bpq (int);
#ifdef TRACE
void bpqshuttrace (void);
#endif
#ifdef BPQSOCK
void st_bpq_cb (struct bpq_cb *);

/* bpqsock.c */
int so_bpq (struct usock *,int);
int so_bpq_listen (struct usock *,int);
int so_bpq_conn (struct usock *);
int so_bpq_recv (struct usock *,struct mbuf **,char *,int *);
int so_bpq_send (struct usock *,struct mbuf *,char *);
int so_bpq_qlen (struct usock *,int);
int so_bpq_shut (struct usock *,int);
int so_bpq_close (struct usock *);
int so_bpq_stat (struct usock *);
char *bpqstate (struct usock *);
char *bpqpsocket (struct sockaddr *);

/* bpqsubr.c */
void bpqrxp (int,void *,void *);
void bpqstop (void);
struct bpq_cb *cr_bpq (void);
void del_bpq (struct bpq_cb *);

/* bpquser.c */
int send_bpq (struct bpq_cb *,struct mbuf *);
struct mbuf *recv_bpq (struct bpq_cb *,uint16);
int bpq_out (struct bpq_cb *);
struct bpq_cb *find_bpq (int);
struct bpq_cb *open_bpq (char *,char *,int,void (*)(),void (*)(),
                          void (*)(),int user);
void disc_bpq (struct bpq_cb *,int);
void reset_bpq (struct bpq_cb *);
int kick_bpq (struct bpq_cb *);
int bpqval (struct bpq_cb *);

/* bpqmail.c */
int bpqstart (int,char *argv[],void *);
int bpq0 (int,char *argv[],void *);
void bpqfinger (int,void *,void *);

#endif /* BPQSOCK */
#endif /* _BPQ_H */
