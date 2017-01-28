/* memory allocation routines
 * Copyright 1991 Phil Karn, KA9Q
 *
 * Adapted from alloc routine in K&R; memory statistics and interrupt
 * protection added for use with net package. Must be used in place of
 * standard Turbo-C library routines because the latter check for stack/heap
 * collisions. This causes erroneous failures because process stacks are
 * allocated off the heap.
 *
 * Mods by G1EMM , PA0GRI
 */
  
#include <dos.h>
#include <alloc.h>
#include "global.h"
#include "proc.h"
#include "cmdparse.h"
#include "mbuf.h"
#include "socket.h"
#include "daemon.h"
#include "pc.h"
#ifdef EMS
#include "memlib.h"
#endif
#ifdef XMS
#include "xms.h"
#include <bios.h>
#endif
#if defined(MEMLOG) && defined(__TURBOC__)
#include <time.h>
#endif
  
extern unsigned long _brklvl;
  
static unsigned long ReallocSys;    /* Count of blocks reallocated to core mem */
static unsigned long Memfail;   /* Count of allocation failures */
static unsigned long Allocs;    /* Total allocations */
static unsigned long Frees;     /* Total frees */
static unsigned long Invalid;   /* Total calls to free with garbage arg */
static unsigned long Yellows;   /* Yellow alert garbage collections */
static unsigned long Reds;      /* Red alert garbage collections */
static unsigned long Overuse;   /* Total calls to free with overused arg */
static unsigned long Intalloc;  /* Calls to malloc with ints disabled */
static unsigned long Intfree;   /* Calls to free with ints disabled */
static unsigned int MinAlloc;   /* Set minumum number of bytes to alloc */
static int Memwait;             /* Number of tasks waiting for memory */
unsigned long Availmem;         /* Heap memory, ABLKSIZE units */
static unsigned long Morecores;
#ifdef notdef
static int Efficient = 1;       /* 0 = normal/fast, 1 = effecient/slow */
#endif
static int Memdebug = 0;        /* 0 = normal, 1 = call logstat() */
#ifdef MULTITASK
unsigned Minheap = 16 * 1024;   /* Min free heap when shelling out */
extern int Nokeys;              /* G8FSL 940426 */
#endif
static char freewarn[] ="free: WARNING! %s (%Fp) pc = %04x:%04x proc %s%c";
static char freewarn1[] = "invalid pointer";
static char HeapSizeStr[] = "heap size %lu, avail %lu (%lu%%), morecores %lu, coreleft %lu";
static char AllocStr[] = "allocs %lu, frees %lu (diff %lu), reallocs %lu, alloc fails %lu\ninvalid frees %lu, overused %lu";
static char Threshold[] = "threshold %lu,"
#ifdef MULTITASK
                          " minheap %u,"
#endif
                          " minalloc %u\n";
static char GarbageStr[] = "garbage collections yellow %lu, red %lu";
#ifdef notdef
static char EfficientStr[] = "efficient %u, threshold %lu";
#endif
static char EfficientStr[] = "threshold %lu";
static char InterruptStr[] = "interrupts-off calls to malloc %lu, free %lu\n";
#ifdef Kelvdebug
static char freewarn2[] = "overused buffer";
#endif
  
static unsigned long Sizes[16];
#ifdef EMS
extern int EMS_Available;
#endif
#ifdef XMS
static int minUMBsegaddr = 0;
#endif
static int logstat __ARGS((void));
  
int dostat __ARGS((int argc,char *argv[],void *p));
static int dofreelist __ARGS((int argc,char *argv[],void *p));
static int doibufsize __ARGS((int argc,char *argv[],void *p));
static int donibufs __ARGS((int argc,char *argv[],void *p));
static int dothresh __ARGS((int argc,char *argv[],void *p));
static int dosizes __ARGS((int argc,char *argv[],void *p));
static int doefficient __ARGS((int argc,char *argv[],void *p));
static int domemdebug __ARGS((int argc,char *argv[],void *p));
static int dominheap __ARGS((int argc,char *argv[],void *p));
static int dominalloc __ARGS((int argc,char *argv[],void *p));
  
static struct cmds DFAR Memcmds[] = {
    "debug",    domemdebug, 0, 0, NULLCHAR,
#ifdef notdef
    "efficient",doefficient,0, 0, NULLCHAR,
#endif
    "freelist", dofreelist, 0, 0, NULLCHAR,
    "ibufsize", doibufsize, 0, 0, NULLCHAR,
    "minalloc", dominalloc, 0, 0, NULLCHAR,
#ifdef MULTITASK
    "minheap",  dominheap,  0, 0, NULLCHAR,
#endif
    "nibufs",   donibufs,   0, 0, NULLCHAR,
    "sizes",    dosizes,    0, 0, NULLCHAR,
    "status",   dostat,     0, 0, NULLCHAR,
    "thresh",   dothresh,   0, 0, NULLCHAR,
    NULLCHAR,
};
  
#ifdef  LARGEDATA
#define HUGE    huge
#else
#define HUGE
#endif
  
union header {
    struct {
        union header HUGE *ptr;
        unsigned long size;
    } s;
#ifdef Kelvdebug
    long l[2];
#endif
};
  
typedef union header HEADER;
#define NULLHDR (HEADER HUGE *)NULL
  
#define ABLKSIZE        (sizeof (HEADER))
  
static HEADER HUGE *morecore __ARGS((unsigned nu));
  
static HEADER Base;
static HEADER HUGE *Allocp = NULLHDR;
static unsigned long Heapsize;
  
static int freecore __ARGS((HEADER HUGE *p));
static void freeheap __ARGS((HEADER HUGE *p));
  
#ifdef Kelvdebug
#define MARKER          0x766c654bL /* Kelv in reverse */
#endif
  
#if defined(MEMLOG) && defined(__TURBOC__)
int memlogger;     /* fileno to log to when nonzero */
static union REGS regs;
static struct SREGS sregs;
struct memrecstruct {
    char alloc_or_free;
    char pad;
#ifdef  LARGECODE
    int16 caller_segment;
#endif
    int16 caller_offset;
#ifdef  LARGEDATA
    int16 ptr_segment;
#endif
    int16 ptr_offset;
/*    time_t   time;
 *    unsigned long filled_size;
 *    unsigned int request_size;
*/
};
static struct memrecstruct MemRec;
static unsigned short far *context;
static unsigned short Codeseg;
static void (*pc)();
#endif

/* Allocate block of 'nb' bytes */
void *
malloc(nb)
unsigned nb;
{
    register HEADER HUGE *p, HUGE *q;
    register unsigned nu;
    int i;
  
    if(!istate())
        Intalloc++;
    if(nb == 0)
        return NULL;
  
    /* Record the size of this request */
    if((i = log2(nb)) >= 0)
        Sizes[i]++;
  
    /* See if a minimum size has been set */
    if(MinAlloc && MinAlloc > nb)
        nb = MinAlloc;
  
#ifndef Kelvdebug
    /* Round up to full block, then add one for header */
  
    nu = ((nb + ABLKSIZE - 1) / ABLKSIZE) + 2;      /* force allocated memory  */
    nu &= 0xfffeU;                                  /* to be on offset 0x0008 */
#else
    /* Round up to full block, then add one for header and one for debug */
  
    nu = ((nb + ABLKSIZE - 1) / ABLKSIZE) + 4;      /* force allocated memory  */
    nu &= 0xfffeU;                                  /* to be on offset 0x0008 */
#endif
  
    if ((q = Allocp) == NULLHDR){
        Base.s.ptr = Allocp = q = &Base;
        Base.s.size = 1;
    }
  
#ifdef notdef
    if(Efficient) {
        Allocp = q = &Base;     /* Start at the very beginning again */
    }
#endif
  
    for (p = q->s.ptr; ; q = p, p = p->s.ptr){
        if (p->s.size >= nu){
            /* This chunk is at least as large as we need */
            if (p->s.size <= nu + 1){
                /* This is either a perfect fit (size == nu)
                 * or the free chunk is just one unit larger.
                 * In either case, alloc the whole thing,
                 * because there's no point in keeping a free
                 * block only large enough to hold the header.
                 */
                q->s.ptr = p->s.ptr;
            } else {
                /* Carve out piece from end of entry */
                p->s.size -= nu;
                p += p->s.size;
                p->s.size = nu;
            }
            p->s.ptr = p;   /* for auditing */
#ifdef Kelvdebug
            p->l[(p->s.size * 2) - 2] = (long)p;    /* debug */
            p->l[(p->s.size * 2) - 1] = MARKER;     /* debug */
#endif
            Allocs++;
            Availmem -= p->s.size;
            p++;

#if defined(MEMLOG) && defined(__TURBOC__)
            if (memlogger) {
            Codeseg = _psp + 0x10;          /* seg of start of our exe's code */
            context = MK_FP(_SS,_BP);       /* ptr to our stack frame */
                                            /* which contains int16 caller_stkbase (BP)
                                                      int16 offset(retaddr)
                                                      int16 seg(retaddr)
                                               in the large model, anyhow */

#ifdef  LARGECODE
            do {  /* find caller outside this seg (in case mallocw, etc, called us) */
                pc = MK_FP(context[2],context[1]);
                context = MK_FP(_SS, *context);   /* previous stack frame */
            } while ((FP_SEG(pc) == FP_SEG(malloc) || /* search prev until new seg */
                      FP_SEG(pc) == FP_SEG(strdup))   /* but other than that of strdup in misc.c */
                     && FP_OFF(context) );  /* no more when at SP:0 */
            MemRec.caller_segment = FP_SEG(pc) - Codeseg;
#else
            pc = MK_FP(_CS,context[1]);   /* how to figure when outside this modeule? */
#endif
            MemRec.caller_offset = FP_OFF(pc);
#ifdef  LARGEDATA
            MemRec.ptr_segment = FP_SEG(p);
#endif
            MemRec.ptr_offset = FP_OFF(p);
/*            MemRec.filled_size = p->s.size;
 *            MemRec.request_size = nb;
 *            MemRec.time = time(NULL);
*/
            MemRec.alloc_or_free = 'A';

            regs.h.ah = 0x40;	/* write CX bytes at DS:DX to handle BX */
            regs.x.bx = memlogger;
            regs.x.cx = sizeof(MemRec);
            regs.x.dx = FP_OFF(&MemRec);
            sregs.ds  = FP_SEG(&MemRec);
            intdosx(&regs, &regs, &sregs);
            }
#endif /* MEMLOG */
#ifdef  LARGEDATA
            /* On the brain-damaged Intel CPUs in
             * "large data" model, make sure the offset field
             * in the pointer we return isn't null.
             * The Turbo C compiler and certain
             * library functions like strrchr() assume this.
             */
            if(FP_OFF(p) == 0){
                /* Return denormalized but equivalent pointer */
                return (void *)MK_FP(FP_SEG(p)-1,16);
            }
#endif
            return (void *)p;
        }
        if (p == Allocp && ((p = morecore(nu)) == NULLHDR)){
            Memfail++;
            return NULL;
        }
    }
}
/* Get more memory from the system and put it on the heap */
static HEADER HUGE *
morecore(nu)
unsigned nu;
{
    char HUGE *cp;
    HEADER HUGE *up;
  
    Morecores++;

#ifdef MULTITASK
    if (Nokeys == 2)
        return NULLHDR;	/* We are shelled out (not just preparing to)
                         * and so must not call sbrk().  -- G8FSL
                         */
#endif

    if ((long)nu * ABLKSIZE > 32767L) return NULLHDR;  /* exceeds sbrk() max */
    if ((int)(cp = (char HUGE *)sbrk(nu * ABLKSIZE)) == -1){
        if(Memdebug==1) {
            log(-1,"morecore: Failure requesting %lu (coreleft %lu)",
            ((unsigned long)nu * ABLKSIZE),coreleft());
            logstat();
            Memdebug=2;     /* no more morecore logging until successful again */
        }
        return NULLHDR;
    }
    if (Memdebug > 1) Memdebug=1;   /* permit logging again */
  
    up = (HEADER *)cp;
    up->s.size = nu;
    up->s.ptr = up; /* satisfy audit */
#ifdef Kelvdebug
    up->l[(up->s.size * 2) - 2] = (long)up;         /* satisfy debug */
    up->l[(up->s.size * 2) - 1] = MARKER;           /* satisfy debug */
#endif
    /* free it to our internal heap for use */
    freeheap(up);
    Heapsize += nu*ABLKSIZE;
    return Allocp;
}
  
/* Put memory block back on heap or back onto system core */
void
free(blk)
void *blk;
{
    HEADER HUGE *p;
    unsigned short HUGE *ptr;
  
    if(!istate())
        Intfree++;
    if(blk == NULL)
        return;         /* Required by ANSI */
#ifdef XMS
    if (minUMBsegaddr == 0) minUMBsegaddr = biosmemory() << 6; /* RAM_kB * 1024 / 16 */
    if (FP_SEG(blk) >= minUMBsegaddr && /* likely is a UMB seg addr */
        XMS_Available &&                /* and XMS drivers exist */
        Release_UMB(FP_SEG(blk)) == 0L) return;  /* UMB seg freed? */
#endif
    p = (HEADER HUGE *)blk - 1;
    /* Audit check */
    if(p->s.ptr != p){
        ptr = (unsigned short *)&blk;
        printf(freewarn,freewarn1,blk,ptr[-1],ptr[-2],Curproc->name,'\n');
        fflush(stdout);
#ifdef STKTRACE
        stktrace();
#endif
        Invalid++;
        log(-1,freewarn,freewarn1,blk,ptr[-1],ptr[-2],Curproc->name,' ');
        logstat();
        return;
    }
#ifdef Kelvdebug
    if(p->l[(p->s.size * 2) - 2] != (long)p || p->l[(p->s.size * 2) - 1] != MARKER){
        ptr = (unsigned short *)&blk;
        printf(freewarn,freewarn2,blk,ptr[-1],ptr[-2],Curproc->name,'\n');
        fflush(stdout);
        Overuse++;
        log(-1,freewarn,freewarn2,blk,ptr[-1],ptr[-2],Curproc->name,' ');
        logstat();
        return;
    }
#endif
  
    Frees++;
  
#if defined(MEMLOG) && defined(__TURBOC__)
    if (memlogger) {
        MemRec.alloc_or_free = 'F';
#ifdef  LARGEDATA
        MemRec.ptr_segment = FP_SEG(blk);
#endif
        MemRec.ptr_offset = FP_OFF(blk);
/*        MemRec.filled_size = p->s.size;
 *        MemRec.time = time(NULL);
 */
        regs.h.ah = 0x40;
        regs.x.bx = memlogger;
        regs.x.cx = sizeof(MemRec);
        regs.x.dx = FP_OFF(&MemRec);
        sregs.ds  = FP_SEG(&MemRec);
        intdosx(&regs, &regs, &sregs);
    }
#endif

    /* if we can add this to the core, we're done
     * else add to our internal heap
     */

    /* We don't want to free back to the core if we are in the process
     * of shelling out - otherwise the "memory minheap" technique (in
     * PC.C) doesn't work   G8FSL 940426 */

#ifdef MULTITASK
       if (Nokeys != 0)
               freeheap(p);
       else
#endif
    if(!freecore(p))
        freeheap(p);
  
    if(Memwait != 0)
        psignal(&Memwait,0);
}
  
/* Put memory block back on internal heap */
void freeheap(HEADER HUGE *p) {
    HEADER HUGE *q;
  
    Availmem += p->s.size;
    /* Search the free list looking for the right place to insert */
    for(q = Allocp; !(p > q && p < q->s.ptr); q = q->s.ptr){
        /* Highest address on circular list? */
        if(q >= q->s.ptr && (p > q || p < q->s.ptr))
            break;
    }
    if(p + p->s.size == q->s.ptr){
        /* Combine with front of this entry */
        p->s.size += q->s.ptr->s.size;
        p->s.ptr = q->s.ptr->s.ptr;
    } else {
        /* Link to front of this entry */
        p->s.ptr = q->s.ptr;
    }
    if(q + q->s.size == p){
        /* Combine with end of this entry */
        q->s.size += p->s.size;
        q->s.ptr = p->s.ptr;
    } else {
        /* Link to end of this entry */
        q->s.ptr = p;
    }
}
  
/* Put memory block back on system core if it fits */
int freecore(HEADER HUGE *p) {
    unsigned long backto, backcnt;
    int backed;
  
    if ((char HUGE *)_brklvl <= (char HUGE *)(p + p->s.size)) {
        backcnt = backto = (p->s.size * ABLKSIZE);
        do  {
            backed = (backcnt < 32767) ? (int) backcnt : 32767;
            sbrk (-1 * backed);
            backcnt -= backed;
        } while (backcnt);
        Heapsize -= backto;
        ReallocSys++;
        return 1;
    }
    return 0;
}
  
#ifdef  notdef  /* Not presently used */
/* Move existing block to new area */
void *
realloc(area,size)
void *area;
unsigned size;
{
    unsigned osize;
    HEADER HUGE *hp;
    char HUGE *cp;
  
    hp = ((HEADER *)area) - 1;
    osize = (hp->s.size -1) * ABLKSIZE;
  
    free(area);     /* Hopefully you have your interrupts off , Phil. */
    if((cp = malloc(size)) != NULL && cp != area)
        memcpy((char *)cp,(char *)area,size>osize? osize : size);
    return cp;
}
#endif
  
/* Allocate block of cleared memory */
void *
calloc(nelem,size)
unsigned nelem; /* Number of elements */
unsigned size;  /* Size of each element */
{
    register unsigned i;
    register char *cp;
  
    i = nelem * size;
    if((cp = malloc(i)) != NULL)
        memset(cp,0,i);
    return cp;
}
  
/* Version of malloc() that waits if necessary for memory to become available */
void *
mallocw(nb)
unsigned nb;
{
    register void *p;
  
    while((p = malloc(nb)) == NULL && (nb)){
        Memwait++;
        pwait(&Memwait);
        Memwait--;
    }
    return p;
}
  
/* Version of calloc that waits if necessary for memory to become available */
void *
callocw(nelem,size)
unsigned nelem; /* Number of elements */
unsigned size;  /* Size of each element */
{
    register unsigned i;
    register char *cp;
  
    i = nelem * size;
    cp = mallocw(i);
    memset(cp,0,i);
    return cp;
}
  
/* Return available memory on our heap plus available system memory */
unsigned long
availmem()
{
    return Availmem * ABLKSIZE + coreleft();
}
  
unsigned long Localheap() {
    return Availmem * ABLKSIZE;
}
  
/* Log heap stats */
static int
logstat(void)
{
    if(Memdebug){
        log(-1,"Memory status:");
        log(-1,HeapSizeStr,Heapsize,Availmem*ABLKSIZE, \
        100L*Availmem*ABLKSIZE/Heapsize,Morecores,coreleft());
        log(-1,AllocStr,Allocs,Frees,Allocs-Frees,ReallocSys,Memfail,Invalid,Overuse);
        log(-1,GarbageStr,Yellows,Reds);
#ifdef notdef
        log(-1,EfficientStr,Efficient,Memthresh);
#endif
        log(-1,EfficientStr,Memthresh);
        log(-1,InterruptStr,Intalloc,Intfree);
        if (MinAlloc) log(-1,"MinAlloc %u", MinAlloc);
    }
  
    return 0;
}
  
/* Print heap stats */
int
dostat(argc,argv,envp)
int argc;
char *argv[];
void *envp;
{
#ifdef EMS
    unsigned int emssize;
#endif
#ifdef XMS
    long XMS_Ret;
#endif
  
    tprintf(HeapSizeStr,Heapsize,Availmem*ABLKSIZE, \
    100L*Availmem*ABLKSIZE/Heapsize,Morecores,coreleft());
    tputc('\n');
    tprintf(AllocStr,Allocs,Frees,Allocs-Frees,ReallocSys,Memfail,Invalid,Overuse);
    tputc('\n');
    tprintf(Threshold,Memthresh,
#ifdef MULTITASK
            Minheap,
#endif
            MinAlloc);
    tprintf(GarbageStr,Yellows,Reds);
    tputc('\n');
    tprintf(InterruptStr,Intalloc,Intfree);
    iqstat();
  
#ifdef EMS
    if(EMS_Available) {
        if(ememmax(&emssize) == 0)
            tprintf("EMS: %u bytes contiguous available.\n",emssize);
    }
#endif
  
#ifdef XMS
    if(XMS_Available) {
        if((XMS_Ret = Total_XMS()) > 0L) {
            tprintf("XMS: total %ld KB, largest block %ld KB, largest UMB %u paras\n", XMS_Ret, Query_XMS(), largestUMB());
        }
    }
#endif
  
    return 0;
}
  
/* Print heap free list */
static int
dofreelist(argc,argv,envp)
int argc;
char *argv[];
void *envp;
{
    HEADER HUGE *p;
    int i = 0;
  
    for(p = Base.s.ptr;p !=(HEADER HUGE *) &Base;p = p->s.ptr){
        tprintf("%4.4x %6lu",FP_SEG(p),p->s.size * ABLKSIZE);
        if(++i == 5){
            i = 0;
            if(tputc('\n') == EOF)
                return 0;
        } else
            tputs(" | ");
    }
    if(i != 0)
        tputc('\n');
    return 0;
}
  
static int
dosizes(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    int i;
  
    for(i=0;i<16;i += 4){
        tprintf("N>=%5u:%7ld| N>=%5u:%7ld| N>=%5u:%7ld| N>=%5u:%7ld\n",
        1<<i,Sizes[i], 2<<i,Sizes[i+1],
        4<<i,Sizes[i+2],8<<i,Sizes[i+3]);
    }
    return 0;
}
  
int
domem(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    if(argc == 1)
        return dostat(0,NULL,NULL);
  
    return subcmd(Memcmds,argc,argv,p);
}
  
static int
dothresh(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setlong(&Memthresh,"Free memory threshold (bytes)",argc,argv);
}
  
static int
donibufs(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    if(setint(&Nibufs,"Interrupt pool buffers",argc,argv) == 0){
        iqclear();
        return 0;
    }
    return 1;
}
  
static int
doibufsize(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setuns(&Ibufsize,"Interrupt buffer size",argc,argv);
}
  
static int
dominalloc(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setuns(&MinAlloc,"Minimum allocation size (bytes)",argc,argv);
}
  
void
gcollect(i,v1,v2)
int i;          /* Args not used */
void *v1;
void *v2;
{
    void (**fp)(int);
    int red;
  
    for(;;){
#ifdef STATUSWIN
        extern int StatusLines;
  
        if(StatusLines) {
            pause(500L);
            UpdateStatus();
            pause(500L);
            UpdateStatus();
        } else
#endif
            pause(1000L);
  
#ifdef CONVERS
        {
            extern void check_buffer_overload(void);
            check_buffer_overload();
        }
#endif
        /* If memory is low, collect some garbage. If memory is VERY
         * low, invoke the garbage collection routines in "red" mode.
         */
        if(availmem() < Memthresh){
#ifdef AGGRESSIVE_GCOLLECT
            if(availmem() < Memthresh/4) {
                where_outta_here(0,251);  /* from MFNOS */
            } else
#endif
            if(availmem() < Memthresh/2){
                red = 1;
                Reds++;
            } else {
                red = 0;
                Yellows++;
            }
            for(fp = Gcollect;*fp != NULL;fp++)
                (**fp)(red);
        }
        /* We don't want to free back to the core if we are in the process
         * of shelling out - otherwise the "memory minheap" technique (in
         * PC.C) doesn't work   G8FSL 940426 */

#ifdef MULTITASK
        if (Nokeys == 0)
#endif
        {
        /* See if the last block on our heap fits 'underneath' the system core.
         * if so, free it to the core ...
         */
            HEADER HUGE *p, HUGE *last;
            char HUGE *calc;
            unsigned long backto, backcnt;
            int backed;
  
            last = 0;
            for (p = Base.s.ptr; p->s.ptr != (HEADER HUGE *)&Base; p = p->s.ptr)
                last = p;
            if (!last)
                continue;
            calc = (char HUGE *)(p + p->s.size);
            if ((char HUGE *)_brklvl <= calc)     {
                backcnt = backto = (p->s.size * ABLKSIZE);
                do  {
                    backed = (backcnt < 32767) ? (int)backcnt : 32767;
                    sbrk (-1 * backed);
                    backcnt -= backed;
                } while (backcnt);
                Heapsize -= backto;
                Availmem -= p->s.size;
                last->s.ptr = &Base;
                ReallocSys++;
            }
        }
    }
}
  
#ifdef notdef
  
static int
doefficient(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setbool(&Efficient,"Efficient/slower mode",argc,argv);
}
#endif
  
static int
domemdebug(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setbool(&Memdebug,"\"Mem stat\" to log after failures",argc,argv);
}
  
#ifdef MULTITASK
static int
dominheap(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    return setuns(&Minheap,"Minimum free heap when shelled out",argc,argv);
}
#endif
