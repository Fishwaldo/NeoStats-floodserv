/* NeoStats - IRC Statistical Services 
** Copyright (c) 1999-2004 Adam Rutter, Justin Hammond, Mark Hetherington
** http://www.neostats.net/
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
**  USA
**
** NeoStats CVS Identification
** $Id$
*/

/*  TODO:
 *  - Check for per user AJPP in addition to current AJPP checks
 *  - Option to ignore registered nicks in AJPP checks
 */

#include "neostats.h"
#include "floodserv.h"

struct fscfg {
	int verbose;
	int nickthreshold;
	int nicksampletime;
	int jointhreshold;
	int joinsampletime;
	char chanlockkey[MAXCHANLEN];
	int chanlocktime;
	int nickflood;
	int nickfloodact;
	int joinflood;
	int joinfloodact;
} fscfg;

/* channel flood tracking (ajpp = average joins per period) */
typedef struct chantrack {
	Channel *c;
	int ajpp;
	time_t ts_lastjoin;
	time_t locked;
}chantrack;

/* nick flood tracking */
typedef struct usertrack {
	Client *u;
	char nick[MAXNICK];
	int changes;
	time_t ts_lastchange;
}usertrack;

/* FloodCheck.c */
int CheckLockChan (void);

static int fs_event_signon( CmdParams *cmdparams );
static int fs_event_quit( CmdParams *cmdparams );
static int fs_event_kill( CmdParams *cmdparams );
static int fs_event_nick( CmdParams *cmdparams );
static int fs_event_newchan( CmdParams *cmdparams );
static int fs_event_delchan( CmdParams *cmdparams );
static int fs_event_joinchan( CmdParams *cmdparams );
static int fs_cmd_status( CmdParams *cmdparams );

Bot *fs_bot;

/* the hash that contains the channels we are tracking */
static hash_t *joinfloodhash;

/* the hash that contains the nicks we are tracking */
static hash_t *nickfloodhash;

static int MaxAJPP = 0;
static char MaxAJPPChan[MAXCHANLEN];

/** about info */
const char *fs_about[] = {
	"Flood protection service",
	NULL
};

const char *fs_copyright[] = {
	"Copyright (c) 1999-2004, NeoStats",
	"http://www.neostats.net/",
	NULL
};

/** Module Info definition 
 * version information about our module
 * This structure is required for your module to load and run on NeoStats
 */
ModuleInfo module_info = {
	"FloodServ",
	"Flood protection service",
	fs_copyright,
	fs_about,
	NEOSTATS_VERSION,
	"3.0prealpha",
	__DATE__,
	__TIME__,
	0,
	0,
};

static bot_setting fs_settings[]=
{
	{"VERBOSE",			&fscfg.verbose,			SET_TYPE_BOOLEAN,	0,	0,			NS_ULEVEL_ADMIN,"verbose",			NULL,		fs_help_set_chanlocktime, NULL, (void *)1 },
	{"NICKFLOOD",		&fscfg.nickflood,		SET_TYPE_BOOLEAN,	0,	0,			NS_ULEVEL_ADMIN,"nickfloodhash",	NULL,		fs_help_set_nickflood, NULL, (void *)1 },
	{"NICKFLOODACT",	&fscfg.nickfloodact,	SET_TYPE_INT,		0,	0,			NS_ULEVEL_ADMIN,"nickfloodact",		NULL,		fs_help_set_nickfloodact, NULL, (void *)0 },
	{"NICKSAMPLETIME",	&fscfg.nicksampletime,	SET_TYPE_INT,		0,	100,		NS_ULEVEL_ADMIN,"nicksampletime",	NULL,		fs_help_set_nicksampletime, NULL, (void *)5 },
	{"NICKTHRESHOLD",	&fscfg.nickthreshold,	SET_TYPE_INT,		0,	100,		NS_ULEVEL_ADMIN,"nickthreshold",	NULL,		fs_help_set_nickthreshold, NULL, (void *)5 },
	{"JOINFLOOD",		&fscfg.joinflood,		SET_TYPE_BOOLEAN,	0,	0,			NS_ULEVEL_ADMIN,"joinflood",		NULL,		fs_help_set_joinflood, NULL, (void *)1 },
	{"JOINFLOODACT",	&fscfg.joinfloodact,	SET_TYPE_INT,		0,	0,			NS_ULEVEL_ADMIN,"joinfloodact",		NULL,		fs_help_set_joinfloodact, NULL, (void *)0 },
	{"JOINSAMPLETIME",	&fscfg.joinsampletime,	SET_TYPE_INT,		1,	1000,		NS_ULEVEL_ADMIN,"joinsampletime",	"seconds",	fs_help_set_joinsampletime, NULL, (void *)5 },
	{"JOINTHRESHOLD",	&fscfg.jointhreshold,	SET_TYPE_INT,		1,	1000,		NS_ULEVEL_ADMIN,"jointhreshold",	NULL,		fs_help_set_jointhreshold, NULL, (void *)5 },
	{"CHANLOCKKEY",		&fscfg.chanlockkey,		SET_TYPE_STRING,	0,	MAXCHANLEN,	NS_ULEVEL_ADMIN, "chanlockkey",		NULL,		fs_help_set_chanlockkey, NULL, (void *)"Eeeek" },
	{"CHANLOCKTIME",	&fscfg.chanlocktime,	SET_TYPE_INT,		0,	600,		NS_ULEVEL_ADMIN,"chanlocktime",		NULL,		fs_help_set_chanlocktime, NULL, (void *)30 },
	{NULL,				NULL,					0,					0,	0, 			0,				NULL,				NULL,		NULL, NULL },
};

static bot_cmd fs_commands[]=
{
	{"STATUS",	fs_cmd_status,	0,	NS_ULEVEL_OPER, fs_help_status,		fs_help_status_oneline},
	{NULL,		NULL,			0, 	0,				NULL, 				NULL}
};

BotInfo fs_botinfo =
{
	"FloodServ",
	"FloodServ1",
	"FS",
	BOT_COMMON_HOST, 
	"Flood protection service",
	BOT_FLAG_SERVICEBOT|BOT_FLAG_DEAF, 
	fs_commands, 
	fs_settings,
};

ModuleEvent module_events[] = {
	{ EVENT_NICK,		fs_event_nick},
	{ EVENT_SIGNON, 	fs_event_signon,	EVENT_FLAG_IGNORE_SYNCH},
	{ EVENT_QUIT, 		fs_event_quit},
	{ EVENT_KILL, 		fs_event_kill},
	{ EVENT_JOIN, 		fs_event_joinchan},
	{ EVENT_NEWCHAN,	fs_event_newchan,	EVENT_FLAG_IGNORE_SYNCH},
	{ EVENT_DELCHAN,	fs_event_delchan},
	{ EVENT_NULL, 		NULL}
};

static int fs_cmd_status(CmdParams *cmdparams)
{
	SET_SEGV_LOCATION();
	irc_prefmsg (fs_bot, cmdparams->source, "Current Top AJPP: %d (in %d Seconds): %s",
		MaxAJPP, fscfg.joinsampletime, MaxAJPPChan);
	return NS_SUCCESS;
}

static int fs_event_joinchan (CmdParams *cmdparams)
{
	chantrack *ci;

	SET_SEGV_LOCATION();
	/* if channel flood protection is disabled, return here */
	if (fscfg.joinflood == 0) {
		return NS_SUCCESS;
	}
	if (IsNetSplit(cmdparams->source)) {
		dlog (DEBUG1, "Ignoring netsplit nick %s", cmdparams->source->name);
		return NS_SUCCESS;
	}
	ci = (chantrack *) GetChannelModPtr (cmdparams->channel);	
	/* if the last join was "SampleTime" seconds ago
	 * reset the time and set ajpp to 1
	 */
	if ((time(NULL) - ci->ts_lastjoin) > fscfg.joinsampletime) {
		dlog (DEBUG2, "ChanJoin: SampleTime Expired, Resetting %s", cmdparams->channel->name);
		ci->ts_lastjoin = time(NULL);
		ci->ajpp = 1;
		return NS_SUCCESS;
	}		
	/* check if ajpp has exceeded the threshold */	
	/* should we have different thresholds for different channel sizes? */		
	ci->ajpp++;	
	dlog (DEBUG2, "check join flood: %d %d", ci->ajpp,  fscfg.jointhreshold);
	if ((ci->ajpp > fscfg.jointhreshold) && (ci->locked == 0)) {
		nlog (LOG_WARNING, "Warning, possible flood on %s. Closing channel. (AJPP: %d/%d Sec, SampleTime %d", ci->c->name, ci->ajpp, (int)(time(NULL) - ci->ts_lastjoin), fscfg.joinsampletime);
		irc_chanalert (fs_bot, "Warning, possible flood on %s. Closing channel. (AJPP: %d/%d Sec, SampleTime %d)", ci->c->name, ci->ajpp, (int)(time(NULL) - ci->ts_lastjoin), fscfg.joinsampletime);			
		irc_globops (fs_bot, "Warning, possible flood on %s. Closing channel. (AJPP: %d/%d Sec, SampleTime %d)", ci->c->name, ci->ajpp, (int)(time(NULL) - ci->ts_lastjoin), fscfg.joinsampletime);			
		irc_chanprivmsg (fs_bot, ci->c->name, "Temporarily closing channel due to possible floodbot attack. Channel will be re-opened in %d seconds", fscfg.chanlocktime);
		/* close flood channel */
		if (fscfg.joinfloodact) {
			irc_cmode (fs_bot, ci->c->name, "+ik", fscfg.chanlockkey);
		}
		ci->locked = time(NULL);
	}
	/* just some record keeping */
	if (ci->ajpp > MaxAJPP) {
		dlog (DEBUG1, "New AJPP record on %s at %d Joins in %d Seconds", cmdparams->channel->name, ci->ajpp, (int)(time(NULL) - ci->ts_lastjoin));
		if (fscfg.verbose) {
			irc_chanalert (fs_bot, "New AJPP record on %s at %d Joins in %d Seconds", cmdparams->channel->name, ci->ajpp, (int)(time(NULL) - ci->ts_lastjoin));
		}
		MaxAJPP = ci->ajpp;
		strlcpy (MaxAJPPChan, cmdparams->channel->name, MAXCHANLEN);
	}
	return NS_SUCCESS;
}

static int fs_event_newchan (CmdParams *cmdparams)
{
	chantrack *ci;

	dlog (DEBUG2, "Creating channel record for %s", cmdparams->channel->name);
	ci = (chantrack *) AllocChannelModPtr (cmdparams->channel, sizeof(chantrack));
	ci->c = cmdparams->channel;
	hnode_create_insert (joinfloodhash, ci, cmdparams->channel->name);	
	return NS_SUCCESS;
}

/* delete the channel from our hash */
static int fs_event_delchan(CmdParams *cmdparams)
{
	chantrack *ci;
	hnode_t *cn;

	SET_SEGV_LOCATION();
	cn = hash_lookup (joinfloodhash, cmdparams->channel->name);
	if (cn) {
		ci = hnode_get (cn);
		hash_delete (joinfloodhash, cn);
		FreeChannelModPtr (ci->c);
		hnode_destroy (cn);
	}
	return NS_SUCCESS;
}

int CheckLockChan (void) 
{
	hscan_t cs;
	hnode_t *cn;
	chantrack *ci;
	
	SET_SEGV_LOCATION();
	/* scan through the channels */
	hash_scan_begin (&cs, joinfloodhash);
	while ((cn = hash_scan_next (&cs)) != NULL) {
		ci = hnode_get (cn);
		/* if the locked time plus chanlocktime is greater than current time, then unlock the channel */
		if ((ci->locked > 0) && (ci->locked + fscfg.chanlocktime < time(NULL))) {
			if (fscfg.joinfloodact) {
				irc_cmode (fs_bot, ci->c->name, "-ik", fscfg.chanlockkey);
				irc_chanalert (fs_bot, "Unlocking %s after flood protection timeout", ci->c->name);
				irc_globops (fs_bot, "Unlocking %s after flood protection timeout", ci->c->name);
				irc_chanprivmsg (fs_bot, ci->c->name, "Unlocking the channel now");
			}
			ci->locked = 0;
		}					
	}
	return 1;
}

/* scan nickname changes */
static int fs_event_nick(CmdParams *cmdparams) 
{
	hnode_t *nfnode;
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	nfnode = hash_lookup (nickfloodhash, cmdparams->source->name);
	if (nfnode) {
		flooduser = hnode_get (nfnode);
		/* remove it from the hash, as the nick has changed */
		hash_delete (nickfloodhash, nfnode);
		/* increment the nflood count */
		flooduser->changes++;
		dlog (DEBUG2, "NickFlood check: %d in %s", flooduser->changes, fscfg.nicksampletime);
		if ((flooduser->changes > fscfg.nickthreshold) && ((time(NULL) - flooduser->ts_lastchange) <= fscfg.nicksampletime)) {
			/* nick change flood */
			irc_chanalert (fs_bot, "NickFlood detected on %s", cmdparams->source->name);
			/* TODO: React to nick flood */	
		} else if ((time(NULL) - flooduser->ts_lastchange) > fscfg.nicksampletime) {
			dlog (DEBUG2, "Resetting nickflood count on %s", cmdparams->source->name);
			flooduser->changes = 1;
			flooduser->ts_lastchange = time(NULL);
		} 
		/* re-insert it into the hash */
		strlcpy (flooduser->nick, cmdparams->source->name, MAXNICK);
		hash_insert (nickfloodhash, nfnode, flooduser->nick);
	}
	return NS_SUCCESS;
}

static int fs_event_signon (CmdParams *cmdparams)
{
	usertrack *flooduser;

	flooduser = (usertrack *) AllocUserModPtr (cmdparams->source, sizeof(usertrack));
	strlcpy (flooduser->nick, cmdparams->source->name, MAXNICK);
	flooduser->changes = 1;
	flooduser->ts_lastchange = time(NULL);
	flooduser->u = cmdparams->source;
	hnode_create_insert (nickfloodhash, flooduser, flooduser->nick);
	dlog (DEBUG2, "Created new nickflood entry");
	return NS_SUCCESS;
}

static int fs_event_quit(CmdParams *cmdparams) 
{
	hnode_t *nfnode;
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	dlog (DEBUG2, "fs_event_quit: looking for %s", cmdparams->source->name);
	nfnode = hash_lookup (nickfloodhash, cmdparams->source->name);
	if (nfnode) {
		flooduser = hnode_get (nfnode);
		hash_delete (nickfloodhash, nfnode);
		FreeUserModPtr (flooduser->u);
       	hnode_destroy (nfnode);
	}
	return NS_SUCCESS;
}

static int fs_event_kill(CmdParams *cmdparams) 
{
	hnode_t *nfnode;
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	dlog (DEBUG2, "fs_event_kill: looking for %s", cmdparams->target->name);
	nfnode = hash_lookup (nickfloodhash, cmdparams->target->name);
	if (nfnode) {
		flooduser = hnode_get (nfnode);
		hash_delete (nickfloodhash, nfnode);
		FreeUserModPtr (flooduser->u);
       	hnode_destroy (nfnode);
	}
	return NS_SUCCESS;
}

/** Init module
 */
int ModInit (Module *mod_ptr)
{
	SET_SEGV_LOCATION();
	os_memset (&fscfg, 0, sizeof (fscfg));
	ModuleConfig (fs_settings);
	/* init the channel hash */	
	joinfloodhash = hash_create (-1, 0, 0);
	/* init the nickfloodhash hash */
	nickfloodhash = hash_create (-1, 0, 0);
	return NS_SUCCESS;
}

/** @brief ModSynch
 *
 *  Startup handler
 *
 *  @param none
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

int ModSynch (void)
{
	SET_SEGV_LOCATION();
	fs_bot = AddBot (&fs_botinfo);
	if (!fs_bot) {
		return NS_FAILURE;
	}
	AddTimer (TIMER_TYPE_INTERVAL, CheckLockChan, "CheckLockChan", 60);
	return NS_SUCCESS;
};

/** Fini module
 * This is required if you need to do cleanup of your module when it ends
 */
void ModFini() 
{
	hscan_t scan;
	hnode_t *node;
	chantrack *ci;
	usertrack *flooduser;
	
	SET_SEGV_LOCATION();
	/* scan through the channels */
	hash_scan_begin (&scan, joinfloodhash);
	while ((node = hash_scan_next (&scan)) != NULL) {
		ci = hnode_get (node);
		hash_scan_delete (joinfloodhash, node);
		FreeChannelModPtr (ci->c);
		hnode_destroy (node);
	}
	hash_destroy (joinfloodhash);
	/* scan through the nicks */
	hash_scan_begin (&scan, nickfloodhash);
	while ((node = hash_scan_next (&scan)) != NULL) {
		flooduser = hnode_get (node);
		hash_scan_delete (nickfloodhash, node);
		FreeUserModPtr (flooduser->u);
       	hnode_destroy (node);
	}
	hash_destroy (nickfloodhash);
}
