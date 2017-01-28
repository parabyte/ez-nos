/*
 *  Simple mail user interface for KA9Q IP/TCP package.
 *  A.D. Barksdale Garbee II, aka Bdale, N3EUA
 *  Copyright 1986 Bdale Garbee, All Rights Reserved.
 *  Permission granted for non-commercial copying and use, provided
 *  this notice is retained.
 *  Copyright 1987 1988 Dave Trulli NN2Z, All Rights Reserved.
 *  Permission granted for non-commercial copying and use, provided
 *  this notice is retained.
 *
 *  Ported to NOS at 900120 by Anders Klemets SM0RGV.
 *
 *  Userlogging, 'RM' and 'KM' implementation,
 *  more-prompts for all types
 *  920307 and later, by Johan. K. Reinalda, WG7J/PA3DIS
 *  Mail indexing by Johan. K. Reinalda, WG7J, June/July 1993
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#ifdef MSDOS
#include <conio.h>
#include <io.h>
#include <dir.h>
#include <dos.h>
#endif
#ifdef UNIX
#include "unix.h"
#endif
#include "global.h"
#include "ftpserv.h"
#include "smtp.h"
#include "proc.h"
#include "usock.h"
#include "socket.h"
#include "telnet.h"
#include "timer.h"
#include "session.h"
#include "files.h"
#include "mailbox.h"
#include "cmdparse.h"
#include "bm.h"
#include "mailutil.h"
#include "dirutil.h"
#include "index.h"
  
#if defined(UNIX) || defined(MICROSOFT)
#include    <sys/types.h>
#endif
/*
#if defined(UNIX) || defined(MICROSOFT) || defined(__TURBOC__)
#include    <sys/stat.h>
#endif
#ifdef AZTEC
#include <stat.h>
#endif
*/
#include <fcntl.h>
#include "bm.h"
#include "mailbox.h"
  
#ifdef MAILCMDS
  
char Badmsg[] = "Invalid Message number %d\n";
char Nomail[] = "No messages\n";
/* static char Noaccess[] = "Unable to access %s\n"; */
static char OpenError[] = "user %s: error %d opening %s\n";
#ifdef HOLD_LOCAL_MSGS
static char ReleaseHeldMsg[] = "Release Msg Hold? (y/n/k):";
#endif
  
static int readnotes __ARGS((struct mbx *m,int ifile));
static long isnewmail __ARGS((struct mbx *m));
static int initnotes __ARGS((struct mbx *m));
static int lockit __ARGS((struct mbx *m));
static int unlockit __ARGS((struct mbx *m));
static void resetlastread __ARGS((struct mbx *m));
static void no_area __ARGS((struct mbx *m));
  
static int
initnotes(m)
struct mbx *m;
{
    int idx;
    char *area;
    int ret;
    char buf[FILE_PATH_SIZE];
  
    /* Reset all mailbox status variables */
    m->change = m->anyread = m->nmsgs = m->current =  m->newmsgs = 0;
    m->mboxsize = 0L;
  
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.ind",Mailspool,area);
    free(area);
  
    if ((idx = open(buf,READBINARY)) == -1)
        /* No mail ! */
        return 0;
    ret = readnotes(m,idx);
    close(idx);
    if (ret != 0)
        return -1;
  
    return 0;
}
  
/* readnotes assumes that idx is pointing to the index file */
static int
readnotes(m,idx)
struct mbx *m;
int idx;
{
    struct let *oldmbox;
    int i;
    long pos;
    struct let *cmsg;
    struct indexhdr hdr;
    struct mailindex ind;
  
    /* Get the number of messages in the mailbox */
    lseek(idx,0L,SEEK_SET);
    if (read_header(idx,&hdr) == -1) return -1;
  
    if(m->mbox)
        oldmbox = m->mbox;
    else
        oldmbox = NULL;
  
    /* Get the new number of messages */
    m->mbox = (struct let *)callocw((unsigned)hdr.msgs+1,sizeof(struct let));
  
    if(oldmbox) {    /* Copy the status of the messages back */
        if (m->nmsgs <= hdr.msgs)      /* unless mbox shrank */
            memcpy(m->mbox,oldmbox,(m->nmsgs+1)*sizeof(struct let));
        else m->current=0;
        free(oldmbox);
    }
    else m->current=0;
  
    cmsg = m->mbox;
    pos = 0;
    m->nmsgs = 0;
    m->newmsgs = 0;
    memset(&ind,0,sizeof(ind));
    default_index(m->area,&ind);
  
    /* Check for unread(ie. new) messages */
    for(i=1;i<=hdr.msgs;i++) {
        pwait(NULL);
        cmsg++;
#ifdef FBBFWD
        cmsg->indexoffset = tell(idx);
#endif
        m->nmsgs++;
        if (read_index(idx,&ind) == -1) {    /* should never happen */
            m->nmsgs--;
            log(-1,"readnotes: index for %s is damaged (#%d)", m->area, i);
            break;
        }
#ifdef USERLOG
        cmsg->msgid = ind.msgid;
#endif
        cmsg->size = ind.size;
        cmsg->start = pos;
        pos += cmsg->size;
        cmsg->status |= ind.status;
#ifdef USERLOG
        if(m->areatype == AREA)
            if(cmsg->msgid <= m->lastread)
                cmsg->status |= BM_READ;
#endif
        if(!(cmsg->status & BM_READ)) {
            m->newmsgs++;
            if(m->current == 0)
                m->current = m->nmsgs; /* first new message */
        }
        default_index(m->area,&ind);
    }
     /* start at one if no new messages */
    if(m->current == 0)
        m->current++;
    m->mboxsize = pos;
#ifdef notdef
    if (pos < 0L || pos > 4000000L)
        log(-1,"readnotes: suspicious len %ld for %s", pos, m->area);
#endif
    if(m->areatype == PRIVATE)  /* our private mail area */
        m->mycnt = m->nmsgs;

    if (m->nmsgs == hdr.msgs) return 0;
    else return -1;
}
  
static char ListHeader[] = "St.  #  TO            FROM     DATE   SIZE SUBJECT\n";
  
/* list headers of a notesfile a message */
/* Rearranged display - WG7J */
int
dolistnotes(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct mbx *m;
    struct let *cmsg;
    char *cp, *datestr;
    int idx;
    char *area;
    int start, stop;
    int i,header,list,listed;
    int searchopt = argc-1;
    int c,usemore=0,lin;
    struct indexhdr hdr;
    struct mailindex ind;
    char buf[FILE_PATH_SIZE];
  
    m = (struct mbx *) p;
  
    /* If this user doesn't have read-permissions,
     * we're not going to let him list anything - WG7J
     */
    if(m->privs & NO_READCMD) {
        tputs(Noperm);
        return 0;
    }
  
    if(m->nmsgs == 0) {
        tputs(Nomail);
        return 0;
    }
  
    if((m->stype == '>' || m->stype == '<' || m->stype ==  'S') && argc == 1) {
        tputs("Search criterium needed!\n");
        return 0;
    }
  
    if((lin=m->morerows) != 0)
        usemore = 1;    /* Display More prompt */
  
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.ind",Mailspool,area);
    free(area);
    if((idx = open(buf,READBINARY)) == -1) {
        closenotes(m);
        tprintf("Can not read index file %s\n",buf);
        return 0;
    }
  
    tprintf("Mail area: %s\n"
    "%d message%s -  %d new\n\n",
    m->area,m->nmsgs,m->nmsgs == 1 ? " " : "s ", m->newmsgs);
  
    stop = m->nmsgs;
#ifdef USERLOG
    if(m->areatype != PRIVATE && m->stype != 'A' && m->stype != 'H')
        start = stop - m->newmsgs + 1;
    else
#endif
        if((argc == 1) & (m->stype == 'L'))
            start = stop;
        else
            start = 1;
    if(argc > 1) {  /* we'll use the LAST arg as any search criterium */
        if(m->stype == 'L')                     /* LL (List Last) command */
            start = stop - atoi(argv[1]) + 1;
        else
            start = atoi(argv[1]);
        if(argc > 2)
            stop = atoi(argv[2]);
  
        /* Check the boundaries (fixes cases where search crit. was taken as range arg) */
        if(stop > m->nmsgs || !stop)
            stop = m->nmsgs;
        if(start < 1)
            start = 1;
    }
    if(start > stop) {
        if((m->stype == ' ') || (m->stype == 'M'))
            tputs("None to list.\n");
        else
            tputs("Invalid range.\n");
        close(idx);
        return 0;
    }
  
    cmsg = m->mbox;
    header = 0;
    listed = 0;
  
    memset(&ind,0,sizeof(ind));
    default_index(m->area,&ind);
  
    /* Read the index file header */
    if (read_header(idx,&hdr) == 0)
     for(i=1;i<=hdr.msgs;i++) {
        pwait(NULL);
        if (read_index(idx,&ind) == -1) goto irerr;
        cmsg++;
        if(start <= i && i <= stop) {
            list = 1;  /* assume we want to list it */
            /* Now see if we should list this one */
            if(m->stype == ' ') {
                if(argc < 2 && (cmsg->status & BM_READ))
                    list = 0;
            } else if(m->stype == '>') {
                if(strstr(ind.to,argv[searchopt]) == NULL)
                    list  = 0;
            } else if( m->stype == '<' ||
            (m->stype == 'M' && m->areatype != PRIVATE)) {
                if(strstr(ind.from,argv[searchopt]) == NULL)
                    list  = 0;
            } else if(m->stype == 'S') {
                cp = strlwr(strdup(ind.subject));   /* case-independent scan */
                if(strstr(cp,argv[searchopt]) == NULL)
                    list  = 0;
                free(cp);
            } else if(m->stype == 'B' || m->stype == 'T') {
                if(m->stype != ind.type)
                    list  = 0;
            } else if(m->stype == 'H' && !(cmsg->status&BM_HOLD))
                list=0;

            if(list) {
#ifdef USERLOG
                /* Check the ID number of this message and
                 * adjust new lastread count, if needed - WG7J
                 */
                if(cmsg->msgid > m->newlastread)
                    m->newlastread = cmsg->msgid;
#endif
                if(!header) {
                    tputs(ListHeader);
                    lin--;
                    header = 1;
                }
                listed = 1;
  
                /* Format date */
                datestr = ctime(&ind.date);
                datestr[10] = '\0';
  
                /* Format from */
                if((cp = strpbrk(ind.from,"@%")) != NULL)
                    *cp = '\0';
  
                tputc((i == m->current) ? '>' : ' ');
                tputc(cmsg->status & BM_DELETE ? 'D' : ' ');
                tputc(cmsg->status & BM_HOLD ? 'H':(cmsg->status & BM_READ ? 'Y' : 'N'));
                tprintf(" %3d %-13.13s %-8.8s %-6.6s %4ld %-35.35s\n",
                i,ind.to,ind.from,&datestr[4],ind.size,ind.subject);
  
                lin--;
  
                /* More prompting added - WG7J */
                if(usemore && lin <= 0){
                    if(charmode_ok(m))
                        c = tkeywait(TelnetMorePrompt,0,
                            (int)(OPT_IS_DEFINED(m,TN_LINEMODE)&OPT_IS_SET(m,TN_LINEMODE)));
                    else  /* For AX.25 and NET/ROM connects - WG7J */
                        c = mykeywait(BbsMorePrompt,m);
                    if(c == -1 || c == 'q' || c == 'Q') {
                        default_index(m->area,&ind);  /* free what we allocated */
                        break;
                    }
                    if(c == '\n' || c == '\r')
                        lin = 1;
                    else
                        lin = m->morerows-1;
  
                    header = 0;
                }
            }
        }
        /* Done with this index, clear it */
        default_index(m->area,&ind);
    }
    else
irerr:
        tprintf("Error reading index of %s\n", m->area);

    close(idx);
    if(!listed)
        tputs(Nomail);
  
    return 0;
}
  
/*  save msg on stream - if noheader set don't output the header */
int
msgtofile(m)
struct mbx *m;
{
    FILE *fp;
    char *area;
    char buf[LINELEN];
    int prevtype = NOHEADER;
  
    if (m->nmsgs == 0) {
        tputs(Nomail);
        return -1;
    }
  
    /* Open the mailbox and go to start of message */
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.txt",Mailspool,area);
    if ((fp=fopen(buf,READ_TEXT)) == NULLFILE) {
        free(area);
        return -1;
    }
    free(area);
    fseek(fp,m->mbox[m->current].start,0);
  
    /* skip some of header (n5knx) */
    while(fgets(buf,sizeof(buf),fp) != NULLCHAR) {
        if (*buf == '\n')
            break;
        switch(htype(buf, &prevtype)) {
        case TO:
        case CC:
        case FROM:
        case DATE:
        case SUBJECT:
        case REPLYTO:
        case APPARTO:
            fputs(buf,m->tfile);
            break;
        }
    }
  
    /* Copy the body */
    do {
        if(!strncmp(buf,"From ",5))
            break;
        fputs(buf,m->tfile);
    } while (fgets(buf,sizeof(buf),fp) != NULLCHAR);
    prevtype = ferror(m->tfile);
    fclose(fp);
    if (prevtype) {
        tputs("Error writing mail file\n");
        return -1;
    }
    return 0;
}
  
/* dodelmsg - delete message in current notesfile */
/* Modified to allow the 'KM' command. 920307 -  WG7J */
/* K[U] msg1 msg2 ...  -or-  K[U] msg#from-msg#to ...  */
/* KA  -or-  KM */
int
dodelmsg(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct mbx *m;
    int msg,i;
    int maxmsg;
    struct let *cmsg;
    char *tmpbuf;
  
    m = (struct mbx *) p;
  
    if (m->nmsgs == 0) {
        tputs(Nomail);
    }
    /* If this user doesn't have read-permissions,
     * we're not going to let him kill anything;
     * allow anyone to kill messages in areas
     * who's names start with 'nts' - WG7J
     */
    /* Check if we have permission to delete others mail */
    else if( (m->privs & (NO_SENDCMD|NO_READCMD)) ||
        ( !(m->privs & FTP_WRITE) && !(m->areatype == PRIVATE) &&
          strnicmp(m->area,"nts",3)) ){
            tputs(Noperm);
    }

    /* Handle KA, KM */
    else if(m->stype == 'A' || m->stype == 'M') {
        msg=0;
        for(i=1,cmsg=&m->mbox[1];i<=m->nmsgs;i++,cmsg++) {
            if(m->stype == 'A' ||
               (m->stype == 'M' && (cmsg->status & BM_READ))  /*found a read msg!*/
              ) cmsg->status |= BM_DELETE, msg++;
        }
        if(msg) m->change = 1;
        tprintf("%d message%s killed!\n",msg,(msg==1) ? "" : "s");
    }
    else if(argc == 1) {  /* handle K and KU (without args) */
        msg = m->current;
        if(m->stype == 'U')
            m->mbox[msg].status &= ~BM_DELETE;
        else {
            m->mbox[msg].status |= BM_DELETE;
            m->change = 1;
        }
        tprintf("Msg %d %sKilled.\n", msg,(m->stype=='U') ? "Un-" : "");
    }
    else for(i = 1; i < argc; ++i) {
        tmpbuf = strchr(argv[i],'-'); /* N5KNX: allow from-to msg specification */
        msg = atoi(argv[i]);
        if (tmpbuf == NULLCHAR)
            maxmsg = msg;
        else
            maxmsg = atoi(++tmpbuf);
        if (maxmsg < msg) {
            tprintf(Badmsg,maxmsg);
            continue;
        }
        for (; msg <= maxmsg; msg++) {
            if(msg < 1 || msg > m->nmsgs) {
                tprintf(Badmsg,msg);
                continue;
            }
            if(m->stype == 'U')
                m->mbox[msg].status &= ~BM_DELETE;
            else {
                m->mbox[msg].status |= BM_DELETE;
                m->change = 1;
            }
            tprintf("Msg %d %sKilled.\n", msg,(m->stype=='U') ? "Un-" : "");
        }
    }
    return 0;
}
  
static void
no_area(struct mbx *m) {
  
    m->area[0] = '\0';
    m->nmsgs = m->current = m->newmsgs = 0;
    m->mboxsize = 0;
    free(m->mbox);
    m->mbox = NULL;
}
  
/* close the temp file while copying mail back to the mailbox */
int
closenotes(m)
struct mbx *m;
{
    struct let *cmsg;
    char *area;
    int i,msgs,keep,error;
    long start,pos,size;
    int idx,idxnew;
    FILE *txt,*new;
    char buf[LINELEN];
    char old[LINELEN];
    struct fwdbbs *newbbs;
    struct indexhdr hdr;
    struct mailindex ind;
  
    if(m->nmsgs == 0)
        return 0;
  
    if(lockit(m)) {
#ifdef USERLOG
        resetlastread(m);
#endif
        return -1;
    }

    if(!m->change || (isnewmail(m) < 0L)) {
        /* no changes were made,
         * or some messages were deleted !
         * In the latter we cannot continue !
         */
        if (m->change) {
#ifdef DEBUG_HDR
            log(m->user,"NOUPDATE for %s: msgs deleted in %s",m->name,m->area);
#endif
        }
#ifdef USERLOG
        resetlastread(m);
#endif
	unlockit(m);
        no_area(m);
        return 0;
    }
    scanmail(m);
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.txt",Mailspool,area);
    if((txt = fopen(buf,READ_BINARY)) == NULLFILE) {
        error=errno;
        goto txt_err;
    }
    sprintf(buf,"%s/%s.ind",Mailspool,area);
    if((idx = open(buf,READBINARY)) == -1) {
        error=errno;
        goto idx_err;
    }
    sprintf(buf,"%s/%s.new",Mailspool,area);
    if((new = fopen(buf,WRITE_TEXT)) == NULLFILE) {
        error=errno;
        goto new_err;
    }
    setvbuf(new, NULLCHAR, _IOFBF, 2048);       /* N5KNX: use bigger output buffer if possible */

    sprintf(buf,"%s/%s.idn",Mailspool,area);
    if((idxnew = open(buf,CREATEBINARY,CREATEMODE)) == -1) {
        error=errno;
sync_err:
        fclose(new);
new_err:
        close(idx);
idx_err:
        fclose(txt);
txt_err:
        free(area);
        if (error)
            log(m->user,OpenError,m->name,error,buf);
#ifdef USERLOG
        resetlastread(m);
#endif
        unlockit(m);
        no_area(m);
        return -1;
    }
  
    msgs = error = 0;

    memset(&ind,0,sizeof(ind));
    default_index(m->area,&ind);
    if (read_header(idx,&hdr) == -1) {
        error++;
#ifdef DEBUG_HDR
        log(-1,"read_header err %d for %s", errno, m->area);
#endif
        goto cleanup;
    }
  
    /* write the header first to new index file */
    default_header(&hdr);
    if (write_header(idxnew,&hdr) == -1) {
        error++;
#ifdef DEBUG_HDR
        log(-1,"write_header err %d for %s", errno, m->area);
#endif
        goto cleanup;
    }
  
  
    /* Copy messages and delete msgs, add status or fwd headers */
    for(cmsg=&m->mbox[1],i = 1; i <= m->nmsgs; i++, cmsg++) {
        default_index("",&ind); /* Free memory from previous index */
        /* Read the index for this message */
        if(read_index(idx,&ind) == -1) {
            sprintf(buf,"\n\007\007 Warning: Index not in sync! Run 'index %s' asap!\n",m->area);
            puts(buf);
#ifdef MAILERROR
            mail_error(buf);
#endif
#ifdef DEBUG_HDR
            log(m->user,"NOUPDATE for %s: %s.ind out-of-sync",m->name,m->area);
#endif
            close(idxnew);
            goto sync_err;  /* cleanup and return -1 */
        }
        keep = 0;
        if(!(cmsg->status & BM_DELETE)) {
            /* Keep this message ! */
            keep = 1;
            msgs++;
            start = ftell(new);
            fseek(txt,cmsg->start,0);
            /* Copy this one to the new file,
             * start with the smtp header
             * The received: line is first
             */
            while(bgets(buf,sizeof(buf),txt) != NULL) {
                pwait(NULL);
                if(*buf == '\0') {
                    /* End of headers */
                    if(cmsg->status & BM_FORWARDED) {
                        fprintf(new,"%s%s\n",Hdrs[XFORWARD],m->name);
                        newbbs = mallocw(sizeof(struct fwdbbs));
                        strcpy(newbbs->call,m->name);
                        newbbs->next = ind.bbslist;
                        ind.bbslist = newbbs;
                    }
                    if(!(ind.status & BM_READ) && (cmsg->status & BM_READ) &&
                    (m->areatype == PRIVATE)) {
                        fprintf(new,"%sR\n",Hdrs[STATUS]);
                        ind.status |= BM_READ;
                    }
                    ind.status = (ind.status & ~BM_HOLD) | (cmsg->status & BM_HOLD);
                    fputc('\n',new);
                    break;
                }
                fprintf(new,"%s\n",buf);
            }
            /* Now do the message body */
            pos = ftell(new);
            while(bgets(buf,sizeof(buf),txt) != NULL) {
                pwait(NULL);
                if(!strncmp(buf,"From ",5)) {
                    /* End of message, write index */
                    ind.size = pos-start;
                    if (WriteIndex(idxnew,&ind)) error++;
                    keep=0;  /* should be redundant, but... */
                    break;
                }
                fprintf(new,"%s\n",buf);
                pos = ftell(new);
            }
        } /* If not message deleted */
    } /* for all messages */
    /* do we need to keep the last one ? */
    if(keep) {
        /* End of message, rewrite index line */
        ind.size = pos-start;
        if (WriteIndex(idxnew,&ind)) error++;
    }
    /* Free the last index */
    default_index("",&ind);
  
    size = ftell(new);
    /* If we have no errors, then delete the old mail and index files,
     * and rename the new one.
     */
    if(ferror(new)) error++;

cleanup:  
    fclose(txt);
    close(idx);
    fclose(new);
    close(idxnew);
  
    if(!error) {
        sprintf(buf,"%s/%s.txt",Mailspool,area);
        sprintf(old,"%s/%s.new",Mailspool,area);
        unlink(buf);
        if(size)
            rename(old,buf);
        else
            /* remove a zero length file */
            unlink(old);
        sprintf(buf,"%s/%s.ind",Mailspool,area);
        sprintf(old,"%s/%s.idn",Mailspool,area);
        unlink(buf);
        if(size)
            rename(old,buf);
        else {
            /* remove a zero length file */
            unlink(old);
#ifdef USERLOG
            sprintf(buf,"%s/%s.inf",Mailspool,area);
            unlink(buf);
//#ifndef AREADOCS
//            sprintf(buf,"%s/%s.usr",Mailspool,area);
//            unlink(buf);
//#endif /* AREADOCS */
#endif /* USERLOG */
        }
    }
    else {  /* some sort of error occurred */
#ifdef DEBUG_HDR
        log(m->user,"NOUPDATE for %s: error writing new %s.txt",m->name,m->area);
#endif
#ifdef USERLOG
        resetlastread(m);
#endif
    }
    unlockit(m);
    no_area(m);
    free(area);
    if(m->areatype == PRIVATE)
        m->mycnt = msgs;    /* Update the size of our private mailbox */
    return (error ? -1 : 0);
}
  
static int
unlockit(m)
struct mbx *m;
{
    if(--m->lockcnt) return(0);
    return(rmlock(Mailspool, m->area));
}

static int
lockit(m)
struct mbx *m;
{
    int c, cnt = 0;
    char *area;
    char MailFileBusy[] = "Mail file is busy. Retry? (y/n)";

    if(m->lockcnt++) return(0);	/* already locked, just incr cntr */
    area = strdup(m->area);
    dirformat(area);
    while(mlock(Mailspool,area)) {
        pause(1000L);   /* Wait one second */
        if(++cnt == 10) {
            cnt = 0;
            switch (m->state) {
                case MBX_REVFWD:
                case MBX_FORWARD:
#ifdef DEBUG_HDR
                log(m->user,"NOUPDATE for %s: can't lock %s",m->name,m->area);
#endif
                case MBX_TRYING:
                    c='n';
                    break;
                default:
                    if(charmode_ok(m))
                        c = tkeywait(MailFileBusy,1,
                            (int)(OPT_IS_DEFINED(m,TN_LINEMODE)&OPT_IS_SET(m,TN_LINEMODE)));
                    else  /* For AX.25 and NET/ROM connects - WG7J */
                        c = mykeywait(MailFileBusy,m);
                    break;
            }
            if (c == -1 || c == 'n' || c == 'N') {
                m->nmsgs = 0;
                m->mboxsize = 0;
                free(area);
                m->lockcnt = 0;
                return 1;
            }
        }
    }
    free(area);
    return 0;
}
  
/* read the next message or the current one if new */
int
doreadnext(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct mbx *m;
    char buf[10], *newargv[2];
  
    m = (struct mbx *) p;
  
    if(m->nmsgs == 0)   /* No mail ! */
        return 0;
  
    if((m->mbox[m->current].status & BM_READ) != 0) {
        if (m->current == 1 && m->anyread == 0)
            ;
        else if (m->current < m->nmsgs) {
            m->current++;
        } else {
            tputs("Last message\n");
            return 0;
        }
    }
    sprintf(buf,"%d",m->current);
    newargv[0] = "r";
    newargv[1] = buf;
    m->anyread = 1;
    return doreadmsg(2,newargv,p);
}
  
extern int MbRead;
  
/* display message on the crt given msg number */
/* Modified to allow the 'RM' command, 920307 - WG7J */
int
doreadmsg(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct mbx *m;
    int c, lin, mask;
    char *area, *cp, *cp2;
    int msg, i, usemore=0, verbose;
    int smtpheader, mbxheader, pathheader, print;
    FILE *txt;
    char *myargv[NARG];
    int myargc;
    int maxmsg, prevtype=NOHEADER;
    struct let *cmsg;
    char *tmpbuf;
    char buf[LINELEN];
  
    m = (struct mbx *) p;
  
    /*Check for read-permissions - WG7J */
    if(m->privs & NO_READCMD) {
        tputs(Noperm);
        return 0;
    }
    if (m->nmsgs == 0) {
        tputs(Nomail);
        return 0;
    }
    if((lin=m->morerows) != 0)
        usemore = 1;    /* Display More prompt */
  
    /*If this is the RM or VM command, setup myargv[]
     *to contain up to NARG message numbers - WG7J
     *N5KNX:If this is the RH/VH command, setup myargv[] to read only held msgs.
     */
    if(m->stype == 'M' || m->stype == 'H') {
        if (m->stype == 'M') {
            mask=BM_READ; msg=0;
            if(!m->newmsgs) {
                tputs(Nomail);
                return 0;
            }
        }
        else
            mask=msg=BM_HOLD;

        /* scan all messsages to find unread/held ones */
        cmsg = &m->mbox[1];
        myargc = 1;
        i = 0;
        while(myargc < NARG && i < m->nmsgs) {
            if((cmsg->status&mask) == msg) { /*found an unread/held msg!*/
                tmpbuf = mallocw(17); /*allocate space for the new argument*/
                myargv[myargc++] = itoa(i+1,tmpbuf,10);
            }
            i++;
            cmsg++;
        }
        if ((argc = myargc) == 1) {  /* mostly could happen in RH/VH cmd */
            tputs(Nomail);
            return 0;
        }
    } else {
        /*simply point to the old arguments*/
        for(i=1;i<argc;i++)
            myargv[i] = argv[i];
    }
    if(argc == 1) {
        tputs("Usage: Read/Verbose #\n");
        return 0;
    }
    m->state = MBX_READ;
  
    /* Open the text file */
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.txt",Mailspool,area);
    free(area);
    if((txt=fopen(buf,READ_TEXT)) == NULLFILE) {
        tprintf("Cannot open mail file %s\n",area);
        return 0;
    }
  
    if((*argv[0] == 'v'))
        verbose = 1;    /* display all header lines */
    else
        verbose = 0;
  
    for(i = 1; i < argc; ++i) {
        tmpbuf = strchr(myargv[i],'-');   /* N5KNX: allow from-to msg specification */
        msg = atoi(myargv[i]);
        if (tmpbuf == NULLCHAR)
            maxmsg = msg;
        else
            maxmsg = atoi(++tmpbuf);
        if (maxmsg < msg) {
            tprintf(Badmsg,maxmsg);
            continue;
        }
        for (; msg <= maxmsg; msg++) {
            if(msg < 1 || msg > m->nmsgs) {
                tprintf(Badmsg,msg);
                goto iamdone;
            }
            cmsg = &m->mbox[msg];
            /* if the message is on hold, only show to sysops - WG7J */
            if((cmsg->status & BM_HOLD) && !(m->privs & SYSOP_CMD)) {
                tprintf("Msg %d:",msg);
                tputs(Noperm);
                continue;
            }
            MbRead++;
#ifdef USERLOG
            /* Check the ID number of this message and
             * adjust new lastread count, if needed - WG7J
             */
            if(cmsg->msgid > m->newlastread)
                m->newlastread = cmsg->msgid;
#endif
            m->current = msg;
  
            /* Only mark your own private area as read and changed.
             * other areas, only mark as read, NOT changed !
             * 910312 - WG7J
             */
            if(!(cmsg->status & BM_READ)) {
                cmsg->status |= BM_READ;
                m->newmsgs--;
                if(m->areatype == PRIVATE && !(m->privs&NO_LASTREAD))
                    m->change = 1;
            }
            tprintf("Message #%d %s\n", msg,
            cmsg->status & BM_DELETE ? "[Deleted]" : cmsg->status&BM_HOLD ? "[Held]" : "");
            --lin;
  
            fseek(txt,cmsg->start,0);
            fgets(buf,sizeof(buf),txt);      /* the 'From ' line */
            if(verbose) {
                tputs(buf);
                lin--;
            }
            smtpheader = 1;
            while(fgets(buf,sizeof(buf),txt) != NULL) {
                if(!strncmp(buf,"From ",5))
                    break;
                print = 1;
                if(!verbose) {
                    if(smtpheader) {
                        if(*buf == '\n') {
                            smtpheader = 0;
                            mbxheader = 1;
                            pathheader = 0;
                        } else {
                            switch(htype(buf, &prevtype)) {
                                case TO:
                                case CC:
                                case FROM:
                                case DATE:
                                case SUBJECT:
                                case REPLYTO:
                                case APPARTO:
                                    break;
                                default:
                                    print = 0;
                            }
                        }
                    } else if(mbxheader) {
                        if(*buf == '\n') {
                            mbxheader = 0;
                        }
                        print = 0;
                        if(strncmp(buf,"R:",2) || \
                        ((cp=strchr(buf,'@')) == NULL) ) {
                            print = 1;
                            mbxheader = 0;
                            if(pathheader)
                                tputc('\n');
                        } else {
                            if(pathheader > 9)  /* max 5+(9+1)*7 chars */
                                continue;
                            if(*++cp == ':')
                                cp++;
                            cp2 = cp;
                            while(*cp2 != '.' && *cp2 != ' ' && *cp2 != '\n')
                                cp2++;
                            *cp2 = 0;
                            if(pathheader++ == 0)
                                tputs(Hdrs[PATH]);
                            else
                                tputc('!');
                            tputs(cp);
                        }
                    }
                }
                if(print) {
                    tputs(buf);
                    if(usemore && --lin <= 0){
                        if(charmode_ok(m))
                            c = tkeywait(TelnetMorePrompt,0,
                                (int)(OPT_IS_DEFINED(m,TN_LINEMODE)&OPT_IS_SET(m,TN_LINEMODE)));
                        else  /* For AX.25 and NET/ROM connects - WG7J */
                            c = mykeywait(BbsMorePrompt,m);
                        lin = m->morerows;
                        if(c == -1 || c == 'q' || c == 'Q')
                            break;
                        if(c == '\n' || c == '\r')
                            lin = 1;
                    }
                }
            }
            if (ferror(txt)) {  /* Maybe area was rewritten after a fwd completed? */
                tprintf("Error accessing msg...try again?\n");
#ifdef notdef
                log(m->user,"%s got err '%s' reading area %s",m->name,sys_errlist[errno],m->area);
#endif
                continue;  /* trying to read others is better than an abort? */
            }
#ifdef HOLD_LOCAL_MSGS
            if(cmsg->status&BM_HOLD) {
                if(charmode_ok(m))
                    c = tkeywait(ReleaseHeldMsg,1,
                        (int)(OPT_IS_DEFINED(m,TN_LINEMODE)&OPT_IS_SET(m,TN_LINEMODE)));
                else { /* For AX.25 and NET/ROM connects - WG7J */
                    c = mykeywait(ReleaseHeldMsg,m);
                }
                if(c == 'y' || c == 'Y') {
                    cmsg->status &= ~BM_HOLD; /* reset flag */
                    m->change=1;  /* force update of index */
                }
                else if (c == 'k' || c == 'K') {
                    cmsg->status |= BM_DELETE;
                    m->change=1;  /* force update of index */
                }
            }
#endif
        }
    }
    iamdone:
    fclose(txt);
    /* If this was 'RM' or 'VM',
     * free the memory allocated for myargv[] - WG7J
     */
    if(m->stype == 'M') {
        for(i=1;i<argc;i++)
            free(myargv[i]);
    }
    return 0;
}
  
/* Set up m->to when replying to a message. The subject is returned in
 * m->line.
 */
int
mbx_reply(argc,argv,m,cclist,rhdr)
int argc;
char *argv[];
struct mbx *m;
struct list **cclist;   /* Pointer to buffer for pointers to cc recipients */
char **rhdr;        /* Pointer to buffer for extra reply headers */
{
    char *cp, cx;
    int msg;
    struct cclist *cc;
    struct mailindex ind;
  
    if(argc == 1)
        msg = m->current;
    else
        msg = atoi(argv[1]);
  
    if(msg < 1 || msg > m->nmsgs) {
        if(m->sid & MBX_SID)
            tputs("NO - ");
        tprintf(Badmsg,msg);
        return -1;
    }
  
    /* clear index */
    memset(&ind,0,sizeof(struct mailindex));
  
    /* Get the index for this message */
    cp = strdup(m->area);
    dirformat(cp);
    if(get_index(msg,cp,&ind) == -1) {
        free(cp);
        return -1;
    }
    free(cp);
  
    /* Free anything that might be allocated
     * since the last call to mbx_to() or mbx_reply()
     */
    free(m->to);
    m->to = NULLCHAR;
    free(m->tofrom);
    m->tofrom = NULLCHAR;
    free(m->tomsgid);
    m->tomsgid = NULLCHAR;
    free(m->origto);
    m->origto = NULLCHAR;
  
    /* Assign data from the index */
    if(strlen(ind.replyto))
        m->to = strdup(ind.replyto);
    else
        m->to = strdup(ind.from);
  
    *(m->line)='\0';
    if(strnicmp(ind.subject,"Re: ",4))
        strcpy(m->line, "Re: ");
    strncat(m->line, ind.subject, MBXLINE-strlen(m->line));
  
    for(cc=ind.cclist;cc;cc=cc->next) {
        msg = strlen(m->name);  /* n5knx: try to avoid sending to self */
        cx = *(cc->to+msg);
        if (strnicmp(cc->to, m->name, msg) == 0 && (cx == '@' || cx == '\0')
           ) continue;
        addlist(cclist,cc->to,0,NULLCHAR);
    }
  
    *rhdr = mallocw(LINELEN);
    sprintf(*rhdr,"In-Reply-To: your message of %s"
    "             <%s>\n",
    ctime(&ind.date),ind.messageid);
    /* Free the index */
    default_index("",&ind);
    return 0;
}
  
#ifdef USERLOG
  
/*get the last message listed/read
 *from the areaname.USR file
 *keeps track for each user.
 *February '92, WG7J
 */
void
getlastread(m)
struct mbx *m;
{
    FILE *Alog;
    char buf[FILE_PATH_SIZE];
    char *cp;
    int found=0;
    char *lognam;
  
    m->lastread = m->newlastread = 0L;
    if ((m->privs&NO_LASTREAD) == 0) {
        lognam = mallocw(strlen(m->name)+2);  /* room for 1 extra char */
        strcpy(lognam,m->name);
        if (m->sid & MBX_SID)
            strcat(lognam, "_");
  
        sprintf(buf,"%s/%s.usr",Mailspool,m->area);
        if ((Alog = fopen(buf,"r+")) == NULLFILE) {
            /* USR file doesn't exist, create it */
            if((Alog = fopen(buf,"w")) == NULLFILE) {
                free(lognam);
                return;
            }
        }
        else      /*Find user in the usr file for this area*/
          for(;;) {
            if(fgets(buf,sizeof(buf),Alog) == NULLCHAR)
                break;
            if((cp=strchr(buf,' ')) != NULLCHAR)
                *cp = '\0';
            if(!stricmp(lognam,buf)) {
                /*found user*/
                cp++;
                while(*cp == ' ')   /*skip blanks*/
                    cp++;
                m->lastread = m->newlastread = atol(cp);
                found = 1;
                break;
            }
          }
        if(!found) {
            /*Add user */
            sprintf(buf,"%s 0\n",lognam);
            fputs(buf,Alog);
        }
        fclose(Alog);
        free(lognam);
    }

//#ifdef AREADOCS
    if (!found && !(m->sid&MBX_SID)
#ifdef FWDFILE
        && (m->family != AF_FILE)
#endif
       ) {  /* show area doc file only once */
        sprintf(buf, "%s/%s.doc", Mailspool, m->area);
        if ((Alog = fopen(buf,READ_TEXT)) != NULLFILE) {
            sendfile(Alog,m->user,ASCII_TYPE,0,m);
            tprintf("\nType   AF %s   to see this info again.\n", m->area);
            fclose(Alog);
        }
    }
//#endif /* AREADOCS */
    return;
}
  
/* Write the new last read id number to the USR file - WG7J
 * only update if this is not a bbs,
 * current area is a public area and not 'help',
 * or anything that starts with 'sys',
 * and a new message was actually listed/read
 */
void
setlastread(m)
struct mbx *m;
{
    FILE *Alog, *tfile;
    char *cp, *lognam;
    char buf[80];
    char tmpname[80];
  
    if(m->newlastread <= m->lastread)
        return;
  
    lognam = mallocw(strlen(m->name)+2);  /* room for 1 extra char */
    strcpy(lognam,m->name);
    if (m->sid & MBX_SID)
        strcat(lognam, "_");
  
    /* Rename the USR file to a tempfile */
    sprintf(buf,"%s/%s.usr",Mailspool,m->area);
  
    /* N5KNX: change suffix from .usr to .tmp */
    sprintf(tmpname,"%s/%s.tmp",Mailspool,m->area);
    if(rename(buf,tmpname))
        /* Can't rename ??? */
        return;
  
    if((Alog = fopen(buf,"w")) == NULLFILE) {
        /* can't creat new USR file ???*/
        rename(tmpname,buf);    /* try to undo the damage */
        return;
    }
  
    if((tfile = fopen(tmpname,"r")) == NULLFILE)
        /* can't open renamed file ??? */
        return;
  
    /*Write all users back, but update this one!*/
    while(fgets(buf,sizeof(buf),tfile) != NULLCHAR) {
        if((cp=strchr(buf,' ')) != NULLCHAR)
            *cp = '\0';
        if(!stricmp(lognam,buf)) {
            /*found this user*/
            sprintf(buf,"%s %lu\n",lognam,m->newlastread);
        } else
            *cp = ' '; /* restore the space !*/
        fputs(buf,Alog);
    }
    fclose(tfile);
    unlink(tmpname);
    fclose(Alog);
    free(lognam);
  
    return;
}
  
/* After some errors it's best to forget where we were, and start over
   next time.  Example: FBB forwarding session closes, but can't update the
   area.txt file with X-Forwarded-To headers.  This would result in connects
   but perhaps no traffic passed.  -- n5knx */
static void
resetlastread(m)
struct mbx *m;
{
    if (m->sid & MBX_SID) {
        m->lastread = m->newlastread = 0;
        setlastread(m);
    }
}

#endif /*USERLOG*/
  
void
scanmail(m)      /* Get any new mail */
struct mbx *m;
{
    int idx;
    char *area;
    int ret;
    char buf[FILE_PATH_SIZE];
    long diff;
  
    if(lockit(m))
        return;
    if ((diff = isnewmail(m)) == 0L) {
        unlockit(m);
        return;
    }
    if(diff < 0L) {
        /* Area wasn't open yet, or the file size has decreased.
         * Any changes we did to this area will be lost,
         * but this is not fatal.
         */
#ifdef DEBUG_HDR
        if (diff < -1L) log(m->user,"NOUPDATE for %s: msgs deleted in %s [scanmail] %ld,%ld,%d",m->name,m->area,diff,m->mboxsize,errno);
#endif
        initnotes(m);
        unlockit(m);
        return;
    }
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.ind",Mailspool,area);
    if ((idx = open(buf,READBINARY)) == -1)
        log(m->user,OpenError,m->name,errno,buf);
    else {
        /* Reread all messages since they may have changed
         * in size after an X-Forwarded-To line was added.
         */
        ret = readnotes(m,idx);   /* get the mail */
        close(idx);
        if(m->areatype == PRIVATE)
            m->mycnt = m->nmsgs;
        if (ret != 0)
            tputs("Error updating mail file\n");
    }
    unlockit(m);
    free(area);
}
  
/* Check the current mailbox to see if new mail has arrived.
 * Returns the difference in size.
 */
static long
isnewmail(m)
struct mbx *m;
{
    int idx;
    long size;
    char *area;
    char buf[FILE_PATH_SIZE];

    if(lockit(m)) return(-1); /* SyncIndex assumes a lock */
    SyncIndex(m->area);       /* ensure index file is current */
    unlockit(m);
  
    area = strdup(m->area);
    dirformat(area);
    sprintf(buf,"%s/%s.ind",Mailspool,area);
    free(area);
  
    if((idx = open(buf,READBINARY)) == -1) {
        if(m->mboxsize == 0)
            return 0;
        return -1;
    }
    close(idx);
  
    /* index file exists, check mailbox */
    strcpy(&buf[strlen(buf) - 3],"txt");
    size = fsize(buf);
    if(m->mboxsize == 0) {
        if(size > 0)
            return -1;  /* New open of mailbox */
        return 0;       /* No mail; no change */
    }
    /* Others */
    return size - m->mboxsize;
}
  
/* Check if the private mail area has changed */
long
isnewprivmail(m)
struct mbx *m;
{
    int idx;
    struct indexhdr hdr;
    char buf[FILE_PATH_SIZE];
  
    sprintf(buf,"%s/%s.ind",Mailspool,m->name);
    if((idx=open(buf,READBINARY)) == -1)
        hdr.msgs = 0;
    else {
        read_header(idx,&hdr);
        close(idx);
    }
  
    return hdr.msgs - m->mycnt; /* != 0 not more than once */
}
  
#endif /* MAILCMDS */
  
/* This function returns the length of a file. The proper thing would be
 * to use stat(), but it fails when using DesqView together with Turbo-C
 * code.
 */
long
fsize(name)
char *name;
{
    long cnt;
    FILE *fp;
  
    if((fp = fopen(name,READ_TEXT)) == NULLFILE)
        return -1L;
    fseek(fp,0L,2);
    cnt = ftell(fp);
    fclose(fp);
    return cnt;
}

/* Print prompt and read one character, telnet version */
int
tkeywait(prompt,flush,linemode)
char *prompt;   /* Optional prompt */
int flush;  /* Flush queued input? */
int linemode;	/* line mode in effect? */
{
    int c, cl, i;
#ifdef notdef_l
    int oldimode,oldomode;  /* obsoleted in 1.10L */
#endif
  
    if(flush && socklen(Curproc->input,0) != 0)
        recv_mbuf(Curproc->input,NULL,0,NULLCHAR,0); /* flush */
    if(prompt == NULLCHAR)
        prompt = "Hit enter to continue";
    tflush();    /* some BSD-derived telnets need opts in separate pkt */
    while (socklen(Curproc->output, 1) > 0)    /* else CRs in pkt are ignored */
        pause(250L);
    tprintf("%s%c%c%c%c%c%c",prompt,IAC,WILL,TN_ECHO,IAC,WILL,TN_SUPPRESS_GA);
    if (linemode)  /* KF8NH: handle Unix telnet clients correctly */
        tprintf("%c%c%c%c%c%c%c",IAC,SB,TN_LINEMODE,1,2,IAC,SE);
    tflush();
  
    /* discard the response */
  
#ifdef notdef_l
    oldimode = sockmode(Curproc->input,SOCK_BINARY);
    oldomode = sockmode(Curproc->output,SOCK_BINARY);
#endif
  
regetchar:
    while((c = rrecvchar(Curproc->input)) == IAC){
        c = rrecvchar(Curproc->input);
        if(c == SB) {
            if((c = rrecvchar(Curproc->input)) == EOF)
                break;
            cl = c;
            c = rrecvchar(Curproc->input);
            while((c != EOF) && !(cl == IAC && c == SE)) {
                cl = c;
                c = rrecvchar(Curproc->input);
            }
        } else if(c > 250 && c < 255)
            rrecvchar(Curproc->input);
    }
  
/* n5knx 1.10L: Darned (j)nos default is EOL STANDARD, so telnetting to a Jnos
   mailbox will result in the Enter key yielding just a CR.  But I thought
   the telnet requirement was CR/LF or CR/NUL.  If I change above rrecvchar()
   to recvchar() so as to consume multiple eol chars, we'll have to press
   Enter twice, or set EOL NULL.  Should I change the default, and make the
   above change, and eliminate the following kludge?  I don't know ... who
   relies on "transparent" mode where Enter yields just a CR?
*/
   if (c == '\0') goto regetchar;  /* we could also test for \n */

#ifdef notdef_l
    sockmode(Curproc->output,oldomode);
    sockmode(Curproc->input,oldimode);
#endif  
    /* Get rid of the prompt */
    for(i=strlen(prompt);i != 0;i--)
        tputc('\b');
    for(i=strlen(prompt);i != 0;i--)
        tputc(' ');
    for(i=strlen(prompt);i != 0;i--)
        tputc('\b');
    tprintf("%c%c%c%c%c%c",IAC,WONT,TN_ECHO,IAC,WONT,TN_SUPPRESS_GA);
    if (linemode)
        tprintf("%c%c%c%c%c%c%c",IAC,SB,TN_LINEMODE,1,1,IAC,SE);
    tflush();
    return c;
}
  
/* Print prompt and read reply,  AX.25 and NETROM version - WG7J
 * We read a line, discard the line ending, and if we find
 * 'N' or 'n' ANYWHERE in the line, return -1, else return the first char.
 */
int
mykeywait(prompt,m)
char *prompt;
struct mbx *m;
{
    char c;

    tputs(prompt);
    tflush();
    if(recvline(m->user, m->line, MBXLINE) == -1  ||
       strpbrk(m->line,"Nn"))    /* Treat n or N specially */
        return -1;

    c = *(m->line);
    if (c == '\n' || c == '\r') c=0;    /* 0 if an empty line was read */
    return c;  
}
  
#ifdef notdef
  
/* Echo some stuff for debugging */
void eout(char *s) {
  
    puts(s);
    fflush(stdout);
    while(!kbhit());
    getch();
}
  
#endif
  
/* Delete mail.lck files. This currently does NOT recurse - WG7J */
void RemoveMailLocks() {
    int done;
    struct ffblk ff;
    char *buf;
  
    buf = mallocw(BUFSIZ);
    sprintf(buf,"%s/*.lck",Mailspool);
    done = findfirst(buf,&ff,0);
    while(!done) {
        sprintf(buf,"%s/%s",Mailspool,ff.ff_name);
        unlink(buf);
        done=findnext(&ff);
    }
    free(buf);
}
  

