/* HTTP server
 * A nearly complete rewrite, extensively expanded, based loosely on
 * the HTTP server of Selcuk Ozturk <seost2+@pitt.edu> &
 * <ashok@biochemistry.cwru.edu>, which were based on
 * Chris McNeil's <cmcneil@mta.ca> Gopher server, which was based in
 * part on fingerd.c
 *
 */

#include "global.h"
#include "ctype.h"
#include "commands.h"
#ifdef MSDOS
#include "hardware.h"
#else
#include <time.h>
#include "mbuf.h"
#include "socket.h"
#include "session.h"
#endif
#if defined(HTTP) || defined(BROWSER)
#include <sys/stat.h>
#include "files.h"
#include "mailbox.h"
#include "smtp.h"
#include "browser.h"
#include "usock.h"
#ifdef HTTPPBBS
#include "domain.h"
#include "bm.h"
#endif
#endif
#ifdef STAT_HTTP
#include "stats.h"
#endif

#ifdef HTTP


#if !defined(_lint)
static char rcsid[] OPTIONAL = "$Id: http.c,v 1.35 1997/09/07 00:31:16 root Exp root $";
#endif

#ifdef NO_SETENV
char *setenv (char const *, char const *, int);
#endif

#define DEFAULT_ERROR_MSG "[an error occurred while processing this directive]"
#define HTTP_TIME_FORMAT "%a, %d %b %Y %T GMT\n"
#define DEFAULT_TIME_FORMAT "%A, %d-%b-%y %T %Z"
#define SIZEFMT_BYTES 0
#define SIZEFMT_KMG 1
#define SECS_IN_6MONTHS 15768000 /* seconds in 1/2 year */

static char entry[] = "<IMG SRC=\"/icons/%s.xbm\" ALT=\"%s\"> ";
static char entry1[] = "<A HREF=\"/%s%s%s\">%-s</a>";
static char entry2[] = "%2d-%s-%02d %02d:%02d %6dK\n";
int HttpUsers = 0;
static int Httpmax = 10;
static int Httpsimc = 5;
static int HttpSessions = 0;
static int Httptdisc = 180;
static int HTTPreferer = 1;
static int HTTPagent = 1;
static int HTTPcookie = 0;
static int HTTPusecookies = 0;
static int HTTPmisc = 1;
static int HTTPpost = 1;
static int HTTPexeccmd = 0;
static int HTTPstatusURL = 1;
#ifdef HTTPCGI
static int HTTPexeccgi = 0;
#endif
#ifdef STRICT_HTTPCALL
static int HTTPstrict = 0;
#endif
#ifdef HTTPPBBS
static int HTTPexecbbs = 0;
static int HTTPanonsend = 0;
extern int32 MbAmprPerms;
extern int32 MbNonAmprPerms;
extern int32 MbHttpPerms;
extern uint32 NonSecureAmpr;
#endif
extern time_t StartTime;
#if 0
static int HTTPscript = 0;
#endif
static FILE * HTTPsavedfp;
static char *HTTPHostname;
static char *HTTPFtpdir = NULLCHAR;

#ifdef MSDOS
#define HTMLEXT	"htm"
#else
#define HTMLEXT "html"
#endif

struct reqInfo {
	int		port;
	int		index;
#if 0
	int		response;
#endif
	int		qsize;		/* not used at the present */
	int		sizefmt;
	int		isvolatile;
#ifdef HTTPPBBS
	int		msgcount;
	int		newmsgcount;
	int		msgnum;
	int		msgcurrent;
	int		anony;
	int		sock;
	long		privs;
#endif
	char		*hostname;
	char		*url;
	char		*method;
	char		*version;
	char		*newcheck;
	char		*from;
	char		*referer;
	char		*agent;
	char		*passwd;
	char		*dirname;
	char		*ftpdirname;
	char 		*usetime;
	char		*useerror;
	char		*realm;
	char		*cookie;
	char		*rmtaddr;
	char		*rmthost;
	char		buf[PLINELEN + 16];
#ifdef HTTPPBBS
	char		*areaname;
	char		*username;
	struct mbx 	*m;
	char		*bbspath;
#endif
};


struct secureURL	{
	char		 *url;
	char		 *pwfile;
	char		 *realm;
	struct secureURL *next;
};
#define NULLSECUREURL ((struct secureURL *)0)

static struct secureURL *securedURLS = NULLSECUREURL;


/* function prototypes */
#ifdef HTTPCGI
static void setup_cgi_variables (struct reqInfo *rq, char *name);
#endif
static int dohttpagent (int argc,char *argv[],void *p);
static int dohttpexeccmd (int argc,char *argv[],void *p);
static int dohttpstatusurl (int argc,char *argv[],void *p);
static int dohttphostname (int argc,char *argv[],void *p);
#ifdef HTTPCGI
static int dohttpexeccgi (int argc,char *argv[],void *p);
#endif
#ifdef HTTPPBBS
static int dohttpexecbbs (int argc,char *argv[],void *p);
static int checkareaperms (struct reqInfo *rq);
#endif
static int dohttpmax (int argc,char *argv[],void *p);
static int dohttpmisc (int argc,char *argv[],void *p);
static int dohttppost (int argc,char *argv[],void *p);
static int dohttpreferer (int argc,char *argv[],void *p);
static int dohttpcookielog (int argc,char *argv[],void *p);
static int dohttpcookies (int argc,char *argv[],void *p);
static int dohttpsecure (int argc,char *argv[],void *p);
#if 0
static int dohttpscript (int argc,char *argv[],void *p);
#endif
static int dohttpsim (int argc,char *argv[],void *p);
static int dohttpstatus (int argc,char *argv[],void *p);
static int dohttptdisc (int argc,char *argv[],void *p);
static int dohttpftpdir (int argc,char *argv[],void *p);

#ifdef STRICT_HTTPCALL
static int dohttpstrict (int argc,char *argv[],void *p);
#endif

static char *assigncookie (struct reqInfo *rq, time_t thetime);
static void httpserv (int s,void *unused,void *p);
static int mkwelcome (const char *Hdir,char *file, int ftpfile, const char *Ftpdir);
static char * decode (char *string);
static void httpHeaders (int type, const char *str, struct reqInfo *rq, int more, const char *realm);
static void httpFileinfo (FILE *fp, struct reqInfo *rq, int thetype, char *vers);
static void openHTTPlog (const char *name);
static void closeHTTPlog (void);
static int getmonth (char *cp);
static int isnewer (time_t thetime,char *tmstr);
static long countusage (const char *file, const char *basedir, int display, int increase);
static void sendhtml (FILE *fp, int firstfile, struct reqInfo *rq);
static void file_or_virtual (char *cp, char *newname, struct reqInfo *rq);
static void send_size (int sizefmt,long size);
static char *ht_time (time_t t,const char *fmt,int gmt);
static char *gmt_ptime (time_t *thetime);
static int addFile (const char *fname, int s);
static struct secureURL *secured_url (struct reqInfo *rq);
static int is_authorized (struct reqInfo *rq);
static int http_authcheck (char *filename, char *username, char *pass);

#ifdef HTTPPBBS
static void addUserFile (struct reqInfo *rq);
static void addMsgFile (char *file, struct reqInfo *rq);
static void doPBBScmd (struct reqInfo *rq, const char *cmdstring);
static void doPBBScmd_init (struct reqInfo *rq);
static void doPBBScmd_term (struct reqInfo *rq);
static void doPBBScmd_exit (struct reqInfo *rq);
static int dohttpanonsend (int argc,char *argv[],void *p);
#endif

extern char *base64ToStr (char *b64);
#ifdef ALLSERV
extern char *getquote (void);
#endif

/* HTTP Header lines sent or processed */
#define HDR_TYPE		0
#define HDR_LENGTH		1
#define HDR_MIME		2
#define HDR_SERVER		3
#define HDR_LOCATION		4
#define HDR_DATE		5
#define HDR_MODIFIED		6
#define HDR_SINCE		7
#define HDR_REFERER		8
#define HDR_AGENT		9
#define HDR_FROM		10
#define HDR_EXPIRES		11
#define HDR_AUTHORIZE		12
#define HDR_AUTHENTICATE	13
#define HDR_COOKIE		14

static const char *HTTPHdrs[] = {
	"Content-Type:",
	"Content-Length:",
	"MIME-version:",
	"Server:",
	"Location:",
	"Date:",
	"Last-Modified:",
	"If-Modified-Since:",
	"Referer:",
	"User-Agent:",
	"From:",
	"Expires:",
	"Authorization:",
	"WWW-Authenticate:",
	"Cookie:"
};


/* First component of MIME types */

static const char *CTypes1[] = {
	"video/",
	"text/",
	"image/",
	"audio/",
	"application/"
};


/* Struct for looking up MIME types from file extensions */
struct FileTypes {
	const char *ext;
	short type1;
	const char *type2;
};


static struct FileTypes HTTPtypes[] = {
	/* Other 'special' entries can be added in front of 'html',
	 * but care must be taken to make sure that the defines for
	 * HTTPbinary, HTTPplain, HTTPform, HTTPhtml and HTTPhtm are correct
	 * and that no entries with a 'NULLCHAR' ext come after 'html'
	 * except for the one that ends the table.
	 */
#define HTTPbinary 0
	{ NULLCHAR,	4,	"octet-stream" },
#define HTTPplain 1
	{ NULLCHAR,	1,	"plain" },
#ifndef _lint
#define HTTPform 2
#endif
	{ NULLCHAR,	4,	"x-www-form-urlencoded" },
#define HTTPhtml 3
	{ "html",	1,	"html" },
#define HTTPhtm 4
	{ "htm",	1,	"html" },
	/* the following can be in any order */
	{ "avi",	0,	"x-msvideo" },
	{ "qt",		0,	"quicktime" },
	{ "mov",	0,	"quicktime" },
	{ "mpeg",	0,	"mpeg" },
	{ "mpg",	0,	"mpeg" },
	{ "rtx",	1,	"richtext" },
	{ "xpm",	2,	"x-xpixmap" },
	{ "xbm",	2,	"x-xbitmap" },
	{ "rgb",	2,	"x-rgb" },
	{ "ppm",	2,	"x-portable-pixmap" },
	{ "pgm",	2,	"x-portable-graymap" },
	{ "pbm",	2,	"x-portable-bitmap" },
	{ "pnm",	2,	"x-portable-anymap" },
	{ "ras",	2,	"x-cmu-raster" },
	{ "tiff",	2,	"tiff" },
	{ "tif",	2,	"tiff" },
	{ "jpeg",	2,	"jpeg" },
	{ "jpg",	2,	"jpeg" },
	{ "gif",	2,	"gif" },
	{ "wav",	3,	"x-wav" },
	{ "aiff",	3,	"x-aiff" },
	{ "aif",	3,	"x-aiff" },
	{ "au",		3,	"basic" },
	{ "snd",	3,	"basic" },
	{ "tar",	4,	"x-tar" },
	{ "man",	4,	"x-trof-man" },
	{ "rtf",	4,	"rtf" },
	{ "eps",	4,	"postscript" },
	{ "ps",		4,	"postscript" },
	{ "sit",	4,	"x-stuffit" },
	{ "hqx",	4,	"mac-binhex40" },
	{ "fif",	4,	"fractals" },
	{ "zip",	4,	"x-zip" },
	{ "gz",		4,	"x-gzip" },
	{ "z",		4,	"x-compress" },
	{ NULLCHAR,	-1,	NULLCHAR }
};


/* Struct to keep tract of ports defined and their root directory */

#define MAXHTTPPORTS 10
struct portsused {
	int	port;
	int	sock;
	uint32	address;
	char	*dirname;
	char	*ftpdirname;
	char	*hostname;
	long	requests;
	long	homehits;
	long	stathits;
#ifdef HTTPPBBS
	long	bbshits;
#endif
};
static struct portsused ports[MAXHTTPPORTS];


#ifdef UNIX
#define OS "Unix"
#define WILDCARD "*"
#else
#define OS "MS-DOS"
#define WILDCARD "*.*"
#endif



static struct cmds Httpcmds[] = {
	{ "agentlogging",	dohttpagent,	0, 0, NULLCHAR },
#ifdef HTTPPBBS
	{ "anonsend",		dohttpanonsend,	0, 0, NULLCHAR },
#endif
	{ "cookielogging",	dohttpcookielog,0, 0, NULLCHAR },
#ifdef HTTPPBBS
	{ "execbbs",		dohttpexecbbs,	0, 0, NULLCHAR },
#endif
#ifdef HTTPCGI
	{ "execcgi",		dohttpexeccgi,	0, 0, NULLCHAR },
#endif
	{ "execcmd",		dohttpexeccmd,	0, 0, NULLCHAR },
	{ "ftpdir",		dohttpftpdir,	0, 0, NULLCHAR },
	{ "hostname",		dohttphostname, 0, 0, NULLCHAR },
	{ "maxcli",		dohttpmax,	0, 0, NULLCHAR },
	{ "misclogging",	dohttpmisc,	0, 0, NULLCHAR },
	{ "postlogging",	dohttppost,	0, 0, NULLCHAR },
	{ "refererlogging",	dohttpreferer,	0, 0, NULLCHAR },
#if 0
	{ "script",		dohttpscript,	0, 0, NULLCHAR },
#endif
	{ "secure",		dohttpsecure,	0, 0, NULLCHAR },
	{ "simult",		dohttpsim,	0, 0, NULLCHAR },
	{ "status",		dohttpstatus,	0, 0, NULLCHAR },
	{ "statusurl",		dohttpstatusurl,0, 0, NULLCHAR },
#ifdef STRICT_HTTPCALL
	{ "strictcall",		dohttpstrict,	0, 0, NULLCHAR },
#endif
	{ "tdisc",		dohttptdisc,	0, 0, NULLCHAR },
	{ "usecookies",		dohttpcookies,	0, 0, NULLCHAR },
	{ NULLCHAR,		NULL,		0, 0, NULLCHAR }
};



int
dohttp (int argc, char *argv[], void *p)
{
	return subcmd (Httpcmds, argc, argv, p);
}



int
dohttpstatus (int argc OPTIONAL, char *argv[], void *p)
{
int found = 0, k;

	for (k = 0; k < MAXHTTPPORTS; k++)	{
		if (ports[k].port)	{
			found++;
			tprintf ("HTTP Server active on %s port #%-4d - %s\n", inet_ntoa (ports[k].address), ports[k].port, ports[k].dirname);
		}
	}
	tputs ("    ");
	if (!found)
		tputs ("No");
	else
		tprintf ("%d", found);
	tprintf (" HTTP Server%s %s active\n\n", (found == 1) ? "" : "s", (found == 1) ? "is" : "are");

	(void) dohttpagent (0, argv, p);
#ifdef HTTPPBBS
	(void) dohttpanonsend (0, argv, p);
#endif
	(void) dohttpcookielog (0, argv, p);
#ifdef HTTPPBBS
	(void) dohttpexecbbs (0, argv, p);
#endif
#ifdef HTTPCGI
	(void) dohttpexeccgi (0, argv, p);
#endif
	(void) dohttpexeccmd (0, argv, p);
	(void) dohttpftpdir (0, argv, p);
	(void) dohttphostname (0, argv, p);
	(void) dohttpmax (0, argv, p);
	(void) dohttpmisc (0, argv, p);
	(void) dohttppost (0, argv, p);
	(void) dohttpreferer (0, argv, p);
	(void) dohttpsecure (0, argv, p);
	(void) dohttpsim (0, argv, p);
#ifdef STRICT_HTTPCALL
	(void) dohttpstrict (0, argv, p);
#endif
	(void) dohttptdisc (0, argv, p);
	(void) dohttpcookies (0, argv, p);
	tputc ('\n');
	return 0;
}



static int
dohttpexeccmd (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPexeccmd, "Enable Server-side include 'exec cmd's", argc, argv);
}



static int
dohttpstatusurl (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPstatusURL, "Enable status URL (/status)", argc, argv);
}



#ifdef HTTPCGI
static int
dohttpexeccgi (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPexeccgi, "Enable Server-side include 'exec cgi's", argc, argv);
}
#endif



#ifdef STRICT_HTTPCALL
static int
dohttpstrict (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPstrict, "Allow HTTP access from callsigns, only: ", argc, argv);
}
#endif



#ifdef HTTPPBBS
static int
dohttpexecbbs (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPexecbbs, "Enable Server-side include 'exec bbs's", argc, argv);
}



static int
dohttpanonsend (int argc, char *argv[], void *p OPTIONAL)
{
	/* related to the NO_HTTP_MAIL permission */
	return setbool (&HTTPanonsend, "Allow anonymous users to SEND mail", argc, argv);
}
#endif



#if 0
static int
dohttpscript (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPscript, "Enable Server-side include 'tscript's", argc, argv);
}
#endif



static int
dohttpsecure (int argc, char *argv[], void *p OPTIONAL)
{
struct secureURL *sec;
int didheader = 0;

	if (argc == 1)	{
		/* display currently defined secured URL's */
		for (sec = securedURLS; sec != NULLSECUREURL; sec = sec->next)	{
			if (!didheader)	{
				didheader = 1;
				tprintf ("\nSecured URLs:\n");
			}
			tprintf ("/%s (%s): %s\n", sec->url, sec->realm, sec->pwfile);
		}
		if (didheader)
			tputc ('\n');
		return 0;
	}

	if (argc != 4)	{
		tputs ("usage: http secure <url_or_dir> <realmstring> <pwdfile>\n");
		return 1;
	}

	if (argv[1][0] != '/')	{
		tputs ("The url MUST begin with a '/'\n");
		return 1;
	}

	if (argv[3][0] != '/')	{
		tputs ("The pwdfile MUST be a complete pathname, starting with a '/'\n");
		return 1;
	}

	sec = (struct secureURL *) callocw (1, sizeof (struct secureURL));
	sec->next = securedURLS;
	securedURLS = sec;
	sec->realm = strdup (argv[2]);
	sec->pwfile = strdup (argv[3]);
	sec->url = strdup (&argv[1][1]);
	return 0;
}



static int
dohttpreferer (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPreferer, "Log Referer Headers", argc, argv);
}



static int
dohttpcookielog (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPcookie, "Log Incoming Cookie Headers", argc, argv);
}



static int
dohttpcookies (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPusecookies, "Assign Cookies", argc, argv);
}



static int
dohttpagent (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPagent, "Log User-Agent Headers", argc, argv);
}



static int
dohttpmisc (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPmisc, "Log Misc Headers", argc, argv);
}



static int
dohttppost (int argc, char *argv[], void *p OPTIONAL)
{
	return setbool (&HTTPpost, "Log POST Requests", argc, argv);
}



static int
dohttpmax (int argc, char *argv[], void *p OPTIONAL)
{
	return setint (&Httpmax, "Max. HTTP connects", argc, argv);
}



static int
dohttpsim (int argc, char *argv[], void *p OPTIONAL)
{
	return setint (&Httpsimc, "Simult. HTTP conn.'s serviced", argc, argv);
}



static int
dohttptdisc (int argc, char *argv[], void *p OPTIONAL)
{
	return setint (&Httptdisc, "HTTP Server tdisc (sec)", argc, argv);
}



static int
dohttphostname (int argc, char *argv[], void *p OPTIONAL)
{
	if (argc < 2)	{
		if (HTTPHostname == NULLCHAR && Hostname)
			HTTPHostname = strdup (Hostname);
		if (HTTPHostname)
			tprintf ("Default HTTP Hostname: %s\n", HTTPHostname);
		return 0;
	}
	if (HTTPHostname)
		free (HTTPHostname);
	HTTPHostname = strdup (argv[1]);
	return 0;
}



static int
dohttpftpdir (int argc, char *argv[], void *p OPTIONAL)
{
	if (argc < 2)	{
		tprintf ("Default HTTP FTP directory: %s\n", (HTTPFtpdir) ? HTTPFtpdir : "none - disabled");
		return 0;
	}
	if (HTTPFtpdir)
		free (HTTPFtpdir);
	HTTPFtpdir = strdup (argv[1]);
	return 0;
}



/* Start up http service */
int
httpstart (int argc, char *argv[], void *p OPTIONAL)
{
int port, k;
uint32 address = INADDR_ANY;

	if (HTTPHostname == NULLCHAR) 	{
		if (Hostname)
			HTTPHostname = strdup (Hostname);
		else
			HTTPHostname = strdup ("localhost");
	}

	if(argc < 2 || argv[1][0] == '-')
		port = IPPORT_HTTP;
	else	{
		if (argv[1][0] == '?')	{
			/* they don't really want to start it, they want usage info */
			tprintf ("usage: start http [<port> [<rootdir> [<hostname> [<ipaddress>] [ftpdir]]]]\n");
			return 0;
		}
		port = atoi(argv[1]);
	}

	if (argc > 4 && argv[4][0] != '-')	{
		address = resolve (argv[4]);
		if (address == 0L)	{
			tprintf ("IP address %s does NOT resolve! HTTP server NOT started on port %d!\n",
				argv[4], port);
			return 0;
		}
	}

	for (k = 0; k < MAXHTTPPORTS; k++) {
		if (ports[k].port == port && ports[k].address == address)	{
			tprintf ("Sorry, but you already have an HTTP server on port #%d for that address!\n", port);
			return 0;
		}
	}

	for (k = 0; k < MAXHTTPPORTS; k++) {
		if (ports[k].sock <= 0)
			break;
	}
	if (k == MAXHTTPPORTS)	{
		tprintf ("Sorry, but all %d HTTP ports are assigned!\n", MAXHTTPPORTS);
		return 0;
	}

	if(argc < 3 || argv[2][0] == '-')
		ports[k].dirname = strdup (HTTPdir);
	else
		ports[k].dirname = strdup (argv[2]);

	if (argc < 4 || argv[3][0] == '-')
		ports[k].hostname = strdup (HTTPHostname);
	else
		ports[k].hostname = strdup (argv[3]);

	if (argc < 5 || argv[4][0] == '-')
		ports[k].ftpdirname = (HTTPFtpdir) ? strdup (HTTPFtpdir) : NULLCHAR;
	else
		ports[k].ftpdirname = strdup (argv[4]);

	ports[k].sock = -1;
	ports[k].port = port;
	ports[k].address = address;

	return (installserver (argc, argv, &ports[k].sock, "HTTP Listener", ports[k].port,
		ports[k].address, "HTTP Server", httpserv, 1024, (void *)k));
}



int
http0 (int argc OPTIONAL, char *argv[] OPTIONAL, void *p OPTIONAL)
{
int port, k;

	if(argc < 2)
		port = IPPORT_HTTP;
	else
		port = atoi(argv[1]);
	for (k = 0; k < MAXHTTPPORTS; k++) {
		if (ports[k].port == port)	{
			ports[k].port = 0;
			free (ports[k].dirname);
			ports[k].dirname = NULLCHAR;
			free (ports[k].hostname);
			ports[k].hostname = NULLCHAR;
			return (deleteserver (&ports[k].sock));
		}
	}
	tprintf ("Sorry, but no HTTP server was found on port #%d!\n", port);
	return 0;
}



/* Creates a copy of string which has '%' escaped charecters decoded
   back to ASCII. Allocates memory. Caller must free it. */


static char *
decode (char *string)
{
int i,k,slen;
char code[3],*tmp;

	slen = (int) strlen (string);
	tmp = mallocw ((unsigned int) (slen + 1));
	i = k = 0;

	while (i <= slen) {
		if (string[i] == '%') {
			string[i] = '#';        /* '%' sign causes problems in log()  */
			code[0] = string[i+1];
			code[1] = string[i+2];
			code[2] = '\0';
			tmp[k] = (char)htoi(code);
			k++;  i += 3;
		} else
			tmp[k++] = string[i++];
	}
	return tmp;
}



#ifdef HTTPPBBS
static void
doPBBScmd (struct reqInfo *rq, const char *cmdstr)
{
	doPBBScmd_init (rq);
	strncpy (rq->m->line, cmdstr, MBXLINE);
	doPBBScmd_term (rq);
}



static void
doPBBScmd_init (struct reqInfo *rq)
{
	rq->m = newmbx (1);
	rq->m->privs = rq->privs;
	rq->m->sid = MBX_HTTP;
	rq->m->user = Curproc->output;
	rq->m->path = rq->bbspath;
	if (!rq->username)
		rq->username = strdup ("anonymous");
	strncpy (rq->m->name, rq->username, 19);
	if (rq->areaname)
		changearea (rq->m, rq->areaname, 0);
}



static void
doPBBScmd_term (struct reqInfo *rq)
{
	if (rq->m)	{
		(void) mbx_parse (rq->m);
		doPBBScmd_exit (rq);
	}
}



static void
doPBBScmd_exit (struct reqInfo *rq)
{
	if (rq->m)	{
		rq->m->path = NULLCHAR;
		setlastread (rq->m);
		exitbbs(rq->m);
		rq->m = NULLMBX;
	}
}



static void
addUserFile (struct reqInfo *rq)
{
char fname[256];

	sprintf (fname, "%s/bbs/user.%s", rq->dirname, HTMLEXT);
	addMsgFile (fname, rq);
}



static void
addMsgFile (char *fname, struct reqInfo *rq)
{
FILE *fp;

	if ((fp = fopen (fname, READ_BINARY)) != NULLFILE) {
		sendhtml (fp, 1, rq);
		(void) fclose (fp);
	}
}



static int
checkareaperms (struct reqInfo *rq)
{
int retval = 0;

	/* only return a 1 if (1) it's our area, (2) we are a sysop, or
	   (3) it is a public area. Else, return a zero
	 */

	if (!stricmp(rq->areaname, rq->username) || (rq->privs & SYSOP_CMD) || isarea(rq->areaname))
		retval = 1;
	return retval;
}

#endif



static int
addFile (const char *fname, int s)
{
FILE *fp;

	if ((fp = fopen (fname, READ_BINARY)) != NULLFILE) {
		(void) sendfile (fp, s, IMAGE_TYPE, 0);
		(void) fclose (fp);
		return 1;
	}
	return 0;
}



static void
httpserv (int s, void *v1, void *p OPTIONAL)
{
char command[PLINELEN], *cp, *newcommand, *tmp, *version;
FILE *fp = NULLFILE;
int port, length, i, err = 1, head = 0, retval = 200;
struct stat sb;
int isdir = 0;
struct reqInfo *rq;
char poststr[PLINELEN + 16];
int posted_message = 0;
struct sockaddr fsocket;
uint32 addr;
register struct usock *up;
int isValidFtpRequest = 0;
time_t nowtime;

	(void) sockowner (s, Curproc);
	(void) sockmode (s, SOCK_ASCII);

	chname (Curproc, "HTTP Server - Init");
	rq = (struct reqInfo *)callocw (1, sizeof (struct reqInfo));
	if (rq == (struct reqInfo *)0)
		return;
		
	rq->sizefmt = SIZEFMT_KMG;
	rq->useerror = strdup (DEFAULT_ERROR_MSG);
	rq->usetime = strdup (DEFAULT_TIME_FORMAT);
	rq->index = (int) v1;
	rq->port = ports[rq->index].port;
	rq->hostname = ports[rq->index].hostname;
	ports[rq->index].requests++;
#ifdef STATS_HTTP
	STATS_addhttp (0);
#endif

	if (getpeername (s, (char *) &fsocket, &i) != -1)	{
		port = DTranslate;
		DTranslate = 0;
		rq->rmtaddr = strdup (psocket (&fsocket));
		if ((cp = strchr (rq->rmtaddr, ':')) != NULLCHAR)
			*cp = 0;
		addr = aton (rq->rmtaddr);
		DTranslate = 1;
		rq->rmthost = strdup (inet_ntoa (addr));
		DTranslate = port;
	} else	{
		rq->rmthost = strdup ("unknown");
		rq->rmtaddr = strdup ("unknown");
	}

	close_s(Curproc->output);
	close_s(Curproc->input);
	Curproc->output = Curproc->input = s;
	rq->sock = s;

	/* see if we have too many connections */
	if (Httpmax <= HttpUsers)	{
		tprintf("HTTP/1.0 503 Service Unavailable\n%s %s%s\n\n<TITLE>Busy</TITLE><H1>Busy</H1>\nTry again later.\n",HTTPHdrs[0],CTypes1[HTTPtypes[HTTPhtml].type1],HTTPtypes[HTTPhtml].type2);
		HttpUsers++;
		goto quit;
	}

	HttpUsers++;
#ifdef STATS_USE
	STATS_adduse (1);
#endif
	sprintf (command, "HTTP Server - Waiting (%s)", rq->rmthost);
	chname (Curproc, command);

	/* only allow Httpsimc connections to be active at a time */
	while (HttpSessions >= Httpsimc)	{
		kalarm (5000L);
		kwait(&HttpSessions);
		kalarm (0L);
		if((up = itop(s)) == NULLUSOCK || up->cb.p == NULLCHAR)
			break;
	}
	HttpSessions++;

	kalarm (Httptdisc * 1000L);
	if (recvline (s, (unsigned char *) command, PLINELEN) == -1)
		goto quit;
	kalarm (0L);

	sprintf (poststr, "HTTP Server - Processing (%s)", rq->rmthost);
	chname (Curproc, poststr);

	port = ports[(int)v1].port;
	rq->dirname = ports[rq->index].dirname;
	if (ports[rq->index].ftpdirname == NULLCHAR && HTTPFtpdir)
		/* newly defined default - allow it to set after the fact */
		ports[rq->index].ftpdirname = strdup (HTTPFtpdir);
	rq->ftpdirname = ports[rq->index].ftpdirname;
	
	rip (command);
	if (!*command)	{
		retval = 400;
		usputs (s, "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: Invalid HTTP/0.9 method.<P></BODY>\n");
		goto quit;
	}

	tmp = decode (command);
	cp = strchr (tmp, ' ');
	if (cp == NULLCHAR)	{
		retval = 400;
		usputs (s, "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: No URL given.<P></BODY>\n");
		goto quit;
	}
	*cp++ = 0;
	newcommand = ++cp;
	version = strchr (newcommand, ' ');
	if (version != NULLCHAR)
		*version++ = 0;
	
	/* at this point, tmp=command, newcommand=ulr, and version=httpversion info */
	if (!strcmp (newcommand, "/"))
		newcommand++;
	if (!*newcommand)
		ports[rq->index].homehits++;
	
	rq->url = strdup (newcommand);
	rq->version = strdup (version);
	rq->method = strdup (tmp);
	length = (int) strlen (newcommand);

	/* if HTTP 0.9 (no version string), only 'GET' is allowed */
	if (version == NULLCHAR && strncmp (command, "GET", 3))	{
		retval = 400;
	  	httpHeaders (HTTPhtml, "400 Bad Request", rq, 0, NULLCHAR);
		usputs (s, "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: Invalid HTTP/0.9 method.<P></BODY>\n");
		goto quit0;
	}

	/* not complete yet, but it at least logs it and responds ;-) */
	poststr[0] = 0;
	if (!strncmp (command, "POST", 4)){
		int postsize = 0;
		int headersdone = 0;

		/* if posting PBBS email, open a file for the post contents */
		if (!strcmp (newcommand, "bbs/sendmail"))
			posted_message = 1;

		if (HTTPpost != 0)	{
			openHTTPlog("post");
			simple_log (s, command);
			closeHTTPlog();
		}
		while (kalarm (Httptdisc * 1000L),recvline (s, (unsigned char *) rq->buf, sizeof(rq->buf)) != -1) {
			kalarm (0L);
			if (headersdone)
				postsize -= (int) strlen(rq->buf);
			rip (rq->buf);
			if (HTTPpost != 0)	{
				openHTTPlog("post");
				simple_log (s, rq->buf);
				closeHTTPlog();
			}

			/* if this is an incoming email posting, check for auth string */
			if (posted_message)	{
				/* look for cookie header line */
				if (!strnicmp (rq->buf, HTTPHdrs[HDR_COOKIE], strlen(HTTPHdrs[HDR_COOKIE])))
					rq->cookie = strdup (&rq->buf[strlen(HTTPHdrs[HDR_COOKIE]) + 1]);
				/* also look for auth header line */
				if (!strnicmp (rq->buf, HTTPHdrs[HDR_AUTHORIZE], strlen(HTTPHdrs[HDR_AUTHORIZE])))
					rq->passwd = strdup (&rq->buf[strlen(HTTPHdrs[HDR_AUTHORIZE]) + 7]);
			}

			if (!strnicmp (rq->buf, HTTPHdrs[1], strlen(HTTPHdrs[1])))	{
				postsize = atoi(&rq->buf[strlen(HTTPHdrs[1])]);
				continue;
			} else if (!*rq->buf && !headersdone)	{
				headersdone = 1;
				if (!postsize)	{
					retval = 400;
					httpHeaders (HTTPhtml, "400 Bad Request", rq, 0, NULLCHAR);
					usputs (s, "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent an improperly formatted POST - no size given.</BODY>\n");
					goto quit0;
				}
				/* one more line with content */
				continue;
			}

			if (!headersdone)
				continue;
				
			if (!posted_message)	{
#if 1
				if (postsize <= 0)
					break;
#endif
				continue;
			}

			/* if this is a posted message, this will be our data line - save it */
			strncpy (poststr, rq->buf, PLINELEN + 16);
			break;
		}
		kalarm (0L);

		if (posted_message)
			goto http_bbs_entry;

		/* if this is NOT an email posting, return an okay and bitbucket the posting action */
post_response:
		retval = 202;
		httpHeaders (HTTPhtml, "202 Accepted", rq, 0, NULLCHAR);
		usputs (s, "<HEAD><TITLE>202 Post Accepted</TITLE></HEAD>\n<BODY><H1>202 Post Accepted</H1>\nThe TNOS server has accepted and logged your posting.</BODY>\n");
		goto quit0;
	}

	if (!strncmp (command, "HEAD", 4))
		head = 1;

	if (version != NULLCHAR && !strnicmp (version, "HTTP/", 5)) {
		/* we must wait to receive header lines, until a blank line */
		while (kalarm (Httptdisc * 1000L), recvline (s, (unsigned char *) rq->buf, sizeof(rq->buf)) != -1) {
			kalarm (0L);
			rip (rq->buf);
			if (!*rq->buf)
				break;
			if (!strnicmp (rq->buf, HTTPHdrs[HDR_COOKIE], strlen(HTTPHdrs[HDR_COOKIE])))	{
				rq->cookie = strdup (&rq->buf[strlen(HTTPHdrs[HDR_COOKIE]) + 1]);
				if (HTTPcookie == 0)
					continue;
				openHTTPlog("cookies");
				sprintf (rq->buf, "%s; URL=%s", rq->cookie, rq->url);
			} else if (!strnicmp (rq->buf, HTTPHdrs[HDR_LENGTH], strlen(HTTPHdrs[HDR_LENGTH]))) {
				rq->qsize = atoi (&rq->buf[strlen(HTTPHdrs[HDR_LENGTH]) + 1]);
				if (HTTPmisc == 0)
					continue;
				openHTTPlog("mischttp");
			} else if (!strnicmp (rq->buf, HTTPHdrs[HDR_SINCE], strlen(HTTPHdrs[HDR_SINCE]))) {
				rq->newcheck = strdup (&rq->buf[strlen(HTTPHdrs[HDR_SINCE]) + 1]);
				if (HTTPmisc == 0)
					continue;
				openHTTPlog("mischttp");
			} else if (!strnicmp (rq->buf, HTTPHdrs[HDR_FROM], strlen(HTTPHdrs[HDR_FROM]))) {
				rq->from = strdup (&rq->buf[strlen(HTTPHdrs[HDR_FROM]) + 1]);
				if (HTTPmisc == 0)
					continue;
				openHTTPlog("mischttp");
			} else if (!strnicmp (rq->buf, HTTPHdrs[HDR_AUTHORIZE], strlen(HTTPHdrs[HDR_AUTHORIZE]))) {
				rq->passwd = strdup (&rq->buf[strlen(HTTPHdrs[HDR_AUTHORIZE]) + 7]);
				if (HTTPmisc == 0)
					continue;
				openHTTPlog("mischttp");
			} else if (!strnicmp (rq->buf, HTTPHdrs[HDR_REFERER], strlen(HTTPHdrs[HDR_REFERER]))) {
				rq->referer = strdup(&rq->buf[strlen(HTTPHdrs[HDR_REFERER]) + 1]);
				if (HTTPreferer == 0)
					continue;
				openHTTPlog("referer");
				sprintf (rq->buf, "%s -> /%s", rq->referer, rq->url);
			} else if (!strnicmp (rq->buf, HTTPHdrs[HDR_AGENT], strlen(HTTPHdrs[HDR_AGENT])))	{
				rq->agent = strdup(&rq->buf[strlen(HTTPHdrs[HDR_AGENT]) + 1]);
				if (HTTPagent == 0)
					continue;
				openHTTPlog("agent");
				sprintf (rq->buf, "%s -> /%s", rq->agent, rq->url);
			} else	{
				if (HTTPmisc == 0)
					continue;
				openHTTPlog("mischttp");
			}
			simple_log (s, rq->buf);
			closeHTTPlog();
		}
		kalarm (0L);
	}
	
	if (!head && strncmp (command, "GET", 3))	{
		/* not GET, HEAD, or POST, so we don't support it.... */
		retval = 400;
	  	httpHeaders (HTTPhtml, "400 Bad Request", rq, 0, NULLCHAR);
		usputs (s, "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: Invalid or unsupported method.<P></BODY>\n");
		goto quit0;
	}

	if (HTTPstatusURL && (!strcmp (newcommand, "status") || !strcmp (newcommand, "status/")))	{
		ports[rq->index].stathits++;
		rq->isvolatile = 1;
	  	httpHeaders (HTTPhtml, "200 Document follows", rq, 0, NULLCHAR);
		usprintf (s, "<HEAD><TITLE>Server Status at %s</TITLE></HEAD>\n", ports[rq->index].hostname);
		usputs (s, "<HTML><BODY><A HREF=\"http://www.lantz.com/tnos/\"><IMG SRC=\"http://www.lantz.com/tnos/tnos.gif\" align=left></A>");
		usprintf (s, "<H1>%s:%d</H1>\n<H2>Server Info</H2>\n", ports[rq->index].hostname, ports[rq->index].port);

		cp = ctime (&StartTime);
		rip (cp);
		usprintf (s, "<TABLE border=0><tr><td>Start Time</td><td>%s</td></tr>\n", cp);
		nowtime = time (&nowtime);
		cp = ctime (&nowtime);
		rip (cp);
		usprintf (s, "<tr><td>Current Time&nbsp;&nbsp;</td><td>%s</td></tr>\n", cp);

		usputs (s, "<tr><td>Server</td><td>TNOS/"VERSION" "OS"</td></tr></table><pre>\n\n\n</pre><H3>Current Counters</H3>\n");
		usprintf (s, "<table border><tr><td>URL Requests</td><td>&nbsp;%ld&nbsp;</td></tr>\n", ports[rq->index].requests);
		usprintf (s, "<tr><td>Home Page Hits</td><td>&nbsp;%ld&nbsp;</td></tr>\n", ports[rq->index].homehits);
		usprintf (s, "<tr><td>Status Page Hits&nbsp;</td><td>&nbsp;%ld&nbsp;</td></tr>\n", ports[rq->index].stathits);
#ifdef HTTPPBBS
		usprintf (s, "<tr><td>PBBS Hits</td><td>&nbsp;%ld&nbsp;</td></tr>\n", ports[rq->index].bbshits);
#endif
		usprintf (s, "<tr><td>Active Requests</td><td>&nbsp;%d&nbsp;</td></tr>\n</table>\n", HttpUsers);

#ifdef STATS_HTTP
		usputs (s, "<pre>\n\n\n</pre><HR><H3>General Statistics</H3><PRE>\n");
		doPBBScmd (rq, "stats h g");

		usputs (s, "<pre>\n\n\n</pre><HR><H3>Daily Statistics</H3><PRE>\n");
		doPBBScmd (rq, "stats h d");

		usputs (s, "<pre>\n\n\n</pre><HR><H3>Weekly Statistics</H3><PRE>\n");
		doPBBScmd (rq, "stats h w");

		usputs (s, "<pre>\n\n\n</pre><HR><H3>Monthly Statistics</H3><PRE>\n");
		doPBBScmd (rq, "stats h m");

		usputs (s, "<pre>\n\n\n</pre><HR><H3>Yearly Statistics</H3><PRE>\n");
		doPBBScmd (rq, "stats h y");
#endif

		usputs (s, "<pre>\n\n\n</pre><HR><H5>For further information on TNOS can be found at <A HREF=\"http://www.lantz.com/tnos\">TNOS Central</A></H5>\n</HTML>\n");
		goto quit0;
	}
#ifdef HTTPPBBS
http_bbs_entry:
	if (!strncmp (newcommand, "bbs", 3))	{
		ports[rq->index].bbshits++;
#ifdef STATS_HTTP
		STATS_addhttp (1);
#endif
		/* if no authentication, it fails */
		if (rq->passwd == NULLCHAR)	{
			goto failauthorize;
		} else {
			/* check the authentication, here */
			char *tmp64, *cp2, pathbuf[MBXLINE], *path = pathbuf;
			if ((tmp64 = base64ToStr(rq->passwd)) == NULLCHAR)
				goto failauthorize;
#if 0
			log (-1, "HTTP Authentication Test: '%s' - '%s'", rq->passwd, (tmp64) ? tmp64 : "unknown");
#endif
			if (tmp64[0] == ':' || (cp2 = strchr (tmp64, ':')) == NULLCHAR)
				goto failauthorize0;

			*cp2++ = 0;
			if((rq->privs = userlogin(tmp64,cp2,&path,MBXLINE,&rq->anony)) == -1 || (rq->privs & NO_HTTP_IP) || (rq->privs & EXCLUDED_CMD))	{
				/* invalid authentication */
failauthorize0:			free (tmp64);
failauthorize:			retval = 401;
				httpHeaders (HTTPhtml, "401 Unauthorized", rq, 0, "the TNOS PBBS");
				usputs (s, "<HEAD><TITLE>Authorization Failed</TITLE></HEAD>\n<BODY><H1>Authorization Failed</H1>\nThe TNOS server requires proper authorization for this URL. Either your browser does not perform authorization, or your authorization has failed.</BODY>\n");
				goto quit0;
			}
#ifdef STRICT_HTTPCALL
			if (HTTPstrict && !(rq->privs & SYSOP_CMD) && !iscall(tmp64))
				goto failauthorize0;
#endif
			if (rq->privs & WAS_ANONY)	{
				int ii, trans;
				char *cptr, *cptr2;

				ii = SOCKSIZE;
				trans = DTranslate; /* Save IP address translation state */
				DTranslate = 0;     /* Force output to be numeric IP addr*/
				if (getpeername(s,(char *)&fsocket,&ii) != -1)	{
					cptr = psocket(&fsocket);
					if ((cptr2 = strchr(cptr, ':')) != NULLCHAR)
						*cptr2 = 0;
					if (strcmp(inet_ntoa(NonSecureAmpr), cptr) && (!*cptr || !strnicmp (cptr, "44.", 3) || !strnicmp (cptr, "127.0.0.1", 9)))	{
						if (MbAmprPerms)
							rq->privs = MbAmprPerms;
					} else if (MbNonAmprPerms)
						rq->privs = MbNonAmprPerms;
				} else if (MbNonAmprPerms)
					rq->privs = MbNonAmprPerms;
				DTranslate = trans;             /* Restore original state */
				if (MbHttpPerms)
					rq->privs = MbHttpPerms;
				rq->privs |= WAS_ANONY;
			}
			rq->username = strdup (tmp64);
			(void) strlwr (rq->username);
			rq->bbspath = strdup (pathbuf);
			free (tmp64);
		}
		/* passed authentication - now check for special URLs */
		
		/* if URL is 'bbs' or 'bbs/', then change it to 'bbs/${username}.html' */
		if (strlen(newcommand) == 3 || !strcmp (newcommand, "bbs/"))	{
			retval = 302;
			sprintf (rq->buf, "bbs/%s.%s", rq->username, HTMLEXT);
			free (rq->url);
			rq->url = strdup (rq->buf);
		  	httpHeaders (HTTPhtml, "302 Found", rq, 0, NULLCHAR);
		  	if (!head)
				usprintf (s, "<TITLE>Document moved</TITLE><H1>Document moved</H1>Please,"
				   "\n<A HREF=\"http://%s/%s\">click here</A>\n",
				   rq->hostname,rq->url);
			goto quit0;
		}

		/* if URL is 'bbs/area', then change it to 'bbs/area/${username}.html' */
		if (!strcmp (newcommand, "bbs/area"))	{
			retval = 302;
			sprintf (rq->buf, "bbs/area/%s.%s", rq->username, HTMLEXT);
			free (rq->url);
			rq->url = strdup (rq->buf);
		  	httpHeaders (HTTPhtml, "302 Found", rq, 0, NULLCHAR);
		  	if (!head)
				usprintf (s, "<TITLE>Document moved</TITLE><H1>Document moved</H1>Please,"
				   "\n<A HREF=\"http://%s/%s\">click here</A>\n",
				   rq->hostname, rq->url);
			goto quit0;
		}

		/* if URL is 'bbs/message/areaname', then change it to 'bbs/area/areaname.html' */
		if (!strncmp (newcommand, "bbs/message/", 12) && !strstr (newcommand, ".htm"))	{
			cp = &newcommand[12];
			sprintf (rq->buf, "bbs/area/%s.%s", cp, HTMLEXT);
			free (rq->url);
			rq->url = strdup (rq->buf);
			retval = 302;
		  	httpHeaders (HTTPhtml, "302 Found", rq, 0, NULLCHAR);
		  	if (!head)
				usprintf (s, "<TITLE>Document moved</TITLE><H1>Document moved</H1>Please,"
				   "\n<A HREF=\"http://%s/%s\">click here</A>\n",
				   rq->hostname, rq->url);
			goto quit0;
		}

		/* if URL is 'bbs/$(username}.html', then build it */
		sprintf (rq->buf, "bbs/%s.%s", rq->username, HTMLEXT);
		if (!strcmp (rq->buf, newcommand))	{
			/* special URL for user's login */
			rq->isvolatile = 1;
		  	httpHeaders (HTTPhtml, "200 Document follows", rq, 1, NULLCHAR);
		  	addUserFile (rq);
			goto quit0;
		}

		/* if URL is 'bbs/area/${areaname}.html', then build an area feature page */
		if (!strncmp (newcommand, "bbs/area/", 9) && !strchr(&newcommand[9], '/') && strstr (newcommand, ".htm"))	{
			char *cp2;
			/* special URL for area listing */
		  	strncpy (rq->buf, &newcommand[9], PLINELEN + 16);
		  	cp2 = strstr (rq->buf, ".htm");
		  	if (cp2)
				*cp2 = 0;
		  	rq->areaname = strdup (rq->buf);

		  	if (!checkareaperms (rq))
		  		goto failauthorize;
		  	
			doPBBScmd_init (rq);
			rq->msgcount = rq->m->nmsgs;
			rq->newmsgcount = rq->m->newmsgs;
			rq->msgcurrent = rq->m->current;

			rq->isvolatile = 1;
		  	httpHeaders (HTTPhtml, "200 Document follows", rq, 1, NULLCHAR);
			sprintf (rq->buf, "%s/bbs/areaopt.%s", rq->dirname, HTMLEXT);
		  	addMsgFile (rq->buf, rq);
			goto quit0;
		}

		/* if URL is 'bbs/area/areaname/xxx.html', then build an area list, starting w/msg#xxx */
		if (!strncmp (newcommand, "bbs/area/", 9))	{
			char *cp2, *cp3;
			/* special URL for area listing */
		  	strncpy (rq->buf, &newcommand[9], PLINELEN + 16);
		  	if ((cp2 = strchr (rq->buf, '/')) == NULLCHAR)
		  		goto failauthorize;
		  	*cp2++ = 0;
		  	rq->areaname = strdup (rq->buf);

		  	if (!checkareaperms (rq))
		  		goto failauthorize;
		  	
		  	if ((cp3 = strchr (cp2, '.')) == NULLCHAR)
		  		goto failauthorize;
		  	*cp3 = 0;

		  	rq->msgnum = atoi(cp2);
		  	if (!rq->msgnum && !strcmp (cp2, "new"))
		  		rq->msgnum = -1;
		  	
			doPBBScmd_init (rq);
			rq->msgcount = rq->m->nmsgs;
			rq->newmsgcount = rq->m->newmsgs;
			rq->msgcurrent = rq->m->current;

			rq->isvolatile = 1;
		  	httpHeaders (HTTPhtml, "200 Document follows", rq, 1, NULLCHAR);

			strcpy (rq->m->line, "L");
			if (rq->msgnum != -1)
				sprintf (&rq->m->line[1], " %d %d", rq->msgnum, rq->msgnum + 99);

			sprintf (rq->buf, "%s/bbs/arealist.%s", rq->dirname, HTMLEXT);
		  	addMsgFile (rq->buf, rq);
			goto quit0;
		}

		/* if URL is 'bbs/message/areaname/xxx.html', then build an message page */
		if (!strncmp (newcommand, "bbs/message/", 12))	{
			char *cp2, *cp3;
			/* special URL for reading messages */
			strncpy (rq->buf, &newcommand[12], PLINELEN + 16);
		  	if ((cp2 = strchr (rq->buf, '/')) == NULLCHAR)
		  		goto failauthorize;
		  	*cp2++ = 0;
		  	rq->areaname = strdup (rq->buf);

		  	if (!checkareaperms (rq))
		  		goto failauthorize;
		  	
		  	if ((cp3 = strchr (cp2, '.')) == NULLCHAR)
		  		goto failauthorize;
		  	*cp3 = 0;
			rq->msgnum = atoi (cp2);

			doPBBScmd_init (rq);
			rq->msgcount = rq->m->nmsgs;
			rq->newmsgcount = rq->m->newmsgs;
			rq->msgcurrent = rq->m->current;

			rq->isvolatile = 1;
		  	httpHeaders (HTTPhtml, "200 Document follows", rq, 1, NULLCHAR);

			sprintf (rq->m->line, "%d", rq->msgnum);
			sprintf (rq->buf, "%s/bbs/msglist.%s", rq->dirname, HTMLEXT);
		  	addMsgFile (rq->buf, rq);
			goto quit0;
		}

		/* if URL is 'bbs/cmd/xxxx', then build an text page from the BBS output of 'xxxx' */
		if (HTTPexecbbs && !strncmp (newcommand, "bbs/cmd/", 8))	{
			char *cp2, *cp3;

			cp2 = &newcommand[8];
			if (!strncmp (cp2, "area?", 5))	{
				cp3 = strchr (&cp2[5], '/');
				if (cp3 != NULLCHAR)	{
					cp2 += 5;
					*cp3++ = 0;
					rq->areaname = strdup (cp2);
					cp2 = cp3;
				}
			}
			while ((cp3 = strchr (cp2, '?')) != NULLCHAR)
		  		*cp3 = ' ';

			rq->isvolatile = 1;
		  	httpHeaders (HTTPplain, "200 Document follows", rq, 0, NULLCHAR);

			doPBBScmd (rq, cp2);
			goto quit0;
		}

		/* if URL is 'bbs/sendmail', then build an email message from the posting */
		if (!strcmp (rq->method, "POST") && posted_message && !strcmp (newcommand, "bbs/sendmail"))	{
			char *tmp2, *tmp3, *tmp4, msgtype = 'P', specialchar[3];
			char *subject = NULLCHAR;
			char fromaddr[128];
			char *toaddr = NULLCHAR;
			FILE *postfp;
			
			if (rq->privs & (NO_HTTP_MAIL | NO_SENDCMD))	{
				retval = 403;
				httpHeaders (HTTPhtml, "403 Forbidden", rq, 0, NULLCHAR);
				usputs (s, "<HEAD><TITLE>403 Post Forbidden</TITLE></HEAD>\n<BODY><H1>403 Post Forbidden</H1>\nThe TNOS server has rejected your posting.<p>\nYou do not have permission to post to this site.\n");
				sprintf (fromaddr, "%s/bbs/sendfail.%s", HTTPdir, HTMLEXT);
				if ((postfp = fopen (fromaddr, "r")) != NULLFILE)	{
					sendhtml (postfp, 1, rq);
					(void) fclose (postfp);
				} else
					usputs (s, "</BODY>\n");

				goto quit0;
			}

			postfp = tmpfile ();
			if (postfp == NULLFILE)
				goto post_response;	/* shouldn't happen */

			while ((cp = strchr (poststr, '+')) != NULLCHAR)
				*cp = ' ';
			
			for (cp = poststr; cp != NULLCHAR; cp = tmp4)	{
				tmp4 = strchr (cp, '&');
				if (tmp4)
					*tmp4++ = 0;
				if ((tmp2 = strchr (cp, '=')) == NULLCHAR)
					continue;
				*tmp2++ = 0;
				if (!strcasecmp (cp, "address"))	{
					toaddr = decode(tmp2);
					continue;
				} else if (!strcasecmp (cp, "subject"))	{
					subject = decode(tmp2);
					continue;
				} else if (!strcasecmp (cp, "type"))	{
					msgtype = *tmp2;
					continue;
				} else if (strncasecmp (cp, "line", 4))
					continue;

				/* This is a data line, add to the temp file */
				while ((tmp3 = strchr (tmp2, '%')) != NULLCHAR)	{
					*tmp3++ = 0;
					fputs (tmp2, postfp);
					specialchar[0] = *tmp3++;
					specialchar[1] = *tmp3++;
					specialchar[2] = 0;
					fputc (htoi (specialchar), postfp);
					tmp2 = tmp3;
				}
				if (*tmp2)
					fputs (tmp2, postfp);
				fputc ('\n', postfp);
				
			}
			if (toaddr)	{
				sprintf (fromaddr, "%s@%s", rq->username, rq->hostname);
				retval = 202;
				httpHeaders (HTTPhtml, "202 Accepted", rq, 0, NULLCHAR);
				usputs (s, "<HEAD><TITLE>202 Post Accepted</TITLE></HEAD>\n<BODY><H1>202 Post Accepted</H1>\nThe TNOS server has accepted and logged your posting.</BODY>\n");

				rewind (postfp);
				(void) rdaemon (postfp, NULLCHAR, fromaddr, toaddr, (subject) ? subject : "(no subject)", msgtype, 0);
				smtptick (NULL);
			} else {
				retval = 400;
			  	httpHeaders (HTTPhtml, "400 Bad Request", rq, 0, NULLCHAR);
				usputs (s, "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: No email address given.<P></BODY>\n");
			}

			(void) fclose (postfp);
			free (toaddr);
			free (subject);
			goto quit0;
		}

		/* otherwise, it must be a regular html file in the 'bbs' directory */
	}
#endif

	if (is_authorized (rq) == FALSE)	{
		/* this url is protected and proper authentication wasn't given */
		retval = 401;
		httpHeaders (HTTPhtml, "401 Unauthorized", rq, 0, rq->realm);
		usputs (s, "<HEAD><TITLE>Authorization Failed</TITLE></HEAD>\n<BODY><H1>Authorization Failed</H1>\nThe TNOS server requires proper authorization for this URL. Either your browser does not perform authorization, or your authorization has failed.</BODY>\n");
		goto quit0;
	}

	if (length == 0)
		sprintf(rq->buf,"%s/index.%s", rq->dirname, HTMLEXT);
	else	{
		if (rq->ftpdirname && !strncmp (newcommand, "ftp", 3))	{
			char *newercommand = newcommand;
			/* viewing/retrieving files from defined FTP directory */
			newercommand += 3;
			if (*newercommand && *newercommand == '/')
				newercommand++;
			sprintf(rq->buf, "%s/%s", rq->ftpdirname, newercommand);
			isValidFtpRequest = 1;
		} else
			sprintf(rq->buf,"%s/%s", rq->dirname, newcommand);
	}

	/* delete any .. to avoid */
	/* anyone telnetting to http and cd'ing to a directory */
	/* other than within Httpdir */

	for ( i=0; i <= (int) strlen(rq->buf); i++ ) {
		if (rq->buf[i] == '.' && rq->buf[i+1] == '.') {
			rq->buf[i] = ' ';
			rq->buf[i+1] = ' ';
		}
	}

	if (stat (rq->buf, &sb) != -1)
		isdir = sb.st_mode & S_IFDIR;
	if (rq->buf[strlen(rq->buf)-1] == '/') {
		strcat (rq->buf, "welcome." HTMLEXT);
		if (isValidFtpRequest || !access (rq->buf, 0))	{
			retval = 200;
		  	httpHeaders (HTTPhtml, "200 Document follows", rq, 0, NULLCHAR);
		  	if (!head)
				err = mkwelcome (rq->dirname, rq->buf, isValidFtpRequest, rq->ftpdirname);
			goto quit0;
		}
	} else if (isdir) {
		retval = 302;
		strncpy (rq->buf, newcommand, PLINELEN + 15);
		strcat (rq->buf, "/");
		free (rq->url);
		rq->url = strdup (rq->buf);
	  	httpHeaders (HTTPhtml, "302 Found", rq, 0, NULLCHAR);
	  	if (!head)
			tprintf ("<TITLE>Document moved</TITLE><H1>Document moved</H1>Please,"
			   "\n<A HREF=\"http://%s/%s\">click here</A>\n",
			   rq->hostname, rq->url);
		goto quit0;
	} else
		fp = fopen (rq->buf, READ_BINARY);

	if (fp != NULLFILE)	{
		int thetype = -1;

		if ((cp = strrchr (rq->buf, '.')) != NULLCHAR)	{
			cp++;
			for (thetype = HTTPhtml; HTTPtypes[thetype].ext != NULLCHAR; thetype++) {
				if (!strnicmp (HTTPtypes[thetype].ext, cp, strlen(HTTPtypes[thetype].ext)))
					break;
			}
		}
		if (thetype == -1 || HTTPtypes[thetype].type1 == -1)
			/* either had no extension, or extension not found in table */
			thetype = (isbinary(fp)) ? HTTPbinary : HTTPplain;

		retval = 200;
		(void) fstat (fileno(fp), &sb);
		if (!head && rq->newcheck && !isnewer (sb.st_mtime, rq->newcheck))	{
			head = 1;
			retval = 304;
		}

		if (!strnicmp (newcommand, "bbs/user/", 9))
			rq->isvolatile = 1;
	  	httpHeaders (thetype, (retval == 304) ? "304 Not Modified" : "200 Document follows", rq, 1, NULLCHAR);
	  	httpFileinfo (fp, rq, thetype, version);
		if (!head)	{
			if (thetype != HTTPhtml && thetype != HTTPhtm)	{	/* if not html, just send */
				tprintf ("\n");
				(void) sendfile (fp, s, IMAGE_TYPE, 0);
			} else				/* otherwise, scan for server-side includes */
				sendhtml (fp, 1, rq);
		}
		(void) fclose (fp);
	} else if (err) {
		retval = 404;
		httpHeaders (HTTPhtml, "404 Not Found", rq, 0, NULLCHAR);
		if (!head)
			tprintf ("<TITLE>Error</TITLE><H1>404 Not Found</H1>The file \"/%s\" does not exist on this server.\n", newcommand);
	}
quit0:
	free (tmp);
	openHTTPlog ("http");
	log (s, "port #%u: \"%s\" %d", port, command, retval);
	closeHTTPlog();
	(void) countusage ("tcount.dat", rq->dirname, 0, 1);
 
quit:
        usflush(Curproc->output);
	kpause (1000);
	close_s (Curproc->output);

	doPBBScmd_exit (rq);
	free (rq->url);
	free (rq->method);
	free (rq->version);
	free (rq->newcheck);
	free (rq->from);
	free (rq->referer);
	free (rq->agent);
	free (rq->passwd);
	free (rq->usetime);
	free (rq->useerror);
	free (rq->realm);
	free (rq->cookie);
	free (rq->rmtaddr);
	free (rq->rmthost);
	
#ifdef HTTPPBBS
	free (rq->areaname);
	free (rq->username);
	free (rq->bbspath);
#endif
	free (rq);

	HttpUsers--;
	HttpSessions--;
	ksignal (&HttpSessions, 1);
	kwait (NULL);
}



static char *
gmt_ptime (time_t *thetime)
{
	return ht_time(*thetime,HTTP_TIME_FORMAT, 1);
}



static char *
ht_time (time_t t, const char *fmt, int gmt)
{
static char ts[LINELEN];
struct tm *tms;

	tms = (gmt ? gmtime(&t) : localtime(&t));

	/* check return code? */
	(void) strftime (ts, LINELEN, fmt, tms);
	return ts;
}



static void
httpFileinfo (FILE *fp, struct reqInfo *rq, int thetype, char *vers)
{
struct stat sb;

	if (vers == NULLCHAR)
		return;		/* it is 0.9, no headers sent */
	if (!rq->isvolatile)
		if (fstat (fileno(fp), &sb) != -1)
			tprintf ("%s %s", HTTPHdrs[HDR_MODIFIED], gmt_ptime(&sb.st_mtime));

	if (thetype != HTTPhtml)
		tprintf ("%s %lu\n", HTTPHdrs[HDR_LENGTH], filelength(fileno(fp)));
}



static char *
assigncookie (struct reqInfo *rq, time_t thetime)
{
static char cookie[32];
char ipaddr[10], buf[32];
int i = SOCKSIZE;
struct sockaddr_in fsocket;
FILE *fp;
long nextCookie = 0;
int assignone = 1;
unsigned long address = INADDR_ANY;

	if (getpeername (rq->sock, (char *) &fsocket, &i) != -1)
		address = fsocket.sin_addr.s_addr;

	/* build ipaddress portion of cookie, which we'll also use to
	   lookup this site, and see if there is already an assigned cookie */
	sprintf (ipaddr, "%02x%02x%02x%02x", (int) ((address >> 24) & 255),
		(int) ((address >> 16) & 255), (int) ((address >> 8) & 255),
		(int) (address & 255));
	
	/* get the next cookie number we should use, and build cookie
	   string, in case we DO assign a new one */
	if ((fp = fopen (Cookiefile, READ_TEXT)) != NULLFILE)	{
		cookie[0] = 0;
		(void) fgets (cookie, 30, fp);
		nextCookie = atol (cookie);
		(void) fclose (fp);
	}
	sprintf (cookie, "%8.8s_%08ld_%012ld", ipaddr, ++nextCookie, thetime);

	/* look to see if there is already a cookie assigned to for this
	   site. Browsers that do NOT support cookies could cause the
	   Cookiedata file to grow to a large size, without this piece of code */
	if ((fp = fopen (Cookiedata, READ_TEXT)) != NULLFILE)	{
		while (fgets (buf, 32, fp) != NULLCHAR)	{
			if (!strncmp (buf, ipaddr, 8))	{
				strncpy (cookie, buf, 32);
				rip (cookie);
				assignone = 0;
				break;
			}
		}
		(void) fclose (fp);
	}
	
	
	if (assignone)	{	/* if we didn't find an old one, save this info */
		if ((fp = fopen (Cookiefile, WRITE_TEXT)) != NULLFILE)	{
			fprintf (fp, "%ld\n", nextCookie);
			(void) fclose (fp);
		} else	{
			/* if we can't save them, log an error, and disable creating more */
			log (-1, "Cannot write data to Cookiefile: '%s'", Cookiefile);
			HTTPusecookies = 0;
		}

		if ((fp = fopen (Cookiedata, APPEND_TEXT)) != NULLFILE)	{
			fprintf (fp, "%s\n", cookie);
			(void) fclose (fp);
		}
	}

	return (cookie);
}



static void
httpHeaders (int type, const char *str, struct reqInfo *rq, int more, const char *realm)
{
time_t thetime;

	if (rq->version == NULLCHAR)
		return;		/* it is 0.9, no headers sent */
	(void) time (&thetime);
	
	tprintf ("HTTP/1.0 %s\n%s %s", str, HTTPHdrs[HDR_DATE], gmt_ptime(&thetime));
	if (rq->isvolatile)
		tprintf ("%s %s", HTTPHdrs[HDR_EXPIRES], gmt_ptime(&thetime));
	if (realm)
		tprintf ("%s basic realm=\"%s\"\n", HTTPHdrs[HDR_AUTHENTICATE], realm);
	tprintf ("%s 1.0\n%s TNOS/"VERSION" "OS"\n", HTTPHdrs[HDR_MIME], HTTPHdrs[HDR_SERVER]);
	tprintf ("%s http://%s/%s\n", HTTPHdrs[HDR_LOCATION], rq->hostname, rq->url);

	if (HTTPusecookies && (rq->cookie == NULLCHAR || !strstr (rq->cookie, "TNOS=")))
		tprintf ("Set-Cookie: TNOS=%s; expires=%s; path=/; domain=%s\n",
			assigncookie (rq, thetime), ht_time (thetime + SECS_IN_6MONTHS, DEFAULT_TIME_FORMAT, 1), rq->hostname);

	tprintf ("%s %s%s\n", HTTPHdrs[HDR_TYPE], CTypes1[HTTPtypes[type].type1], HTTPtypes[type].type2);

	/* must have this line, when no more headers needed */
	if (more == 0)
		tputs ("\n");
}



static int
mkwelcome (const char *Hdir, char *file, int ftpfile, const char *Ftpdir)
{
char *dirstring,*p,*cp;
const char *ccp, *bitmap;
struct ffblk ffblk;
int done,pre = 0, k;
char *updir = NULLCHAR;
char *endstr;
int isadir;

	if ((dirstring = (char *)mallocw (strlen(file) + 16)) == NULL)
		return 0;

	strcpy (dirstring, file);
	(void) addFile (dirstring, Curproc->output);

	cp = strrchr (dirstring, '/');
	if (cp)
		*++cp = '\0';
	if (!ftpfile && strlen(Hdir) != strlen(dirstring))	{
		updir = strdup(&dirstring[strlen(Hdir)]);
		p = strrchr (updir, '/');
		if (p)
			*p = 0;
		p = strrchr (updir, '/');
		if (p == NULLCHAR)
			updir[0] = 0;
		else
			*++p = 0;
	} else if (ftpfile && strlen(Ftpdir) != strlen(dirstring))	{
		updir = strdup(&dirstring[strlen(Ftpdir)]);
		p = strrchr (updir, '/');
		if (p)
			*p = 0;
		p = strrchr (updir, '/');
		if (p == NULLCHAR)
			updir[0] = 0;
		else
			*++p = 0;
	}
	
	endstr = &dirstring[strlen(dirstring)];

	strcat (dirstring, HTTPheadfile);
	pre = addFile (dirstring, Curproc->output);

	strcpy (endstr, WILDCARD);
	done = findfirst (dirstring, &ffblk, (FA_HIDDEN|FA_SYSTEM|FA_DIREC));
	if (done)	{
		free (dirstring);
		return -1;
	}
	*endstr = 0;
	
	(void) sockmode (Curproc->output, SOCK_ASCII);
	(void) setflush (Curproc->output, -1); /* we will do our own flushing */
	p = dirstring + 1 + ((ftpfile) ? strlen(Ftpdir) : strlen(Hdir));
	if (ftpfile)	{
		p = (char *) mallocw (strlen (p) + 5);
		sprintf (p, "ftp/%s", dirstring + 1 + ((ftpfile) ? strlen(Ftpdir) : strlen(Hdir)));
	}
	undosify (p);
	if (!pre)
		tprintf ("<HEAD><TITLE>Index of /%s</TITLE></HEAD></BODY><H1>Index of /%s</H1>\n", p, p);
	tputs ("<PRE>\n");

	/* include the 'include' file, if it exists in this directory */
	strcpy (endstr, HTTPinclude);
	if (addFile (dirstring, Curproc->output))
		tputs ("<P>\n");
	*endstr = 0;

	tputs ("<IMG SRC=\"/icons/blank.xbm\" ALT=\"      \"> Name                   Last modified     Size\n<HR>\n");
	if (updir != NULLCHAR)	{
		if (*updir)
			tprintf ("<IMG SRC=\"/icons/menu.xbm\" ALT=\"[DIR ]\"> <A HREF=\"%s%s\">Parent Directory</a>\n", (ftpfile) ? "/ftp" : "", updir);
		free (updir);
	}
	while (!done) {
		isadir = 0;

		if (ffblk.ff_name[0] != '.')	{
			if (ffblk.ff_attrib & FA_DIREC)	{
				ccp = "[DIR ]";
				bitmap = "menu";
				isadir = 1;
			} else {
				ccp = "[FILE]";
				bitmap = "text";
				k = (int) (strlen(ffblk.ff_name) - 4);
				if (k > 0 && (!strnicmp (&ffblk.ff_name[k], ".gif", 4) || !strnicmp (&ffblk.ff_name[k], ".jpg", 4) || !strnicmp (&ffblk.ff_name[k], ".xbm", 4)))
					bitmap = "image";
			}
			tprintf (entry, bitmap, ccp);
			tprintf (entry1, p, ffblk.ff_name, (isadir) ? "/" : "", ffblk.ff_name);
			for (k = (int) strlen (ffblk.ff_name); k < 22; k++)
				tputc (' ');
			tprintf (entry2,
#ifdef MSDOS
				(ffblk.ff_fdate) & 0x1f,	/* day */
				Months[((ffblk.ff_fdate >> 5) & 0xf)-1], /* month */
				(ffblk.ff_fdate >> 9) + 80,	/* year */
				(ffblk.ff_ftime >> 11) & 0x1f,	/* hour */
				(ffblk.ff_ftime >> 5) & 0x3f,	/* minutes */
#else
				ffblk.ff_ftime.tm_mday,		/* day */
				Months[ffblk.ff_ftime.tm_mon],	/* month */
				ffblk.ff_ftime.tm_year,		/* year */
				ffblk.ff_ftime.tm_hour,		/* hour */
				ffblk.ff_ftime.tm_min,		/* minute */
#endif
				(int)((ffblk.ff_fsize + 1023L) / 1024L));
		}
		kwait (NULL);
		done = findnext (&ffblk);
	}
	tputs ("</PRE>\n");
	usflush (Curproc->output);

	strcat (dirstring, HTTPtailfile);
	if (!addFile (dirstring, Curproc->output))
		tputs ("</BODY>\n");
	free (dirstring);
	if (ftpfile)
		free (p);
	return 0;
}


extern FILE *Logfp;



static void
openHTTPlog (const char *name)
{
char filename[256];

	HTTPsavedfp = Logfp;
	sprintf (filename, "%s/%s.log", LOGdir, name);
	Logfp = fopen (filename, APPEND_TEXT);
}



static void
closeHTTPlog (void)
{
	if (Logfp != NULLFILE)
		(void) fclose (Logfp);
	Logfp = HTTPsavedfp;
}



static int
getmonth (char *cp)
{
int k;

	for (k = 0; k < 12; k++)	{
		if (!strnicmp (cp, Months[k], 3))	{
			return k;
		}
	}
	return 0;
}



static long
countusage (const char *file, const char *basedir, int display, int increase)
{
char name[256], buf[32];
FILE *fp;
long count = 0;

	sprintf (name,"%s/counts/%s", basedir, file);
	if ((fp = fopen (name, READ_TEXT)) != NULLFILE) {
		(void) fgets (buf, sizeof(buf), fp);
		(void) fclose (fp);
		count = atol (buf);
	}
	if (increase)	{
		count++;
		if ((fp = fopen (name, WRITE_TEXT)) != NULLFILE) {
			fprintf (fp, "%ld", count);
			(void) fclose (fp);
		}
	}
	if (display)
		tprintf ("%ld", count);
	return (count);
}



static int
isnewer (time_t thetime, char *tmstr)
{
time_t new;
struct tm t, *ltm;
char *cp, *cp2;
int format = 0;
time_t offset;

	/* the string should be in one of these three formats:
	 * (a) RFC 822 (preferred) - Sun, 06 Nov 1994 08:49:37 GMT
	 * (b) RFC 850		   - Sunday, 06-Nov-94 08:49:37 GMT
	 * (c) ANSI C's asctime    - Sun Nov  6 08:49:37 1994
	 */
	/* skip past the name of the weekday */
	cp = skipwhite (tmstr);
	cp = skipnonwhite (cp);
	cp = skipwhite (cp);
	if (isdigit (*cp))	{	/* format a or b */
		cp[2] = 0;
		t.tm_mday = atoi (cp);
		cp += 3;
		t.tm_mon = getmonth (cp);
		cp += 4;
		cp2 = skipnonwhite (cp);
		*cp2 = 0;
		t.tm_year = atoi (cp);
		cp = ++cp2;
	} else {			/* format c */
		t.tm_mon = getmonth (cp);
		cp += 4;
		t.tm_mday = atoi (cp);
		cp += 3;
		format = 1;
	}

	/* time parsing is common between the three formats */
	sscanf (cp, "%02d:%02d:%02d", &t.tm_hour, &t.tm_min, &t.tm_sec);
	/* then parse the year, if format c */
	if (format) {
		cp += 9;
		t.tm_year = atoi (cp);
	}
	if (t.tm_year > 1900)
		t.tm_year -= 1900;
#ifdef HAVE_TM_ISDST
	t.tm_isdst = -1;
#endif
	new = mktime (&t);
	ltm = localtime(&new);		/* to get timezone info */
#if defined(HAVE_TM_ISDST) && !defined(MSDOS)
	offset = ((timezone - (ltm->tm_isdst * 3600L)) * -1);
#else
	offset = (ltm->tm_gmtoff * -1);
#endif
	new += offset;
	return (new != thetime);
}



static void
send_size (int sizefmt, long size)
{
	if (sizefmt == SIZEFMT_BYTES)	{
		tprintf ("%ld", size);
		return;
	}
	if (size == -1)
		tputs ("    -");
	else {
		if (!size)
			tputs ("   0K");
		else if (size < 1024)
			tputs ("   1K");
		else if (size < 1048576)
			tprintf ("%4ldK", size / 1024L);
		else
			tprintf ("%4ldM", size / 1048576L);
	}
}



#ifdef HTTPCGI
static void
setup_cgi_variables (struct reqInfo *rq, char *name)
{
char buf[80], *cp;
char *scriptname;


	if (name == NULLCHAR)
		scriptname = strdup (rq->url);
	else
		scriptname = name;
	(void) setenv ("GATEWAY_INTERFACE", "CGI/1.1", 0);
	(void) setenv ("SERVER_NAME", rq->hostname, 0);
	(void) setenv ("SERVER_SOFTWARE", "TNOS/"VERSION" "OS, 0);
	(void) setenv ("SERVER_PROTOCOL", "HTTP/1.0", 0);
	sprintf (buf, "%d", rq->port);
	(void) setenv ("SERVER_PORT", buf, 1);
	(void) setenv ("DOCUMENT_ROOT", rq->dirname, 1);
	(void) setenv ("REQUEST_METHOD", rq->method, 1);
	(void) setenv ("REMOTE_ADDR", (rq->rmtaddr) ? rq->rmtaddr : "", 1);
	(void) setenv ("REMOTE_HOST", (rq->rmthost) ? rq->rmthost : "", 1);
	(void) setenv ("SCRIPT_NAME", scriptname, 1);
	(void) setenv ("HTTP_REFERER", (rq->referer) ? rq->referer : "", 1);
	(void) setenv ("HTTP_USER_AGENT", (rq->agent) ? rq->agent : "", 1);

	if ((cp = strchr (scriptname, '?')) != NULLCHAR)	{
		*cp++ = 0;
		(void) setenv ("QUERY_STRING", cp, 1);
	} else
		(void) setenv ("QUERY_STRING", "", 1);

	sprintf (buf, "sysop@%s", rq->hostname);
	(void) setenv ("SERVER_ADMIN", buf, 0);
	
	/* not completely to spec */
	sprintf (buf, "%s/%s", rq->dirname, scriptname);
	(void) setenv ("SCRIPT_FILENAME", buf, 1);

	/* not supported, yet:
		PATH_INFO, PATH_TRANSLATED, AUTH_TYPE
		REMOTE_USER, REMOTE_IDENT, CONTENT_TYPE,
		CONTENT_LENGTH, HTTP_FROM, HTTP_ACCEPT
	 */

	if (scriptname != name)
		free (scriptname);
}
#endif



static void
sendhtml (FILE *fp, int firstfile, struct reqInfo *rq)
{
time_t t;
struct stat ft;
char *cp, *cp1, *cp2, c;
char *remainder, *cmdname, newname[256];
FILE *tfp;
char *bufptr;
char buf[PLINELEN + 16];
int c2;
struct tag *tg;
int val;


#ifdef HTTPCGI
	setup_cgi_variables (rq, NULLCHAR);
#endif
	(void) sockmode (Curproc->output, SOCK_BINARY);

	if (firstfile)	{	/* if first file (not included file), search for meta tags */
	    	while ((c2 = fgetc (fp)) != EOF) {
			if (c2 == '<')	{
				tg = parse_tag (fp);
				if (tg != NULLTAG && !strcmp (tg->name, "meta"))	{
					/* found a meta tag */
					cp1 = find_option (tg, "http-equiv", NULL);
					if (cp1 != NULLCHAR)	{
			
						cp2 = find_option (tg, "content", NULL);
						tprintf ("%s: %s\n", cp1, (cp2) ? cp2 : "");
						free (cp1);
						free (cp2);
					}
				}
				delete_tag (tg);
			}
		}
		clearerr (fp);
		rewind (fp);
		tprintf ("\n");
	}

    	while ((fgets (buf, PLINELEN + 15, fp)) != NULLCHAR) {
		kwait(NULL);
		bufptr = buf;
		while (bufptr  && (cp1 = strstr (bufptr, "<!--#")) != NULLCHAR) {
			if (cp1 != bufptr)	{
				*cp1 = 0;
				tputs (bufptr);
			}
			cp1 += 5;
			remainder = strstr (cp1, "-->");
			if (remainder != NULLCHAR) {
				*remainder = 0;
				remainder = &remainder[3];
			}
			cmdname = skipwhite (cp1);
			cp1 = skipnonwhite (cmdname);
			cp1 = skipwhite (cp1);

			if (!strnicmp (cmdname, "echo", 4)) {
				if (!strnicmp (cp1, "var=\"", 5)) {
					(void) time (&t);
					cp1 += 5;
					if ((cp2 = strrchr (cp1, '\"')) != NULLCHAR)
						*cp2 = 0;
					if (!stricmp (cp1, "DATE_LOCAL"))
						tputs (ht_time (t, rq->usetime, 0));
					else if (!stricmp (cp1, "DATE_GMT"))
						tputs (ht_time (t, HTTP_TIME_FORMAT, 1));
					else if (!stricmp (cp1, "HOSTNAME"))
						tputs ((rq->hostname) ? rq->hostname : "unknownhost");
					else if (!stricmp (cp1, "DOCUMENT_URI"))
						tprintf ("/%s", rq->url);
					else if (!stricmp (cp1, "VERSION"))
						tputs (Version);
#ifdef HTTPPBBS
					else if (!stricmp (cp1, "USERNAME"))
						tputs ((rq->username) ? rq->username : "unknownUser");
					else if (!stricmp (cp1, "AREANAME"))
						tputs ((rq->areaname) ? rq->areaname : "unknownArea");
					else if (!stricmp (cp1, "MESSAGECOUNT"))
						tprintf ("%d", rq->msgcount);
					else if (!stricmp (cp1, "NEWMSGCOUNT"))
						tprintf ("%d", rq->newmsgcount);
					else if (!stricmp (cp1, "CURMESSAGE"))
						tprintf ("%d", rq->msgcurrent);
					else if (!stricmp (cp1, "MESSAGENUM"))
						tprintf ("%d", rq->msgnum);
					else if (!stricmp (cp1, "PREV_MSGNUM"))
						tprintf ("%d", rq->msgnum - 1);
					else if (!stricmp (cp1, "NEXT_MSGNUM"))
						tprintf ("%d", rq->msgnum + 1);
					else if (!stricmp (cp1, "PERMISSIONS"))
						doPBBScmd (rq, "security");
					else if (!stricmp (cp1, "PREV_LIST_START"))
						tprintf ("%d", (rq->msgnum > 100) ? rq->msgnum - 100 : 1);
					else if (!stricmp (cp1, "NEXT_LIST_START"))
						tprintf ("%d", rq->msgnum + 100);
					else if (!stricmp (cp1, "PREV_LIST_BUTTON"))	{
						if (rq->msgnum > 1)	{
							sprintf (rq->buf, "%s/bbs/plbutton.cat", rq->dirname);
							if ((tfp = fopen (rq->buf, "r")) != NULLFILE) {
								sendhtml (tfp, 0, rq);
								(void) fclose (tfp);
							}
						}
					} else if (!stricmp (cp1, "PREV_MSG_BUTTON"))	{
						if (rq->msgnum > 1)	{
							sprintf (rq->buf, "%s/bbs/pmbutton.cat", rq->dirname);
							if ((tfp = fopen (rq->buf, "r")) != NULLFILE) {
								sendhtml (tfp, 0, rq);
								(void) fclose (tfp);
							}
						}
					} else if (!stricmp (cp1, "NEXT_MSG_BUTTON"))	{
						if (rq->msgcount > rq->msgnum)	{
							sprintf (rq->buf, "%s/bbs/nmbutton.cat", rq->dirname);
							if ((tfp = fopen (rq->buf, "r")) != NULLFILE) {
								sendhtml (tfp, 0, rq);
								(void) fclose (tfp);
							}
						}
					} else if (!stricmp (cp1, "NEXT_LIST_BUTTON"))	{
						if (rq->msgcount > rq->msgnum + 100)	{
							sprintf (rq->buf, "%s/bbs/nlbutton.cat", rq->dirname);
							if ((tfp = fopen (rq->buf, "r")) != NULLFILE) {
								sendhtml (tfp, 0, rq);
								(void) fclose (tfp);
							}
						}
					} else if (!stricmp (cp1, "AREA_LIST") || !stricmp (cp1, "AREA_MSG"))
						doPBBScmd_term (rq);
#endif
#ifdef ALLSERV
					else if (!stricmp (cp1, "QUOTE")) {
						cp2 = getquote ();
						tputs ((cp2) ? cp2 : "[quote not found]");
					}
#endif
					else if (!stricmp (cp1, "DOCUMENT_NAME"))	{
						if (rq->url)	{
							cp2 = strrchr (rq->url, '/');
							if (cp2)
								cp2++;
							else
								cp2 = rq->url;
							tputs (cp2);
						}
					} else if (!stricmp (cp1, "LAST_MODIFIED")) {
						(void) fstat (fileno(fp), &ft);
						tputs (ht_time (ft.st_mtime, rq->usetime, 0));
					} else if (!stricmp (cp1, "TOTAL_HIT_COUNT") || !stricmp (cp1, "TOTAL_HITS"))
						(void) countusage ("tcount.dat", rq->dirname, 1, 0);
					else if (!stricmp (cp1,"REQ_FROM")) {
						if (rq->from)
							tputs(rq->from);
					} else if (!stricmp (cp1,"REQ_REFERER")) {
						if (rq->referer)
							tputs (rq->referer);
					} else if (!stricmp (cp1,"REQ_AGENT")) {
						if (rq->agent)
							tputs (rq->agent);
					} else if (!stricmp (cp1,"REQ_COOKIE")) {
						if (rq->cookie)
							tputs (rq->cookie);
					} else if ((cp = getenv (cp1)) != NULLCHAR)
						tputs (cp);
					else
						tputs (rq->useerror);
				} else if (!strnicmp (cp1, "count=\"", 7) || !strnicmp (&cp1[1], "count=\"", 7))	{
					c = (char) tolower(*cp1);
					switch (c) {
						case 'c':	/* count */
						case 'i':	/* icount */
						case 's':	/* scount */
								cp1 = strchr (cp1, '\"');
								if (cp1)	{
									cp2 = strrchr (++cp1, '\"');
									if (cp2)
										*cp2 = 0;
									(void) countusage (cp1, rq->dirname, (c != 's'), (c != 'c'));
								}
								break;
						default:	tputs (rq->useerror);

					}
				} else
					tputs (rq->useerror);
			} else if (!strnicmp (cmdname, "config", 6)) {
				if (!strnicmp (cp1, "timefmt=\"", 9)) {
					cp1 += 9;
					if ((cp2 = strrchr (cp1, '\"')) != NULLCHAR)
						*cp2 = 0;
					free (rq->usetime);
					rq->usetime = strdup (cp1);
				} else if (!strnicmp (cp1, "sizefmt=\"", 9)) {
					c = (char) tolower(cp1[9]);
					/* either 'abbrev' or 'bytes' */
					rq->sizefmt = (c == 'a') ? SIZEFMT_KMG : SIZEFMT_BYTES;
				} else if (!strnicmp (cp1, "errmsg=\"", 8)) {
					cp1 += 8;
					if ((cp2 = strrchr (cp1, '\"')) != NULLCHAR)
						*cp2 = 0;
					free (rq->useerror);
					rq->useerror = strdup (cp1);					
				} else
					tputs (rq->useerror);
			} else if (!strnicmp (cmdname, "include", 7)) {
				file_or_virtual (cp1, newname, rq);
				if (*newname && (tfp = fopen (newname, "r")) != NULLFILE) {
					sendhtml (tfp, 0, rq);
					(void) fclose (tfp);
				} else
					tputs (rq->useerror);
			} else if (!strnicmp (cmdname, "fsize", 5)) {
				file_or_virtual (cp1, newname, rq);
				if (*newname)	{
					(void) stat (newname, &ft);
					send_size (rq->sizefmt, (long) ft.st_size);
				} else
					tputs (rq->useerror);
                        } else if (!strnicmp (cmdname, "flastmod", 8)) {
				file_or_virtual (cp1, newname, rq);
				if (*newname)	{
					(void) stat (newname, &ft);
					tputs (ht_time (ft.st_mtime, rq->usetime, 0));
				} else
					tputs (rq->useerror);
#if 0
			} else if (HTTPscript && !strnicmp (cmdname, "tscript", 7)) {
				/* to be added later */
				tputs (rq->useerror);
#endif
			} else if (!strnicmp (cmdname, "exec", 4))	{
				/* exec cgi no supported, at least not yet */
#ifdef HTTPPBBS
				if (HTTPexecbbs && !strnicmp (cp1, "bbs=\"", 5)) {
					if ((cp2 = strrchr (cp1, '\"')) != NULLCHAR)
						*cp2 = 0;
					doPBBScmd (rq, &cp1[5]);
				} else
#endif
#ifdef HTTPCGI
				if (HTTPexeccgi && !strnicmp (cp1, "cgi=\"", 5)) {
					if ((cp2 = strrchr (cp1, '\"')) != NULLCHAR)
						*cp2 = 0;
					if ((tfp = popen (&cp1[5], "r")) != NULLFILE) {
						while ((val = fgetc (tfp)) != EOF)
							tputc (uchar(val));
					}
					(void) pclose (tfp);
				} else
#endif
				if (HTTPexeccmd && !strnicmp (cp1, "cmd=\"", 5)) {
					if ((cp2 = strrchr (cp1, '\"')) != NULLCHAR)
						*cp2 = 0;
					if ((tfp = popen (&cp1[5], "r")) != NULLFILE) {
						while ((val = fgetc (tfp)) != EOF)
							tputc (uchar(val));
					}
					(void) pclose (tfp);
				} else
					tputs (rq->useerror);
			} else {
				tprintf ("<!--#%s-->", cmdname);
			}
			bufptr = remainder;
		}
		if (bufptr != NULLCHAR)
			tputs (bufptr);
	}
}



static void
file_or_virtual (char *cp, char *newname, struct reqInfo *rq)
{
char *cp2, *cp3;

	*newname = 0;
	if (!strnicmp (cp, "virtual=\"", 9))	{
		/* virtual filename, based on root http directory */
		cp2 = &cp[9];
		if (!strncmp (cp2, "../", 3) || !strncmp (cp2, "./", 2))
			cp2 = (strchr (cp2, '/') + 1);		/*lint !e613 * can't happen, see above line */
		sprintf (newname, "%s/%s", rq->dirname, cp2);
	} else if (!strnicmp (cp, "file=\"", 6)) {
		/* absolute filename */
		cp2 = &cp[6];
		if (!strncmp (cp2, "../", 3) || !strncmp (cp2, "./", 2))
			cp2 = (strchr (cp2, '/') + 1);		/*lint !e613 * can't happen, see above line */
		if (*cp2 == '/')
			cp2++;
		cp3 = strrchr (rq->url, '/');
		sprintf (newname, "%s%s/%s", rq->dirname, (cp3) ? cp3 : "", cp2);
	}
	if (*newname && (cp2 = strrchr (newname, '\"')) != NULLCHAR)
		*cp2 = 0;
}



static struct secureURL *
secured_url (struct reqInfo *rq)
{
struct secureURL *sec;

	/* lookup rq->url in securedURLS table, and return entry or NULL */
	for (sec = securedURLS; sec != NULLSECUREURL; sec = sec->next)	{
		/* 'http sec' url's ending in '/' protect entire directories */
		if (sec->url[strlen (sec->url) - 1] == '/')	{
			if (!strncmp (sec->url, rq->url, strlen (sec->url)))
				return sec;
		} else if (!strcmp (sec->url, rq->url))	/* single protected URL */
			return sec;
	}
	return NULLSECUREURL;
}



static int
is_authorized (struct reqInfo *rq)
{
int retval = FALSE;
struct secureURL *sec;
char *tmp64 = NULLCHAR, *cp2;

	/* See if URL is secured, if not return TRUE */
	if ((sec = secured_url (rq)) == NULLSECUREURL)
		return TRUE;
		
	rq->realm = strdup (sec->realm);
	/* this URL is secured */
	if (rq->passwd != NULLCHAR)	{
		/* check the authentication, here */
		if (((tmp64 = base64ToStr(rq->passwd)) != NULLCHAR) && ((cp2 = strchr (tmp64, ':')) != NULLCHAR))	{
			*cp2++ = 0;
			retval = http_authcheck (sec->pwfile, tmp64, cp2);
		}
		free (tmp64);
	}
	return retval;
}



static int
http_authcheck (char *filename, char *username, char *pass)
{
char buf[80];
char *cp = NULLCHAR;
char *cp1;
FILE *fp;
int retval = FALSE;

	if ((fp = fopen (filename, "r")) == NULLFILE)	/* Authorization file doesn't exist */
		log (-1, "Authorization file %s not found\n", filename);
	else	{
		while ((void) fgets (buf, sizeof(buf), fp), !feof (fp)) {
			if (buf[0] == '#')
				continue;	/* Comment */

			if ((cp = strchr (buf, ':')) == NULLCHAR)
				continue;	/* Bogus entry */

			*cp++ = '\0';		/* Now points to password */
			if (strcmp (username, buf) == 0)	{
				if ((cp1 = strchr (cp, ':')) == NULLCHAR)
					break;

				*cp1 = '\0';
				if (strcmp (cp, pass) != 0)
					break;	/* Password required, but wrong one given */

				retval = TRUE;	/* whew! finally made it!! */
				break;
			}
		}
		(void) fclose(fp);
	}
	return retval;
}

#endif /* HTTP */

#if defined(BROWSER) || defined(HTTP)

struct tag *
parse_tag (FILE *fp)
{
struct tag *tag;
struct options *opt;
int c, theindex = 0;
char buf[128];
int endquote;
int lookfor;

	tag = (struct tag *) callocw (1, sizeof (struct tag));
	c = fgetc (fp);
	if (c == '!')	{
		/* this is an HTML comment - whether we see the '--' or not */
		tag->name = strdup ("!--");

		/* eat the two '-'s */
		c = fgetc (fp);
		if (c != '-')	{	/* this covers !DOCTYPE tags */
			while (c != '>')
				c = fgetc (fp);
			return tag;
		}
		c = fgetc (fp);
		for ( ; ; )	{
			c = fgetc (fp);
			while (!feof (fp) && c != '-')
				c = fgetc (fp);
			if (feof (fp))
				break;
			c = fgetc (fp);
			if (c != '-')
				continue;
			c = fgetc (fp);
			while (!feof (fp) && c != '>')
				c = fgetc (fp);
			break;
		}
		return tag;
	}
	if (c == '/')	{
		tag->endtag = 1;
		c = fgetc (fp);
	}

	while (!isspace (c) && c != '>')	{
		buf[theindex++] = (char) c;
		c = fgetc (fp);
	}
	buf[theindex] = 0;
	(void) strlwr (buf);
	tag->name = strdup (buf);
	while (c != '>')	{
		theindex = 0;
		opt = (struct options *) callocw (1, sizeof (struct options));
		opt->next = tag->opts;
		tag->opts = opt;
	
		c = fgetc (fp);
		while (!isspace (c) && c != '>' && c != '=')	{
			buf[theindex++] = (char) c;
			c = fgetc (fp);
		}
		buf[theindex] = 0;
		(void) strlwr (buf);
		opt->name = strdup(buf);
		if (c != '=')
			continue;

		theindex = 0;
		c = fgetc (fp);
		lookfor = ' ';

		if (c == '"')	{
			endquote = 1;
			lookfor = c;
			c = fgetc (fp);
		} else
			endquote = 0;
		while (c != lookfor)	{
			if (!endquote && ((c == '>') || isspace (c)))
				break;
			buf[theindex++] = (char) c;
			c = fgetc (fp);
		}
		buf[theindex] = 0;
		rip (buf);		/* just in case */
		opt->value = strdup (buf);
	}
	return tag;
}



void
delete_tag (struct tag *tag)
{
struct options *opt, *last;

	if (tag == NULLTAG)
		return;

	free (tag->name);
	for (opt = tag->opts; opt != NULLOPTIONS; opt = last)	{
		free (opt->name);
		free (opt->value);
		last = opt->next;
		free (opt);
	}
}



char *
find_option (struct tag *tag, const char *str, int *found)
{
struct options *opt;

	if (found)
		*found = 0;
	for (opt = tag->opts; opt != NULLOPTIONS; opt = opt->next)	{
		if (!strcasecmp (str, opt->name))	{
			if (found)
				*found = 1;
			return (opt->value) ? strdup (opt->value) : NULLCHAR;
		}
	}
	return NULLCHAR;	
}

#ifdef NO_SETENV
char *
setenv (char const *label, char const *value, int unused OPTIONAL)
{
char *temp;

	temp = (char *) mallocw (strlen (label) + strlen (value) + 2);
	sprintf (temp, "%s=%s", label, value);
	(void) putenv (temp);
	free (temp);
	return NULLCHAR;
}
#endif


#endif

