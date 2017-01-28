/* Version control information for "info" command display */
/* Definition for processor type added */
/* Cleanup of information display - K2MF 4/93 */

#include "global.h"
#include "mbuf.h"
#include "socket.h"
#include "commands.h"   /* prototype for doinfo() */
#if (defined(AX25) && defined(AXIP))
#include "ax25.h"	/* bring in NAX25 definition */
#endif

#define NOS_VERSION "eznos"	/* change just this for a new version */

char shortversion[] = "NOS" NOS_VERSION ;

#if (defined(MAILBOX) || defined(MBFWD))
#if defined(FBBCMP)
char MboxIdFC[]  = "[NOS-" NOS_VERSION "-BFHIM$]\n";
#endif
#if defined(FBBFWD)
char MboxIdF[]  = "[NOS-" NOS_VERSION "-FHIM$]\n";
#endif /* FBBFWD */
char MboxId[]   = "[NOS-" NOS_VERSION "-IHM$]\n";
#endif /* MAILBOX || MBFWD */

char Version2[] = "by Johan. K. Reinalda, WG7J/PA3DIS\n"
                  "and James P. Dugal, N5KNX\n"
				  "Compiled by DOS Solutions,\n"
				  "As EZNOS, DOS Ethernet FTP/HTTP Server\n"

#ifdef UNIX
       "Linux/Unix/POSIX modifications by Brandon S. Allbery, KF8NH\n"
#endif
        "";

extern int Numrows,Numcols;

char Version[] = NOS_VERSION " ("
#if defined UNIX
#if defined(linux)
    "Linux"
#elif defined(sun)
    "Solaris"
#if defined(__sparc)
    " SPARC"
#endif
#else
    "Unix"
#endif
#elif defined CPU86
    "8088"
#elif defined CPU186
    "80186"
#elif defined CPU286
    "80286"
#elif defined CPU386
    "80386"
#elif defined CPU486
    "80486"
#elif defined CPU586
    "PENTIUM"
#else
    "cpu unknown"
#endif
    ")";

#ifdef ALLCMD
static char *
compilerver(void)
{
	static char result[24];

#if defined(__GNUC__)
#if defined(__GNUC_MINOR__)
	sprintf(result, "GCC %d.%d", __GNUC__, __GNUC_MINOR__);
#else
	sprintf(result, "GCC %d", __GNUC__);
#endif
#elif defined(__BORLANDC__)
	sprintf(result, "BC %04X", __BORLANDC__);
#elif defined(__TURBOC__)
	sprintf(result, "TC %04X", __TURBOC__);
#else
	strcpy(result,"unknown compiler");
#endif
	return(result);
}

int
doinfo(argc,argv,p)
int argc;
char *argv[];
void *p;
{
#if defined(UNIX) && defined(SM_CURSES)
    extern char curseslibver[];
#endif

	tprintf("NOS %s, compiled %s %s by %s",
             Version,__DATE__,__TIME__,compilerver());
#ifdef WHOFOR
    tprintf("\nfor %s,", WHOFOR);
#endif
    tputs(" containing:\n\n");

#ifndef SERVERS
    tputs("TCP Servers:  None !\n");
#else

    tputs("TCP Servers:"
#ifdef SMTPSERVER
    " SMTP"
#ifdef SMTP_VALIDATE_LOCAL_USERS
    "(validated)"
#endif
#ifdef SMTP_DENY_RELAY
    "(filtered)"
#endif
#ifdef TRANSLATEFROM
    "(translated)"
#endif
#ifdef SMTP_REFILE
    "(refiled)"
#endif
#endif /* SMTPSERVER */

#ifdef FINGERSERVER
    " FINGER"
#endif
#ifdef FTPSERVER
    " FTP"
#endif
#ifdef TCPGATE
    " TCPGATE"
#endif
    "\n");

#if (defined(TELNETSERVER) || defined(RSYSOPSERVER) || defined(TRACESERVER) || defined(TTYLINKSERVER) || defined(DISCARDSERVER) || defined(ECHOSERVER) )
    tputs("            "

#ifdef TELNETSERVER
    " TELNET"
#endif
#ifdef RSYSOPSERVER
    " RSYSOP"
#endif
#ifdef TRACESERVER
    " TRACE"
#endif
#ifdef TTYLINKSERVER
    " TTYLINK"
#endif
#ifdef DISCARDSERVER
    " DISCARD"
#endif
#ifdef ECHOSERVER
    " ECHO"
#endif
    "\n");
#endif /* TELNETSERVER || RSYSOPSERVER || TRACESERVER || TTYLINKSERVER || DISCARDSERVER || ECHOSERVER */

#if (defined(CALLSERVER) || defined(BUCKTSR) || defined(SAMCALLB) || defined(QRZCALLB) || defined(CONVERS) || defined(NNTPS) || defined(POP2SERVER) || defined(POP3SERVER) || defined(RDATESERVER) || defined(IDENTSERVER) || defined(TERMSERVER) || defined(HTTP))
    tputs("            "

#ifdef CALLSERVER
    " CALLBOOK (CD-ROM)"
#endif

#ifdef BUCKTSR
    " CALLBOOK (BUCKTSR)"
#endif /* BUCKTSR */

#ifdef SAMCALLB
    "  CALLBOOK (SAM)"
#endif /* SAMCALLB */

#ifdef QRZCALLB
    " CALLBOOK (QRZ)"
#endif /* QRZCALLB */

#ifdef CONVERS
    " CONVERS"
#endif

#ifdef NNTPS
    " NNTP"
#endif

#ifdef POP2SERVER
    " POP2"
#endif

#ifdef POP3SERVER
    " POP3"
#endif

#ifdef HTTP
    " HTTP"
#endif

#ifdef RDATESERVER
    " TIME"
#endif

#ifdef TERMSERVER
    " TERM"
#endif

#ifdef IDENTSERVER
    " IDENT"
#endif

    "\n");
#endif /* CALLSERVER || CONVERS || NNTPS || POP2SERVER || POP3SERVER || RDATESERVER || TERMSERVER etc */

#endif /* SERVERS */

    tputs("TCP Clients: SMTP"

#ifdef FINGERSESSION
    " FINGER"
#endif

#ifdef FTPSESSION
    " FTP"
#endif

#ifdef TELNETSESSION
    " TELNET"
#endif

#ifdef TTYLINKSESSION
    " TTYLINK"
#endif

    "\n");

#if (defined(CALLCLI) || defined(CONVERS) || defined(NNTP) || defined(NNTPS) || defined(POP2CLIENT) || defined(POP3CLIENT) || defined(RLOGINCLI) || defined(RDATECLI) || defined(LOOKSESSION))
    tputs ("            "

#ifdef CALLCLI
    " CALLBOOK (CD-ROM)"
#endif

#ifdef CONVERS
    " CONVERS"
#endif

#if (defined(NNTP) || defined(NNTPS))
    " NNTP"
#ifdef NN_USESTAT
    "(stat)"
#endif
#if defined(NNTPS) && defined(NEWS_TO_MAIL)
    "(2mail)"
#endif
#endif

#ifdef POP2CLIENT
    " POP2"
#endif

#ifdef POP3CLIENT
    " POP3"
#endif

#ifdef RLOGINCLI
    " RLOGIN"
#endif

#ifdef RDATECLI
    " TIME"
#endif

#ifdef LOOKSESSION
    " LOOK"
#endif

    "\n");
#endif /* CALLCLI || CONVERS || NNTP || NNTPS || POP2CLIENT || POP3CLIENT || RLOGINCLI || RDATECLI || LOOKSESSION */

#ifdef LZW
    tputs("    with LZW compression for TCP sockets\n");
#endif /* LZW */

#if (defined(TCPACCESS) || defined(IPACCESS))
    tputs("    with "

#ifdef TCPACCESS
    "TCP"
#endif

#if (defined(TCPACCESS) && defined(IPACCESS))
    "/"
#endif

#ifdef IPACCESS
    "IP"
#endif

    " access controls\n");
#endif /* TCPACCESS || IPACCESS */


#if (defined(DOMAINSERVER) || defined(REMOTESERVER) || defined(BOOTPSERVER))
    tputs("UDP Servers:"

#ifdef DOMAINSERVER
    " DOMAIN-NAMESERVER"
#endif /* DOMAINSERVER */

#ifdef REMOTESERVER
    " REMOTE"
#if defined(UDP_DYNIPROUTE) && defined(ENCAP)
    "(with DYNIP)"
#endif
#endif /* REMOTESERVER */

#ifdef BOOTPSERVER
    " BOOTP"
#endif

    "\n");
#endif /* DOMAINSERVER || REMOTESERVER || BOOTPSERVER */

#if (defined(REMOTECLI) || defined(BOOTPCLIENT))
    tputs("UDP Clients:"

#ifdef REMOTECLI
    " REMOTE"
#endif

#ifdef BOOTPCLIENT
    " BOOTP"
#endif

    "\n");
#endif /* UDP Clients */

#ifdef MAILBOX

#ifdef TIPSERVER
	tputs("TIP ");
#endif

    tputs("Mailbox Server"

#if defined(MBOX_DYNIPROUTE) && defined(ENCAP) && defined(TELNETSERVER)
    "(with DYNIP)"
#endif
#ifdef XMODEM
    " with Xmodem file transfer"
#endif /* XMODEM */

    "\n");

#ifdef MAILCMDS
    tputs("Full Service BBS"

#if (defined(EXPIRY) || defined(MAILFOR) || defined(MBFWD) || defined(RLINE))
    " with:"
#endif

#ifdef EXPIRY
    "\n     Message and BID expiry"
#endif

#ifdef MAILFOR
    "\n     'Mail For' beaconing"
#endif

#ifdef MBFWD
    "\n     AX.25 mail forwarding"
#ifdef FBBFWD
    "\n           with FBB Style"
#ifdef FBBCMP
    " Compressed"
#endif
    " Forwarding"
#endif /* FBBFWD */
#endif /* MBFWD */

#ifdef RLINE
    "\n     BBS 'R:-line' compatibility"
#endif /* RLINE */

    "\n");
#endif /* MAILCMDS */

#endif /* MAILBOX */

#if (defined(AXIP) || defined(ENCAP))
    tputs("Internet Services:"

#ifdef AXIP
    "  AX.25 Digipeating");
    tprintf(" (%d ports)", NAX25);
    tputs(
#endif /* AXIP */

#ifdef ENCAP
    "  IP Encapsulation"
#endif /* ENCAP */

    "\n");
#endif /* AXIP || ENCAP */

#ifdef HOPCHECKSESSION
	tputs("Hopcheck IP path tracing\n");
#endif /* HOPCHECKSESSION */

#ifdef RIP
    tputs("RIP-2"
#ifdef RIP98
          "/-98"
#endif
                 " Routing Protocol\n");
#endif /* RIP */

#ifdef RSPF
	tputs("Radio Shortest Path First Protocol (RSPF)\n");
#endif /* RSPF */

#ifdef RARP
	tputs("Reverse Address Resolution Protocol (RARP)\n");
#endif /* RARP */

#ifdef MD5AUTHENTICATE
        tputs("MD5 Authentication\n");
#endif

#ifdef MSDOS
    tprintf("%d interrupt buffers of %d bytes\n",Nibufs,Ibufsize);
#endif

#ifdef ASY
#ifdef UNIX
    tputs("Generic termios interface driver\n");
#else
    tputs("Generic async (8250/16450/16550) interface driver\n");
#endif

#if (defined(KISS) || defined(AX25) || defined(NRS))
    tputs("Async interface drivers:"

#ifdef KISS
    "  KISS-TNC"
#endif /* KISS */

#ifdef POLLEDKISS
    "  POLLED-KISS"
#endif /* POLLEDKISS */

#ifdef AX25
    "  AX.25"
#endif /* AX25 */

#ifdef NRS
    "  NET/ROM-TNC"
#endif /* NRS */

    "\n");
#endif /* KISS || AX25 || NRS */

#endif /* ASY */

#ifdef BPQ
    tputs("Bpq Host driver\n");
#endif

#ifdef NETROM
	tputs("NET/ROM network interface\n");
#endif /* NETROM */

#if (defined(PPP) || defined(SLIP))
    tputs("Async IP drivers:"

#ifdef PPP
    "  Point-to-Point (PPP)"
#endif /* PPP */

#ifdef SLIP
    "  Serial Line (SLIP)"
#endif /* SLIP */

    "\n");

#ifdef DIALER
    tputs("      with modem dialer for SLIP/PPP\n");
#endif /* DIALER */

#ifdef VJCOMPRESS
    tputs("      with Van Jacobson compression for PPP/SLIP\n");
#endif /* VJCOMPRESS */

#endif /* PPP || SLIP */

#ifdef PACKET
	tputs("FTP Software's PACKET driver interface\n");
#endif /* PACKET */

#ifdef APPLETALK
	tputs("Appletalk interface for MacIntosh\n");
#endif /* APPLETALK */

#ifdef ARCNET
	tputs("ARCnet via PACKET driver\n");
#endif /* ARCNET */

#ifdef DRSI
	tputs("DRSI PCPA low-speed driver\n");
#endif /* DRSI */

#ifdef EAGLE
    tputs("Eagle card 8530 driver\n");
#endif /* EAGLE */

#ifdef ETHER
    tputs("Generic ethernet driver\n");
#endif /* ETHER */

#ifdef HAPN
	tputs("Hamilton Area Packet Network driver\n");
#endif /* HAPN */

#ifdef HS
	tputs("High speed (56 kbps) modem driver\n");
#endif /* HS */

#ifdef PACKETWIN
	tputs("Gracilis PackeTwin driver\n");
#endif /* PACKETWIN */

#ifdef PC_EC
	tputs("3-Com 3C501 Ethernet controller driver\n");
#endif /* PC_EC */

#ifdef PC100
	tputs("PAC-COM PC-100 driver\n");
#endif /* PC100 */

#ifdef PI
	tputs("PI SCC card with DMA driver (VE3IFB)\n");
#endif /* PI */

#ifdef SCC
    tputs("Generic SCC (8530) driver (PE1CHL)\n");
#endif /* SCC */

#ifdef SLFP
	tputs("SLFP via PACKET driver\n");
#endif /* SLFP */
    
#ifdef TRACE
	tputs("Hardware interface packet tracing"
#ifdef MONITOR
	" (minimal monitor-style trace available)"
#endif
	"\n");
#endif /* TRACE */


#ifdef PRINTEROK
        tputs("Parallel printer\n");
#endif

#ifdef MULTITASK
/*      tputs("The Russell Nelson modsets\n"); */
	tputs("Multitasking capability when shelling out to MS-DOS\n");
#endif /* MULTITASK */

#if defined(UNIX) && defined(SM_CURSES)
    tprintf("Linked with (n)curses version %s\n", curseslibver);
#endif

#ifdef AXUISESSION
    tputs("AX.25 UI packet tx/rx\n");
#endif

#ifdef EDITOR
    tputs(
#ifdef ED
          "Ed"
#endif
#ifdef TED
	  "Ted"
#endif
          " ASCII text editor\n");
#endif

#if (defined(STKTRACE) || defined(SWATCH) || defined(MEMLOG) || defined(SHOWFH) || defined(PS_TRACEBACK)) && defined(MSDOS) || defined(CHKSTK)
    tputs("Debug features:"
#ifdef MSDOS
#ifdef STKTRACE
    " STKTRACE"
#endif
#ifdef SWATCH
    " SWATCH"
#endif
#ifdef MEMLOG
    " MEMLOG"
#endif
#ifdef SHOWFH
    " SHOWFH"
#endif
#ifdef PS_TRACEBACK
    " TRACEBACK"
#endif
#endif /* MSDOS */

#ifdef CHKSTK
    " CHKSTK"
#endif

    "\n");
#endif

	return 0;
}

#endif /* ALLCMD */


