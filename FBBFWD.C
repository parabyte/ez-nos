/*
 * The code in this file was adapted from forward.c by KF5MG, to support
 * FBB-style ax.25 forwarding.
 */

#include <ctype.h>
#include <time.h>
#include "global.h"
#ifdef FBBFWD
#include "bm.h"
#include "mailbox.h"
#include "mailutil.h"
#include "smtp.h"
#include "cmdparse.h"
#include "proc.h"
#include "socket.h"
#include "timer.h"
#include "usock.h"
#include "netuser.h"
#include "ax25.h"
#include "netrom.h"
#include "nr4.h"
#include "files.h"
#include "index.h"
#ifdef FBBCMP
#include "lzhuf.h"
#include "mailfor.h"
#endif /* FBBCMP */
#ifdef UNIX
#include "unix.h"
/* n5knx: Avoid stdout so line endings are handled right */
#define PRINTF tcmdprintf
#define PUTS(x) tcmdprintf("%s%c",x,'\n')
#define PUTCHAR(x) tcmdprintf("%c",x)
#else
#define PRINTF printf
#define PUTS puts
#define PUTCHAR putchar
#endif

/* By setting the fp to NULL, we can check in exitbbs()
 * whether a tempfile has been closed or not - WG7J
 */
#define MYFCLOSE(x) { fclose(x); x = (FILE *) 0; }
#define CTLZ    26              /* EOF for CP/M systems */

extern int MbForwarded;

/*
     *** Protocole Error (x) messages.
     0: Subject Packet does not start with a SOH (01) Byte.
     1: Checksum of message is wrong.
     2: Message could not be uncompressed.
     3: Received binary frame is not STX (02) or EOT (04).
     4: Checksum of proposals is wrong.
     5: Answer to proposals must start with "F" or "**".
     6: Answer to proposals must be "FS".
     7: More than 5 answers (with "+", "-" or "=") to proposals.
     8: Answer to proposal is not "+", "-" or "=".
     9: The number of answers does not match the number of proposals.
    10: More than 5 proposals have been received.
    11: The number of fields in a proposal is wrong (6 fields).
    12: Protocol command must be "FA", "FB", "F>", "FF" or "FQ".
    13: Protocol line starting with a letter which is not "F" or "*".
*/

#define FBBerror(x,y) { tprintf("*** Protocol Error (%d)\n", x); log(y,"fbbfwd: detected FBB protocol error %d", x); }

#ifdef FBBVERBOSELOG
static char RmtDisconnect[] = "FBBFWD: remote disconnected early.";
#endif

#ifdef FBBCMP

static int dofbbacceptnote  __ARGS((struct mbx *, char *));
static int fbbsendmsg __ARGS((struct fwd *, int, FILE *));

extern char *Mbhaddress;
extern char *Mbfwdinfo;
extern char *Mbqth;
extern char *Mbzip;
extern int Mbsmtptoo;
extern int Mbheader;
extern char shortversion[];

/* fbbsendmsg() is adapted from sendmsg() in forward.c.  It writes the
 * message to tfile just as we would send it using sendmsg().
 * N5KNX 2/95: if we can't send a complete msg, return code -1, otherwise
 * return code 0.
 */
static int
fbbsendmsg (f, msgn, tfile)
     struct fwd *f;
     int    msgn;
     FILE   *tfile;                     // Data is returned in this
                                        // file handle. It is open
                                        // when we're called and we
                                        // close it when we leave.
{
   struct mbx *m = f->m;

   int  i       = 0;
   int  rheader = 0;
   int  result  = 0;
   long start;
   char buf[LINELEN];

   /* If the data part of the message starts with "R:" the RFC-822
    * headers will not be forwarded. Instead we will add an R:
    * line of our own.
    */
   if (Mbheader)
     {
        /* First send recv. date/time and bbs address */
        fprintf (tfile, "R:%s", mbxtime (f->ind.mydate));
        /* If exists, send H-address */
        if (Mbhaddress != NULLCHAR)
           fprintf (tfile, " @:%s", Mbhaddress);
        /* location, if any */
        if (Mbqth != NULLCHAR)
           fprintf (tfile, " [%s]", Mbqth);
        /* if there is info, put it next */
        if (Mbfwdinfo != NULLCHAR)
           fprintf (tfile, " %s", Mbfwdinfo);
        /* number of the message */
        fprintf (tfile, " #:%lu", (f->ind.msgid % 100000L));  /* n5knx: use rightmost 5 digits */
        /* The BID, if any */
        if (f->bid[0] != '\0')
           fprintf (tfile, " $:%s", &f->bid[1]);
        /* zip code of the bbs */
        if (Mbzip != NULLCHAR)
           fprintf (tfile, " Z:%s", Mbzip);
//      fputc ('\n',tfile);
        fputs("\r\n", tfile);
     }

   /* Open the mailbox file */
   sprintf (buf, "%s/%s.txt", Mailspool, m->area);
   if ((m->mfile = fopen (buf,READ_BINARY)) == NULLFILE) goto early_quit;

   /* point to start of this message in file */
   start = m->mbox[msgn].start;
   fseek (m->mfile, start, SEEK_SET);
   if (ferror(m->mfile)) goto early_quit;

   /* If we also send the smtp headers, now see if the message
    * has any R: headers. If so, send them first.
    */
   if (Mbsmtptoo) {
      while (bgets (buf, sizeof (buf), m->mfile) != NULLCHAR) {
         if (!*buf)
            break;          /* End of smtp headers */
      }
      if(feof(m->mfile) || ferror(m->mfile)) goto early_quit;

      /* Found start of msg text, check for R: lines */
      while (bgets (buf, sizeof (buf), m->mfile) != NULLCHAR && !strncmp (buf, "R:", 2)) {
         rheader = 1;
         fputs (buf,tfile);
//       fputc ('\n',tfile);
         fputs("\r\n", tfile);
      }
      /* again point to start of this message in file */
      fseek (m->mfile, start, SEEK_SET);
      if(ferror(m->mfile)) goto early_quit;
   }

   /* Go past the SMTP headers to the data of the message.
    * Check if we need to forward the SMTP headers!
    * 920114 - WG7J
    */
   if (Mbsmtptoo && (rheader || Mbheader))
//    fputc ('\n',tfile);
      fputs("\r\n", tfile);
   i = NOHEADER;
   while (bgets (buf, sizeof (buf), m->mfile) != NULLCHAR && *buf) {
      if (Mbsmtptoo) {
         /* YES, forward SMTP headers TOO ! */
         switch (htype (buf, &i)) {
           case XFORWARD:   /* Do not forward the "X-Forwarded-To:" lines */
           case STATUS:     /* Don't forward the "Status:" line either */
           case BBSTYPE:
           case SUBJECT:
           case TO:
           case APPARTO:
           case CC:
           case DATE:
              break;
           case FROM:
              /* Don't forward the "From: " line either.
               * make it ">From: "
               */
              fputc ( '>', tfile);
              /*note fall-through */
           default:
              if (!strncmp (buf, "From ", 5))
                 fputc ('>',tfile);
              fputs (buf,tfile);
//            fputc ('\n',tfile);
              fputs("\r\n", tfile);
         }
      }
   }
   if(feof(m->mfile) || ferror(m->mfile)) goto early_quit;

   /* Now we are at the start of message text.
    * the rest of the message is treated below.
    * Remember that R: lines have already been sent,
    * if we sent smtp headers !
    */
   i = 1;

   while (bgets (buf, sizeof (buf), m->mfile) != NULLCHAR && strncmp (buf, "From ", 5)) {
      if (i) {
           if (!strncmp (buf, "R:", 2)) {
              if (Mbsmtptoo)
                 continue;
           } else {
              i = 0;
              if (*buf) {
                 /* Ensure body is separated from R: line */
//               fputc ('\n',tfile);
                 fputs("\r\n", tfile);
              }
           }
      }
      fputs (buf,tfile);
      fputs ("\r\n",tfile);
   }

   if(feof(m->mfile)) clearerr(m->mfile);  /* only place EOF is acceptable */

early_quit:
   if(m->mfile == NULLFILE || ferror(m->mfile) || feof(m->mfile) ||
        (ftell(m->mfile)-start < m->mbox[msgn].size)) {
#ifdef JPDEBUG
        log(m->user,"fbbsendmsg: truncated %s msg %d would result from err %d",m->area,msgn,errno);
#endif
        fputs("\r\n*** Cannot find complete message body!\r\n", tfile);
#if defined(MAILERROR) && defined(JPDEBUG)
        mail_error("FBBsendmsg: %s: Cannot find body for %s msg %d", \
            m->name,m->area,msgn);
#endif
/* OK, we were unable to send a complete msg body.  We rely on returning an
   error code to result in a disconnect rather than a completed msg.
*/
//        m->user = -1;  /* this will fail subsequent recvline(m->user...) calls */
        result = -1;   /* this will prevent a /ex or ^Z from being sent */
   }

   fclose(m->mfile);
   m->mfile = NULL;


   // close the data file.
   fclose(tfile);
   return result;
}
#endif /* FBBCMP */


/* FBB Fowarding comments:

   FBB Forwarding sends messages in groups instead of normal pbbs forwarding
   that sends everything. i.e. You send 5 messages, he sends 5 messages, you
   send 5 messages, he sends 5 messages, etc.

   In our case... we send up to 5 messages from a forwarding area and then
   let the other system send us messages. When we get to the end of the
   messages in an area, we may send less than 5 messages. When we're done
   with the area... the other system will send us messages and we'll switch
   to another message area and start sending messages again. So... he may
   send 5 messages each time, and we might send 1 message each time... it just
   depends on how many messages are in each area. Hope that makes sense.
*/

#define FBBMAXMSGS  5   /* Maximum number of messages to process. */

#ifdef EXPIRY
extern int Eproc;
int    FBBSendingCnt  = 0;   /* how many dofbbsend()'s in progress (see expire.c) */
#endif

#define fbbUNKNOWN 0
#define fbbNO      1
#define fbbYES     2
#define fbbDEFER   3

// This code converts/parses a FBB FA/FB type message to a fbbpacket structure.
static
int fbbparse(struct fbbpacket *msglst, char *fbline){
   char *cp;
   char *atbbs;
   char *to;

   /* Here's an example FBB message header packet.
      This one happens to contain 3 message headers.

      FB B KF5MG USA NOS 34535_KF5MG 298
      FB B KF5MG USA NOS 34537_KF5MG 303
      FB B KF5MG USA NOS 34541_KF5MG 309
   */

   // Data Type ( FA = compressed ascii msg; FB = compressed binary file, or uncompressed msg)
   cp = strtok(fbline," ");
   strncpy(msglst->fbbcmd, cp, sizeof(msglst->fbbcmd));

   // Message type ( P, B, or T )
   cp = strtok('\0', " ");
   msglst->type = *cp;

   // From id ( Userid only )
   cp = strtok('\0', " ");
   free(msglst->from);
   msglst->from = strdup(cp);

   // hostname or flood-area ( USA, WW, KF5MG.#DFW.TX.USA.NOAM, etc.)
   cp = strtok('\0', " ");
   atbbs = strdup(cp);

   // To: id ( 6 char max. )
   cp = strtok('\0', " ");
   to = strdup(cp);

   // Bid ( ax.25 PBBS message id )
   cp = strtok('\0', " ");
   free(msglst->messageid);
   msglst->messageid = strdup(cp);

   // Size ( At present, we don't do anything with this. )
   cp = strtok('\0', " ");
   msglst->size = atoi(cp);

   // set msglst->to equal to the 'to id' '@' 'hostname'
   free(msglst->to);
   msglst->to = malloc(strlen(to) + strlen(atbbs) + 2);

   strcpy(msglst->to, to);
#ifdef use_local	/* see makecl() in forward.c */
   if (stricmp(atbbs, "local")) {  /* N5KNX: drop @atbbs if == "local" */
#endif
      strcat(msglst->to, "@");
      strcat(msglst->to, atbbs);
#ifdef use_local
   }
#endif

   free(to);
   free(atbbs);

   return 1;
}


// This code processes a FS packet from a remote system.
// Format is: FS ccccc     where each c is a character:
// A '+' means the message is accepted.
//   '-' means the message is rejected.
//   '=' means the message is accepted but don't send it yet.
//       deferred messages (=) are a real pain to process. :(
//
// RC =  0 means an error was detected.
//       1 means an 'FF' was received from the remote system.
//       2 means an 'Fx' (where x is something) was received.

static
int fbbdofs(struct fbbpacket *msglst,
            struct fwd *f,
            int    msgcnt,
            long   *mindeferred,
            long   *maxdeferred,
            int    idx)
{

   int i;
   struct mbx *m;

#ifdef FBBCMP
   FILE *tfile;
   int  rc;
   int  NotConnected = FALSE;
#endif

   m      = f->m;

   // Send any data in our buffer.
   usflush(m->user);
   // Get the FS line.
  alarm(Mbtdiscinit*1000L);
   if(recvline (m->user, m->line, MBXLINE) == -1 ) {
#ifdef FBBVERBOSELOG
       log(m->user, RmtDisconnect);
#endif
       return 0;
   }
   alarm(0);
   rip(m->line);

   // Make sure we got a "FS " line. Anything else is an error.
   if(strnicmp(m->line,"FS ", 3) != 0) {
      tprintf("FBB Error: Expected 'FS ' string. Received \'%s\' string.\n",m->line);
      log(m->user, "FBB error with %s: expected FS, got %s", m->name,m->line);
      return 0;
   }

   // Check to see if we got the right number of responses.
   if((strlen(m->line) != msgcnt + 3))
      return 0;

   // Validate FS line. Clean up unused entries.
   for(i=0;i<FBBMAXMSGS;i++) {
      if(i<msgcnt) {
         if((m->line[3+i] != '+') &&
            (m->line[3+i] != '-') &&
            (m->line[3+i] != '=')) {
                tprintf("FBB Error: Expected '+, -, or ='. Received \'%c\'.\n",m->line[3+i]);
                log(m->user,"FBB error with %s: Expected +-=, got %c", m->name,m->line[3+i]);
                return 0;
         }
      } else  {
         // Zero out and free rest of the FS structure.
         msglst[i].accept = fbbUNKNOWN;
         free(msglst[i].to);
         free(msglst[i].rewrite_to);
         free(msglst[i].from);
         free(msglst[i].messageid);
         free(msglst[i].sline);
         msglst[i].to        = \
         msglst[i].rewrite_to= \
         msglst[i].from      = \
         msglst[i].messageid = \
         msglst[i].sline     = NULLCHAR;
      }
   }


   // The FS line is OK. Now we can send the '+' messages and delete the
   // '-' messages. The '=' messages can be ignored for now.
   for(i=0;i<msgcnt;i++) {
#ifdef FBBCMP
      if(NotConnected)
          break;
#endif
      pwait(NULL);
      rip(msglst[i].sline); // pretty up log.

      MbForwarded++;

      if(m->line[3+i] == '+') {
         // Message is wanted. We need to send it.
         // Don't update the X-Forwarded Header yet. FBB does not tell us
         // if it received the message or not. We have to send all our
         // messages and then see if the connect is still there. If we get
         // a valid FBB Response, then we can assume that the messages were
         // delivered and update the X-Forwarded flag.

         msglst[i].accept = fbbYES;

         // I don't like having to re-read the index here, but sendmsg()
         // needs a lot of index info so it needs to be done.

         // Re-position filepointer to index....
         lseek(idx,m->mbox[msglst[i].number].indexoffset,SEEK_SET);

         // Clear/Free the previous index
         default_index(m->area,&f->ind);

         // and read the index.
         if (read_index(idx,&f->ind) == -1) return(0);  /* should not happen */

         // sendmsg() assumes that f->bid will hold the message id.
         // msglst[i].bid was saved from the makecl() call.
         strcpy(f->bid, msglst[i].bid);

#ifdef FBBCMP
         if(f->m->sid & MBX_FBBCMP) {
              // Open a tmpfile for fbbsendmsg to place
              // the mssage data in. This file is not closed,
              // but passed to send_yapp for encoding and
              // transmitting.

              strcpy(f->iFile, tmpnam(NULL));
              if((tfile = fopen(f->iFile, "w+b")) == NULLFILE) {
                 log(m->user, "FBBFWD: fbbdofs: Error %d opening %s", errno, f->iFile);
                 return 0;
              }
              // Prepare the text of the message.
              if (fbbsendmsg (f, msglst[i].number, tfile) == -1)
                 NotConnected = TRUE;   /* some error preparing msg */
              else {
                 strcpy(f->oFile, tmpnam(NULL));
                 rc = send_yapp(f->m->user, f, f->ind.subject); // Send it.
                 if(rc == 0) {
#ifdef FBBVERBOSELOG
                    log(m->user,"FBBFWD: send_yapp() error.");
#endif
                    NotConnected = TRUE;
                 }
              }
              // erase the input and output file.
              unlink(f->iFile);
              unlink(f->oFile);
         } else
#endif /* FBBCMP */
         {
           // Send a subject line.
           tprintf("%s\n",f->ind.subject);


           // Send the text of the message.
           if (sendmsg(f, msglst[i].number) == -1) {  /* aborted */
               default_index(m->area,&f->ind);
               break;
           }

           // Send a ctrl-z ... that's OCTAL(32)
           tputs("\032\n");
         }

         usflush(m->user);

         // and finally free the index.
         default_index(m->area,&f->ind);

      } else
         // The remote system doesn't want this message.
         // It might be a NO (-) or it might be a LATER (=)
         // Go ahead and mark the msg as so. If the link goes
         // down, we still want to mark this message as unwanted.
         // One the link goes back up, we don't want to ask the
         // remote system again about this because it already
         // told us no. This is handeled differently than '+'
         // messages.

         if(m->line[3+i] == '-') {
            msglst[i].accept = fbbNO;
            // mark message as forwarded or deleted.
            if(m->areatype == AREA)
               m->mbox[msglst[i].number].status |= BM_FORWARDED;
            else
               m->mbox[msglst[i].number].status |= BM_DELETE;
            m->change = 1;

            log(m->user,"MBOX bbs mail refused: %s", msglst[i].sline);
         } else
            // The remote system want's this message... later.... not now.
            // To handle this... we ignore the message for now and we
            // attempt to update the m->lastread message number to the
            // first deferred message. That way the next time we transfer
            // messages, we'll ask about this message again. BUT... during
            // this tranfer sequence, we don't want to keep asking about
            // the  same message so we have to keep track of the highest
            // deferred message.
            // Anyway... the mindeferred counter is the FIRST deferred
            // message we do and is used to update the m->lastread counter.
            // The maxdeferred counter is the last deferred message.

            if(m->line[3+i] == '=') {
               msglst[i].accept = fbbDEFER;
               // Re-position filepointer to index....
               lseek(idx,m->mbox[msglst[i].number].indexoffset,SEEK_SET);

               // Clear/Free the previous index
               default_index(m->area,&f->ind);

               // and read the index.
               if(read_index(idx,&f->ind) == -1) return(0);  /* should not happen */

               // maxdeferred will point to the highest deferred msg we've
               // processed.
               if(*maxdeferred < f->ind.msgid)
                  *maxdeferred = f->ind.msgid;

               // mindeferred will point to the first deferred msg we've
               // processed.
               if(*mindeferred == 0)
                  *mindeferred = f->ind.msgid;

               // and finally free the index.
               default_index(m->area,&f->ind);
            }
   } // end for

#ifdef FBBCMP
   if(NotConnected)
       return 0;
#endif


   // Now we'll get a line from the remote system to make sure that the
   // messages were received ok.

   usflush(m->user);
   alarm(Mbtdiscinit*1000L);
   if(recvline(m->user,m->line,MBXLINE) == -1) {
      // There was a problem. We did not get a response from the remote
      // system after we sent our messages.
      // We'll go ahead and delete the '-' messages. He didn't want them
      // now, so he won't want them later.
#ifdef FBBVERBOSELOG
      log(m->user, RmtDisconnect);
#endif
      return 0;
   } else {
        alarm(0);
        if (strncmp (m->line, "*** ", 4) == 0) {  /* *** Protocole err, *** Checksum err, ... */
             rip(m->line);
             log (m->user, "FBB error from %s: %s", m->name, m->line);
             return 0;
        }

      // We got a response back, so we can update our '+' messages.
      for(i=0;i<FBBMAXMSGS;i++) {
         if(msglst[i].accept == fbbYES) {
            // mark message as sent.
            if(m->areatype == AREA)
               m->mbox[msglst[i].number].status |= BM_FORWARDED;
            else
               m->mbox[msglst[i].number].status |= BM_DELETE;
            m->change = 1;

            // For each valid note.
            log(m->user,"MBOX bbs mail sent: %s ", msglst[i].sline);

            free(msglst[i].to);
            free(msglst[i].rewrite_to);
            free(msglst[i].from);
            free(msglst[i].messageid);
            free(msglst[i].sline);
            msglst[i].to        = \
            msglst[i].rewrite_to= \
            msglst[i].from      = \
            msglst[i].messageid = \
            msglst[i].sline     = NULLCHAR;
         }
      }
   }

   // Change forward direction
   m->state = MBX_REVFWD;
   if(strnicmp(m->line,"FF", 2) == 0)
      return 1;
   else
      return 2;
}

// This code receives the FB packet ( up to 5 messages ) from a FBB
// system. Once we get this FB Packet, we'll figure out which notes we
// want... send our response ( FS string ) and receive the messages.

/* Return codes for dofbbrecv:
   0 => Error, unexpected data format in m->line,
        or can't deliver mail.  Disconnect required.
   1 => All OK, msgs received and processed properly.
   2 => FF received, remote BBS has no data for us.
   3 => FQ received, remote BBS wants to quit.
*/
 
static
int dofbbrecv(struct fwd *f)
{
    int  rc;
    int  msgcnt;
    int  msgsize = 0;

    int  FBBok;
    int  FBBdone;

    int  i,err;
    int  FirstTime;
    int  NotConnected = FALSE;

    unsigned int checksum;
    char FBBresp[FBBMAXMSGS+4];	/* Must be FBBMAXMSGS+4 bytes */
    char *cp;


    struct fbbpacket *msglst;
    struct mbx       *m;

    msglst = f->msglst;
    m      = f->m;

    msgcnt = 0;


    FBBdone   = FALSE;
    FirstTime = TRUE;

    checksum = 0;

    // Loop till we've received a F>, FF, FQ, ***, or 5 FB blocks.
    for(;;) {
        // If FirstTime = TRUE it means that we've (probably?) already gotten a
        // line from the remote system and don't need to ask for it
        // again.
        if(!FirstTime || m->line[0] == 0) {
            alarm(Mbtdiscinit*1000L);
            if(recvline (m->user, m->line, MBXLINE) == -1) {
#ifdef FBBVERBOSELOG
                log(m->user, RmtDisconnect);
#endif
                break;  // The connection has closed on us.
            }
            alarm(0);
        }
        FirstTime = FALSE;

        pwait(NULL);
        // Start off  with FBBok = FALSE.
        // If we get a FB or F> we'll reset it to TRUE and
        // continue on. If we get something else we'll exit.
        FBBok = FALSE;

        // strip any trailing NL characters.
        rip(m->line);

#ifdef notdef
        // lowercase command.
        cp = m->line;
        while(*cp){
           if(isupper(*cp))
              *cp = tolower(*cp);
           ++cp;
        }
#endif

        if(strnicmp(m->line,"ff",2) == 0) {
           // End of FBB Packet
           return 2;
        }

        if(strnicmp(m->line,"fq",2) == 0) {
           // End of FBB Packet
           return 3;
        }

        if(strnicmp(m->line,"f>",2) == 0) {
           if (strlen(m->line) > 3) { // Compare computed Checksum against supplied checksum.
              checksum += htoi(skipwhite(m->line+2)); /* skip to cksum char(s) */
              if (checksum & 0xFF) {  /* invalid sum */
                 log(m->user, "FBBFWD: %s Proposals checksum err (%x)", m->name, checksum&0xFF);
                 break;  /* FBBdone is FALSE, so we'll return code 0 => disconnect */
              }
           }
           // End of FBB Packet
           FBBdone = TRUE;
           break;
        }

        if((strnicmp (m->line, "fa", 2) == 0) || (strnicmp (m->line, "fb", 2) == 0)) {
           // Another message
           if(msgcnt >= FBBMAXMSGS) {
              // Too many in fact. We're only supposed to accept
              // 5 (FBBMAXMSGS) msgs.
              log(m->user, "FBBFWD: > 5 messages offered.");
              break;  /* we never accept more than 5 ! */
           } else {
              // add the data to the checksum count.
              cp = m->line;
              i=0;  /* count fields in FA/FB proposal */
              while (*cp) {
                 checksum += *cp & 0xff;
                 if (*cp == ' ') {
                     i++;  /* we expect single blank delimiters */
                     if (*(cp+1) == ' ')   /* 2 adjacent blanks are illegal */
                         i=-100; /* assure too-few-fields complaint */
                 }
                 cp++;
              }

              // Add a checksum count for the FBB style end-of-line marker.
              // It doesn't uses a CR\LF but uses a CR instead.
              checksum += '\r' & 0xff;
#ifdef FBBVERBOSELOG
              log(m->user,"MBOX %s: %s",m->name,m->line);  /* we don't log rec'd bulls for other-style BBSes */
#endif
              // Convert the FBB message string to it's ax.25 counterpart.
              if(i < 6) {  /* too few fields? */
                  FBBerror(11,m->user);
                  break;  /* with FBBdone at FALSE */
              }
              fbbparse(&msglst[msgcnt], m->line);
              msgsize += msglst[msgcnt].size;
              msgcnt++;
              FBBok = TRUE;
           }
        }

        // If we didn't reset the FBBok flag, something is wrong and we
        // need to exit.
        if(!FBBok) {
           break;
        }

    }

    // We've exited the FB....F> loop.
    // If FBBdone = TRUE we're ok... otherwise we received bad data.
    if((!FBBdone))	/* proposals error, etc. */
       return 0;

    // FB....F> data looks ok. Now process it and build our FS line.
    strcpy(FBBresp, "FS ");
    // Check each message in the list to see if we want it or not.
    for(i=0;i<msgcnt;i++) {
       pwait(NULL);
       // first check the msgid. If that works, run it through the
       // rewrite file.
       if(msgidcheck(msglst[i].messageid)) {
          // Nope... already have this one.
          msglst[i].accept = fbbNO;
       } else {
          // Tentativly accept this... now check the validity of the address.
          msglst[i].accept = fbbYES;
          pwait(NULL);
          free(msglst[i].rewrite_to);
          msglst[i].rewrite_to = NULLCHAR;
          if((cp = rewrite_address(msglst[i].to, REWRITE_TO)) != NULLCHAR) {
             // See if this is on the reject list.
             if(!strcmp(cp,"refuse")) {
                // oops.. rejected...
                msglst[i].accept = fbbNO;
                free(cp);
             } else {
                // keep track of new, rewritten address. This is passed
                // to the mboxmail dosend() code when we send the msg.
                msglst[i].rewrite_to = cp;
             }
          }

          // If we're still OK, make sure the address resolves ok.
          if(msglst[i].accept == fbbYES) {
             if(msglst[i].rewrite_to == NULLCHAR) {
                if(validate_address(msglst[i].to) == 0){
                   msglst[i].accept = fbbNO;
                }
             } else {
                if(validate_address(msglst[i].rewrite_to) == 0){
                   msglst[i].accept = fbbNO;
                   free(msglst[i].rewrite_to);
                   msglst[i].rewrite_to = NULLCHAR;
                }
             }
          }
       }

       // By now.. we know if we want the message or not.... update the
       // FBBResp string.
       //
       switch (msglst[i].accept) {
       case fbbUNKNOWN:
          log(m->user,"FBB Programming err: (%s) msglst.accept = Unknown", m->name);
          FBBresp[i+3] = '-';
          break;
       case fbbNO:
          FBBresp[i+3] = '-';
          break;
       case fbbYES:
          FBBresp[i+3] = '+';
          break;
       case fbbDEFER:
          // Currently, we don't defer but here it is anyway.
          FBBresp[i+3] = '=';
          break;
       } /* endswitch */
    }
    // Add a null to the end of the string.
    FBBresp[i+3] = '\0';

    // Now we can send it.
    tprintf("%s\n", FBBresp);
    usflush(m->user);
#ifdef FBBVERBOSELOG
    // Log FBB response only if really interested
    log(m->user,"our FBB response is %s",FBBresp);
#endif

    // The remote system will start sending the messages.
    // Loop through the message list.
    //
    for(i=0;i<msgcnt;i++) {
       if(NotConnected)
           // We've lost the connection.....
           break;
       if(msglst[i].accept == fbbYES)
#ifdef FBBCMP
           if (f->m->sid & MBX_FBBCMP) {
#ifdef FBBDEBUG2
               log(m->user,"FBBFWD: Processing new FBB Compressed Message.");
#endif
               // Receive the message and uncompress it.
               // Setup input and output file names.
               strcpy(f->iFile, tmpnam(NULL));
               strcpy(f->oFile, tmpnam(NULL));
               rc = recv_yapp(m->user, f, &m->subject, Mbtdiscinit*1000L);
               if(rc == 0) {
#ifdef FBBVERBOSELOG
                   log(m->user,"FBBFWD: recv_yapp() error.");
#endif
                   NotConnected = TRUE;
                   // erase the input and output file.
                   unlink(f->iFile);
                   unlink(f->oFile);
               } else {

                   // Process the message.
                   free(m->to);
                   free(m->tofrom);
                   free(m->tomsgid);
                   free(m->origto);
                   free(m->origbbs);
                   free(m->date);
//                   m->to      = \
//                   m->tofrom  = \
//                   m->tomsgid = \
                   m->origto  = \
                   m->origbbs = \
                   m->date    = NULLCHAR;
//                 // m->subject has the subject so don't free it.
//                 free(m->subject);
//                 m->subject = NULLCHAR;

                   if (msglst[i].rewrite_to != NULLCHAR) {
                       strlwr(m->to     = strdup(msglst[i].rewrite_to));
                       strlwr(m->origto = strdup(msglst[i].to));
                   } else {
                       strlwr(m->to     = strdup(msglst[i].to));
                   } /* end if */
                   strlwr(m->tomsgid = strdup(msglst[i].messageid));
                   strlwr(m->tofrom  = strdup(msglst[i].from));
/* n5knx: Bad idea:strlwr(m->subject); */
                   m->stype = msglst[i].type;

                   rc = dofbbacceptnote(m, f->oFile);
#ifdef FBBVERBOSELOG
                   if(rc != 1)
                       log(m->user, "FBBFWD: dofbbacceptnote() failed.");
#endif
                   // erase the input and output files.
                   unlink(f->iFile);
                   unlink(f->oFile);

                   free(m->to);
                   free(m->tofrom);
                   free(m->tomsgid);
                   free(m->origto);
                   free(m->origbbs);
                   free(m->subject);
                   free(m->date);
                   m->to      = \
                   m->tofrom  = \
                   m->tomsgid = \
                   m->origto  = \
                   m->origbbs = \
                   m->subject = \
                   m->date    = NULLCHAR;
#ifdef FBBDEBUG2
                   log(m->user,"FBBFWD: Finished w/ FBB Compressed Message.");
#endif
               }
           } else
#endif /* FBBCMP */
        {
          pwait(NULL);
          sprintf(m->line, "S%c %s < %s $%s_%s->%s",
                                                  msglst[i].type,
                                                  msglst[i].to,
                                                  msglst[i].from,
                                                  "fbbbid",
                                                  msglst[i].rewrite_to,
                                                  msglst[i].messageid);

          // Call mbx_parse to parse the send command.
          err=mbx_parse(m);
          // Close any open files left from an error.
          if (m->tfile) {
              fclose(m->tfile);
              m->tfile = NULLFILE;
          }
          if(m->tfp) {
              fclose(m->tfp);
              m->tfp   = NULLFILE;
          }
          if(err == -2) return(0);  /*N5KNX: error while processing msg, no choice but to disconnect */
       }
    }

#ifdef FBBDEBUG
    if(Mtrace)
        PUTS("FBBRECV: We've sent our data.");
#endif

    if(NotConnected)
        // We got disconneted... tell the caller.
        return 0;
    else
        return 1;
}



// This is where the forwarding starts.
/* dofbbsend return codes:
   0 => no data sent to remote system (time constraints, not in forward.bbs, etc)
   1 => All OK, msgs were sent to remote BBS
   2 => no data found to send to remote BBS
   3 => Error while sending data to remote BBS
*/
static
int dofbbsend(struct fwd *f)
{
   int    msgcnt;
   int    TMsgCnt;
   int    err = 0;
   int    i;
   int    rc;
   int    idx;
   int    MsgOk;

   int    FBBdone;
   int    FBBok;
   long   FirstRead=0L;
   long   idxloc;

   char   *cp,*dp;
   char   fn[FILE_PATH_SIZE];
   char   oldarea[64];

   char   *savefsline;

   struct indexhdr hdr;
   struct fwdbbs *bbs;

   // for makecl()
   int    bulletin = (f->m->areatype == AREA);
   char   line[80];

   struct fwdarealist   *fwdarea;
   struct fwdarealist   *curarea;
   struct fwdarealist   *newarea;

   struct fbbpacket *msglst;
   struct mbx       *m;

   msglst = f->msglst;
   m      = f->m;

   savefsline = NULLCHAR;

    // Check to see if we have any info to send and make sure it's the
    // right time to send data.
    if(fwdinit(f->m) == -1) {
       // The bbs is not in the forward.bbs file or it's the wrong time
       // to send.
       // Return a 0 to indicate that we have no data for the remote
       // system.
       return 0;
    } else {
       // It's ok to send data to the remote system.
       strcpy(oldarea,m->area);

       // Check to see if we've already read the list of areas to send
       // for this bbs. If f->FwdAreas == NULL, we haven't read the
       // list so we need to do so. If it's not NULL then we've already
       // read the list and we don't need to do it again.
       //
       // Since FBB calls this code for each block of 5 messages, we
       // want to reduce the number of times we read the foward.bbs
       // file to pull out the forward area info.

       if(!f->FwdAreas) {
          // Set curarea to NULL to prevent a warning message.
          curarea = NULL;

          while(!err && fgets(m->line,MBXLINE,m->tfile) != NULLCHAR) {
             pwait(NULL);
             // Skip over non-area lines.
             if(*m->line == '-')     /* end of record reached */
                break;
             cp = m->line;
             rip(cp);           /* adds extra null at end */
             /* skip spaces */
             while(*cp && (*cp == ' ' || *cp == '\t'))
                cp++;
/*             if(*cp == '\0' || *cp == '.' || *cp == '#' ||
                *cp == '+'  || *cp == '&' || *cp == '@')*/
               if(strchr(FWD_SCRIPT_CMDS,*cp)!=NULLCHAR)
                continue;       /* ignore empty or connect-script lines */

             /* find end of area name, and beginning of optional destination string */
             for (dp=cp; *dp && *dp != ' ' && *dp != '\t' && *dp != '\n'; dp++) ;
             if (*dp) *dp++ = '\0';   /* mark end of area name string */

             // Get memory for area info
             newarea = malloc(sizeof(struct fwdarealist));

             // Setup area info
             newarea->name        = strdup(cp);
             newarea->mindeferred = 0;
             newarea->maxdeferred = 0;
             newarea->next        = NULL;

             /* process optional destination field */
             cp=dp;  /* strip leading blanks */
             while(*cp && (*cp == ' ' || *cp == '\t'))
                 cp++;
             /* find end of optional destination */
             for (dp=cp; *dp && *dp != ' ' && *dp != '\t' && *dp != '\n'; dp++) ;
             if (*dp) *dp = '\0';
             newarea->opt_dest    = strdup(cp);

             // Insert area into list
             if(f->FwdAreas == NULL) {
                 f->FwdAreas = newarea;
             } else {
                 curarea->next = newarea;
             }
             curarea = newarea;
          }
       }
       // We're done with the forward.bbs file so we can close it.
       fclose(m->tfile);
       m->tfile = NULLFILE;
    }

    TMsgCnt = 0;
    FBBok  = TRUE;

    fwdarea = f->FwdAreas;
    while(fwdarea) {
        // Change to area.

        cp = fwdarea->name;
        changearea(f->m,cp);

        /* Now create the index filename */
        sprintf(fn,"%s/%s.ind",Mailspool,cp);

        /* Loop backwards through message list, stopping when we've found a
         * message that we've already processed.  Remember msgid's may not be
         * monotonic due to MC/MM command use.
         */

        for(i = m->nmsgs; i; i--) {
            if(!fwdarea->maxdeferred) {
               if(m->mbox[i].msgid == m->lastread) { /* can't use <= since MC/MM exist */
                  break;
               }
            } else {
               if(m->mbox[i].msgid == fwdarea->maxdeferred) {
                  break;
               }
            }
        }

        FBBdone = FALSE;

        if(Mtrace)
           PRINTF("Processing area %s. LastMsgidRead=%ld: begin at %d of %d.\n",
                  cp, m->lastread, i, m->nmsgs);

        if(i) FirstRead = m->mbox[i].msgid;

        // Check for new messages.
        if(i != m->nmsgs) {
           i++;
           /* open the index file */
           if((idx=open(fn,READBINARY)) != -1) {
               /* check if there are any messages in this area
                * that need to be forwarded.
                */
               if(read_header(idx,&hdr) != -1) {
                   // Position at correct index entry.
                   lseek(idx,m->mbox[i].indexoffset,SEEK_SET);

                   // Reset msgcnt;
                   msgcnt  = 0;
                   FBBdone = FALSE;

                   // i is set above.... points to next message to
                   // process.
                   for(; i<=m->nmsgs; i++) {
                       pwait(NULL);
                       MsgOk = TRUE;

                       if (read_index(idx,&f->ind) == -1) break;  

                       /* Check x-forwarded-to fields */
                       for(bbs=f->ind.bbslist;bbs;bbs=bbs->next) {
                          if(!stricmp(bbs->call,m->name)) {
                             MsgOk = FALSE;
                             break;
                          }
                       }

                       if(f->ind.status & BM_HOLD) {  /* treat as deferred msg */
                          MsgOk = FALSE;
                          if(fwdarea->maxdeferred < f->ind.msgid)
                             fwdarea->maxdeferred = f->ind.msgid;
                          if(fwdarea->mindeferred == 0)
                             fwdarea->mindeferred = f->ind.msgid;
                       }

                       // We want to send this message.
                       if(MsgOk) {
                          // Build FB line.
                          rc = makecl(f, i, fwdarea->opt_dest, line, NULL, &bulletin);
                          // If FB line ok, send it.
                          if(rc != -1) {
                             // Copy FB line for message log.
                             free(msglst[msgcnt].sline);
                             msglst[msgcnt].sline = strdup(line);

                             // Keep track of message number in area.
                             msglst[msgcnt].number = i;

                             // Keep track of makecl() modified bid.
                             strcpy(msglst[msgcnt].bid, f->bid);

                             // uppercase the FB line and send it.
                             strupr(line);
                             tprintf("%s", line);

                             fbbparse(&msglst[msgcnt], line);
                             msgcnt++;
                             TMsgCnt++;

                             // If we've filled our FB Block
                             if(msgcnt >= FBBMAXMSGS) {
                                // Send a end-of FB Block flag
                                tprintf("F>\n");

                                // Process an incoming FS and send any accepted messages.
                                idxloc = tell(idx); /* VK1ZAO: remember current position in index */
                                rc = fbbdofs(msglst, f, msgcnt, &fwdarea->mindeferred, &fwdarea->maxdeferred, idx);
                                free(savefsline);
                                savefsline = strdup(m->line);

                                // Reset counter.
                                msgcnt  = 0;

                                if(!rc) {
                                   FBBok = FALSE;
                                   FBBdone = TRUE;
                                } else
                                if(rc == 1) {
                                   FBBok = TRUE;
                                   FBBdone = FALSE;
#ifdef EXPIRY
                                   if (Eproc) FBBdone = TRUE;  /* quit early so expire can run */
                                   else  /* must restore index file position */
#endif
                                   lseek(idx,idxloc,SEEK_SET);  // VK1ZAO: Return to our location in index
                                } else
                                if(rc == 2) {
                                   FBBok = TRUE;
                                   FBBdone = TRUE;
                                }
                             }
                          }
                       }

                       /* Done with this index, clear it */
                       default_index(m->area,&f->ind);
                       m->newlastread = m->mbox[i].msgid;

                       scanmail(f->m);
                       pwait(NULL);

                       // If we got an error from dofs() or the response
                       // from dofs() was 2, we're done with this message
                       // transfer. If we got a 1 from dofs() we can send
                       // more messages now so we don't need to exit.
                       if(FBBdone)
                         break;
                       FBBok = TRUE;  /* n5knx: Init, in case for() terminates. */
                   } // end for()

                   /* Done with this index, clear it */
                   default_index(m->area,&f->ind);

                   // Finish off any FB messages.
                   if(msgcnt) {
                      // Send a end-of FB Block flag
                      tprintf("F>\n");
                      // Process an incoming FS and receive messages.
                      /* no need to save index file position as we will close(idx) shortly */
                      rc = fbbdofs(msglst, f, msgcnt, &fwdarea->mindeferred, &fwdarea->maxdeferred, idx);
                      free(savefsline);
                      savefsline = strdup(m->line);

                      // Reset counter.
                      msgcnt  = 0;

                      if(!rc) {
                         FBBok = FALSE;
                         FBBdone = TRUE;
                      } else
                      if(rc == 1) {
                         FBBok = TRUE;
                         FBBdone = FALSE;
#ifdef EXPIRY
                         if (Eproc) FBBdone = TRUE;  /* quit early so expire can run */
#endif
                      } else
                      if(rc == 2) {
                         FBBok = TRUE;
                         FBBdone = TRUE;
                      }
                   } // end if()
               } // end if()
           } // end if()
           close(idx);
           idx = 0;

           // If we got an error from dofs() or the response
           // from dofs() was 2, we're done with this message
           // transfer. If we got a 1 from dofs() we can send
           // more messages now so we don't need to exit.
           if(FBBdone) {
             break;
           }
        } // end if()

        // Done with the area. Close the *.txt file if it was open.
        if(m->mfile) {
            fclose(m->mfile);
            m->mfile = NULL;
        }

        if(fwdarea->mindeferred != 0) {
           m->lastread    = 0;
           m->newlastread = 0; /* we should use the msgid of the msg preceding fwdarea->mindeferred */
        }

        if(FBBdone)
           break;

        // Do next area.
        fwdarea = fwdarea->next;
    } // end while()

#ifdef FBB_OLD_SCANNER
    /* N5KNX: Play it safe, and always scan from first msg in an area.
       Enable this if optimized scanning has unforseen flaws */
    m->lastread = m->newlastread = 0;
#else
    // An error occured. Reset m->lastread so we'll process this area again.
    if(!FBBok) {
        m->lastread    = 0;
        m->newlastread = FirstRead;
    }
#endif

    // change back to original message area.
    if(*oldarea != '\0')
        changearea(f->m,oldarea);


    // set up the result from fbbdofs() call.
    if(savefsline) {
       strcpy(m->line, savefsline);
       free(savefsline);
    }

    if(!FBBok)
       // We had an error.
       return 3;

    if(TMsgCnt == 0) {
#ifdef FBBDEBUG
       if(Mtrace)
          PUTS("dofbbsend: No Data to send.");
#endif
       // We had no data.
       return 2;
    }

    return 1;
}


// This is the main entry point for FBB forwarding.
int
dofbbfwd(argc,argv,p)
int argc;
char *argv[];
void *p;
{
    struct fwd       f;
    struct mbx       *m;
    struct fbbpacket *msglst;

    int    i;
    int    Done;
    int    rc;
    int    FBBRdone;              // Receiving system has no more data.
    int    NeedData;
    int    FBBfwd;

    struct fwdarealist *fwdareas;
    struct fwdarealist *curareas;


    f.m = (struct mbx *)p;
    m   = f.m;

    memset(&f.ind,0,sizeof(struct mailindex));

    log(f.m->user,"MBOX FBB forwarding mail to: %s", f.m->name);

    // First verify that this is a FBB type system.
    if(!(f.m->sid & MBX_FBBFWD)) {
       tprintf("Huh?\n");
       usflush(m->user);
       return -1;
    }

#ifdef FBBCMP
    // Get data buffers for LZHUF transfers.
    if ((f.m->sid & MBX_FBBCMP) && !AllocDataBuffers(&f)) {
        log(f.m->user, "FBBFWD: insufficient memory for buffers");
        tputs("FQ\n");  /* for good measure, before we disconnect */
        usflush(m->user);
        return -1;
    }
#endif

    // Start with fresh copy of FwdAreas list
    f.FwdAreas = NULL;

    // Get memory for the msglst array.
    f.msglst = (struct fbbpacket *)callocw(FBBMAXMSGS,sizeof(struct fbbpacket));
    msglst = f.msglst;

    // Clear out the msglst array.
    for(i=0;i<FBBMAXMSGS;i++) {
       msglst[i].to        = NULLCHAR;
       msglst[i].from      = NULLCHAR;
       msglst[i].messageid = NULLCHAR;
       msglst[i].sline     = NULLCHAR;
       msglst[i].rewrite_to= NULLCHAR;
       msglst[i].size      = 0;
    }

    NeedData = FALSE;
    Done     = FALSE;
    FBBfwd   = FALSE;

    // See if we were called because we received an FA or FB from the remote system.
    if(argc == 0) {
       f.m->state = MBX_FORWARD; // We're in send mode.
       FBBRdone   = FALSE;
       NeedData   = TRUE;
       FBBfwd     = TRUE;
    }
    else
    if((strnicmp(argv[0],"FB",2) == 0) || (strnicmp (argv[0], "FA", 2) == 0)) {
       if(argc != 7) {
          tprintf("Huh?\n");
          usflush(m->user);
          Done     = TRUE;
       } else {
          sprintf(f.m->line,"FA %s %s %s %s %s %s",argv[1],  /* treat FB as FA ???*/
                                                   argv[2],
                                                   argv[3],
                                                   argv[4],
                                                   argv[5],
                                                   argv[6]);
          strupr(f.m->line);  /* mbx_parse forced lowercase, but we need uppercase for cksum */
          // indicate that we're in receive mode.
          f.m->state = MBX_REVFWD;
          FBBRdone = FALSE;
       }
    } else {
       // Must have received a FF from the remote system.
       f.m->state = MBX_FORWARD; // We're in send mode.
       FBBRdone = TRUE;
    }


    if(Mtrace)
       PUTS("FBB Forwarding started.");

    while(!Done) {
       pwait(NULL);
       if(f.m->state == MBX_REVFWD) {
#ifdef FBBDEBUG
          if(Mtrace)
             PUTS("FBBFWD: Receiving data from remote system.");
#endif

          if(NeedData) {
             // null out the line.
             f.m->line[0] = 0;
          }

          // Receive data from remote system.
          // Process FB....F> block.
          rc = dofbbrecv(&f);
          if(rc == 0) {
             // An error occured.
             Done = TRUE;
             if (*m->line)
                log(m->user,"FBB error with %s. Last read: %s",m->name, m->line);
          }
          else
             if(rc == 2)
                // Remote system sent us a FF
                FBBRdone = TRUE;
          else
             if(rc == 3) {
                // Remote system sent us a FQ so break; and disconnect.
                Done = TRUE;
             }

          // Change status.
          f.m->state  = MBX_FORWARD;
       } else {
#ifdef FBBDEBUG
          if(Mtrace)
             PUTS("FBBFWD: Sending data to remote system.");
#endif

          NeedData = FALSE;
          // Change status.
#ifdef EXPIRY
          FBBSendingCnt++;    /* lock out expire() */
#endif
          rc = dofbbsend(&f);
#ifdef EXPIRY
          if(--FBBSendingCnt == 0) psignal(&FBBSendingCnt, 1);
#endif /* EXPIRY */
          if(rc == 3)
             // An error occured.
             break;
          if((rc == 0) || (rc == 2)) {
             // We had no data for remote system.
             if(FBBRdone) {
                // They have no more data for us....
                // So we break out of this loop and send our FQ and disconnect.
#ifdef FBBDEBUG
                if(Mtrace)
                   PUTS("FBBSEND: No Data to send. No Data to receive.");
#endif
                break;
             } else {
                // Tell them that we don't have any data for them.
#ifdef FBBDEBUG
                if(Mtrace)
                   PUTS("FBBSEND: No Data to send. Sending FF");
#endif
                tprintf("FF\n");
                usflush(m->user);
                NeedData = TRUE;
             }
          }

          // Change status.
          f.m->state  = MBX_REVFWD;
       }
    } /* endwhile */

    // free anything in the msglst array.
    for(i=0;i<FBBMAXMSGS;i++) {
       free(msglst[i].to);
       free(msglst[i].rewrite_to);
       free(msglst[i].from);
       free(msglst[i].messageid);
       free(msglst[i].sline);
    }

    // Now free the msglst array.
    free(msglst);

    // Free our FwdAreas list
    fwdareas = f.FwdAreas;
    while(fwdareas) {
        free(fwdareas->name);
        free(fwdareas->opt_dest);
        curareas = fwdareas;
        fwdareas = fwdareas->next;
        free(curareas);
    }

#ifdef FBBCMP
    // Free our buffers.
    if (f.m->sid & MBX_FBBCMP)
        FreeDataBuffers(&f);
#endif

    // We're done. Send our FQ and we're out of here.
    tprintf("FQ\n");
    usflush(m->user);
#ifdef notdef   /* moved to exitbbs() */
    while(socklen(m->user,1) > 0)   /* wait for I frame to be ACKed - K5JB */
        pause(500L);  /* so as to get the FQ transmitted before we disconnect */
#endif
    if(FBBfwd)
       exitfwd(m);
    else
       return domboxbye(0,NULL,m);
    return 0;
}

#ifdef FBBCMP
/*****************************************************************************\
 * This code is called after a FBB type message has been decoded. It it based
 * on the dosend() code in mboxmail.c. This code reads the message from a file
 * instead of a user's socket so no user interaction is performed.
 *
 * return codes:   0 => fatal error, should disconnect
 *                 1 => all OK
 *
 *
 * It should be possible to use/modify this code ( in the future ) to process
 * FBB style import message files.
 *
 * Also for code simplicty and code reduction.... I'd like to see the dosend()
 * code call this to process a file received from a user.
 *
\*****************************************************************************/

extern int MbRecvd;
static int
dofbbacceptnote(struct mbx *m, char *infile)
{
    FILE   *fpInFile;
    char   *cp2;
    char   *host, *cp, fullfrom[MBXLINE], *rhdr = NULLCHAR;
    int    fail  = 0;
    struct list *cclist = NULLLIST;

#ifdef RLINE
    struct tm t;
#define  ODLEN   16
#define  OBLEN   32
    char tmpline[MBXLINE];
    char fwdbbs[NUMFWDBBS][FWDBBSLEN+1];
    int  myfwds  = 0;
    int  i;
    int  zulu=0;
    int  check_r = 0;
    int  found_r = 0;
    char origdate[ODLEN+1];
    char origbbs[OBLEN+1];
    int  loops   = 0;
    char Me[15];

    origdate[0]  = '\0';
    origbbs[0]   = '\0';
#endif

    /* Now check to make sure we can create the needed tempfiles - WG7J */
    if((m->tfile = tmpfile()) == NULLFILE)
        return 0;

    if ((fpInFile = fopen(infile, "r+b")) == NULLFILE) {
        log(m->user, "dofbbacceptnote: error %d opening %s", errno, infile);
        return 0;  /* exitbbs() will close m->tfile */
    }

#ifdef RLINE
    /* Only accept R: lines from bbs's */
    if((Rdate || Rreturn || Rfwdcheck || Mbloophold)){
        /* Going to interpret R:headers,
         * we need another tempfile !
         */
        if((m->tfp = tmpfile()) == NULLFILE) {
            /* disconnect to avoid the other bbs to think that we already have
             * the message !
             */
            fclose(fpInFile);
            return 0;    /* exitbbs() will close m->tfile */
        }
        /* Now we got enough :-) */
        check_r = 1;
        Checklock++;
        /* Set the call, used in loop detect code - WG7J */
        if(Mbloophold) {
           strncpy(Me,(Mbhaddress?Mbhaddress:Hostname),sizeof(Me));
           Me[sizeof(Me)-1] = '\0';

           if((cp = strchr(Me,'.')) != NULLCHAR)
              *cp = '\0'; /* use just the callsign */
        }
    }
#endif

#ifdef RLINE
    if(!check_r) {
#endif
       mbx_data(m,cclist,rhdr);
       /*Finish smtp headers*/
       fprintf(m->tfile,"\n");
#ifdef RLINE
    }
#endif

    m->state = MBX_DATA;
    while(fgets(m->line, MBXLINE, fpInFile)) {
       // Strip off any trailing EOL chars.
       rip(m->line);

       if(m->line[0] != CTLZ && stricmp(m->line, "/ex")) {
#ifdef RLINE
          if(check_r) {
             /* Check for R: lines to start with */
             if(!strncmp(m->line,"R:",2)) { /*found one*/
                found_r = 1;
                /*Write this line to the second tempfile
                 *for later rewriting to the real one
                 */
                fprintf(m->tfp,"%s\n",m->line);
                /* Find the '@[:]CALL.STATE.COUNTRY'or
                 * or the '?[:]CALL.STATE.COUNTRY' string
                 * The : is optional.
                 */
                if(((cp=strchr(m->line,'@')) != NULLCHAR) ||
                   ((cp=strchr(m->line,'?')) != NULLCHAR) ) {
                   if((cp2=strpbrk(cp," \t\n")) != NULLCHAR)
                      *cp2 = '\0';
                   /* Some bbs's send @bbs instead of @:bbs*/
                   if(*++cp == ':')
                      cp++;
                   /* if we use 'return addres'
                    * copy whole 'domain' name
                    */
                   if(Rreturn)
                      if(strlen(cp) <= OBLEN)
                         strcpy(origbbs,cp);
                   /* Optimize forwarding ? */
                   if(Rfwdcheck || Mbloophold) {
                      /*if there is a HADDRESS, cut off after '.'*/
                      if((cp2=strchr(cp,'.')) != NULLCHAR)
                         *cp2 = '\0';
                      if(Mbloophold)
                         /* check to see if this is my call ! */
                         if(!stricmp(Me,cp))
                            loops++;
                      /*cross-check with MyFwds list*/
                      if(Rfwdcheck) {
                         for(i=0;i<Numfwds;i++) {
                             if(!strcmp(MyFwds[i],cp)) {
                                /*Found one !*/
                                strcpy(fwdbbs[myfwds++],cp);
                                break;
                             }
                         }
                      }
                   }
                }
                if(Rdate) {
                   /* Find the 'R:yymmdd/hhmmz' string */
                   if((cp=strchr(m->line,' ')) != NULLCHAR) {
                      *cp = '\0';
                      if(strlen(m->line+2) <= ODLEN)
                         strcpy(origdate,m->line+2);
                   }
                }
             } else {
                /* The previous line was last R: line
                 * so we're done checking
                 * now write the smtp headers and
                 * all saved R: lines to the right tempfile
                 */
                check_r = 0;
                Checklock--;
                /*Did we actually find one ?*/
                if(found_r) {
                   if(Rreturn)
                      m->origbbs = strdup(strlwr(origbbs));
                   if(Rdate) {
                      if((cp=strchr(origdate,'/')) != NULLCHAR) {
                         if((*(cp+5) == 'z') || (*(cp+5) == 'Z')) {
                            *(cp+5) = '\0';
                            zulu = 1;
                         }
                         t.tm_min = atoi(cp+3);
                         *(cp+3) = '\0';
                         t.tm_hour = atoi(cp+1);
                         *cp = '\0';
                         t.tm_mday = atoi(&origdate[4]);
                         origdate[4] = '\0';
                         t.tm_mon = (atoi(&origdate[2]) - 1);
                         origdate[2] = '\0';
                         t.tm_year = atoi(origdate);
                         /* Set the date in rfc 822 format */
                         if((unsigned)t.tm_mon < 12) {  /* bullet-proofing */
                             m->date = mallocw(40);
                             sprintf(m->date,"%.2d %s %02d %02d:%02d:00 %.3s\n",
                                 t.tm_mday, Months[t.tm_mon], t.tm_year,
                                 t.tm_hour, t.tm_min, zulu ? "GMT" : "");
                         }
                      }
                   }
                }
                /* Now write the headers,
                 * possibly adding Xforwarded lines for bulletins,
                 * or anything that has a BID.
                 * Add the X-Forwarded lines and loop detect
                 * headers FIRST,
                 * this speeds up forwarding...
                 */
                if(Mbloophold && loops >= Mbloophold)
                    fprintf(m->tfile,"%sLoop\n",Hdrs[XBBSHOLD]);
                if(Rfwdcheck && found_r && ((m->stype == 'B') || (m->tomsgid)) ){
                   /*write Xforwarded headers*/
                   for(i=0;i<myfwds;i++) {
                       fprintf(m->tfile,"%s%s\n",Hdrs[XFORWARD],fwdbbs[i]);
                   }
                }
                /*write regular headers*/
                mbx_data(m,cclist,rhdr);
                /* Finish smtp headers */
                fprintf(m->tfile,"\n");

                /* Now copy the R: lines back */
                if(found_r) {
                   rewind(m->tfp);
                   while(fgets(tmpline,sizeof(tmpline),m->tfp)!=NULLCHAR)
                      fputs(tmpline,m->tfile);
                }
                MYFCLOSE(m->tfp);

                /* And add this first non-R: line */
                fprintf(m->tfile,"%s\n",m->line);
                if(m->line[strlen(m->line)-1] == CTLZ)
                   goto eol_ctlz;
             }
          } else
#endif
             fprintf(m->tfile,"%s\n",m->line);
          if(m->line[strlen(m->line)-1] == CTLZ)
              goto eol_ctlz;
       } else {
eol_ctlz:
#ifdef RLINE
          if(check_r) {
             /* Hmm, this means we never finished the R: headers
              * tmp file still open !
              */
             MYFCLOSE(m->tfp);
          }
#endif
       }
    } // End While

    fclose(fpInFile);
    free(rhdr);

    if((host = strrchr(m->to,'@')) == NULLCHAR) {
       host = Hostname;        /* use our hostname */
       if(m->origto != NULLCHAR) {
          /* rewrite_address() will be called again by our
           * SMTP server, so revert to the original address.
           */
          free(m->to);
          m->to = m->origto;
          m->origto = NULLCHAR;
       }
    }
    else
       host++; /* use the host part of address */

    /* make up full from name for work file */
    if(m->tofrom != NULLCHAR)
        sprintf(fullfrom,"%s%%%s@%s",m->tofrom, (m->origbbs!=NULLCHAR)?m->origbbs:m->name, Hostname);
    else
       sprintf(fullfrom,"%s@%s",m->name,Hostname);
    if(cclist != NULLLIST && stricmp(host,Hostname) != 0) {
       fseek(m->tfile,0L,0);   /* reset to beginning */
       fail = queuejob(m->tfile,Hostname,cclist,fullfrom);
       del_list(cclist);
       cclist = NULLLIST;
    }
    addlist(&cclist,m->to,0,NULLCHAR);
    fseek(m->tfile,0L,0);
    fail += queuejob(m->tfile,host,cclist,fullfrom);
    del_list(cclist);
    MYFCLOSE(m->tfile);
    MbRecvd++;
/* dosend() invoked smtptick() [with a slight delay] at this point, to start
   processing the msgs we just added to the queue.  We could do likewise, or
   invoke smtptick() from exitbbs().  The competing concepts are: 1) speed up
   handling of the queue, so the bids get written to the history file, and
   2) minimize memory and cpu demand by delaying the smtp processing until we
   are finished.
*/

    return 1;
}
#endif /* FBBCMP */
#endif // FBBFWD
