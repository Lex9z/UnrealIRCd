/*
 *   Unreal Internet Relay Chat Daemon, src/s_conf.c
 *   (C) 1998-2000 Chris Behrens & Fred Jacobs (comstud, moogle)
 *   (C) 2000-2002 Carsten V. Munk and the UnrealIRCd Team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#ifdef __hpux
#include "inet.h"
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
#include <time.h>
#endif
#include <string.h>
#ifdef GLOBH
#include <glob.h>
#endif
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#include "h.h"
#include "inet.h"
#include "proto.h"
#ifdef _WIN32
#undef GLOBH
#endif

#define ircstrdup(x,y) if (x) MyFree(x); if (!y) x = NULL; else x = strdup(y)
#define ircfree(x) if (x) MyFree(x); x = NULL
#define ircabs(x) (x < 0) ? -x : x

/* 
 * Some typedefs..
*/
typedef struct _confcommand ConfigCommand;
struct	_confcommand
{
	char	*name;
	int	(*conffunc)(ConfigFile *conf, ConfigEntry *ce);
	int 	(*testfunc)(ConfigFile *conf, ConfigEntry *ce);
};

typedef struct _conf_operflag OperFlag;
struct _conf_operflag
{
	long	flag;
	char	*name;
};

/* Config commands */

static int	_conf_admin		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_me		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_oper		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_class		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_drpass		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_ulines		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_include		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_tld		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_listen		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_allow		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_except		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_vhost		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_ban		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_set		(ConfigFile *conf, ConfigEntry *ce);
#ifdef STRIPBADWORDS
static int	_conf_badword		(ConfigFile *conf, ConfigEntry *ce);
#endif
static int	_conf_deny		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_dcc		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_deny_version	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_conf_help		(ConfigFile *conf, ConfigEntry *ce);

/* 
 * Validation commands 
*/

static int	_test_admin		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_me		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_oper		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_class		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_drpass		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_ulines		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_include		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_tld		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_listen		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_except		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_vhost		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_ban		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_set		(ConfigFile *conf, ConfigEntry *ce);
#ifdef STRIPBADWORDS
static int	_test_badword		(ConfigFile *conf, ConfigEntry *ce);
#endif
static int	_test_deny		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_deny_dcc		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_deny_link		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_deny_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_deny_version	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_allow_channel	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_loadmodule	(ConfigFile *conf, ConfigEntry *ce);
static int	_test_log		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_alias		(ConfigFile *conf, ConfigEntry *ce);
static int	_test_help		(ConfigFile *conf, ConfigEntry *ce);
 
/* This MUST be alphabetized */
static ConfigCommand _ConfigCommands[] = {
	{ "admin", 		_conf_admin,		_test_admin 	},
	{ "allow",		_conf_allow,		_test_allow	},
#ifdef STRIPBADWORDS
	{ "badword",	_conf_badword,		_test_badword	},
#endif
	{ "class", 		_conf_class,		_test_class	},
	{ "drpass",		_conf_drpass,		_test_drpass	},
	{ "except",		_conf_except,		_test_except	},
	{ "include",	NULL,		  		_test_include	},
	{ "listen", 	_conf_listen,		_test_listen	},
	{ "me", 		_conf_me,			_test_me	},
	{ "oper", 		_conf_oper,			_test_oper	},
	{ "tld",		_conf_tld,			_test_tld	},
	{ "ulines",		_conf_ulines,		_test_ulines	},
	{ "vhost", 		_conf_vhost,		_test_vhost	},

/*
	{ "link", 		_conf_link,		_test_link	},
	{ "ban", 		_conf_ban,		_test_ban	},
	{ "set",		_conf_set,		_test_set	},
	{ "deny",		_conf_deny,		_test_deny	},
	{ "loadmodule",		_conf_loadmodule, 	_test_loadmodule},
	{ "log",		_conf_log,		_test_log	},
	{ "alias",		_conf_alias,		_test_alias	},
	{ "help",		_conf_help,		_test_help	},*/
};

static int _OldOperFlags[] = {
	OFLAG_LOCAL, 'o',
	OFLAG_GLOBAL, 'O',
	OFLAG_REHASH, 'r',
	OFLAG_DIE, 'D',
	OFLAG_RESTART, 'R',
	OFLAG_HELPOP, 'h',
	OFLAG_GLOBOP, 'g',
	OFLAG_WALLOP, 'w',
	OFLAG_LOCOP, 'l',
	OFLAG_LROUTE, 'c',
	OFLAG_GROUTE, 'L',
	OFLAG_LKILL, 'k',
	OFLAG_GKILL, 'K',
	OFLAG_KLINE, 'b',
	OFLAG_UNKLINE, 'B',
	OFLAG_LNOTICE, 'n',
	OFLAG_GNOTICE, 'G',
	OFLAG_ADMIN_, 'A',
	OFLAG_SADMIN_, 'a',
	OFLAG_NADMIN, 'N',
	OFLAG_COADMIN, 'C',
	OFLAG_ZLINE, 'z',
	OFLAG_WHOIS, 'W',
	OFLAG_HIDE, 'H',
	OFLAG_INVISIBLE, '^',
	OFLAG_TKL, 't',
	OFLAG_GZL, 'Z',
	0, 0
};

static OperFlag _OperFlags[] = {
	{ OFLAG_LOCAL,		"local" },
	{ OFLAG_GLOBAL,		"global" },
	{ OFLAG_REHASH,		"can_rehash" },
	{ OFLAG_DIE,		"can_die" },
	{ OFLAG_RESTART,        "can_restart" },
	{ OFLAG_HELPOP,         "helpop" },
	{ OFLAG_GLOBOP,         "can_globops" },
	{ OFLAG_WALLOP,         "can_wallops" },
	{ OFLAG_LOCOP,		"locop"},
	{ OFLAG_LROUTE,		"can_localroute" },
	{ OFLAG_GROUTE,		"can_globalroute" },
	{ OFLAG_LKILL,		"can_localkill" },
	{ OFLAG_GKILL,		"can_globalkill" },
	{ OFLAG_KLINE,		"can_kline" },
	{ OFLAG_UNKLINE,	"can_unkline" },
	{ OFLAG_LNOTICE,	"can_localnotice" },
	{ OFLAG_GNOTICE,	"can_globalnotice" },
	{ OFLAG_ADMIN_,		"admin"},
	{ OFLAG_SADMIN_,	"services-admin"},
	{ OFLAG_NADMIN,		"netadmin"},
	{ OFLAG_COADMIN,	"coadmin"},
	{ OFLAG_ZLINE,		"can_zline"},
	{ OFLAG_WHOIS,		"get_umodew"},
	{ OFLAG_INVISIBLE,	"can_stealth"},
	{ OFLAG_HIDE,		"get_host"},
	{ OFLAG_TKL,		"can_gkline"},
	{ OFLAG_GZL,		"can_gzline"},
	{ 0L, 	NULL  }
};

static OperFlag _ListenerFlags[] = {
	{ LISTENER_NORMAL, 	"standard"},
	{ LISTENER_CLIENTSONLY, "clientsonly"},
	{ LISTENER_SERVERSONLY, "serversonly"},
	{ LISTENER_REMOTEADMIN, "remoteadmin"},
	{ LISTENER_JAVACLIENT, 	"java"},
	{ LISTENER_MASK, 	"mask"},
	{ LISTENER_SSL, 	"ssl"},
	{ 0L,			NULL },
};

static OperFlag _LinkFlags[] = {
	{ CONNECT_AUTO,	"autoconnect" },
	{ CONNECT_SSL,	"ssl"		  },
	{ CONNECT_ZIP,	"zip"		  },
	{ CONNECT_QUARANTINE, "quarantine"},
	{ 0L, 		NULL }
};

static OperFlag _LogFlags[] = {
	{ LOG_ERROR, "errors" },
	{ LOG_KILL, "kills" },
	{ LOG_TKL, "tkl" },
	{ LOG_CLIENT, "connects" },
	{ LOG_SERVER, "server-connects" },
	{ LOG_KLINE, "kline" },
	{ LOG_OPER, "oper" },
	{ 0L, NULL }
};

#ifdef USE_SSL
static OperFlag _SSLFlags[] = {
	{ SSLFLAG_FAILIFNOCERT, "fail-if-no-clientcert" },
	{ SSLFLAG_VERIFYCERT, "verify-certificate" },
	{ SSLFLAG_DONOTACCEPTSELFSIGNED, "no-self-signed" },
	{ 0L, NULL }	
};
#endif

/*
 * Utilities
*/

void	ipport_seperate(char *string, char **ip, char **port);
long	config_checkval(char *value, unsigned short flags);

/*
 * Parser
*/

ConfigFile		*config_load(char *filename);
void			config_free(ConfigFile *cfptr);
static ConfigFile 	*config_parse(char *filename, char *confdata);
static void 		config_entry_free(ConfigEntry *ceptr);
ConfigEntry		*config_find_entry(ConfigEntry *ce, char *name);
/*
 * Error handling
*/

void 			config_error(char *format, ...);
void 			config_status(char *format, ...);
void 			config_progress(char *format, ...);

#ifdef _WIN32
extern void 	win_log(char *format, ...);
extern void		win_error();
#endif

/*
 * Config parser (IRCd)
*/
int			init_conf(char *rootconf, int rehash);
int			load_conf(char *filename);
void			config_rehash();
int			config_run();
/*
 * Configuration linked lists
*/
ConfigItem_me		*conf_me = NULL;
ConfigItem_class 	*conf_class = NULL;
ConfigItem_class	*default_class = NULL;
ConfigItem_admin 	*conf_admin = NULL;
ConfigItem_admin	*conf_admin_tail = NULL;
ConfigItem_drpass	*conf_drpass = NULL;
ConfigItem_ulines	*conf_ulines = NULL;
ConfigItem_tld		*conf_tld = NULL;
ConfigItem_oper		*conf_oper = NULL;
ConfigItem_listen	*conf_listen = NULL;
ConfigItem_allow	*conf_allow = NULL;
ConfigItem_except	*conf_except = NULL;
ConfigItem_vhost	*conf_vhost = NULL;
ConfigItem_link		*conf_link = NULL;
ConfigItem_ban		*conf_ban = NULL;
ConfigItem_deny_dcc     *conf_deny_dcc = NULL;
ConfigItem_deny_channel *conf_deny_channel = NULL;
ConfigItem_allow_channel *conf_allow_channel = NULL;
ConfigItem_deny_link	*conf_deny_link = NULL;
ConfigItem_deny_version *conf_deny_version = NULL;
ConfigItem_log		*conf_log = NULL;
ConfigItem_unknown	*conf_unknown = NULL;
ConfigItem_unknown_ext  *conf_unknown_set = NULL;
ConfigItem_alias	*conf_alias = NULL;
ConfigItem_include	*conf_include = NULL;
ConfigItem_help		*conf_help = NULL;
#ifdef STRIPBADWORDS
ConfigItem_badword	*conf_badword_channel = NULL;
ConfigItem_badword      *conf_badword_message = NULL;
#endif

aConfiguration		iConf;
ConfigFile		*conf = NULL;

int			config_error_flag = 0;

/*
*/

void	ipport_seperate(char *string, char **ip, char **port)
{
	char *f;

	if (*string == '[')
	{
		f = strrchr(string, ':');
	        if (f)
	        {
	        	*f = '\0';
	        }
	        else
	        {
	 		*ip = NULL;
	        	*port = NULL;
	        }

	        *port = (f + 1);
	        f = strrchr(string, ']');
	        if (f) *f = '\0';
	        *ip = (*string == '[' ? (string + 1) : string);
	        
	}
	else if (strchr(string, ':'))
	{
		*ip = strtok(string, ":");
		*port = strtok(NULL, "");
	}
	else if (!strcmp(string, my_itoa(atoi(string))))
	{
		*ip = "*";
		*port = string;
	}
}



long config_checkval(char *value, unsigned short flags) {
	char *text;
	long ret = 0;


	if (flags == CFG_YESNO) {
		for (text = value; *text; text++) {
			if (!isalnum(*text))
				continue;
			if (tolower(*text) == 'y' || (tolower(*text) == 'o' &&
tolower(*(text+1)) == 'n') || *text == '1' || tolower(*text) == 't') {
				ret = 1;
				break;
			}
		}
	}
	else if (flags == CFG_SIZE) {
		int mfactor = 1;
		char *sz;
		for (text = value; *text; text++) {
			if (!isalpha(*text))
				text++;
			if (isalpha(*text)) {
				if (tolower(*text) == 'k') 
					mfactor = 1024;
				else if (tolower(*text) == 'm') 
					mfactor = 1048576;
				else if (tolower(*text) == 'g') 
					mfactor = 1073741824;
				else 
					mfactor = 1;
				sz = text;
				while (isalpha(*text))
					text++;

				*sz-- = 0;
				while (sz-- > value && *sz) {
					if (isspace(*sz)) 
						*sz = 0;
					if (!isdigit(*sz)) 
						break;
				}
				ret += atoi(sz+1)*mfactor;
				
			}
		}
		mfactor = 1;
		sz = text;
		sz--;
		while (sz-- > value) {
			if (isspace(*sz)) 
				*sz = 0;
			if (!isdigit(*sz)) 
				break;
		}
		ret += atoi(sz+1)*mfactor;

		
	}
	else if (flags == CFG_TIME) {
		int mfactor = 1;
		char *sz;
		for (text = value; *text; text++) {
			if (!isalpha(*text))
				text++;
			if (isalpha(*text)) {
				if (tolower(*text) == 'w')
					mfactor = 604800;	
				else if (tolower(*text) == 'd') 
					mfactor = 86400;
				else if (tolower(*text) == 'h') 
					mfactor = 3600;
				else if (tolower(*text) == 'm') 
					mfactor = 60;
				else 
					mfactor = 1;
				sz = text;
				while (isalpha(*text))
					text++;

				*sz-- = 0;
				while (sz-- > value && *sz) {
					if (isspace(*sz)) 
						*sz = 0;
					if (!isdigit(*sz)) 
						break;
				}
				ret += atoi(sz+1)*mfactor;
				
			}
		}
		mfactor = 1;
		sz = text;
		sz--;
		while (sz-- > value) {
			if (isspace(*sz)) 
				*sz = 0;
			if (!isdigit(*sz)) 
				break;
		}
		ret += atoi(sz+1)*mfactor;

		
	}

	return ret;
}

ConfigFile *config_load(char *filename)
{
	struct stat sb;
	int			fd;
	int			ret;
	char		*buf = NULL;
	ConfigFile	*cfptr;

#ifndef _WIN32
	fd = open(filename, O_RDONLY);
#else
	fd = open(filename, O_RDONLY|O_BINARY);
#endif
	if (fd == -1)
	{
		config_error("Couldn't open \"%s\": %s\n", filename, strerror(errno));
		return NULL;
	}
	if (fstat(fd, &sb) == -1)
	{
		config_error("Couldn't fstat \"%s\": %s\n", filename, strerror(errno));
		close(fd);
		return NULL;
	}
	if (!sb.st_size)
	{
		close(fd);
		return NULL;
	}
	buf = MyMalloc(sb.st_size+1);
	if (buf == NULL)
	{
		config_error("Out of memory trying to load \"%s\"\n", filename);
		close(fd);
		return NULL;
	}
	ret = read(fd, buf, sb.st_size);
	if (ret != sb.st_size)
	{
		config_error("Error reading \"%s\": %s\n", filename,
			ret == -1 ? strerror(errno) : strerror(EFAULT));
		free(buf);
		close(fd);
		return NULL;
	}
	/* Just me or could this cause memory corrupted when ret <0 ? */
	buf[ret] = '\0';
	close(fd);
	cfptr = config_parse(filename, buf);
	free(buf);
	return cfptr;
}

void config_free(ConfigFile *cfptr)
{
	ConfigFile	*nptr;

	for(;cfptr;cfptr=nptr)
	{
		nptr = cfptr->cf_next;
		if (cfptr->cf_entries)
			config_entry_free(cfptr->cf_entries);
		if (cfptr->cf_filename)
			free(cfptr->cf_filename);
		free(cfptr);
	}
}

/* This is the internal parser, made by Chris Behrens & Fred Jacobs */
static ConfigFile *config_parse(char *filename, char *confdata)
{
	char		*ptr;
	char		*start;
	int			linenumber = 1;
	ConfigEntry	*curce;
	ConfigEntry	**lastce;
	ConfigEntry	*cursection;

	ConfigFile	*curcf;
	ConfigFile	*lastcf;

	lastcf = curcf = MyMalloc(sizeof(ConfigFile));
	memset(curcf, 0, sizeof(ConfigFile));
	curcf->cf_filename = strdup(filename);
	lastce = &(curcf->cf_entries);
	curce = NULL;
	cursection = NULL;
	/* Replace \r's with spaces .. ugly ugly -Stskeeps */
	for (ptr=confdata; *ptr; ptr++)
		if (*ptr == '\r')
			*ptr = ' ';

	for(ptr=confdata;*ptr;ptr++)
	{
		switch(*ptr)
		{
			case ';':
				if (!curce)
				{
					config_status("%s:%i Ignoring extra semicolon\n",
						filename, linenumber);
					break;
				}
				*lastce = curce;
				lastce = &(curce->ce_next);
				curce->ce_fileposend = (ptr - confdata);
				curce = NULL;
				break;
			case '{':
				if (!curce)
				{
					config_status("%s:%i: No name for section start\n",
							filename, linenumber);
					continue;
				}
				else if (curce->ce_entries)
				{
					config_status("%s:%i: Ignoring extra section start\n",
							filename, linenumber);
					continue;
				}
				curce->ce_sectlinenum = linenumber;
				lastce = &(curce->ce_entries);
				cursection = curce;
				curce = NULL;
				break;
			case '}':
				if (curce)
				{
					config_error("%s:%i: Missing semicolon before close brace\n",
						filename, linenumber);
					config_entry_free(curce);
					config_free(curcf);

					return NULL;
				}
				else if (!cursection)
				{
					config_status("%s:%i: Ignoring extra close brace\n",
						filename, linenumber);
					continue;
				}
				curce = cursection;
				cursection->ce_fileposend = (ptr - confdata);
				cursection = cursection->ce_prevlevel;
				if (!cursection)
					lastce = &(curcf->cf_entries);
				else
					lastce = &(cursection->ce_entries);
				for(;*lastce;lastce = &((*lastce)->ce_next))
					continue;
				break;
			case '#':
				ptr++;
				while(*ptr && (*ptr != '\n'))
					 ptr++;
				if (!*ptr)
					break;
				ptr--;
				continue;
			case '/':
				if (*(ptr+1) == '/')
				{
					ptr += 2;
					while(*ptr && (*ptr != '\n'))
						ptr++;
					if (!*ptr)
						break;
					ptr--; /* grab the \n on next loop thru */
					continue;
				}
				else if (*(ptr+1) == '*')
				{
					int commentstart = linenumber;
					int commentlevel = 1;

					for(ptr+=2;*ptr;ptr++)
					{
						if ((*ptr == '/') && (*(ptr+1) == '*'))
						{
							commentlevel++;
							ptr++;
						}

						else if ((*ptr == '*') && (*(ptr+1) == '/'))
						{
							commentlevel--;
							ptr++;
						}

						else if (*ptr == '\n')
							linenumber++;

						if (!commentlevel)
							break;
					}
					if (!*ptr)
					{
						config_error("%s:%i Comment on this line does not end\n",
							filename, commentstart);
						config_entry_free(curce);
						config_free(curcf);
						return NULL;
					}
				}
				break;
			case '\"':
				start = ++ptr;
				for(;*ptr;ptr++)
				{
					if ((*ptr == '\\') && (*(ptr+1) == '\"'))
					{
						char *tptr = ptr;
						while((*tptr = *(tptr+1)))
							tptr++;
					}
					else if ((*ptr == '\"') || (*ptr == '\n'))
						break;
				}
				if (!*ptr || (*ptr == '\n'))
				{
					config_error("%s:%i: Unterminated quote found\n",
							filename, linenumber);
					config_entry_free(curce);
					config_free(curcf);
					return NULL;
				}
				if (curce)
				{
					if (curce->ce_vardata)
					{
						config_status("%s:%i: Ignoring extra data\n",
							filename, linenumber);
					}
					else
					{
						curce->ce_vardata = MyMalloc(ptr-start+1);
						strncpy(curce->ce_vardata, start, ptr-start);
						curce->ce_vardata[ptr-start] = '\0';
					}
				}
				else
				{
					curce = MyMalloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = MyMalloc((ptr-start)+1);
					strncpy(curce->ce_varname, start, ptr-start);
					curce->ce_varname[ptr-start] = '\0';
					curce->ce_varlinenum = linenumber;
					curce->ce_fileptr = curcf;
					curce->ce_prevlevel = cursection;
					curce->ce_fileposstart = (start - confdata);
				}
				break;
			case '\n':
				linenumber++;
				/* fall through */
			case '\t':
			case ' ':
			case '=':
			case '\r':
				break;
			default:
				if ((*ptr == '*') && (*(ptr+1) == '/'))
				{
					config_status("%s:%i Ignoring extra end comment\n",
						filename, linenumber);
					ptr++;
					break;
				}
				start = ptr;
				for(;*ptr;ptr++)
				{
					if ((*ptr == ' ') || (*ptr == '=') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == ';'))
						break;
				}
				if (!*ptr)
				{
					if (curce)
						config_error("%s: Unexpected EOF for variable starting at %i\n",
							filename, curce->ce_varlinenum);
					else if (cursection) 
						config_error("%s: Unexpected EOF for section starting at %i\n",
							filename, cursection->ce_sectlinenum);
					else
						config_error("%s: Unexpected EOF.\n", filename);
					config_entry_free(curce);
					config_free(curcf);
					return NULL;
				}
				if (curce)
				{
					if (curce->ce_vardata)
					{
						config_status("%s:%i: Ignoring extra data\n",
							filename, linenumber);
					}
					else
					{
						curce->ce_vardata = MyMalloc(ptr-start+1);
						strncpy(curce->ce_vardata, start, ptr-start);
						curce->ce_vardata[ptr-start] = '\0';
					}
				}
				else
				{
					curce = MyMalloc(sizeof(ConfigEntry));
					memset(curce, 0, sizeof(ConfigEntry));
					curce->ce_varname = MyMalloc((ptr-start)+1);
					strncpy(curce->ce_varname, start, ptr-start);
					curce->ce_varname[ptr-start] = '\0';
					curce->ce_varlinenum = linenumber;
					curce->ce_fileptr = curcf;
					curce->ce_prevlevel = cursection;
					curce->ce_fileposstart = (start - confdata);
				}
				if ((*ptr == ';') || (*ptr == '\n'))
					ptr--;
				break;
		} /* switch */
	} /* for */
	if (curce)
	{
		config_error("%s: Unexpected EOF for variable starting on line %i\n",
			filename, curce->ce_varlinenum);
		config_entry_free(curce);
		config_free(curcf);
		return NULL;
	}
	else if (cursection)
	{
		config_error("%s: Unexpected EOF for section starting on line %i\n",
				filename, cursection->ce_sectlinenum);
		config_free(curcf);
		return NULL;
	}
	return curcf;
}

static void config_entry_free(ConfigEntry *ceptr)
{
	ConfigEntry	*nptr;

	for(;ceptr;ceptr=nptr)
	{
		nptr = ceptr->ce_next;
		if (ceptr->ce_entries)
			config_entry_free(ceptr->ce_entries);
		if (ceptr->ce_varname)
			free(ceptr->ce_varname);
		if (ceptr->ce_vardata)
			free(ceptr->ce_vardata);
		free(ceptr);
	}
}

ConfigEntry		*config_find_entry(ConfigEntry *ce, char *name)
{
	ConfigEntry *cep;
	
	for (cep = ce; cep; cep = cep->ce_next)
		if (cep->ce_varname && !strcmp(cep->ce_varname, name))
			break;
	return cep;
}


void config_error(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "[error] %s\n", buffer);
#else
		win_log("[error] %s", buffer);
#endif
	else
		ircd_log(LOG_ERROR, "config error: %s", buffer);
	sendto_realops("error: %s", buffer);
	/* We cannot live with this */
	config_error_flag = 1;
}

/* Like above */
void config_status(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "* %s\n", buffer);
#else
		win_log("* %s", buffer);
#endif
	sendto_realops("%s", buffer);
}

void config_progress(char *format, ...)
{
	va_list		ap;
	char		buffer[1024];
	char		*ptr;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);
	if ((ptr = strchr(buffer, '\n')) != NULL)
		*ptr = '\0';
	if (!loop.ircd_booted)
#ifndef _WIN32
		fprintf(stderr, "* %s\n", buffer);
#else
		win_log("* %s", buffer);
#endif
	sendto_realops("%s", buffer);
}


int	init_conf(char *rootconf, int rehash)
{

	if (conf)
	{
		config_error("%s:%i - Someone forgot to clean up", __FILE__, __LINE__);
		return -1;
	}

	if (load_conf(rootconf) > 0)
	{
		if (config_test() < 0)
		{
			config_error("IRCd configuration failed to pass testing");
#ifdef _WIN32
			if (!rehash)
				win_error();
#endif
			return -1;
		}
		if (rehash)
			config_rehash();
		if (config_run() < 0)
		{
			config_error("Bad case of config errors. Server will now die. This really shouldn't happen");
#ifdef _WIN32
			if (!rehash)
				win_error();
#endif
			abort();
		}
	}
	else	
	{
		config_error("IRCd configuration failed to load");
#ifdef _WIN32
		if (!rehash)
			win_error();
#endif
		return -1;
	}
	return 0;
}

int	load_conf(char *filename)
{
	ConfigFile 	*cfptr, *cfptr2, **cfptr3;
	ConfigEntry 	*ce;
	int		ret;

	config_status("Loading config file %s ..", filename);
	if ((cfptr = config_load(filename)))
	{
		for (cfptr3 = &conf, cfptr2 = conf; cfptr2; cfptr2 = cfptr2->cf_next)
			cfptr3 = &cfptr2->cf_next;
		*cfptr3 = cfptr;
		config_status("Searching through %s for include files..", filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
			if (!strcmp(ce->ce_varname, "include"))
			{
				 ret = _conf_include(cfptr, ce);
				 if (ret < 0) 
					 	return ret;
			}
			return 1;
	}
	else
	{
		config_error("Could not load config file %s", filename);
		return -1;
	}	
}

void	config_rehash()
{
}

int	config_run()
{
	return -1;
}

ConfigCommand *config_binary_search(char *cmd) {
	int start = 0;
	int stop = sizeof(_ConfigCommands)/sizeof(_ConfigCommands[0])-1;
	int mid;
	while (start <= stop) {
		mid = (start+stop)/2;
		if (smycmp(cmd,_ConfigCommands[mid].name) < 0) {
			stop = mid-1;
		}
		else if (smycmp(cmd,_ConfigCommands[mid].name) == 0) {
			return &_ConfigCommands[mid];
		}
		else
			start = mid+1;
	}
	return NULL;
}

int	config_test()
{
	ConfigEntry 	*ce;
	ConfigFile	*cfptr;
	ConfigCommand	*cc;
	int		errors = 0;

	for (cfptr = conf; cfptr; cfptr = cfptr->cf_next)
	{
		config_status("Testing %s", cfptr->cf_filename);
		for (ce = cfptr->cf_entries; ce; ce = ce->ce_next)
		{
			if (!ce->ce_varname)
			{
				config_error("%s:%i: %s:%i: null ce->ce_varname",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					__FILE__, __LINE__);
				return -1;
			}
			if ((cc = config_binary_search(ce->ce_varname))) {
				if ((cc->testfunc) && (cc->testfunc(cfptr, ce) < 0))
					errors++;
			}
			else 
			{
				config_error("%s:%i: unknown directive %s", 
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
					ce->ce_varname);
				errors++;
			}
		}
	}
	if (errors > 0)
	{
		config_error("%i errors encountered", errors);
	}
	return (errors > 0 ? -1 : 1);
}

/*
 * Service functions
*/

ConfigItem_deny_dcc	*Find_deny_dcc(char *name)
{
	ConfigItem_deny_dcc	*p;

	if (!name)
		return NULL;

	for (p = conf_deny_dcc; p; p = (ConfigItem_deny_dcc *) p->next)
	{
		if (!match(name, p->filename))
			return (p);
	}
	return NULL;
}

ConfigItem_alias *Find_alias(char *name) {
	ConfigItem_alias *alias;

	if (!name)
		return NULL;

	for (alias = conf_alias; alias; alias = (ConfigItem_alias *)alias->next) {
		if (!stricmp(alias->alias, name))
			return alias;
	}
	return NULL;
}

ConfigItem_class	*Find_class(char *name)
{
	ConfigItem_class	*p;

	if (!name)
		return NULL;

	for (p = conf_class; p; p = (ConfigItem_class *) p->next)
	{
		if (!strcmp(name, p->name))
			return (p);
	}
	return NULL;
}

ConfigItem_oper	*Find_oper(char *name)
{
	ConfigItem_oper	*p;

	if (!name)
		return NULL;

	for (p = conf_oper; p; p = (ConfigItem_oper *) p->next)
	{
		if (!strcmp(name, p->name))
			return (p);
	}
	return NULL;
}

ConfigItem_listen	*Find_listen(char *ipmask, int port)
{
	ConfigItem_listen	*p;

	if (!ipmask)
		return NULL;

	for (p = conf_listen; p; p = (ConfigItem_listen *) p->next)
	{
		if (!match(p->ip, ipmask) && (port == p->port))
			return (p);
		if (!match(ipmask, p->ip) && (port == p->port))
			return (p);
	}
	return NULL;
}

ConfigItem_ulines *Find_uline(char *host) {
	ConfigItem_ulines *ulines;

	if (!host)
		return NULL;

	for(ulines = conf_ulines; ulines; ulines =(ConfigItem_ulines *) ulines->next) {
		if (!stricmp(host, ulines->servername))
			return ulines;
	}
	return NULL;
}


ConfigItem_except *Find_except(char *host, short type) {
	ConfigItem_except *excepts;

	if (!host)
		return NULL;

	for(excepts = conf_except; excepts; excepts =(ConfigItem_except *) excepts->next) {
		if (excepts->flag.type == type)
			if (!match(excepts->mask, host))
				return excepts;
	}
	return NULL;
}

ConfigItem_tld *Find_tld(char *host) {
	ConfigItem_tld *tld;

	if (!host)
		return NULL;

	for(tld = conf_tld; tld; tld = (ConfigItem_tld *) tld->next)
	{
		if (!match(tld->mask, host))
			return tld;
	}
	return NULL;
}


ConfigItem_link *Find_link(char *username,
			   char *hostname,
			   char *ip,
			   char *servername)
{
	ConfigItem_link	*link;

	if (!username || !hostname || !servername || !ip)
		return NULL;

	for(link = conf_link; link; link = (ConfigItem_link *) link->next)
	{
		if (!match(link->servername, servername) &&
		    !match(link->username, username) &&
		    (!match(link->hostname, hostname) || !match(link->hostname, ip)))
			return link;
	}
	return NULL;

}

ConfigItem_ban 	*Find_ban(char *host, short type)
{
	ConfigItem_ban *ban;

	/* Check for an except ONLY if we find a ban, makes it
	 * faster since most users will not have a ban so excepts
	 * don't need to be searched -- codemastr
	 */

	for (ban = conf_ban; ban; ban = (ConfigItem_ban *) ban->next)
		if (ban->flag.type == type)
			if (!match(ban->mask, host)) {
				/* Person got a exception */
				if (type == CONF_BAN_USER && Find_except(host, CONF_EXCEPT_BAN))
					return NULL;
				return ban;
			}
	return NULL;
}

ConfigItem_ban 	*Find_banEx(char *host, short type, short type2)
{
	ConfigItem_ban *ban;

	/* Check for an except ONLY if we find a ban, makes it
	 * faster since most users will not have a ban so excepts
	 * don't need to be searched -- codemastr
	 */

	for (ban = conf_ban; ban; ban = (ConfigItem_ban *) ban->next)
		if ((ban->flag.type == type) && (ban->flag.type2 == type2))
			if (!match(ban->mask, host)) {
				/* Person got a exception */
				if (Find_except(host, type))
					return NULL;
				return ban;
			}
	return NULL;
}

int	AllowClient(aClient *cptr, struct hostent *hp, char *sockhost)
{
	ConfigItem_allow *aconf;
	char *hname;
	int  i, ii = 0;
	static char uhost[HOSTLEN + USERLEN + 3];
	static char fullname[HOSTLEN + 1];

	for (aconf = conf_allow; aconf; aconf = (ConfigItem_allow *) aconf->next)
	{
		if (!aconf->hostname || !aconf->ip)
			goto attach;
		if (hp)
			for (i = 0, hname = hp->h_name; hname;
			    hname = hp->h_aliases[i++])
			{
				strncpyzt(fullname, hname,
				    sizeof(fullname));
				add_local_domain(fullname,
				    HOSTLEN - strlen(fullname));
				Debug((DEBUG_DNS, "a_il: %s->%s",
				    sockhost, fullname));
				if (index(aconf->hostname, '@'))
				{
					/*
					 * Doing strlcpy / strlcat here
					 * would simply be a waste. We are
					 * ALREADY sure that it is proper 
					 * lengths
					*/
					(void)strcpy(uhost, cptr->username);
					(void)strcat(uhost, "@");
				}
				else
					*uhost = '\0';
				/* 
				 * Same here as above
				 * -Stskeeps 
				*/
				(void)strncat(uhost, fullname,
				    sizeof(uhost) - strlen(uhost));
				if (!match(aconf->hostname, uhost))
					goto attach;
			}

		if (index(aconf->ip, '@'))
		{
			strncpyzt(uhost, cptr->username, sizeof(uhost));
			(void)strcat(uhost, "@");
		}
		else
			*uhost = '\0';
		(void)strncat(uhost, sockhost, sizeof(uhost) - strlen(uhost));
		if (!match(aconf->ip, uhost))
			goto attach;
		continue;
	      attach:
/*		if (index(uhost, '@'))  now flag based -- codemastr */
		if (!aconf->flags.noident)
			cptr->flags |= FLAGS_DOID;
		if (!aconf->flags.useip && hp) 
			strncpyzt(uhost, fullname, sizeof(uhost));
		else
			strncpyzt(uhost, sockhost, sizeof(uhost));
		get_sockhost(cptr, uhost);
		/* FIXME */
		if (aconf->maxperip)
		{
			ii = 1;
			for (i = LastSlot; i >= 0; i--)
				if (local[i] && MyClient(local[i]) &&
#ifndef INET6
				    local[i]->ip.S_ADDR == cptr->ip.S_ADDR)
#else
				    !bcmp(local[i]->ip.S_ADDR, cptr->ip.S_ADDR, sizeof(cptr->ip.S_ADDR)))
#endif
				{
					ii++;
					if (ii > aconf->maxperip)
					{
						exit_client(cptr, cptr, &me,
							"Too many connections from your IP");
						return -5;	/* Already got one with that ip# */
					}
				}
		}
		if ((i = Auth_Check(cptr, aconf->auth, cptr->passwd)) == -1)
		{
			exit_client(cptr, cptr, &me,
				"Password mismatch");
			return -5;
		}
		if ((i == 2) && (cptr->passwd))
		{
			MyFree(cptr->passwd);
			cptr->passwd = NULL;
		}
		if (!((aconf->class->clients + 1) > aconf->class->maxclients))
		{
			cptr->class = aconf->class;
			cptr->class->clients++;
		}
		else
		{
			sendto_one(cptr, rpl_str(RPL_REDIR), me.name, cptr->name, aconf->server ? aconf->server : defserv, aconf->port ? aconf->port : 6667);
			return -3;
		}
		return 0;
	}
	return -1;
}

ConfigItem_vhost *Find_vhost(char *name) {
	ConfigItem_vhost *vhost;

	for (vhost = conf_vhost; vhost; vhost = (ConfigItem_vhost *)vhost->next) {
		if (!strcmp(name, vhost->login))
			return vhost;
	}
	return NULL;
}


ConfigItem_deny_channel *Find_channel_allowed(char *name)
{
	ConfigItem_deny_channel *dchannel;
	ConfigItem_allow_channel *achannel;

	for (dchannel = conf_deny_channel; dchannel; dchannel = (ConfigItem_deny_channel *)dchannel->next)
	{
		if (!match(dchannel->channel, name))
			break;
	}
	if (dchannel)
	{
		for (achannel = conf_allow_channel; achannel; achannel = (ConfigItem_allow_channel *)achannel->next)
		{
			if (!match(achannel->channel, name))
				break;
		}
		if (achannel)
			return NULL;
		else
			return (dchannel);
	}
	return NULL;
}

void init_dynconf(void)
{
	bzero(&iConf, sizeof(iConf));
}

/* Report the unrealircd.conf info -codemastr*/
void report_dynconf(aClient *sptr)
{
	sendto_one(sptr, ":%s %i %s :*** Dynamic Configuration Report ***",
	    me.name, RPL_TEXT, sptr->name);
	sendto_one(sptr, ":%s %i %s :kline-address: %s", me.name, RPL_TEXT,
	    sptr->name, KLINE_ADDRESS);
	sendto_one(sptr, ":%s %i %s :modes-on-connect: %s", me.name, RPL_TEXT,
	    sptr->name, get_modestr(CONN_MODES));
	if (OPER_ONLY_STATS)
		sendto_one(sptr, ":%s %i %s :oper-only-stats: %s", me.name, RPL_TEXT,
			sptr->name, OPER_ONLY_STATS);
	sendto_one(sptr, ":%s %i %s :anti-spam-quit-message-time: %d", me.name, RPL_TEXT,
		sptr->name, ANTI_SPAM_QUIT_MSG_TIME);
#ifdef USE_SSL
	sendto_one(sptr, ":%s %i %s :ssl::egd: %s", me.name, RPL_TEXT,
		sptr->name, EGD_PATH ? EGD_PATH : (USE_EGD ? "1" : "0"));
	sendto_one(sptr, ":%s %i %s :ssl::certificate: %s", me.name, RPL_TEXT,
		sptr->name, SSL_SERVER_CERT_PEM);
	sendto_one(sptr, ":%s %i %s :ssl::key: %s", me.name, RPL_TEXT,
		sptr->name, SSL_SERVER_KEY_PEM);
	sendto_one(sptr, ":%s %i %s :ssl::trusted-ca-file: %s", me.name, RPL_TEXT, sptr->name,
	 iConf.trusted_ca_file ? iConf.trusted_ca_file : "<none>");
	sendto_one(sptr, ":%s %i %s :ssl::options: %s %s %s", me.name, RPL_TEXT, sptr->name,
		iConf.ssl_options & SSLFLAG_FAILIFNOCERT ? "FAILIFNOCERT" : "",
		iConf.ssl_options & SSLFLAG_VERIFYCERT ? "VERIFYCERT" : "",
		iConf.ssl_options & SSLFLAG_DONOTACCEPTSELFSIGNED ? "DONOTACCEPTSELFSIGNED" : "");
#endif

	sendto_one(sptr, ":%s %i %s :options::show-opermotd: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWOPERMOTD);
	sendto_one(sptr, ":%s %i %s :options::hide-ulines: %d", me.name, RPL_TEXT,
	    sptr->name, HIDE_ULINES);
	sendto_one(sptr, ":%s %i %s :options::webtv-support: %d", me.name, RPL_TEXT,
	    sptr->name, WEBTV_SUPPORT);
	sendto_one(sptr, ":%s %i %s :options::no-stealth: %d", me.name, RPL_TEXT,
	    sptr->name, NO_OPER_HIDING);
	sendto_one(sptr, ":%s %i %s :options::identd-check: %d", me.name, RPL_TEXT,
	    sptr->name, IDENT_CHECK);
	sendto_one(sptr, ":%s %i %s :options::fail-oper-warn: %d", me.name, RPL_TEXT,
	    sptr->name, FAILOPER_WARN);
	sendto_one(sptr, ":%s %i %s :options::show-connect-info: %d", me.name, RPL_TEXT,
	    sptr->name, SHOWCONNECTINFO);
	sendto_one(sptr, ":%s %i %s :maxchannelsperuser: %i", me.name, RPL_TEXT,
	    sptr->name, MAXCHANNELSPERUSER);
	sendto_one(sptr, ":%s %i %s :auto-join: %s", me.name, RPL_TEXT,
	    sptr->name, AUTO_JOIN_CHANS ? AUTO_JOIN_CHANS : "0");
	sendto_one(sptr, ":%s %i %s :oper-auto-join: %s", me.name,
	    RPL_TEXT, sptr->name, OPER_AUTO_JOIN_CHANS ? OPER_AUTO_JOIN_CHANS : "0");
	sendto_one(sptr, ":%s %i %s :static-quit: %s", me.name, 
		RPL_TEXT, sptr->name, STATIC_QUIT ? STATIC_QUIT : "<none>");	
	sendto_one(sptr, ":%s %i %s :dns::timeout: %li", me.name, RPL_TEXT,
	    sptr->name, HOST_TIMEOUT);
	sendto_one(sptr, ":%s %i %s :dns::retries: %d", me.name, RPL_TEXT,
	    sptr->name, HOST_RETRIES);
	sendto_one(sptr, ":%s %i %s :dns::nameserver: %s", me.name, RPL_TEXT,
	    sptr->name, NAME_SERVER);
}

/* Report the network file info -codemastr */
void report_network(aClient *sptr)
{
	sendto_one(sptr, ":%s %i %s :*** Network Configuration Report ***",
	    me.name, RPL_TEXT, sptr->name);
	sendto_one(sptr, ":%s %i %s :network-name: %s", me.name, RPL_TEXT,
	    sptr->name, ircnetwork);
	sendto_one(sptr, ":%s %i %s :default-server: %s", me.name, RPL_TEXT,
	    sptr->name, defserv);
	sendto_one(sptr, ":%s %i %s :services-server: %s", me.name, RPL_TEXT,
	    sptr->name, SERVICES_NAME);
	sendto_one(sptr, ":%s %i %s :hosts::global: %s", me.name, RPL_TEXT,
	    sptr->name, oper_host);
	sendto_one(sptr, ":%s %i %s :hosts::admin: %s", me.name, RPL_TEXT,
	    sptr->name, admin_host);
	sendto_one(sptr, ":%s %i %s :hosts::local: %s", me.name, RPL_TEXT,
	    sptr->name, locop_host);
	sendto_one(sptr, ":%s %i %s :hosts::servicesadmin: %s", me.name, RPL_TEXT,
	    sptr->name, sadmin_host);
	sendto_one(sptr, ":%s %i %s :hosts::netadmin: %s", me.name, RPL_TEXT,
	    sptr->name, netadmin_host);
	sendto_one(sptr, ":%s %i %s :hosts::coadmin: %s", me.name, RPL_TEXT,
	    sptr->name, coadmin_host);
	sendto_one(sptr, ":%s %i %s :hiddenhost-prefix: %s", me.name, RPL_TEXT,
	    sptr->name, hidden_host);
	sendto_one(sptr, ":%s %i %s :help-channel: %s", me.name, RPL_TEXT,
	    sptr->name, helpchan);
	sendto_one(sptr, ":%s %i %s :stats-server: %s", me.name, RPL_TEXT,
	    sptr->name, STATS_SERVER);
	sendto_one(sptr, ":%s %i %s :hosts::host-on-oper-up: %i", me.name, RPL_TEXT, sptr->name,
	    iNAH);
	sendto_one(sptr, ":%s %i %s :cloak-keys: %X", me.name, RPL_TEXT, sptr->name,
		CLOAK_KEYCRC);
}



/*
 * Actual config parser funcs
*/

int	_conf_include(ConfigFile *conf, ConfigEntry *ce)
{
	int	ret = 0;
#ifdef GLOBH
	glob_t files;
	int i;
#elif defined(_WIN32)
	HANDLE hFind;
	WIN32_FIND_DATA FindData;
	char cPath[MAX_PATH], *cSlash = NULL, *path;
#endif
	if (!ce->ce_vardata)
	{
		config_status("%s:%i: include: no filename given",
			ce->ce_fileptr->cf_filename,
			ce->ce_varlinenum);
		return -1;
	}
#if !defined(_WIN32) && !defined(_AMIGA) && DEFAULT_PERMISSIONS != 0
	chmod(ce->ce_vardata, DEFAULT_PERMISSIONS);
#endif
#ifdef GLOBH
#if defined(__OpenBSD__) && defined(GLOB_LIMIT)
	glob(ce->ce_vardata, GLOB_NOSORT|GLOB_NOCHECK|GLOB_LIMIT, NULL, &files);
#else
	glob(ce->ce_vardata, GLOB_NOSORT|GLOB_NOCHECK, NULL, &files);
#endif
	if (!files.gl_pathc) {
		globfree(&files);
		config_status("%s:%i: include %s: invalid file given",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		return -1;
	}	
	for (i = 0; i < files.gl_pathc; i++) {
		ret = load_conf(files.gl_pathv[i]);
		if (ret < 0)
		{
			globfree(&files);
			return ret;
		}
	}
	globfree(&files);
#elif defined(_WIN32)
	bzero(cPath,MAX_PATH);
	if (strchr(ce->ce_vardata, '/') || strchr(ce->ce_vardata, '\\')) {
		strncpyzt(cPath,ce->ce_vardata,MAX_PATH);
		cSlash=cPath+strlen(cPath);
		while(*cSlash != '\\' && *cSlash != '/' && cSlash > cPath)
			cSlash--; 
		*(cSlash+1)=0;
	}
	hFind = FindFirstFile(ce->ce_vardata, &FindData);
	if (!FindData.cFileName) {
		config_status("%s:%i: include %s: invalid file given",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			ce->ce_vardata);
		FindClose(hFind);
		return -1;
	}
	if (cPath) {
		path = MyMalloc(strlen(cPath) + strlen(FindData.cFileName)+1);
		strcpy(path,cPath);
		strcat(path,FindData.cFileName);
		ret = load_conf(path);
		free(path);
	}
	else
		ret = load_conf(FindData.cFileName);
	if (ret < 0)
	{
		FindClose(hFind);
		return ret;
	}
	ret = 0;
	while (FindNextFile(hFind, &FindData) != 0) {
		if (cPath) {
			path = MyMalloc(strlen(cPath) + strlen(FindData.cFileName)+1);
			strcpy(path,cPath);
			strcat(path,FindData.cFileName);
			ret = load_conf(path);
			free(path);
			if (ret < 0)
				break;
		}
		else
			ret = load_conf(FindData.cFileName);
	}
	FindClose(hFind);
	if (ret < 0)
		return ret;
#else
	return (load_conf(ce->ce_vardata));
#endif
	return 1;
}

int	_test_include(ConfigFile *conf, ConfigEntry *ce)
{
	return 1;
}

int	_conf_admin(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_admin *ca;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		ca = MyMallocEx(sizeof(ConfigItem_admin));
		if (!conf_admin)
			conf_admin_tail = ca;
		ircstrdup(ca->line, cep->ce_varname);
		AddListItem(ca, conf_admin);
	}
	return 1;
}

int	_test_admin(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int 	    errors = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank admin item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			errors++;
			continue;
		}
	}
	return (errors == 0 ? 1 : -1);
}

int	_conf_me(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (!conf_me)
	{
		conf_me = MyMallocEx(sizeof(ConfigItem_me));
	}
	cep = config_find_entry(ce->ce_entries, "name");
	ircfree(conf_me->name);
	ircstrdup(conf_me->name, cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "info");
	ircfree(conf_me->info);
	ircstrdup(conf_me->name, cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "numeric");
	conf_me->numeric = atol(cep->ce_vardata);
	return 1;
}

int	_test_me(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	long	    l;
	int	    errors = 0;
	
	if (!(cep = config_find_entry(ce->ce_entries, "name")))
	{
		config_error("%s:%i: me::name missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: me::name without contents",
				cep->ce_fileptr->cf_filename, ce->ce_varlinenum);
			errors++;
		}
		else
		{
			if (!strchr(cep->ce_vardata, '.'))
			{	
				config_error("%s:%i: illegal me::name, must be fully qualified hostname",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
		}
	}
	if (!(cep = config_find_entry(ce->ce_entries, "info")))
	{
		config_error("%s:%i: me::info missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: me::info without contents",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			errors++;
		}
		else
		if (strlen(cep->ce_vardata) > (REALLEN-1))
		{
			config_error("%s:%i: too long me::info, must be max. %i characters",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum, REALLEN-1);
			errors++;
		
		}
	}
	if (!(cep = config_find_entry(ce->ce_entries, "numeric")))
	{
		config_error("%s:%i: me::numeric missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: me::name without contents",
				cep->ce_fileptr->cf_filename, ce->ce_varlinenum);
			errors++;
		}
		else
		{
			l = atol(cep->ce_vardata);
			if ((l < 0) && (l > 254))
			{
				config_error("%s:%i: illegal me::numeric error (must be between 0 and 254)",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);
				errors++;
			}
		}
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank me line",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			errors++;
			continue;
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: me::%s without parameter",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "name"))
		{} else
		if (!strcmp(cep->ce_varname, "info"))
		{} else
		if (!strcmp(cep->ce_varname, "numeric"))
		{}
		else
		{
			config_error("%s:%i: unknown directive me::%s",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_varname);
			errors++; continue;
		}
	}
	return (errors == 0 ? 1 : -1);
}

/*
 * The oper {} block parser
*/

int	_conf_oper(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_oper *oper = NULL;
	ConfigItem_oper_from *from;
	OperFlag *ofp = NULL;
	unsigned char	isnew = 0;

	if (!ce->ce_vardata)
	{
		config_status("%s:%i: oper without name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}

	if (!(oper = Find_oper(ce->ce_vardata)))
	{
		oper =  MyMallocEx(sizeof(ConfigItem_oper));
		oper->name = strdup(ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
	}
	
	cep = config_find_entry(ce->ce_entries, "password");
	oper->auth = Auth_ConvertConf2AuthStruct(cep);
	cep = config_find_entry(ce->ce_entries, "class");
	oper->class = Find_class(cep->ce_vardata);
	if (!oper->class)
	{
		config_status("%s:%i: illegal oper::class, unknown class '%s' using default of class 'default'",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_vardata);
		oper->class = default_class;
	}
	
	cep = config_find_entry(ce->ce_entries, "flags");
	if (!cep->ce_entries)
	{
		char *m = "*";
		int *i, flag;

		for (m = (*cep->ce_vardata) ? cep->ce_vardata : m; *m; m++) {
			for (i = _OldOperFlags; (flag = *i); i += 2)
				if (*m == (char)(*(i + 1))) {
					oper->oflags |= flag;
					break;
				}
		}
	}
	else
	{
		for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
		{
			/* this should have been olp ;) -Stskeeps */
			for (ofp = _OperFlags; ofp->name; ofp++)
			{
				if (!strcmp(ofp->name, cepp->ce_varname))
				{
					oper->oflags |= ofp->flag;
					break;
				}
			}
		}
	}
	if ((cep = config_find_entry(ce->ce_entries, "swhois")))
	{
		ircstrdup(oper->swhois, cep->ce_vardata);
	}
	if ((cep = config_find_entry(ce->ce_entries, "snomask")))
	{
		ircstrdup(oper->snomask, cep->ce_vardata);
	}
	cep = config_find_entry(ce->ce_entries, "from");
	for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
	{
		if (!strcmp(cepp->ce_varname, "userhost"))
		{
			from = MyMallocEx(sizeof(ConfigItem_oper_from));
			ircstrdup(from->name, cepp->ce_vardata);
			AddListItem(from, oper->from);
		}
	}
	if (isnew)
		AddListItem(oper, conf_oper);
	return 1;
}

int	_test_oper(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_oper *oper = NULL;
	ConfigItem_oper_from *from;
	OperFlag *ofp = NULL;
	unsigned char	isnew = 0;
	int	errors = 0;
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: oper without name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: oper item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "password"))
		{
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: oper::password without contents",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			/* should have some auth check if ok .. */
			continue;
		}
		if (!cep->ce_entries)
		{
			/* standard variable */
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: oper::%s without parameter",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum,
					cep->ce_varname);
				errors++; continue;
			}
			if (!strcmp(cep->ce_varname, "class"))
			{
			}
			else if (!strcmp(cep->ce_varname, "swhois")) {
			}
			else if (!strcmp(cep->ce_varname, "snomask")) {
			}
			else if (!strcmp(cep->ce_varname, "flags"))
			{
			}
			else
			{
				config_error("%s:%i: unknown directive oper::%s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
						cep->ce_varname);
				errors++; continue;
			}
		}
		else
		{
			/* Section */
			if (!strcmp(cep->ce_varname, "flags"))
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!cepp->ce_varname)
					{
						config_error("%s:%i: oper::flags item without variable name",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++; continue;
					}
					/* this should have been olp ;) -Stskeeps */
					for (ofp = _OperFlags; ofp->name; ofp++)
					{
						if (!strcmp(ofp->name, cepp->ce_varname))
							break;
					}
					if (!ofp->name)
					{
						config_error("%s:%i: unknown oper flag '%s'",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
							cepp->ce_varname);
						errors++; continue;
					}
				}
				continue;
			}
			else
			if (!strcmp(cep->ce_varname, "from"))
			{
				for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
				{
					if (!cepp->ce_varname)
					{
						config_error("%s:%i: oper::from item without variable name",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++; continue;
					}
					if (!cepp->ce_vardata)
					{
						config_error("%s:%i: oper::from::%s without parameter",
							cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum,
							cepp->ce_varname);
						errors++; continue;
					}
					if (!strcmp(cepp->ce_varname, "userhost"))
					{
					}
					else
					{
						config_error("%s:%i: unknown directive oper::from::%s",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
							cepp->ce_varname);
						errors++; continue;
					}
				}
				continue;
			}
			else
			{
				config_error("%s:%i: unknown directive oper::%s (section)",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
						cep->ce_varname);
				errors++; continue;
			}
		}

	}
	if (!config_find_entry(ce->ce_entries, "password"))
	{
		config_error("%s:%i: oper::password missing", ce->ce_fileptr->cf_filename,
			cep->ce_varlinenum);
		errors++;
	}	
	if (!config_find_entry(ce->ce_entries, "from"))
	{
		config_error("%s:%i: oper::from missing", ce->ce_fileptr->cf_filename,
			cep->ce_varlinenum);
		errors++;
	}	
	if (!config_find_entry(ce->ce_entries, "class"))
	{
		config_error("%s:%i: oper::class missing", ce->ce_fileptr->cf_filename,
			cep->ce_varlinenum);
		errors++;
	}	
	return (errors == 0 ? 1 : -1);
	
}

/*
 * The class {} block parser
*/
int	_conf_class(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_class *class;
	unsigned char isnew = 0;

	if (!(class = Find_class(ce->ce_vardata)))
	{
		class = MyMallocEx(sizeof(ConfigItem_class));
		ircstrdup(class->name, ce->ce_vardata);
		isnew = 1;
	}
	else
	{
		isnew = 0;
	}
	cep = config_find_entry(ce->ce_entries, "pingfreq");
	class->pingfreq = atol(cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "maxclients");
	class->maxclients = atol(cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "sendq");
	class->sendq = atol(cep->ce_vardata);
	if ((cep = config_find_entry(ce->ce_entries, "connfreq")))
	{
		class->connfreq = atol(cep->ce_vardata);
	}
	if (isnew) 
		AddListItem(class, conf_class);
	return 1;
}

int	_test_class(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry 	*cep;
	long		l;
	int		errors = 0;
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: class without name",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++; 
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: class item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: class item without parameter",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "pingfreq"))
		{} else
		if (!strcmp(cep->ce_varname, "maxclients"))
		{} else
		if (!strcmp(cep->ce_varname, "connfreq"))
		{} else
		if (!strcmp(cep->ce_varname, "sendq"))
		{}
		else
		{
			config_error("%s:%i: unknown directive class::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			errors++; continue;
		}
	}
	if ((cep = config_find_entry(ce->ce_entries, "pingfreq")))
	{
		l = atol(cep->ce_vardata);
		if (l < 1)
		{
			config_error("%s:%i: class::pingfreq with illegal value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		}
	}
	else
	{
		config_error("%s:%i: class::pingfreq missing",
			ce->ce_fileptr->cf_filename, cep->ce_varlinenum);
		errors++;
	}
	if ((cep = config_find_entry(ce->ce_entries, "maxclients")))
	{
		l = atol(cep->ce_vardata);
		if (!l)
		{
			config_error("%s:%i: class::maxclients with illegal value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		}
	}
	else
	{
		config_error("%s:%i: class::maxclients missing",
			ce->ce_fileptr->cf_filename, cep->ce_varlinenum);
		errors++;
	}
	if ((cep = config_find_entry(ce->ce_entries, "sendq")))
	{
		l = atol(cep->ce_vardata);
		if (!l)
		{
			config_error("%s:%i: class::sendq with illegal value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		}
	}
	else
	{
		config_error("%s:%i: class::sendq missing",
			ce->ce_fileptr->cf_filename, cep->ce_varlinenum);
		errors++;
	}
	if ((cep = config_find_entry(ce->ce_entries, "connfreq")))
	{
		l = atol(cep->ce_vardata);
		if (l < 10)
		{
			config_error("%s:%i: class::connfreq with illegal value (<10)",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		}
	}
	
	return (errors == 0 ? 1 : -1);
}

int     _conf_drpass(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;

	if (!conf_drpass) {
		conf_drpass =  MyMallocEx(sizeof(ConfigItem_drpass));
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "restart"))
		{
			if (conf_drpass->restartauth)
				Auth_DeleteAuthStruct(conf_drpass->restartauth);
			
			conf_drpass->restartauth = Auth_ConvertConf2AuthStruct(cep);
		}
		else if (!strcmp(cep->ce_varname, "die"))
		{
			if (conf_drpass->dieauth)
				Auth_DeleteAuthStruct(conf_drpass->dieauth);
			
			conf_drpass->dieauth = Auth_ConvertConf2AuthStruct(cep);
		}
	}
	return 1;
}

int     _test_drpass(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int errors = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: drpass item without variable name",
			 cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: missing parameter in drpass:%s",
			 cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
			 	cep->ce_varname);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "restart"))
		{}
		else if (!strcmp(cep->ce_varname, "die"))
		{
		}
		else
		{
			config_status("%s:%i: warning: unknown drpass directive '%s'",
				 cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				 cep->ce_varname);
			errors++; continue;
		}
	}
	return (errors == 0 ? 1 : -1);
}

/*
 * The ulines {} block parser
*/
int	_conf_ulines(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_ulines *ca;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_status("%s:%i: blank uline item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			continue;
		}
		ca = MyMallocEx(sizeof(ConfigItem_ulines));
		ircstrdup(ca->servername, cep->ce_varname);
		AddListItem(ca, conf_ulines);
	}
	return 1;
}

int	_test_ulines(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int 	    errors = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank uline item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			errors++;
			continue;
		}
	}
	return (errors == 0 ? 1 : -1);
}

int     _conf_tld(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_tld *ca;

	ca = MyMallocEx(sizeof(ConfigItem_tld)); cep =
	config_find_entry(ce->ce_entries, "mask");
	ca->mask = strdup(cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "motd");
	ca->motd = read_motd(cep->ce_vardata); 
	ca->motd_file = strdup(cep->ce_vardata);
	ca->motd_tm = motd_tm;
	cep = config_find_entry(ce->ce_entries, "rules");
	ca->rules = read_rules(cep->ce_vardata);
	ca->rules_file = strdup(cep->ce_vardata);
	
	if ((cep = config_find_entry(ce->ce_entries, "channel")))
	{
		ca->channel = strdup(cep->ce_vardata);
	}	
	AddListItem(ca, conf_tld);
	return 1;
}

int     _test_tld(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	int	    errors = 0;
	int	    fd = -1;
        for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank tld item",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum);
			errors++; continue;
		}
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: missing parameter in tld::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_varname);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "mask")) {
		}
		else if (!strcmp(cep->ce_varname, "motd")) {
		}
		else if (!strcmp(cep->ce_varname, "rules")) {
		}
		else if (!strcmp(cep->ce_varname, "channel")) {
		}
		else
		{
			config_error("%s:%i: unknown directive tld::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_varname);
			errors++; continue;
		}
	}
	if (errors)
		return -1;

	if (!(cep = config_find_entry(ce->ce_entries, "mask")))
	{
		config_error("%s:%i: tld::mask missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	if (!(cep = config_find_entry(ce->ce_entries, "motd")))
	{
		config_error("%s:%i: tld::motd missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
		{
			config_error("%s:%i: tld::motd: %s: %s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_vardata, strerror(errno));
			errors++;
		}
		else
			close(fd);
		
	}
	
	if (!(cep = config_find_entry(ce->ce_entries, "rules")))
	{
		config_error("%s:%i: tld::rules missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (((fd = open(cep->ce_vardata, O_RDONLY)) == -1))
		{
			config_error("%s:%i: tld::rules: %s: %s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				cep->ce_vardata, strerror(errno));
			errors++;
		}
		else
			close(fd);
	}
	
	return (errors == 0 ? 1 : -1);
}

int	_conf_listen(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_listen *listen = NULL;
	OperFlag    *ofp;
	char	    copy[256];
	char	    *ip;
	char	    *port;
	int	    iport;
	unsigned char	isnew = 0;

	if (!ce->ce_vardata)
	{
		return -1;
	}

	strcpy(copy, ce->ce_vardata);
	/* Seriously cheap hack to make listen <port> work -Stskeeps */
	ipport_seperate(copy, &ip, &port);
	if (!ip || !*ip)
	{
		return -1;
	}
	if (strchr(ip, '*') && strcmp(ip, "*"))
	{
		return -1;
	}
	if (!port || !*port)
	{
		return -1;
	}
	iport = atol(port);
	if ((iport < 0) || (iport > 65535))
	{
		return -1;
	}
	if (!(listen = Find_listen(ip, iport)))
	{
		listen = MyMallocEx(sizeof(ConfigItem_listen));
		listen->ip = strdup(ip);
		listen->port = iport;
		isnew = 1;
	}
	else
	{
		isnew = 0;
	}


	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "options"))
		{
			listen->options = 0;
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				for (ofp = _ListenerFlags; ofp->name; ofp++)
				{
					if (!strcmp(ofp->name, cepp->ce_varname))
					{
						if (!(listen->options & ofp->flag))
							listen->options |= ofp->flag;
						break;
					}
				}
			}
#ifndef USE_SSL
			if (listen->options & LISTENER_SSL)
			{
				config_status("%s:%i: listen with SSL flag enabled on a non SSL compile",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
						cep->ce_varname);
				listen->options &= ~LISTENER_SSL;
			}
#endif
		}

	}
	if (isnew)
		AddListItem(listen, conf_listen);
	listen->flag.temporary = 0;
	return 1;
}

int	_test_listen(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	ConfigItem_listen *listen = NULL;
	OperFlag    *ofp;
	char	    copy[256];
	char	    *ip;
	char	    *port;
	int	    iport;
	int	    errors = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: listen without ip:port",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}

	strcpy(copy, ce->ce_vardata);
	/* Seriously cheap hack to make listen <port> work -Stskeeps */
	ipport_seperate(copy, &ip, &port);
	if (!ip || !*ip)
	{
		config_error("%s:%i: listen: illegal ip:port mask",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (strchr(ip, '*') && strcmp(ip, "*"))
	{
		config_error("%s:%i: listen: illegal ip, (mask, and not '*')",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (!port || !*port)
	{
		config_error("%s:%i: listen: missing port in mask",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	iport = atol(port);
	if ((iport < 0) || (iport > 65535))
	{
		config_error("%s:%i: listen: illegal port (must be 0..65536)",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: listen item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!cep->ce_vardata && !cep->ce_entries)
		{
			config_error("%s:%i: listen::%s without parameter",
				cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum,
				cep->ce_varname);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "options"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!cepp->ce_varname)
				{
					config_error("%s:%i: listen::options item without variable name",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
					errors++; continue;
				}
				for (ofp = _ListenerFlags; ofp->name; ofp++)
				{
					if (!strcmp(ofp->name, cepp->ce_varname))
					{
						break;
					}
				}
				if (!ofp->name)
				{
					config_error("%s:%i: unknown listen option '%s'",
						cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
						cepp->ce_varname);
					errors++; continue;
				}
			}
		}
		else
		{
			config_error("%s:%i: unknown directive listen::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			errors++; continue;
		}

	}
	return (errors == 0 ? 1 : -1);
}


int	_conf_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_allow *allow;

	if (ce->ce_vardata)
	{
		if (!strcmp(ce->ce_vardata, "channel"))
		{
			return (_conf_allow_channel(conf, ce));
		}
		else
		{
			ConfigItem_unknown *ca2 = MyMalloc(sizeof(ConfigItem_unknown));
			ca2->ce = ce;
			AddListItem(ca2, conf_unknown);
			return -1;
		}
	}

	allow = MyMallocEx(sizeof(ConfigItem_allow));
	cep = config_find_entry(ce->ce_entries, "ip");
	allow->ip = strdup(cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "hostname");
	allow->hostname = strdup(cep->ce_vardata);
	cep = config_find_entry(ce->ce_entries, "password");
	allow->auth = Auth_ConvertConf2AuthStruct(cep);
	cep = config_find_entry(ce->ce_entries, "class");
	allow->class = Find_class(cep->ce_vardata);
	if (!allow->class)
	{
		config_status("%s:%i: illegal allow::class, unknown class '%s' using default of class 'default'",
			cep->ce_fileptr->cf_filename,
			cep->ce_varlinenum,
			cep->ce_vardata);
			allow->class = default_class;
	}
	if ((cep = config_find_entry(ce->ce_entries, "maxperip")))
	{
		allow->maxperip = atoi(cep->ce_vardata);
	}
	if ((cep = config_find_entry(ce->ce_entries, "redirect-server")))
	{
		allow->server = strdup(cep->ce_vardata);
	}
	if ((cep = config_find_entry(ce->ce_entries, "redirect-port")))
	{
		allow->port = atoi(cep->ce_vardata);
	}
	if ((cep = config_find_entry(ce->ce_entries, "options")))
	{
		for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next) {
			if (!strcmp(cepp->ce_varname, "noident"))
				allow->flags.noident = 1;
			else if (!strcmp(cepp->ce_varname, "useip")) 
				allow->flags.useip = 1;
		}
	
	}
	AddListItem(allow, conf_allow);
	return 1;
}

int	_test_allow(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_allow *allow;
	unsigned char isnew = 0;
	int		errors = 0;
	if (ce->ce_vardata)
	{
		if (!strcmp(ce->ce_vardata, "channel"))
		{
			return (_test_allow_channel(conf, ce));
		}
		else
		{
			config_error("%s:%i: allow item with unknown type",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
	}

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_status("%s:%i: allow item without variable name",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "ip"))
		{
		} else
		if (!strcmp(cep->ce_varname, "maxperip"))
		{
		} else
		if (!strcmp(cep->ce_varname, "hostname"))
		{
		} else
		if (!strcmp(cep->ce_varname, "password"))
		{
		} else
		if (!strcmp(cep->ce_varname, "class"))
		{
		}
		else if (!strcmp(cep->ce_varname, "redirect-server"))
		{
		}
		else if (!strcmp(cep->ce_varname, "redirect-port")) {
		}
		else if (!strcmp(cep->ce_varname, "options")) {
		}
		else
		{
			config_error("%s:%i: unknown directive allow::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
			errors++; continue;
		}
	}
	
	return (errors > 0 ? -1 : 1);
}

int	_conf_allow_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_allow_channel 	*allow = NULL;
	ConfigEntry 	    	*cep;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			allow = MyMallocEx(sizeof(ConfigItem_allow_channel));
			ircstrdup(allow->channel, cep->ce_vardata);
			AddListItem(allow, conf_allow_channel);
		}
	}
	return 1;
}

int	_test_allow_channel(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry		*cep;
	int			errors = 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname || !cep->ce_vardata)
		{
			config_error("%s:%i: allow channel item without contents",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++; continue;
		}
		if (!strcmp(cep->ce_varname, "channel"))
		{
		}
		else
		{
			config_error("%s:%i: allow channel item with unknown type '%s'",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, 
				cep->ce_varname);
			errors++;
		}
	}
	return (errors == 0 ? 1 : -1);
}

int     _conf_except(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep, *cep2, *cep3;
	ConfigItem_except *ca;


	if (!strcmp(ce->ce_vardata, "ban")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca = MyMallocEx(sizeof(ConfigItem_except));
				ca->mask = strdup(cep->ce_vardata);
				ca->flag.type = CONF_EXCEPT_BAN;
				AddListItem(ca, conf_except);
			}
			else {
			}
		}
	}
	else if (!strcmp(ce->ce_vardata, "scan")) {
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mask")) {
				ca = MyMallocEx(sizeof(ConfigItem_except));
				ca->mask = strdup(cep->ce_vardata);
				ca->flag.type = CONF_EXCEPT_SCAN;
				AddListItem(ca, conf_except);
			}
			else {
			}
		}

	}
	else if (!strcmp(ce->ce_vardata, "tkl")) {
		cep2 = config_find_entry(ce->ce_entries, "mask");
		cep3 = config_find_entry(ce->ce_entries, "type");
		ca = MyMallocEx(sizeof(ConfigItem_except));
		ca->mask = strdup(cep2->ce_vardata);
		if (!strcmp(cep3->ce_vardata, "gline"))
			ca->type = TKL_KILL|TKL_GLOBAL;
		else if (!strcmp(cep3->ce_vardata, "gzline"))
			ca->type = TKL_ZAP|TKL_GLOBAL;
		else if (!strcmp(cep3->ce_vardata, "shun"))
			ca->type = TKL_SHUN|TKL_GLOBAL;
		else if (!strcmp(cep3->ce_vardata, "tkline"))
			ca->type = TKL_KILL;
		else if (!strcmp(cep3->ce_vardata, "tzline"))
			ca->type = TKL_ZAP;
		else 
		{}
		
		ca->flag.type = CONF_EXCEPT_TKL;
		AddListItem(ca, conf_except);
	}
	else {
	}
	return 1;
}

int     _test_except(ConfigFile *conf, ConfigEntry *ce)
{

	ConfigEntry *cep, *cep2, *cep3;
	int	    errors = 0;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: except without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}

	if (!strcmp(ce->ce_vardata, "ban")) {
		if (!config_find_entry(ce->ce_entries, "mask"))
		{
			config_error("%s:%i: except ban without mask item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: except ban item without contents",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
			}
			else
			{
				config_error("%s:%i: unknown except ban item %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				errors++;
				continue;
			}
		}
		return (errors > 0 ? -1 : 1);
	}
	else if (!strcmp(ce->ce_vardata, "scan")) {
		if (!config_find_entry(ce->ce_entries, "mask"))
		{
			config_error("%s:%i: except scan without mask item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: except scan item without contents",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mask"))
			{
			}
			else
			{
				config_error("%s:%i: unknown except scan item %s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				errors++;
				continue;
			}
		}
		return (errors > 0 ? -1 : 1);
	}
	else if (!strcmp(ce->ce_vardata, "tkl")) {
		if (!(cep2 = config_find_entry(ce->ce_entries, "mask")))
		{
			config_error("%s:%i: except tkl without mask item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		if (!(cep3 = config_find_entry(ce->ce_entries, "type")))
		{
			config_error("%s:%i: except tkl without type item",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		if (!cep2->ce_vardata)
		{
			config_error("%s:%i: except tkl::mask without contents",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		if (!cep3->ce_vardata)
		{
			config_error("%s:%i: except tkl::type without contents",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
			return -1;
		}
		if (!strcmp(cep3->ce_vardata, "gline")) {}
		else if (!strcmp(cep3->ce_vardata, "gzline")){}
		else if (!strcmp(cep3->ce_vardata, "shun")) {}
		else if (!strcmp(cep3->ce_vardata, "tkline")) {}
		else if (!strcmp(cep3->ce_vardata, "tzline")) {}
		else 
		{
			config_error("%s:%i: unknown except tkl type %s",
				cep3->ce_fileptr->cf_filename, cep3->ce_varlinenum,
				cep3->ce_vardata);
			return -1;

		}
		return 1;
	}
	else {
		config_error("%s:%i: unknown except type %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum, 
			ce->ce_vardata);
		return -1;
	}
	return 1;
}

/*
 * vhost {} block parser
*/
int	_conf_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigItem_vhost *vhost;
	ConfigItem_oper_from *from;
	ConfigEntry *cep, *cepp;
	vhost = MyMallocEx(sizeof(ConfigItem_vhost));
	cep = config_find_entry(ce->ce_entries, "vhost");
	{
		char *user, *host;
		user = strtok(cep->ce_vardata, "@");
		host = strtok(NULL, "");
		if (!host)
			vhost->virthost = strdup(user);
		else {
			vhost->virtuser = strdup(user);
			vhost->virthost = strdup(host);
		}
	}
	cep = config_find_entry(ce->ce_entries, "login");
	vhost->login = strdup(cep->ce_vardata);	
	cep = config_find_entry(ce->ce_entries, "password");
	vhost->auth = Auth_ConvertConf2AuthStruct(cep);
	cep = config_find_entry(ce->ce_entries, "from");
	for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
	{
		if (!strcmp(cepp->ce_varname, "userhost"))
		{
			from = MyMallocEx(sizeof(ConfigItem_oper_from));
			ircstrdup(from->name, cepp->ce_vardata);
			AddListItem(from, vhost->from);
		}
	}
	if ((cep = config_find_entry(ce->ce_entries, "swhois")))
		vhost->swhois = strdup(cep->ce_vardata);
	AddListItem(vhost, conf_vhost);
	return 1;
}

int	_test_vhost(ConfigFile *conf, ConfigEntry *ce)
{
	int errors = 0;
	ConfigEntry *vhost, *swhois, *from, *login, *password, *cep;
	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty vhost block", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (!(vhost = config_find_entry(ce->ce_entries, "vhost")))
	{
		config_error("%s:%i: vhost::vhost missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (!vhost->ce_vardata)
		{
			config_error("%s:%i: vhost::vhost without contents",
				vhost->ce_fileptr->cf_filename, vhost->ce_varlinenum);
			errors++;
		}	
	}
	if (!(login = config_find_entry(ce->ce_entries, "login")))
	{
		config_error("%s:%i: vhost::login missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
		
	}
	else
	{
		if (!login->ce_vardata)
		{
			config_error("%s:%i: vhost::login without contents",
				login->ce_fileptr->cf_filename, login->ce_varlinenum);
			errors++;
		}
	}
	if (!(password = config_find_entry(ce->ce_entries, "password")))
	{
		config_error("%s:%i: vhost::password missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
		
	}
	else
	{
		if (!password->ce_vardata)
		{
			config_error("%s:%i: vhost::password without contents",
				password->ce_fileptr->cf_filename, password->ce_varlinenum);
			errors++;
		}
	}
	if (!(from = config_find_entry(ce->ce_entries, "from")))
	{
		config_error("%s:%i: vhost::from missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (!from->ce_entries)
		{
			config_error("%s:%i: vhost::from block without contents",
				from->ce_fileptr->cf_filename, from->ce_varlinenum);
			errors++;
		}
		else
		{
			for (cep = from->ce_entries; cep; cep = cep->ce_next)
			{
				if (!cep->ce_varname)
				{
					config_error("%s:%i: vhost::from block item without variable name",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
					continue;
				}
				
				if (!strcmp(cep->ce_varname, "userhost"))
				{
					if (!cep->ce_vardata)
					{
						config_error("%s:%i: vhost::from::userhost item without contents",
							cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
						errors++;
						continue;	
					}
				}
				else
				{
					config_error("%s:%i: vhost::from unknown block item '%s'",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum, 
						cep->ce_varname);
					errors++;
					continue;	
				}
			}
		}
	}
	if ((swhois = config_find_entry(ce->ce_entries, "swhois")))
	{
		if (!swhois->ce_vardata)
		{
			config_error("%s:%i: vhost::swhois without contents",
				swhois->ce_fileptr->cf_filename, swhois->ce_varlinenum);
			errors++;
		}
	}
	if (errors > 0)
		return -1;
	return 1;
}

#ifdef STRIPBADWORDS
int     _conf_badword(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep;
	ConfigItem_badword *ca;
	char *tmp;
	short regex = 0;

	ca = MyMallocEx(sizeof(ConfigItem_badword));

	cep = config_find_entry(ce->ce_entries, "word");
	for (tmp = cep->ce_vardata; *tmp; tmp++) {
		if ((int)*tmp < 65 || (int)*tmp > 123) {
			regex = 1;
			break;
		}
	}
	if (regex) {
		ircstrdup(ca->word, cep->ce_vardata);
	}
	else {
		ca->word = MyMalloc(strlen(cep->ce_vardata) + strlen(PATTERN) -1);
		ircsprintf(ca->word, PATTERN, cep->ce_vardata);
	}
	if ((cep = config_find_entry(ce->ce_entries, "replace"))) {
		ircstrdup(ca->replace, cep->ce_vardata);
	}
	if (!strcmp(ce->ce_vardata, "channel"))
		AddListItem(ca, conf_badword_channel);
	else if (!strcmp(ce->ce_vardata, "message"))
		AddListItem(ca, conf_badword_message);
	return 1;

}

int _test_badword(ConfigFile *conf, ConfigEntry *ce) { 
	int errors = 0;
	ConfigEntry *word, *replace;
	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty badword block", 
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (!ce->ce_vardata)
	{
		config_error("%s:%i: badword without type",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	else if (strcmp(ce->ce_vardata, "channel") && strcmp(ce->ce_vardata, "message")) {
			config_error("%s:%i: badword with unknown type",
				ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return -1;
	}
	if (!(word = config_find_entry(ce->ce_entries, "word")))
	{
		config_error("%s:%i: badword::word missing",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}
	else
	{
		if (!word->ce_vardata)
		{
			config_error("%s:%i: badword::word without contents",
				word->ce_fileptr->cf_filename, word->ce_varlinenum);
			errors++;
		}	
	}
	if ((replace = config_find_entry(ce->ce_entries, "replace")))
	{
		if (!replace->ce_vardata)
		{
			config_error("%s:%i: badword::replace without contents",
				replace->ce_fileptr->cf_filename, replace->ce_varlinenum);
			errors++;
		}
	}	
	return (errors > 0 ? -1 : 1); 
}
#endif



/*
 * Actually use configuration
*/

void	run_configuration(void)
{
	ConfigItem_listen 	*listenptr;

	for (listenptr = conf_listen; listenptr; listenptr = (ConfigItem_listen *) listenptr->next)
	{
		if (!(listenptr->options & LISTENER_BOUND))
		{
			if (add_listener2(listenptr) == -1)
			{
				ircd_log(LOG_ERROR, "Failed to bind to %s:%i", listenptr->ip, listenptr->port);
			}
				else
			{
			}
		}
	}
}

int     rehash(aClient *cptr, aClient *sptr, int sig)
{
	return 0;
}

void	link_cleanup(ConfigItem_link *link_ptr)
{
	ircfree(link_ptr->servername);
	ircfree(link_ptr->username);
	ircfree(link_ptr->bindip);
	ircfree(link_ptr->hostname);
	ircfree(link_ptr->hubmask);
	ircfree(link_ptr->leafmask);
	ircfree(link_ptr->connpwd);
#ifdef USE_SSL
	ircfree(link_ptr->ciphers);
#endif
	Auth_DeleteAuthStruct(link_ptr->recvauth);
	link_ptr->recvauth = NULL;
}


void	listen_cleanup()
{
	int	i = 0;
	ConfigItem_listen *listen_ptr;
	ListStruct *next;
	for (listen_ptr = conf_listen; listen_ptr; listen_ptr = (ConfigItem_listen *)next)
	{
		next = (ListStruct *)listen_ptr->next;
		if (listen_ptr->flag.temporary && !listen_ptr->clients)
		{
			ircfree(listen_ptr->ip);
			DelListItem(listen_ptr, conf_listen);
			MyFree(listen_ptr);
			i++;
		}
	}
	if (i)
		close_listeners();
}
