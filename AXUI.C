/*
 * axui <iface> [unproto_call]   --   send unproto packets via <iface>.
 * From TNOS by KO4KS.  Mods by K5JB.
*/  
#include <ctype.h>
#include "global.h"
#ifdef AXUISESSION
#include "session.h"
#include "smtp.h"
#include "usock.h"
#include "socket.h"
#include "mailbox.h"
#include "commands.h"
#include "ax25.h"
#include "pktdrvr.h"
#include "iface.h"

#define HEADER_DIDDLE

int Axui_sock = -1;             /* Socket number listening for AX25 UI frames */
static int ui_timestamp;	/* Time and etc, in displayed header - K5JB */
static char DigisUsed[] = "Illegal when digis in use.\n";

int doaxui(int argc, char *argv[],void *p) {
char *cp;
char name[AXBUF];
char buf[256];
int i;
char tmpcall[AXALEN];
char tmpcall2[AXALEN];
struct session *sp;
struct mbuf *bp;
struct iface *axif,*ifc;
int first = 1;
  
	/* Check if this comes from console */
	if(Curproc->input != Command->input)
		return 1;

	/* Check to see if AXUI is already running. Only one copy at a time */
	if (Axui_sock != -1)	{
		tprintf("%s already running\n", Sestypes[AXUITNC]);
		return 1;
	}

	if(((axif = if_lookup(argv[1])) == NULLIF) || (axif->type != CL_AX25)) {
		tprintf("Iface %s not defined or not an AX25 type interface\n",argv[1]);
		return 1;
	}

	if (argc == 2 || setcall (tmpcall, argv[2]) == -1)
		memcpy (tmpcall, Ax25multi[IDCALL], AXALEN);
        if(argc > 3)  /* digis present? */
            if(connect_filt(argc,argv,tmpcall,axif) == 0)
                return 1;

	/* Now everything seems okay ! Get a session */
	if((sp = newsession("axui",AXUITNC,1)) == NULLSESSION) {
		tputs(TooManySessions);
		return 1;
	}
  
restart:
	tprintf("%s%s session %u UI frames %s->%s on interface %s\n", 
		(first) ? "" : "\n", Sestypes[sp->type],sp->num, pax25(buf, axif->hwaddr), pax25(name,tmpcall), axif->name);
	Axui_sock = Curproc->output;
	first = 0;
  
	/* Process whatever's typed on the terminal */
	while(recvline(Curproc->input,buf,sizeof(buf)-1) >= 0) {
		if(buf[0] == '/') {
			rip (buf);
			cp = skipnonwhite(buf);  /* advance to first arg */
			cp = skipwhite(cp);
			/* process commands */
			switch(tolower(buf[1])) {
				case 'h':
				case '?':	tputs("<Cmds>: /c call; /i iface; /q (to quit); /t (toggle timestamp)\n");
						goto restart;
						/* break; */
				case 'c':	if (argc > 3) {
							tputs(DigisUsed);
							break;
						}
						if (setcall (tmpcall2, cp) == -1)
							break;
						memcpy (tmpcall, tmpcall2, AXALEN);
						goto restart;
						/* break; */
				case 'i':	if (argc > 3) {
							tputs(DigisUsed);
							break;
						}
						if(((ifc = if_lookup(cp)) != NULLIF) && (ifc->type == CL_AX25)) {
							axif = ifc;
							goto restart;
						} else
							tputs ("<invalid interface>\n");
						break;
				case 'b':
				case 'e':
				case 'q':	goto done;
						/* break; */
				case 't':	ui_timestamp = !ui_timestamp;
						break;
			}
		} else	{
			i = strlen(buf);
			if((bp = alloc_mbuf(i)) == NULLBUF)
				goto done;  /* no mem => fatal err */
			/* unwritten protocol is that AX.25 lines end in \r, not \n.
			 * recvline will always return at least one character.  If the
			 * operater typed more than sizeof(buf) - 1 without eol, TOUGH! - K5JB
			 */
			buf[i - 1] = '\r';

			bp->cnt = i;	
			memcpy(bp->data,buf,(size_t)i);

			(*axif->output)(axif, tmpcall, axif->hwaddr, PID_NO_L3, bp);	/* send it */
		}
		usflush(Curproc->output);
	}
done:
	if (argc > 3)
		ax_drop(tmpcall, axif, 0);  /* remove digi route added by connect_filt */
	Axui_sock = -1;
	tprintf("\n%s session %u closed: EOF\n", Sestypes[sp->type],sp->num);
	keywait(NULLCHAR,1);
	freesession(sp);
	return 0;
}


static char lastsrc[AXALEN], lastdest[AXALEN];

/* axui_input called from axnl3() */
void
axui_input(iface,axp,src,dest,bp,mcast)
struct iface *iface;
struct ax25_cb *axp;  /* this will be NULL */
char *src;
char *dest;
struct mbuf *bp;
int mcast;
{
time_t timer;
char *cp;
char buf[256];
int16 n;
int16 nn = 0;	/* K5JB */
char thissrc[AXBUF],thisdest[AXBUF];

	if(Axui_sock != -1)	{
		pax25(thissrc,src);
		pax25(thisdest,dest);
#ifdef HEADER_DIDDLE	/* Your choice - K5JB */
		if(memcmp (lastsrc, src, AXALEN) || memcmp (lastdest, dest, AXALEN)){
			memcpy(lastsrc,src,AXALEN);
			memcpy(lastdest,dest,AXALEN);
			if(ui_timestamp){
				time(&timer);
				cp = ctime(&timer);
				usprintf (Axui_sock, "\n%.24s - %s recv: %s->%s:\n", cp, iface->name,
					thissrc, thisdest);
			}else
				usprintf (Axui_sock, "\n%s->%s:\n",thissrc,thisdest);
		}
#else
		if(ui_timestamp){
			time(&timer);
			cp = ctime(&timer);
			usprintf (Axui_sock, "\n%.24s - %s recv: %s->%s:\n", cp, iface->name,
				thissrc, thisdest);
		}else
			usprintf (Axui_sock, "\n%s->%s:\n",thissrc,thisdest);
#endif
		while((n = pullup(&bp,buf,sizeof(buf) - 1)) != 0){	/* added -1 K5JB */
			buf[n] = 0;
			nn = usprintf(Axui_sock,"%s",buf);  /* was usputs */
		}
		if(nn && buf[nn - 1] != '\n')
			usputc (Axui_sock, '\n');	/* rare we wouldn't do this */
		usflush (Axui_sock);
	}
	free_p(bp);
}


#endif /* AXUISESSION */
