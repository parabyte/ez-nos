/* Main-level network program:
 * initialization
 * keyboard processing
 * generic user commands
 *
 * Copyright 1991 Phil Karn, KA9Q
 */
#include <time.h>
#if defined(__TURBOC__) && defined(MSDOS)
#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <conio.h>
#include <ctype.h>
#include <dir.h>
#include <alloc.h>
#endif
#ifdef UNIX
#include <fcntl.h>
#endif
#include "global.h"
#ifdef  ANSIPROTO
#include <stdarg.h>
#endif
#include "mbuf.h"
#include "timer.h"
#include "proc.h"
#include "iface.h"
#include "ip.h"
#include "tcp.h"
#include "udp.h"
#include "ax25.h"
#include "kiss.h"
#include "enet.h"
#include "netrom.h"
#include "bpq.h"
#include "ftpcli.h"
#include "telnet.h"
#include "tty.h"
#include "session.h"
#include "hardware.h"
#include "bm.h"
#include "usock.h"
#include "socket.h"
#ifdef LZW
#include "lzw.h"
#endif
#include "cmdparse.h"
#include "commands.h"
#include "daemon.h"
#include "devparam.h"
#include "domain.h"
#include "files.h"
#include "main.h"
#include "mailbox.h"
#include "remote.h"
#include "trace.h"
#ifdef fprintf
#undef fprintf
#endif
#include "mailutil.h"
#include "smtp.h"
#include "index.h"
#ifdef XMS
#include "xms.h"
#endif
#ifdef EMS
#include "memlib.h"
#endif
#ifdef UNIX
#include "sessmgr.h"
#endif
  
#undef BETA 1
  
#ifdef UNIX
/*#define BETA 1*/
#endif
#ifdef BSAHAX
#define BETA 1
#endif
  
#if defined(__TURBOC__) || (defined __BORLANDC__)
/* Don't even think about changing the following #pragma :-) - WG7J */
#pragma option -a-
  
/* The following is from the Borland Runtime Library Version 3.0 :
 * Copyright (c) 1987, 1990 by Borland International
 */
typedef struct
{
    unsigned char windowx1;
    unsigned char windowy1;
    unsigned char windowx2;
    unsigned char windowy2;
    unsigned char attribute;
    unsigned char normattr;
    unsigned char currmode;
    unsigned char screenheight;
    unsigned char screenwidth;
    unsigned char graphicsmode;
    unsigned char snow;
    union {
        char far * p;
        struct { unsigned off,seg; } u;
    } displayptr;
} VIDEOREC;
extern VIDEOREC _video;
  
char *Screen_Address(void){
    /* Might have to add some code to get the address of the virtualized
     * DV screen here, if we go that route.
     */
    return _video.displayptr.p;
}
#pragma option -a
  
#endif /* TURBOC || BORLANDC */
  
extern struct cmds DFAR Cmds[],DFAR Startcmds[],DFAR Stopcmds[],DFAR Attab[];
  
#if     (!defined(MSDOS) || defined(ESCAPE))    /* PC uses F-10 key always */
static char Escape = 0x03;      /* default escape character is ^] changed to ^c*/
#endif
  
#ifdef __TURBOC__
int dofstat __ARGS((void));
#endif
static char Prompt[] = "eznos> ";
char NoRead[] = "Can't read %s: %s\n";
char Badhost[] = "Unknown host %s\n";
char Badinterface[] = "Interface \"%s\" unknown\n";
char Existingiface[] = "Interface %s already exists\n";
char Nospace[] = "No space!!\n";    /* Generic malloc fail message */
char Nosversion[] = "NOS version %s\n";
#ifdef MSDOS
char NosLoadInfo[] = "NOS load info: CS=0x%04x DS=0x%04x";
#endif
char Noperm[] = "Permission denied.\n";
char Nosock[] = "Can't create socket\n";
char SysopBusy[] = "The sysop is unavailable just now. Try again later.\n";
char TelnetMorePrompt[] = "--more--";
char BbsMorePrompt[] = "More(N=no)? ";
char *Hostname;
char *Motd;                     /* Message Of The Day */
#if !defined(MAILBOX) || defined(TTYLINKSERVER) || defined(TTYCALL)
int Attended = TRUE;            /* default to attended mode */
#else
int Attended;
#endif
int ThirdParty = TRUE;                  /* Allows 3rd party mail by default */
int main_exit;          /* from main program (flag) */
int DosPrompt;          /* Use a dos-like prompt */
int Mprunning;          /* flag for other parts (domain) to signal
                         * that we are fully configured running.
                         */
long UtcOffset;         /* UTC - localtime, in secs */

static int ErrorPause;
static int Step;
static int RmLocks = 1;
  
extern int StLen2;
extern char *StBuf2;
extern char *StBuf3;
#ifdef MAILBOX
#define MAXSTATUSLINES 3
#else
#define MAXSTATUSLINES 2
#endif
#ifdef STATUSWIN
int StatusLines = MAXSTATUSLINES;
#else
int StatusLines = 0;
#endif
  
struct proc *Cmdpp;
#ifndef UNIX
struct proc *Display;
#endif
#ifdef  LZW
int Lzwmode = LZWCOMPACT;
int16 Lzwbits = LZWBITS;
#endif
  
#ifdef TRACE
int Tracesession = 1;
struct session *Trace = NULLSESSION;
int TraceColor = 0;    /* PE1DGZ */
int stdoutSock = -1;
#endif
  
static char *DumpAddr = NULL;           /* Memory dump pointer */
time_t StartTime;        /* Time that NOS was started */
static int Verbose;
  
extern void assign_filenames __ARGS((char *));
static void logcmd __ARGS((char *));
/* char *Screen_Address(void); */
  
  
int Numrows,Numcols;    /* screen size at startup - WG7J */
struct hist *Histry;    /* command recall stuff */
int Histrysize;
int Maxhistory = 20;
  
#ifdef MSDOS
#ifdef EMS
int EMS_Available;
#endif
#ifdef XMS
int XMS_Available;
#endif
#endif /* MSDOS */
  
#ifdef UNIX
static char **origargv;
int coredumpok=0;
#endif
  
#ifdef MSDOS
/* The text_info before we start NOS */
struct text_info PrevTi;
#endif
#ifdef STATUSWIN
extern char MainStColors,SesStColors;
#endif
#ifdef SPLITSCREEN
extern char MainColors,SplitColors;
void GiveColor(char *s,char *attr);
#endif
  
#if defined(NNTP) || defined(NNTPS)
extern void RemoveNntpLocks(void);
#endif

#if defined(MEMLOG) && defined(__TURBOC__)
static FILE  *MemRecFp;
extern int memlogger;   /* see alloc.c */
#endif

int main(int argc,char *argv[])
{
    int ForceIndex = 0;
    char *inbuf,*intmp;
    FILE *fp;
    struct mbuf *bp;
    struct tm *tm;
    int c,i;
#ifdef UNIX
    int no_itimer, did_init;
    char *trace_sm = 0, *trace_smopt = 0;
    char *def_sm = 0, *def_smopt = 0;
    static int oops;
#endif
    struct cur_dirs dirs;

#ifdef MSDOS
#ifdef EMS
    unsigned long emssize;
#endif
#ifdef XMS
    long XMS_Ret;
#endif
  
#if defined(MEMLOG) && defined(__TURBOC__)
     if ((MemRecFp = fopen("MEMLOG.DAT", "wb")) != NULLFILE)
         memlogger = fileno(MemRecFp);
#endif
#endif /* MSDOS */

#ifdef UNIX
    if (oops++)
    {
        iostop();
        fprintf(stderr, "NOS PANIC: NOS main re-entered.\n");
        fflush(stderr);
        fflush(stdout);
        kill(getpid(), 11);
    }
    origargv = argv;
#endif
  
    time(&StartTime);           /* NOS Start_Up time */
    /* Adjust timezone if dst is in effect, to yield UtcOffset */
    tm = localtime(&StartTime);
    UtcOffset = timezone - (tm->tm_isdst>0 ? 3600L : 0L);  /* UTC - localtime, in secs */

#ifdef UNIX
    SRANDOM((getpid() << 16) ^ time((long *) 0));
#else
    randomize();
#endif
  
#if defined(__TURBOC__) || defined(__BORLANDC__)
    /* Borland's library calls int10. Some vga mode utilities do not
     * report the screen sizes correctly into the internal _video structure.
     * This can cause the screen size to be faulty in the gettextinfo call.
     * Instead, read the BIOS data area to get the correct screen info,
     * and update the _video structure for later calls to
     * gettextinfo(), clrscr(), etc... - WG7J
     *
     * If this doesn't work, you can now overwrite the values with
     * the -r and -c command line options - WG7J
     */
    Numrows = (int) *(char far *)MK_FP(0x40,0x84) + 1;
    gettextinfo(&PrevTi);
    Numcols = PrevTi.screenwidth;
#ifdef SPLITSCREEN
    MainColors = PrevTi.attribute;
    MainColors &= 0xf7; /* turn off highlite */
#endif
/*
    Numrows = PrevTi.screenheight;
*/
    if(Numrows == 1)
        Numrows = NROWS;  /* use value from config.h */
  
#endif
  
#ifdef MSDOS
    SwapMode = MEM_SWAP;
    Screen = Screen_Address();
  
    /* If EMS is found, XMS is not used for swapping - WG7J */
#ifdef EMS
    if(ememavl(&emssize) == 0) {
        EMS_Available = 1;
        SwapMode = EMS_SWAP;
    }
#endif
  
#ifdef XMS
    if((XMS_Available = Installed_XMS()) != 0)
#ifdef EMS
        if(!EMS_Available)
#endif
            SwapMode = XMS_SWAP;
#endif
  
#endif /* MSDOS */

    Hostname = strdup("unknown");  /* better than (null) */

#ifdef UNIX
    no_itimer = 0;
    did_init = 0;
    while((c = getopt(argc,argv,"d:f:g:u:w:x:y:z:nveltilCDS:T:")) != EOF){
#else
    while((c = getopt(argc,argv,"g:d:r:c:f:m:u:w:x:y:z:nbveltiI")) != EOF){
#endif
        switch(c){
#ifdef  __TURBOC__
            case 'b':       /* Use BIOS for screen output */
                directvideo = 0;
                break;
#endif
            case 'd':   /* Root directory for various files */
                initroot(optarg);
                break;
            case 'e':   /* Pause after error lines */
                ErrorPause = 1;
                break;
#if defined(MSDOS) || defined(UNIX)
            case 'f':
                assign_filenames(optarg);
#ifdef UNIX
                did_init = 1;
#endif
                break;
#endif
#if defined(TRACE)
            case 'g':   /* PE1DGZ: trace color (0=>none, 1=>mono, 2=>color) */
                TraceColor = atoi(optarg);
                break;
            case 'n':
                Tracesession = 0; /* No session for tracing */
                break;
#endif /* TRACE */
            case 'i':
                ForceIndex = 1;
                break;
            case 'I':
                ForceIndex = 2;  /* k5jb: -I => don't check indices */
                break;
            case 'l':   /* Don't remove mail locks */
                RmLocks = 0;
                break;
#ifdef MSDOS
            case 'm':   /* Swap mode, 0=EMS (default),1=XMS,2=MEM,3=FILE - WG7J */
                i = atoi(optarg);
#ifdef XMS
                if(i == 1 && XMS_Available) {
                    SwapMode = XMS_SWAP;
                } else
#endif
                    if(i == 2)
                        SwapMode = MEM_SWAP;
                    else if(i == 3)
                        SwapMode = FILE_SWAP;
                break;
#endif
#ifndef UNIX
            case 'r':   /* Number of rows on screen */
                Numrows = atoi(optarg);
                break;
            case 'c':   /* Number of columns on screen */
                Numcols = atoi(optarg);
                break;
#endif /* !UNIX */
            case 't':
                Step = 1;
            /* note fallthrough that engages -v too */
            case 'v':
                Verbose = 1;
                break;

#ifdef STATUSWIN
            case 'u':   /* No status lines */
                StatusLines = atoi(optarg);
                if(StatusLines > MAXSTATUSLINES)
                    StatusLines = MAXSTATUSLINES;
                break;
        /* Color options - WG7J */
            case 'w':
                GiveColor(optarg,&MainStColors);
                MainStColors |= 0x08; /* Turn on highlite, so it shows on b/w */
                break;
            case 'x':
                GiveColor(optarg,&SesStColors);
                SesStColors |= 0x08; /* Turn on highlite, so it shows on b/w */
                break;
#endif
#ifdef SPLITSCREEN
            case 'y':
                GiveColor(optarg,&MainColors);
                MainColors &= 0xf7; /* Turn off highlite, so high video shows! */
                textattr(MainColors);   /* Set the color for startup screen ! */
                break;
            case 'z':
                GiveColor(optarg,&SplitColors);
                SplitColors |= 0x08;    /* Turn on higlite, so it shows on b/w */
                break;
#endif

#ifdef UNIX
            case 'C':
                coredumpok = 1;  /* for debugging */
                break;

            case 'D':
                no_itimer = 1;	/* for debugging */
                break;
            case 'S':
                if (sm_lookup(optarg, &def_smopt))
                    def_sm = optarg;
                else
                    printf("No session manager \"%s\", using default\n", optarg);
                break;
            case 'T':
                if (sm_lookup(optarg, &trace_smopt))
                    trace_sm = optarg;
                else
                    printf("Session manager for trace not found, using default\n");
                break;
#endif
        }
    }
  
#if defined(UNIX) && defined(DOTNOSRC)
    if (!did_init)
	assign_filenames(".nosrc");
#endif

#ifndef UNIX
#if defined(__TURBOC__) || defined(__BORLANDC__)
    /* Set the internal structure, in case there was a command
     * line overwrite - WG7J
     */
    _video.screenheight = (unsigned char)Numrows;
    _video.windowx2 = (unsigned char)(Numcols - 1);
    _video.windowy2 = (unsigned char)(Numrows - 1);
#endif
    ScreenSize = 2 * Numrows * Numcols;
#endif /* !UNIX */  
  
#ifdef STATUSWIN
#ifdef UNIX
    Numcols = 132;  /* reasonable max value? We could query terminfo DB...*/
#endif

    if(StatusLines > 1) {
#ifdef MAILBOX
        StBuf2 = mallocw(Numcols+3);
        StLen2 = sprintf(StBuf2,"\r\nBBS:");
        if(StatusLines > 2)
#endif
        {
            StBuf3 = mallocw(Numcols+3);
            sprintf(StBuf3,"\r\n");
        }
    }
#endif
  
#ifdef MSDOS
#ifdef XMS
    if(XMS_Available)
        /* Calculate space in kb for screen */
        ScreenSizeK = (ScreenSize / 1024) + 1;
#endif
#endif
  
  
    kinit();
    ipinit();
#ifdef UNIX
    ioinit(no_itimer);
#else
    ioinit();
#endif
    Cmdpp = mainproc("cmdintrp");
  
    Sessions = (struct session *)callocw(Nsessions,sizeof(struct session));
#ifdef TRACE
    /* Map stdout to a socket so that trace code works properly - WG7J */
    if((stdoutSock = stdoutsockfopen()) == -1)
        tputs("Error: stdout socket can not be opened!");

    if(Tracesession)
#ifdef UNIX
    {
        if (!trace_sm)
            trace_sm = Trace_sessmgr;
        Trace = sm_newsession(trace_sm, NULLCHAR, TRACESESSION, 0);
    }
#else
        Trace = newsession(NULLCHAR,TRACESESSION,0);
#endif
#endif /* TRACE */

#ifdef UNIX
    if (!def_sm)
        def_sm = Command_sessmgr;
    Command = Lastcurr = sm_newsession(def_sm, NULLCHAR, COMMAND, 0);
#else
    Command = Lastcurr = newsession(NULLCHAR,COMMAND,0);
#endif
    init_dirs(&dirs);
    Command->curdirs=&dirs;
  
#ifndef UNIX
    Display = newproc("display",250,display,0,NULLCHAR,NULL,0);
#endif
    tprintf(Nosversion,Version);
    tputs(Version2);
    tputs("Copyright 1991 by Phil Karn (KA9Q) and contributors.\n");
#ifdef MSDOS
#ifdef EMS
    if(EMS_Available)
        tputs("EMS detected.\n");
#endif
#ifdef XMS
    if(XMS_Available)
        tputs("XMS detected.\n");
#endif
#endif
  
#ifdef BETA
    tputs("\007\007\007\n==> This is a BETA version; be warned ! <==\n\n");
#endif
  
    rflush();
  
    /* Start background Daemons */
    {
        struct daemon *tp;
  
        for(tp=Daemons;;tp++){
            if(tp->name == NULLCHAR)
                break;
            newproc(tp->name,tp->stksize,tp->fp,0,NULLCHAR,NULL,0);
        }
    }
  
    if(optind < argc){
        /* Read startup file named on command line */
        if((fp = fopen(argv[optind],READ_TEXT)) == NULLFILE)
            tprintf(NoRead,argv[optind],sys_errlist[errno]);
    } else {
        /* Read default startup file named in files.c (autoexec.nos) */
        if((fp = fopen(Startup,READ_TEXT)) == NULLFILE)
            tprintf(NoRead,Startup,sys_errlist[errno]);
    }
    if(fp != NULLFILE){
        inbuf = mallocw(BUFSIZ);
        intmp = mallocw(BUFSIZ);
        while(fgets(inbuf,BUFSIZ,fp) != NULLCHAR){
            strcpy(intmp,inbuf);
            if(Verbose){
                tputs(intmp);
                rflush();
            }
            /* Are we stepping through autoexec.nos ? - WG7J */
            if(Step) {
                int c;
                Command->ttystate.edit = Command->ttystate.echo = 0;
                c = toupper(keywait("Execute cmd ?",1));
                Command->ttystate.edit = Command->ttystate.echo = 1;
                if(c != 'Y')
                    continue;
            }
            if(cmdparse(Cmds,inbuf,NULL) != 0){
                tprintf("input line: %s",intmp);
                if(ErrorPause)
                    keywait(NULLCHAR,1);
                rflush();
            }
        }
        fclose(fp);
        free(inbuf);
        free(intmp);
    }
  
    /* Log that we started nos, but do this after the config file is read
     * such that logging can be disabled - WG7J
     */
#ifdef WHOFOR
    log(-1,"NOS %s (%.24s) was started (availmem=%lu)",Version,WHOFOR,availmem());
#else
    log(-1,"NOS %s was started (availmem=%lu)",Version,availmem());
#endif
  
    /* Update .txt files that have old index files - WG7J (unless bypassed by -I) */
    if (ForceIndex != 2)
        UpdateIndex(NULL,ForceIndex);
  
    if(RmLocks) {
        RemoveMailLocks();
#if defined(NNTP) || defined(NNTPS)
        RemoveNntpLocks();
#endif
#ifdef MBFWD
        (void)rmlock(Historyfile,NULLCHAR);
#endif
    }
  
    Mprunning = 1;  /* we are on speed now */
  
    /* Now loop forever, processing commands */
    for(;;){
        if(DosPrompt)
            tprintf("%s ",dirs.dir);
        if(!StatusLines)
            tprintf("%lu ",farcoreleft());
        tputs(Prompt);
        usflush(Command->output);
        if(recv_mbuf(Command->input,&bp,0,NULLCHAR,0) != -1){
            logcmd(bp->data);
            (void)cmdparse(Cmds,bp->data,Lastcurr);
            free_p(bp);
        }
    }
}
/* Keyboard input process */
/* Modified to support F-key session switching,
 * from the WNOS3 sources - WG7J
 */
void
keyboard(i,v1,v2)
int i;
void *v1;
void *v2;
{
    int c;
    struct mbuf *bp;
    int j,k;
    struct session *sp;
#ifdef STATUSWIN
    int newsession;
#endif
  
    /* Keyboard process loop */
    for(;;){
        c = kbread();
#ifdef STATUSWIN
        newsession = 0;
#endif
#if(!defined(MSDOS) || defined(ESCAPE))
        if(c == Escape && Escape != 0)
            c = -2;
#endif
        if(c == -2 && Current != Command){
            /* Save current tty mode and set cooked */
            Lastcurr = Current;
            Current = Command;
            swapscreen(Lastcurr,Current);
#ifdef STATUSWIN
            newsession = 1;
#endif
        }
        if((c < -2) && (c > -12)) {             /* F1 to F9 pressed */
#ifdef TRACE
            /* If F9 is pressed, -11 is returned and we swap to Trace - WG7J */
            if(c == -11 && Tracesession) {
                if(Current != Trace) {
                    /* Save current tty mode and set cooked */
                    swapscreen(Current,Trace);
                    Lastcurr = Current;
                    Current = Trace;
#ifdef STATUSWIN
                    newsession = 1;
#endif
                } else {
                    /* Toggle back to previous session */
                    Current = Lastcurr;
                    Lastcurr = Trace;
                    swapscreen(Lastcurr,Current);
#ifdef STATUSWIN
                    newsession = 1;
#endif
                }
            } else {
#endif
                /* Swap directly to F-key session - WG7J */
                k = (-1 * c) - 2;
                for(sp = Sessions, j = 0; sp < &Sessions[Nsessions]; sp++) {
                    if(sp->type == COMMAND)
                        continue;
                    j++;
                    if(sp->type != FREE && j == k) {
                        Lastcurr = Current;
                        Current = sp;
                        swapscreen(Lastcurr,Current);
#ifdef STATUSWIN
                        newsession = 1;
#endif
                        break;
                    }
                }
#ifdef TRACE
            }
#endif
        }
#ifdef STATUSWIN
        if(newsession)
            UpdateStatus();
#endif
        Current->row = Numrows - 1 - StatusLines;
#ifdef SPLITSCREEN
        if (Current->split)
            Current->row -= 2;
#endif
        psignal(&Current->row,1);
        if(c >= 0){
#ifdef UNIX
            if (Current->morewait) /* end display pause, if any */
                Current->morewait = 2;
#endif
            /* If the screen driver was in morewait state, this char
             * has woken him up. Toss it so it doesn't also get taken
             * as normal input. If the char was a command escape,
             * however, it will be accepted; this gives the user
             * a way out of lengthy output.
             */
            if(!Current->morewait && (bp = ttydriv(Current,c)) != NULLBUF)
                send_mbuf(Current->input,bp,0,NULLCHAR,0);
        }
    }
}
  
extern int Kblocked;
extern char *Kbpasswd;
#ifdef LOCK
  
/*Lock the keyboard*/
int
dolock(argc,argv,p)
int argc;
char *argv[];
void *p;
{
  
    extern char Noperm[];
  
    /*allow only keyboard users to access the lock command*/
    if(Curproc->input != Command->input) {
        tputs(Noperm);
        return 0;
    }
    if(argc == 1) {
        if(Kbpasswd == NULLCHAR)
            tputs("Set password first\n");
        else {
            Kblocked = 1;
            tputs("Keyboard locked\n");
            Command->ttystate.echo = 0; /* Turn input echoing off! */
        }
        return 0;
    }
    if(argc == 3) {
        if(*argv[1] == 'p') {   /*set the password*/
            free(Kbpasswd);             /* OK if already null */
            Kbpasswd = NULLCHAR;        /* reset the pointer */

            if(!strlen(argv[2]))
                return 0;           /* clearing the buffer */
            Kbpasswd = strdup(argv[2]);
            return 0;
        }
    }
  
    tputs("Usage: lock password \"<unlock password>\"\n"
    "or    'lock' to lock the keyboard\n");
  
    return 0;
}
  
#endif
  
/*this is also called from the remote-server for the 'exit' command - WG7J*/
void
where_outta_here(resetme,retcode)
int resetme, retcode;
{
    time_t StopTime;
    FILE *fp;
    char *inbuf,*intmp;
#if (defined(UNIX) || defined(EXITSTATS)) && defined(MAILBOX)
    extern int Totallogins;
    extern int BbsUsers;
    extern int DiffUsers;
#ifdef MAILCMDS
    extern int MbSent,MbRead,MbRecvd;
#ifdef MBFWD
    extern int MbForwarded;
#endif
#endif /* MAILCMDS */
#endif /* (UNIX||EXITSTATS) && MAILBOX */
  
    /* Execute sequence of commands taken from file "~/onexit.nos" */
    /* From iw0cnb */
    if((fp = fopen(Onexit,READ_TEXT)) != NULLFILE){
        inbuf = malloc(BUFSIZ);  /* n5knx: don't block forever here! */
        intmp = malloc(BUFSIZ);
        if (inbuf && intmp)
        while(fgets(inbuf,BUFSIZ,fp) != NULLCHAR){
            strcpy(intmp,inbuf);
            if(Verbose){
                tputs(intmp);
                rflush();
            }
            if(cmdparse(Cmds,inbuf,NULL) != 0){
                tprintf("input line: %s",intmp);
            }
        }
        fclose(fp);
        free(inbuf);
        free(intmp);
    }
    time(&StopTime);
    main_exit = TRUE;       /* let everyone know we're out of here */
    reset_all();
    if(Dfile_updater != NULLPROC)
        alert(Dfile_updater,0); /* don't wait for timeout */
    pause(3000);    /* Let it finish */
#ifdef TRACE
    shuttrace();
#ifdef BPQ
    bpqshuttrace();
#endif
#endif
#ifdef WHOFOR
    log(-1,"NOS %s (%.24s) was stopped (%d)", Version, WHOFOR, retcode);
#else
    log(-1,"NOS %s was stopped (%d)", Version, retcode);
#endif
#ifdef UNIX
#ifdef MAILBOX
    /* 1.11d: robbed dombmailstats code to log stats at shutdown */ 
    log(-1,"Core: %lu, "
    "Up: %s, "
    "Logins: %d, "
    "Users: %d, "
    "Count: %d",
    farcoreleft(),tformat(secclock()),Totallogins,BbsUsers,DiffUsers);
  
#ifdef MAILCMDS
#ifdef MBFWD
    log(-1,"Sent: %d, "
    "Read: %d, "
    "Rcvd: %d, "
    "Fwd: %d",
    MbSent,MbRead,MbRecvd,MbForwarded);
#else
    log(-1,"Sent: %d, "
    "Read: %d, "
    "Rcvd: %d", MbSent,MbRead,MbRecvd);
#endif
#endif /* MAILCMDS */
#endif /* MAILBOX */

    detach_all_asy();     /* make sure everything is unlocked */
    pwait(NULL);
#endif
    iostop();
#if defined(MEMLOG) && defined(__TURBOC__)
    if (memlogger) {
        memlogger=0;
        fclose(MemRecFp);
    }
#endif

    if(resetme)
#ifdef UNIX
    {
        if (fork() == 0)
            abort();
        execvp(origargv[0], origargv); /* re-run NOS */
    }
#else
    sysreset();
#endif

#ifdef MSDOS
#ifdef EMS
    effreeall();
#endif
#ifdef XMS
    /* Free any possible XMS used for the screen */
    Free_Screen_XMS();
#endif
    window(1,1,Numcols,Numrows);
    textattr(PrevTi.attribute);
    clrscr();
#endif /* MSDOS */

    exit(retcode);
}
  
int
doexit(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int retcode = 0;
  
#if defined(MEMLOG) && defined(__TURBOC__)
    if (memlogger) {
        memlogger=0;
        fclose(MemRecFp);
    }
#endif

    if (argc == 2) retcode = atoi(argv[1]);

    if(strnicmp(Curproc->name, "at ",3) == 0)   /* From the AT command */
        where_outta_here(0, retcode);
  
    if(Curproc->input == Command->input) {  /* From keyboard */
        Command->ttystate.edit = Command->ttystate.echo = 0;
        if(toupper(keywait("Are you sure? ",0))=='Y') {
            tprintf("NOS Exiting - Uptime => %s\n", tformat(secclock()));
            where_outta_here(0,retcode); /*No reset!*/
        }
        Command->ttystate.edit = Command->ttystate.echo = 1;
        return 0;
    }
   /* Anything else; probably mailbox-sysop */
	return -2;
}
  
extern char Chostname[];
  
int
dohostname(argc,argv,p)
int argc;
char *argv[];
void *p;
{
#ifdef CONVERS
    char *cp;
#endif
  
    if(argc < 2)
        tprintf("%s\n",Hostname);
    else {
        struct iface *ifp;
        char *name;
  
        if((ifp = if_lookup(argv[1])) != NULLIF){
            if((name = resolve_a(ifp->addr, FALSE)) == NULLCHAR){
                tputs("Interface address not resolved\n");
                return 1;
            } else {
                free(Hostname);           /* ok if already null */
                Hostname = name;
                tprintf("Hostname set to %s\n", name );
            }
        } else {
            free(Hostname);           /* ok if already null */
            Hostname = strdup(argv[1]);
            /* Remove trailing dot */
            if(Hostname[strlen(Hostname)-1] == '.')
                Hostname[strlen(Hostname)-1] = '\0';
        }
#ifdef CONVERS
    /* If convers hostname not set yet, set it to first 10 chars
     * of the hostname. If there are '.' from the right, cut off
     * before that. - WG7J
     */
        if(Chostname[0] == '\0') {
            strncpy(Chostname,Hostname,CNAMELEN);
            Chostname[CNAMELEN] = '\0';  /* remember is CNAMELEN+1 chars */
            if((cp = strrchr(Chostname,'.')) != NULLCHAR)
                *cp = '\0';
        }
#endif
#if defined(MAILBOX) && !defined(AX25)
        setmbnrid();
#endif
    }
    return 0;
}
  
int Uselog = 1;
  
int
dolog(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setbool(&Uselog,"disk logging",argc,argv);
}

  
void log(int s,char *fmt, ...)
{
    va_list ap;
    char *cp, ML[FILE_PATH_SIZE];
    time_t t;
    int i;
    struct sockaddr fsocket;
    FILE *fp;
  
    if(!Uselog)
        return;
  
    time(&t);
    cp = ctime(&t);
#ifdef UNIX
    if (*(cp+8) == ' ') *(cp+8) = '0';  /* 04 Feb, not b4 Feb */
#endif
  
    sprintf(ML,"%s/%2.2s%3.3s%2.2s",LogsDir,cp+8,cp+4,cp+22);
    if ((fp = fopen(ML,APPEND_TEXT)) == NULLFILE)
        return;
  
    i = SOCKSIZE;
  
    fprintf(fp,"%9.9s",cp+11);
    if(getpeername(s,(char *)&fsocket,&i) != -1)
        fprintf(fp, " %s",psocket(&fsocket));
    fprintf(fp, " - ");
    va_start(ap,fmt);
    vfprintf(fp,fmt,ap);
    va_end(ap);
    fprintf(fp,"\n");
    fclose(fp);
}

#if defined(UNIX) && defined(DOS_NAME_COMPAT)
static char *
dosnameformat(const char *name)
{
    static char dosname[9];  /* 8 + NUL */

    strncpy(dosname, name, 8);  /* copy at most first 8 */
    dosname[8] = '\0';  /* in case was > 8 chars long */
    return dosname;
}
#endif

int
dohelp(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    register struct cmds *cmdp;
    int i;
    char buf[FILE_PATH_SIZE];
#if defined(MORESESSION) || defined(DIRSESSION) || defined(FTPSESSION)
    char **pargv;
#else
    FILE *fp;
#ifdef MAILBOX
    struct mbx *m = NULLMBX;
#endif /* MAILBOX */
#endif /* MORESESSION || DIRSESSION || FTPSESSION */
  
/* ?  => display compiled-command names 
 * help   =or=   help ?  => show /help/help
 * help X => show /help/X
 */
    if(*argv[0] == '?' ) {
        tputs("Main commands:\n");
        memset(buf,' ',sizeof(buf));
        buf[75] = '\n';
        buf[76] = '\0';
        for(i=0,cmdp = Cmds;cmdp->name != NULL;cmdp++,i = (i+1)%5){
            strncpy(&buf[i*15],cmdp->name,strlen(cmdp->name));
            if(i == 4){
                tputs(buf);
                memset(buf,' ',sizeof(buf));
                buf[75] = '\n';
                buf[76] = '\0';
            }
        }
        if(i != 0)
            tputs(buf);
    } else {
        sprintf(buf,"%s/%s",CmdsHelpdir,"help");  /* default */
        if(argc > 1) {
            for(i=0; Cmds[i].name != NULLCHAR; ++i)
                if(!strncmp(Cmds[i].name,argv[1],strlen(argv[1]))) {
                    if(*argv[1] != '?')
#if defined(UNIX) && defined(DOS_NAME_COMPAT)
                        sprintf(buf,"%s/%s",CmdsHelpdir,dosnameformat(Cmds[i].name));
#else
                        sprintf(buf,"%s/%s",CmdsHelpdir,Cmds[i].name);
#endif
                    break;
                }
            if(Cmds[i].name == NULLCHAR) {
                tputs("Unknown command; type \"?\" for list\n");
                return 0;
            }
        }
#if defined(MORESESSION) || defined(DIRSESSION) || defined(FTPSESSION)
        pargv = (char **)callocw(2,sizeof(char *));
        pargv[1] = strdup(buf);
        if(Curproc->input == Command->input) {   /* from console? */
            newproc("more",512,(void (*)__ARGS((int,void *,void *)))morecmd,2,(void *)pargv,p,1);
        } else {
            morecmd(2,pargv,p);
            free(pargv[1]);
            free(pargv);
        }
#else
        if((fp = fopen(buf,READ_TEXT)) == NULLFILE)
            tprintf(NoRead,buf,sys_errlist[errno]);
        else {
#ifdef MAILBOX
            for (m=Mbox; m!=NULLMBX; m=m->next)
                if (m->proc == Curproc) break;
#endif
            sendfile(fp,Curproc->output,ASCII_TYPE,0,m);
            fclose(fp);
        }
#endif
    }
    return 0;
}
  
/* Attach an interface
 * Syntax: attach <hw type> <I/O address> <vector> <mode> <label> <bufsize> [<speed>]
 */
int
doattach(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return subcmd(Attab,argc,argv,p);
}
/* Manipulate I/O device parameters */
int
doparam(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int param,set;
    int32 val;
    register struct iface *ifp;
  
    if((ifp = if_lookup(argv[1])) == NULLIF){
        tprintf(Badinterface,argv[1]);
        return 1;
    }
    if(ifp->ioctl == NULL){
        tputs("Not supported\n");
        return 1;
    }
    if(argc < 3){
        for(param=1;param<=16;param++){
            val = (*ifp->ioctl)(ifp,param,FALSE,0L);
            if(val != -1)
                tprintf("%s: %ld\n",parmname(param),val);
        }
        return 0;
    }
    param = devparam(argv[2]);
    if(param == -1){
        tprintf("Unknown parameter %s\n",argv[2]);
        return 1;
    }
    if(argc < 4){
        set = FALSE;
        val = 0L;
    } else {
        set = TRUE;
        val = atol(argv[3]);
    }
    val = (*ifp->ioctl)(ifp,param,set,val);
    if(val == -1){
        tprintf("Parameter %s not supported\n",argv[2]);
    } else {
        tprintf("%s: %ld\n",parmname(param),val);
    }
    return 0;
}
  
/* Display or set IP interface control flags */
int
domode(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    register struct iface *ifp;
  
    if((ifp = if_lookup(argv[1])) == NULLIF){
        tprintf(Badinterface,argv[1]);
        return 1;
    }
    if(argc < 3){
        tprintf("%s: %s\n",ifp->name,
        (ifp->flags & CONNECT_MODE) ? "VC mode" : "Datagram mode");
        return 0;
    }
    switch(argv[2][0]){
        case 'v':
        case 'c':
        case 'V':
        case 'C':
            ifp->flags |= CONNECT_MODE;
            break;
        case 'd':
        case 'D':
            ifp->flags &= ~CONNECT_MODE;
            break;
        default:
            tprintf("Usage: %s [vc | datagram]\n",argv[0]);
            return 1;
    }
    return 0;
}
  
#if     (!defined(MSDOS) || defined(ESCAPE))
int
doescape(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    if(argc < 2)
        tprintf("0x%x\n",Escape);
    else
        Escape = *argv[1];
    return 0;
}
#endif  MSDOS
  
  
#if defined(REMOTESERVER) || defined(REMOTECLI)
/* Generate system command UDP packet.
 * Synopsis (client commands):
 * remote [-p port#] [-k key] <hostname> reset|exit
 * remote [-p port#] [-a kick_target_hostname] <hostname> kickme
 * remote [-p port] -k key -r dest_ip_addr[/#bits] <hostname> add|drop
 * Synopsis (server commands):
 * remote -g gateway_key  (for gateway add/drop validation)
 * remote -s system_key   (for reset/exit validation)
 */

char remote_options[] =
#ifdef REMOTECLI
                        "a:p:k:"
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
                        "r:"
#endif
#endif /* REMOTECLI */
#ifdef REMOTESERVER
                        "s:"
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
                        "g:"
#endif
#endif /* REMOTESERVER */
                             ;  /* all supported options */
  
int
doremote(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int c;
#ifdef REMOTECLI
    char *cmd,*host, *key = NULLCHAR;
    char *data,x;
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
    char *route = NULLCHAR;
    int rlen;
#endif
    int s;
    int32 addr = 0;
    int16 port = IPPORT_REMOTE;   /* Set default */
    int16 len;
    int klen=0;
	struct sockaddr_in fsock;
    int cleanup;
#endif
  
    optind = 1;             /* reinit getopt() */
    while((c = getopt(argc,argv,remote_options)) != EOF){
        switch(c){
#ifdef REMOTECLI
            case 'a':
                if((addr = resolve(optarg)) == 0){
                    tprintf(Badhost,optarg);
                    return -1;
                }
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'k':
                key = optarg;
                klen = strlen(key);
                break;
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
            case 'r':
                route = optarg;
                rlen = strlen(route);
                break;
#endif
#endif /* REMOTECLI */
#ifdef REMOTESERVER
            case 's':
                free(Rempass);
                Rempass = strdup(optarg);
                return 0;       /* Only set local password */
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
            case 'g':
                free(Gatepass);
                Gatepass = strdup(optarg);
                return 0;       /* set gateway-route password */
#endif
#endif /* REMOTESERVER */
        }
    }
    if(optind > argc - 2){
        tputs("Insufficient args\n");
        return -1;
    }

#ifdef REMOTECLI
    host = argv[optind++];
    cmd = argv[optind];
    fsock.sin_family = AF_INET;
    if((fsock.sin_addr.s_addr = resolve(host)) == 0){
        tprintf(Badhost,host);
        return -1;
    }
    fsock.sin_port = port;
  
    if((s = socket(AF_INET,SOCK_DGRAM,0)) == -1){
        tputs(Nosock);
        return 1;
    }
    len = 1;
    /* Did the user include a password or kick target? */
    if(addr != 0 && cmd[0] == 'k')       /* kick */
        len += sizeof(int32);
  
    if(key) {
        if (cmd[0] == 'r' || cmd[0] == 'e')  /* reset|exit */
            len += klen;
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
        else if (cmd[0] == 'a' || cmd[0] == 'd')  /* add|drop */
            if (route)
                len += rlen+1 + klen;
#endif
    }
  
    data = (len == 1) ? &x : mallocw((size_t)len);

	cleanup = 0;
  
    switch(cmd[0]){
        case 'e':
        case 'r':
            data[0] = (cmd[0] == 'r' ? SYS_RESET : SYS_EXIT);
            if(key)
                strncpy(&data[1],key,(size_t)klen);
            break;

        case 'k':
            data[0] = KICK_ME;
            if(addr)
                put32(&data[1],addr);
            break;

#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
        case 'a':
        case 'd':
            data[0] = (cmd[0] == 'a') ? ROUTE_ME : UNROUTE_ME;
            if (route) {
                data[1] = (char)rlen;
                strcpy(&data[2],route);
                strncpy(&data[2+rlen], key, (size_t)klen); /* always a key too */
            }
            break;
#endif

        default:
            tprintf("Unknown remote command %s\n",cmd);
            cleanup = 1;
	}
	if (!cleanup)
    {
    /* Form the command packet and send it */
    if(sendto(s,data,len,0,(char *)&fsock,sizeof(fsock)) == -1){
        tprintf("sendto failed: %s\n",sys_errlist[errno]);

	}
    if(data != &x)
        free(data);
    close_s(s);
#endif /* REMOTECLI */
    return 0;
}
#endif /* REMOTESERVER || REMOTECLI */
  
/* Remark [args]  --  copy arguments to stdout, then write a NL.  -- N5KNX */
int
doremark(argc,argv,p)
int argc;
char *argv[];
void *p;
{

    while (argc>1) {
        tputs(argv[1]);
        argv++;
        argc--;
        if (argc != 1)
            tputc(' ');
    }
    tputc ('\n');
    return 0;
}

#if defined(MORESESSION) || defined(DIRSESSION) || defined(FTPSESSION)
  
int
morecmd(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct session *sp;
    FILE *fp;
    char fname[FILE_PATH_SIZE];
	int werdone = 0, row = 0;
    int usesession = 0;
  
    /* Use a session if this comes from console - WG7J*/
    if(Curproc->input == Command->input) {
        usesession = 1;
        if((sp = newsession(argv[1],MORE,0)) == NULLSESSION){
            return 1;
        }
        /* Put tty into raw mode so single-char responses will work */
        sp->ttystate.echo = sp->ttystate.edit = 0;
        row = Numrows - 1 - StatusLines;
    }
  
    strcpy(fname,make_fname(Command->curdirs->dir,argv[1]));
    if((fp = fopen(fname,READ_TEXT)) == NULLFILE){
        tprintf(NoRead,fname,sys_errlist[errno]);
        if(usesession) {
            keywait(NULLCHAR,1);
            freesession(sp);
        }
        return 1;
    }
    while(fgets(fname,sizeof(fname),fp) != NULLCHAR){
        if((argc < 3) || (strstr(fname,argv[2])!=NULLCHAR)) {
            tputs(fname);
            if(usesession) {
                if(--row == 0){
                    row = keywait(TelnetMorePrompt,0);
                    switch(row){
                        case -1:
                        case 'q':
						case 'Q':
                        	werdone = 1;
							break;
                        case '\n':
                        case '\r':
                            row = 1;
                            break;
                        case '\t':
                            clrscr();
                        case ' ':
                        default:
                            row = Numrows - 1 - StatusLines;
					}
					if (werdone) /* leave while loop, replaces GOTO 'done' */
                    break;
                }
            }
        }
    }
    fclose(fp);
    if(usesession) {
        keywait(NULLCHAR,1);
        freesession(sp);
    }
    return 0;
}
#endif /* MORE | DIR | FTP SESSION */
  
#ifdef MORESESSION
int
domore(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    char **pargv;
    int i;
  
    if(Curproc->input == Command->input) {
        /* Make private copy of argv and args,
         * spawn off subprocess and return.
         */
        pargv = (char **)callocw((size_t)argc,sizeof(char *));
        for(i=0;i<argc;i++)
            pargv[i] = strdup(argv[i]);
        newproc("more",512,(void (*)__ARGS((int,void *,void *)))morecmd,argc,(void *)pargv,p,1);
    } else
        morecmd(argc,argv,p);
    return 0;
}
#endif /* MORESESSION */
  
#ifdef ALLCMD
int
dotaillog(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    char logfile[FILE_PATH_SIZE];
    time_t t;
    char *cp;
  
    /* Create the log file name */
    time(&t);
    cp = ctime(&t);
#ifdef UNIX
    if (*(cp+8) == ' ') *(cp+8) = '0';  /* 04 Feb, not b4 Feb */
#endif
    sprintf(logfile,"%s/%2.2s%3.3s%2.2s",LogsDir,cp+8,cp+4,cp+22);
  
    argc = 2;   /* ignore any provided args */
    argv[1] = logfile;
    return dotail(argc,argv,p);
}
  
int
dotail(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    register int handle, i;
    register unsigned line = 0, rdsize = 2000;
    long length;
    char *buffer;
    char fname[FILE_PATH_SIZE];
    int maxlines = Numrows - StatusLines - 6;   /* same room for status lines, + fudge */
  
    buffer = callocw(2000, sizeof (char));
  
    strcpy(fname,make_fname(Command->curdirs->dir,argv[1]));  /* WA7TAS: relativize path */
    if ((handle = open (fname, O_BINARY | O_RDONLY)) == -1) {
        tprintf(NoRead,argv[1],sys_errlist[errno]);
        free(buffer);
        return -1;
    }
    length = filelength(handle);
  
    if (length > 2000) {
        length -= 2000;
    } else {
        rdsize = (int) length;
        length = 0;
    }
  
    lseek (handle, length, SEEK_SET);
    if (read (handle, buffer, rdsize) == -1) {
        tprintf(NoRead,argv[1],sys_errlist[errno]);
        close(handle);
        free(buffer);
        return -1;
    }
  
    for (i = rdsize - 1; i > 0; i--) {
        if (buffer[i] == '\n')
            line++;
        if (line == (unsigned)maxlines)
            break;
    }
    for (; (unsigned)i < rdsize; i++)
        tputc(buffer[i]);
  
    tputc('\n');
    close(handle);
    free(buffer);
    return 0;
}
#endif /*ALLCMD*/
  
/* No-op command */
int
donothing(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return 0;
}

/* Pause command */  
int
dopause(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int secs;

    if(argc > 1) {
        secs = atoi(argv[1]);
        if (secs > 0) pause(1000L * secs);
    }
    return 0;
}

#ifdef MAILERROR
static int SendError = 1;
  
int doerror(int argc,char *argv[],void *p) {
    return setbool(&SendError,"Mail errors",argc,argv);
}
  
/* Mail a system message to the sysop - WG7J */
void
mail_error(char *fmt, ...)
{
    FILE *wrk,*txt;
    va_list ap;
    char *cp;
    long t,msgid;
    char fn[FILE_PATH_SIZE];
  
    if(!SendError)
        return;
  
    /* Get current time */
    time(&t);
  
    /* get the message id for this message */
    msgid = get_msgid();
  
    /* Create the smtp work file */
    sprintf(fn,"%s/%ld.wrk",Mailqdir,msgid);
    if((wrk = fopen(fn,"w")) == NULL)
        return;
  
     /* Create the smtp text file */
    sprintf(fn,"%s/%ld.txt",Mailqdir,msgid);
    if((txt = fopen(fn,"w")) == NULL) {
        fclose(wrk);
        return;
    }
  
    /* Fill in the work file */
    fprintf(wrk,"%s\nMAILER-DAEMON@%s\nsysop@%s",Hostname,Hostname,Hostname);
    fclose(wrk);
  
    /* Fill in the text file headers */
    fprintf(txt,"%s%s",Hdrs[DATE],ptime(&t));
    fprintf(txt,"%s<%ld@%s>\n",Hdrs[MSGID],msgid,Hostname);
    fprintf(txt,"%sMAILER-DAEMON@%s\n",Hdrs[FROM],Hostname);
    fprintf(txt,"%ssysop@%s\n",Hdrs[TO],Hostname);
    fprintf(txt,"%sSystem message\n\n",Hdrs[SUBJECT]);
  
    /* Print the text body */
    cp = ctime(&t);
    fprintf(txt,"On %s",cp);
    va_start(ap,fmt);
    vfprintf(txt,fmt,ap);
    va_end(ap);
    fputc('\n',txt);
    fclose(txt);
  
    /* Now kick the smtp server */
    smtptick(NULL);
}
#endif /* MAILERROR */
  
int
dosource(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int linenum = 0;
    char *inbuf,*intmp;
    FILE *fp;
  
    /* Read command source file */
    if((fp = fopen(argv[1],READ_TEXT)) == NULLFILE){
        tprintf(NoRead,argv[1],sys_errlist[errno]);
        return 1;
    }
  
    inbuf = malloc(BUFSIZ);
    intmp = malloc(BUFSIZ);
    while(fgets(inbuf,BUFSIZ,fp) != NULLCHAR){
        strcpy(intmp,inbuf);
        linenum++;
        if(Verbose)
            tputs(intmp);
        if(cmdparse(Cmds,inbuf,NULL) != 0){
            tprintf("*** file \"%s\", line %d: %s\n",
            argv[1],linenum,intmp);
        }
    }
    fclose(fp);
    free(inbuf);
    free(intmp);
    return 0;
}
  
#ifdef TTYLINKSERVER
/* if unattended mode is set - restrict ax25, telnet and maybe other sessions */
int
doattended(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setbool(&Attended,"Attended flag",argc,argv);
}
#endif
  
/* if ThirdParty is not set - restrict the mailbox (S)end command to local only */
int
dothirdparty(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setbool(&ThirdParty,"Third-Party mail flag",argc,argv);
}
  
#if defined(ALLCMD)
int
domdump(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    unsigned int i;
    char *addr,*cp;
    unsigned int segment,offset;
    unsigned int len = 8 * 16;      /* default is 8 lines of hex dump */
#ifdef UNIX
    extern int _start;   /* Approximates the lowest addr we can access */
#endif
  
    if(argc < 2 || argc > 3) {
        tputs("Usage:- dump <hex-address | .> [decimal-range] \n");
        return 0;
    }
    if(argv[1][0] == '.')
        addr = DumpAddr;                /* Use last end address */
    else {
        if((cp=strchr(argv[1],':')) != NULL) {
            /* Interpret segment:offset notation */
            *cp++ = '\0';
            segment = (unsigned int) htol(argv[1]);
            offset = (unsigned int) htol(cp);
            addr = MK_FP(segment,offset);
        } else
            addr = ltop(htol(argv[1]));     /* get address of item being dumped */
    }
  
    if(argc == 3) {
        len = atoi(argv[2]);
        len = ((len + 15) >> 4) << 4;   /* round up to modulo 16 */
    }
  
#ifdef UNIX
    if (addr < (char *)&_start  ||  addr+len > (char *)sbrk(0)) {
        tprintf("Address exceeds allowable range %lx..%lx\n",
                FP_SEG(&_start), FP_SEG(sbrk(0)));
        return 0;
    }
#endif

    if(len < 1 || len > 256) {
        tputs("Invalid dump range. Valid is 1 to 256\n");
        return 0;
    }
#ifdef UNIX
    tprintf("            Main Memory Dump Of Location %lx\n", FP_SEG(addr));
#else
    tprintf("            Main Memory Dump Of Location %Fp\n", addr);
#endif
    tputs("Addr (offset)           Hexadecimal                         Ascii\n");
    tputs("----                    -----------                         -----\n\n");
  
    for(i = 0; i < len; i += 16)
        fmtline(Curproc->output, (int16)i, (char *)(addr + i), (int16)16);
    DumpAddr = (char *)(addr + i);          /* update address */
    return 0;
}
#endif
  
int
dowrite(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int s;
    struct mbx *m;
  
    if((s = atoi(argv[1])) == 0) { /* must be a name */
#ifdef MAILBOX
        /* check the mailbox users */
        for(m=Mbox;m;m=m->next){
            if(!stricmp(m->name,argv[1]))
                break;
        }
        if(!m)
            return 0;
        s = m->user;
#else
        return 0;
#endif
    }
    usprintf(s,"*** Msg from SYSOP: %s\n",argv[2]);
    usflush(s);
  
    return 0;
}
  
#ifdef MAILBOX
/* write a message to all nodeshell users
 * argv[1] is the message.
 */
int
dowriteall(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct mbx *m;
  
    for(m=Mbox;m;m=m->next){
        if(m->sid & MBX_SID)
            continue;
        usprintf(m->user,"*** Msg from SYSOP: %s\n",argv[1]);
        usflush(m->user);
    }
    return 0;
}
#endif
  
  
int
dostatus(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    time_t nowtime,nosseconds;
    struct tm *tm;
  
#if defined(STATUSWIN) && defined(STATUSWINCMD)
    if (argc == 2 && Curproc->input == Command->input) {
        /* only keyboard user can issue statuswindow on|off */
        static int orig_statuslines = 0;
        int onoff;

        if (!setbool(&onoff, "", argc, argv)) {  /* on or off */
            if (!onoff && StatusLines) { /* valid off */
                orig_statuslines = StatusLines;
                StatusLines = 0;
            }
            else if (onoff && !StatusLines) {  /* valid on */
                StatusLines = orig_statuslines;
            }
            else return 0;  /* ignore invalid args */
        }
        else return 0;  /* ignore invalid args */

        window(1,1+StatusLines,Numcols,Numrows);  /* don't bother to restore split screen */
        clrscr();       /* Start with a fresh slate */
        Command->row = Numrows - 1 - StatusLines;  /* revise session info */
#ifdef SPLITSCREEN
        Command->split = 0;
        textattr(MainColors);
#endif
        return 0;
    }
#endif /* STATUSWIN && STATUSWINCMD */

    time(&nowtime);                       /* current time */
    nosseconds = secclock();                        /* Current NOS lifetime */
    /* May as well revise our stored UtcOffset in case we crossed a boundary */
    tm = localtime(&nowtime);
    UtcOffset = timezone - (tm->tm_isdst>0 ? 3600L : 0L);  /* UTC - localtime, in secs */
  
    tprintf(Nosversion,Version);
    tputs(Version2);
#if defined( WHOFOR ) && ! defined(ALLCMD)
    tprintf("for %s\n", WHOFOR);
#endif
    tprintf("Tty: %d rows, %d columns\n",Numrows,Numcols);
  
#ifdef  MSDOS
    tprintf(NosLoadInfo, _CS, _DS);
#endif
    tprintf("\nThe system time is %s", ctime(&nowtime));
    tprintf("NOS was started on %s\n", ctime(&StartTime));
    tprintf("Uptime => %s\n", tformat(secclock()));
    tprintf("Localtime is UTC%+ld minutes\n", -UtcOffset/60L);
#ifdef notdef
    nowtime -= StartTime;
    if(nosseconds < nowtime-1)
        tprintf("NOS has lost %lu seconds in missed clock ticks!\n", \
        nowtime - nosseconds);
#endif
    tputc('\n');
#ifdef TTYLINKSERVER
    tprintf("The station is currently %sttended.\n", Attended ? "A" : "Una");
#endif
#ifdef notdef  /* was ALLCMD */
    tputs("The 'Message Of The Day' is ");
    if(Motd != NULLCHAR)
        tprintf("\n%s",Motd);
    else
        tputs("not set!\n");
#endif
#ifdef  __TURBOC__
    dofstat();              /* print status of open files */
#endif
    return 0;
}
  
#ifdef ALLCMD
int
domotd(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    if(argc > 2) {
        tputs("Usage: motd \"<your message>\"\n");
        return 1;
    }
  
    if(argc < 2) {
        if(Motd != NULLCHAR)
            tputs(Motd);
    } else {
        free(Motd);                     /* OK if already NULL */
        Motd = NULLCHAR;                /* reset the pointer */
  
        if(!strlen(argv[1]))
            return 0;                       /* clearing the buffer */
  
        Motd = mallocw(strlen(argv[1])+5);      /* allow for the EOL char etc */
        strcpy(Motd, argv[1]);
        strcat(Motd, "\n");                     /* add the EOL char */
    }
    return 0;
}
#endif /*ALLCMD*/
  
#ifdef  __TURBOC__
/*
 * Fstat utility code.
 * Converted to go into NOS by Kelvin Hill - G1EMM  April 9, 1990
 */
  
extern unsigned char _osmajor;
  
static char    *localcopy(char far *);
static char    *progname(int16);
  
extern char *Taskers[];
extern int Mtasker;
  
/* n5knx: see simtelnet msdos/info/inter54?.zip by Ralf Brown for details */
int
dofstat()
{
    union REGS regs;
    struct SREGS segregs;
    char far *pfiletab, far *pnext, far *fp;
    char far *name, far *plist, far *entry;
    char file[13], ownername[9], ownerext[5];
    int nfiles, i, j, numhandles, entrylen;
    int16 access, devinfo, progpsp;
    long length, offset;
    int heading = 0;
    static char DFAR header[] =
           "\n"
           "                 Table of Open Files.\n"
           "                 --------------------\n"
           "Name           length   offset hnd acc PSP device type/owner\n"
           "----           ------   ------ --- --- --- -----------------\n";

  
    regs.h.ah = 0x52;       /* DOS list of lists */
    intdosx(&regs, &regs, &segregs);
  
    /* make a pointer to start of master list */
    plist = (char far *) MK_FP(segregs.es, regs.x.bx);
  
    /* pointer to start of file table */
    pfiletab = (char far *) MK_FP(*(int far *) (plist + 6), *(int far *) (plist + 4));
  
    switch (_osmajor) {
        case 2:
            entrylen = 40;  /* DOS 2.x */
            break;
        case 3:
            entrylen = 53;  /* DOS 3.1 + */
            break;
        case 4:
        case 5:                 /* DOS 5.x - like dos 4.x */
        case 6:                 /* and NOW DOS 6.x also */
     /* case 7:            /* DOS 7.x gives blank filenames .. why bother? */
            entrylen = 59;  /* DOS 4.x through 6.x */
            break;
        default:
            tprintf("File table not available under %s\n",Taskers[Mtasker]);
            return 1;
    }
  
    for (;;) {
        /* pointer to next file table */
        pnext = (char far *) MK_FP(*(int far *) (pfiletab + 2), *(int far *) (pfiletab + 0));
        nfiles = *(int far *) (pfiletab + 4);
#ifdef FDEBUG
        tprintf("\nFile table at %Fp entries for %d files\n", pfiletab, nfiles);
#endif
        for (i = 0; i < nfiles; i++) {
  
            /*
             * cycle through all files, quit when we reach an
             * unused entry
             */
            entry = pfiletab + 6 + (i * entrylen);
            if (_osmajor >= 3) {
                name = entry + 32;
                strncpy(file, localcopy(name), 11);
                file[11] = '\0';
                numhandles = *(int far *) (entry + 0);
                access = (int) *(char far *) (entry + 2);
                length = *(long far *) (entry + 17);
                offset = *(long far *) (entry + 21);
                devinfo = *(int far *) (entry + 5);
                progpsp = *(int far *) (entry + 49);
            } else {
                name = entry + 4;
                strncpy(file, localcopy(name), 11);
                file[11] = '\0';
                numhandles = (int) *(char far *) (entry + 0);
                access = (int) *(char far *) (entry + 1);
                length = *(long far *) (entry + 19);
                offset = *(long far *) (entry + 36);
                devinfo = (int) *(char far *) (entry + 27);
            }
            if ((strlen(file) > 0) && (numhandles > 0) && !(devinfo & 0x80)) {
                if(!heading) {
                    tputs(header);
                    heading++;              /* header now printed */
                }
                tprintf("%8.8s.%3.3s%9ld%9ld %3d ",
                        file, &file[8], length, offset, numhandles);
                switch (access) {
                    case 0:
                        tputs("r  ");
                        break;
                    case 1:
                        tputs("w  ");
                        break;
                    case 2:
                        tputs("rw ");
                        break;
                    default:
                        tputs("   ");
                }
                if (_osmajor >= 3)
                    tprintf("%04X ", progpsp);
                else
                    tputs("---- ");
                tprintf("drive %c: ", 'A' + (devinfo & 0x1F));
                if (devinfo & 0x8000)
                    tputs("(network) ");
                if (_osmajor >= 3) {
                    /*
                     * only DOS 3+ can find out
                     * the name of the program
                     */
                    fnsplit(progname(progpsp), NULL, NULL, ownername, ownerext);
                    tprintf("   [%s%s]\n", strlwr(ownername), strlwr(ownerext));
                } else {
                    tputc('\n');
                }
            }
            if (!strlen(file))
                return 0;
        }
        pfiletab = pnext;
    }
}
  
/* Make a copy of a string pointed to by a far pointer */
static char *
localcopy(s)
char far *s;
{
    static char localstring[80];
    char far *p = s;
    char *l = localstring;
    int i = 0;
  
    while (*p && i++ < 79) {
        *l++ = *p++;
    }
  
    *l = '\0';
  
    return (localstring);
}
  
/*
 * Return a near pointer to a character string with the full path name of the
 * program whose PSP is given in the argument.  If the argument is invalid,
 * this may return gibberish since [psp]:0026, the segment address of the
 * environment of the program, could be bogus.  However, a good PSP begins
 * with 0xCD 0x20, and we'll check for that (N5KNX). Beyond the last
 * environment string is a null marker, a word count (usually 1), then the
 * full pathname of the owner of the environment.  This only works for DOS 3+.
 */
static char *
progname(pid)
int16 pid;
{
    int16 far   *envsegptr; /* Pointer to seg address of environment */
    char far       *envptr; /* Pointer to pid's environment  */
    int16 far  *envsizeptr; /* Pointer to environment size */
    int16          envsize; /* Size of pid's environment */
    int16             ppid; /* Parent psp address */
  
    /* Verify that this is a valid PSP, ie, it begins with INT 20h (2 bytes) */
    if (*(int16 far *) MK_FP(pid, 0) != 0x20CD)
        return "unknown";

    /* find the parent process psp at offset 0x16 of the psp */
    ppid = *(int16 far *) MK_FP(pid, 0x16);
  
    /* find the environment at offset 0x2C of the psp */
    envsegptr = (int16 far *) MK_FP(pid, 0x2C);
    envptr = (char far *) MK_FP(*envsegptr, 0);
  
    /*
     * Make a pointer that contains the size of the environment block.
     * Must point back one paragraph (to the environments MCB plus three
     * bytes forward (to the MCB block size field).
     */
    envsizeptr = (int16 far *) MK_FP(*envsegptr - 1, 0x3);
    envsize = *envsizeptr * 16;     /* x 16 turns it into bytes */
  
    while (envsize) {
        /* search for end of environment block, or NULL */
        while (--envsize && *envptr++);
  
        /*
         * Now check for another NULL immediately following the first
         * one located and a word count of 0001 following that.
         */
        if (!*envptr && *(int16 far *) (envptr + 1) == 0x1) {
            envptr += 3;
            break;
        }
    }
  
    if (envsize) {
        /* Owner name found - return it */
        return (localcopy(envptr));
    } else {
        if (pid == ppid) {
            /*
             * command.com doesn't leave it's name around, but if
             * pid = ppid then we know we have a shell
             */
            return ("-shell-");
        } else {
            return ("unknown");
        }
    }
}
#endif /*__TURBOC__*/
  
int
doprompt(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setbool(&DosPrompt,"prompt",argc,argv);
}
  
/* Command history, see also pc.c - WG7J */
void logcmd(char *cmd) {
    struct hist *new;
    char *cp;
  
    if(!Maxhistory)     /* don't keep history */
        return;
  
    /* Get rid of \n; this is also done in cmdparse().
     * We HAVE to do this here, since the string is NOT null-terminated when
     * it comes from recv_mbuf()  !!!! rip() makes it nullterminated.
     */
    rip(cmd);
    cp = cmd;
    while(*cp == ' ' || *cp == '\t')
        cp++;
    if(!*cp)     /* Empty command */
        return;
  
    if(Histrysize < Maxhistory) {     /* Add new one */
        Histrysize++;
        if(!Histry) {           /* Empty list */
        /* Initialize circular linked list */
            Histry = mallocw(sizeof(struct hist));
            Histry->next = Histry->prev = Histry;
        } else {
            new = mallocw(sizeof(struct hist));
        /* Now link it in */
            Histry->next->prev = new;
            new->next = Histry->next;
            new->prev = Histry;
            Histry->next = new;
            Histry = new;
        }
    } else {
    /* Maximum number stored already, use the oldest entry */
        Histry = Histry->next;
        free(Histry->cmd);
    }
    Histry->cmd = strdup(cp);
}
  
int
dohistory(int argc,char *argv[],void *p) {
    struct hist *h;
    int num;
  
    if(argc > 1) {
        Maxhistory = atoi(argv[1]);
        return 0;
    }
    tprintf("Max recall %d\n",Maxhistory);
    if((h = Histry) == NULL)
        return 0;
    num = 0;
    do {
        tprintf("%.2d: %s\n",num++,h->cmd);
        h = h->prev;
    } while(h != Histry);
    return 0;
}
  
#ifdef __BORLANDC__
  
/* This adds some additional checks to the fopen()
 * in the Borland C++ Run Time Library.
 * It fixes problem with users trying to open system devices like
 * CON, AUX etc and hang a system.
 * WG7J, 930205
 * reworked by N5KNX 8/94 to allow output to printers iff PRINTEROK is defined,
 * AND the printer can accept output.  It's still risky; JNOS can lockup if
 * the printer won't accept output.
 */
#undef fopen
FILE _FAR *_Cdecl fopen(const char _FAR *__path, const char _FAR *__mode);

static char *InvalidName[] = {
    "NUL",
    "CON","CON:",
    "AUX","AUX:",
    "PRN","PRN:",
    "LPT1","LPT1:",
    "LPT2","LPT2:",
    "LPT3","LPT3:",
    "COM1","COM1:",
    "COM2","COM2:",
    "COM3","COM3:",
    "COM4","COM4:",
    "MOUSE$",
    "CLOCK$",
    NULLCHAR
};
  
FILE *newfopen (const char *filename, const char *type) {
    char entname[9];
    char *cp, *cp1;
    int i,j;
#include <bios.h>
  
    cp = strrchr(filename, '\\');
    cp1 = strrchr(filename, '/');
    if (cp < cp1) cp = cp1;
    if (cp == NULLCHAR) cp = (char *)filename;
    else cp++;
    if ((cp1 = strchr(cp, '.')) == NULLCHAR) j = strlen(cp);
    else j = (int)(cp1 - cp);
    if (j==0) return NULL;   /* path is to a dir, or entryname is .XXX, clearly bad */
    if (j>8) j=8;
    strncpy(entname, cp, j); entname[j] = '\0';

    for(i=0;InvalidName[i] != NULLCHAR;i++)
	if(stricmp(InvalidName[i],entname) == 0) {
	    if (strpbrk(type, "wWaA") == NULLCHAR) return NULL;  /* must be writing */
#if defined(PRINTEROK)
	    j=-1;
	    if (strnicmp(cp, "prn", 3)==0 ) j=0;
	    else if (strnicmp(cp, "lpt", 3)==0) j=(*(cp+3) - '1');
	    if (j>=0 && j<3 && ((i=biosprint(2,0,j)&0xb9) == 0x90)) break;  /* must be selected, no errors, not busy */
#endif
	    return NULL;
	}

    return fopen (filename, type);
}
#endif /* __BORLANDC__ */
  
#ifdef REPEATSESSION
  
#if defined(__TURBOC__) && !defined(__BORLANDC__)
/* N5KNX: TurboC 2.0 lacks _setcursortype(), which we supply here for dorepeat */
void _setcursortype(int style)
{
/* From TurboC++ conio.h: */
#define _NOCURSOR      0
#define _SOLIDCURSOR   1
#define _NORMALCURSOR  2
  
/* Int 0x10 reg cx codes: */
#define   STD_CURSOR      0x0607
#define   BLK_CURSOR      0x0006
#define   NO_CURSOR       0x2607
  
    union REGS regs;
  
    if (style == _NOCURSOR) regs.x.cx = NO_CURSOR;
    else if (style == _SOLIDCURSOR) regs.x.cx = BLK_CURSOR;
    else if (style == _NORMALCURSOR) regs.x.cx = STD_CURSOR;
    else return;
    regs.x.ax = 0x0100; /* set cursor */
    int86(0x10, &regs, &regs);
}
#endif
  
/* Repeat a command - taken from 930104 KA9Q NOS
   WA3DSP 1/93
*/
int
dorepeat(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int32 interval;
    int ret;
    struct session *sp;
  
    if(isdigit(argv[1][0])){
        interval = atol(argv[1]);
        argc--;
        argv++;
    } else {
        interval = MSPTICK;
    }
    if((sp = newsession(argv[1],REPEAT,1)) == NULLSESSION){
        tputs(TooManySessions);
        return 1;
    }
    _setcursortype(_NOCURSOR);
    while(sp==Current){
        /*  clrscr(); */
        /* gotoxy seems to work better - turn cursor off?? */
        gotoxy(1,1);
        ret = subcmd(Cmds,argc,argv,p);
        if(ret != 0 || pause(interval) == -1)
            break;
    }
    _setcursortype(_NORMALCURSOR);
    freesession(sp);
    return 0;
}
#endif /* REPEATSESSION */
  
  
/* Index given mailbox files */
int doindex(int argc,char *argv[],void *p) {
    int i;
  
    for(i=1;i<argc;i++) {
        if(*argv[i] == '*') {
            UpdateIndex(NULL,1);
            /* No reason to do others */
            return 0;
        } else {
            dirformat(argv[i]);
            /* Attempt to lock the mail file! */
            if(mlock(Mailspool,argv[i])) {
                tprintf("Can not lock '%s/%s.txt'!\n",Mailspool,argv[i]);
                continue;
            }
            if(IndexFile(argv[i],0) != 0)
                tprintf("Error writing index file for %s\n",argv[i]);
            /* Remove the lock */
            rmlock(Mailspool,argv[i]);
        }
    }
  
    return 0;
}
  
/* interpret a string "f+b", where f and b are numbers indicating
 * foreground and background colors to make up the text attribute
 */
void GiveColor(char *s,char *attr) {
    char *cp;
  
    if((cp=strchr(s,'+')) != NULL) {
        *cp++ = '\0';
        *attr = (char) atoi(s) & 0x0f;  /* Foreground color */
        *attr += ((char) atoi(cp) & 0x07) << 4;  /* Background, no blinking ! */
    }
}
