/* NeoStats - IRC Statistical Services 
** Copyright (c) 1999-2004 Adam Rutter, Justin Hammond, Mark Hetherington
** http://www.neostats.net/
**
**  This program is ns_free software; you can redistribute it and/or modify
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

/* http://sourceforge.net/projects/muhstik/ */

#include "neostats.h"
#include "floodserv.h"

struct FloodServ {
	int verbose;
	int nickthreshold;
	int nicksampletime;
	int jointhreshold;
	int joinsampletime;
	char chankey[MAXCHANLEN];
	int closechantime;
	int floodprot;
} FloodServ;

/* the structure to keep track of joins per period (ajpp = average joins per period) */
typedef struct ChanInfo {
	Channel *c;
	int ajpp;
	time_t joinsampletime;
	int locked;
}ChanInfo;

/* this is the nickflood stuff */
typedef struct nicktrack {
	char nick[MAXNICK];
	int changes;
	int when;
}nicktrack;

/* FloodCheck.c */
int CheckLockChan(void);
int CleanNickFlood(void);
static int fs_event_quit(CmdParams *cmdparams);
static int fs_event_nick(CmdParams *cmdparams);
static int fs_event_delchan(CmdParams *cmdparams);
static int fs_event_joinchan(CmdParams *cmdparams);
static int fs_cmd_status(CmdParams *cmdparams);

Bot *fs_bot;

/* the hash that contains the channels we are tracking */
static hash_t *FC_Chans;

/* the hash that contains the nicks we are tracking */
static hash_t *nickflood;

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
	{"FLOODPROT",		&FloodServ.floodprot,		SET_TYPE_BOOLEAN,	0,	0,			NS_ULEVEL_ADMIN,"Dofloodprot",	NULL,	fs_help_set_floodprot, NULL, (void *)1 },
	{"NICKSAMPLETIME",	&FloodServ.nicksampletime,	SET_TYPE_INT,		0,	100,		NS_ULEVEL_ADMIN,"nicksampletime",		NULL,	fs_help_set_nicksampletime, NULL, (void *)5 },
	{"NICKTHRESHOLD",	&FloodServ.nickthreshold,	SET_TYPE_INT,		0,	100,		NS_ULEVEL_ADMIN,"nickthreshold",		NULL,	fs_help_set_nickthreshold, NULL, (void *)5 },
	{"JOINSAMPLETIME",	&FloodServ.joinsampletime,	SET_TYPE_INT,		1,	1000,		NS_ULEVEL_ADMIN,"joinsampletime",	"seconds",fs_help_set_joinsampletime, NULL, (void *)5 },
	{"JOINTHRESHOLD",	&FloodServ.jointhreshold,	SET_TYPE_INT,		1,	1000,		NS_ULEVEL_ADMIN,"jointhreshold",NULL,	fs_help_set_jointhreshold, NULL, (void *)5 },
	{"CHANKEY",			&FloodServ.chankey,			SET_TYPE_STRING,	0,	MAXCHANLEN,	NS_ULEVEL_ADMIN, "chankey",		NULL,	fs_help_set_chankey, NULL, (void *)"Eeeek" },
	{"CHANLOCKTIME",	&FloodServ.closechantime,	SET_TYPE_INT,		0,	600,		NS_ULEVEL_ADMIN,"ChanLockTime", NULL,	fs_help_set_chanlocktime, NULL, (void *)30 },
	{NULL,				NULL,						0,					0,	0, 			0,				NULL,			NULL,	NULL, NULL },
};

static bot_cmd fs_commands[]=
{
	{"STATUS",	fs_cmd_status,	0,	NS_ULEVEL_OPER, fs_help_status,		fs_help_status_oneline},
	{NULL,		NULL,			0, 	0,				NULL, 				NULL}
};

BotInfo ss_botinfo =
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
	{ EVENT_QUIT, 		fs_event_quit},
	{ EVENT_KILL, 		fs_event_quit},
	{ EVENT_JOIN, 		fs_event_joinchan},
	{ EVENT_DELCHAN,	fs_event_delchan},
	{ EVENT_NULL, 		NULL}
};

static int fs_cmd_status(CmdParams *cmdparams)
{
	SET_SEGV_LOCATION();
	irc_prefmsg (fs_bot, cmdparams->source, "Current Top AJPP: %d (in %d Seconds): %s", MaxAJPP, FloodServ.joinsampletime, MaxAJPPChan);
	return NS_SUCCESS;
}

static int fs_event_joinchan(CmdParams *cmdparams)
{
	ChanInfo *ci;
	hnode_t *cn;

	SET_SEGV_LOCATION();
	
	if (cmdparams->source->flags && NS_FLAGS_NETJOIN) {
		return NS_SUCCESS;
	}
	/* if channel flood protection is disabled, return here */
	if (FloodServ.floodprot == 0) {
		return NS_SUCCESS;
	}

	/* find the chan in FloodServ's list */
	cn = hash_lookup(FC_Chans, cmdparams->channel->name);
	if (!cn) {

		/* if it doesn't exist, means we have to create it ! */
		dlog (DEBUG2, "Creating Channel Record in JoinSection %s", cmdparams->channel->name);
		ci = ns_malloc (sizeof(ChanInfo));
		ci->ajpp = 0;
		ci->joinsampletime = 0;
		ci->c = cmdparams->channel;
		ci->locked = 0;
		cn = hnode_create(ci);
		hash_insert(FC_Chans, cn, cmdparams->channel->name);
	} else {		
		ci = hnode_get(cn);
	}
		
	/* Firstly, if the last join was "SampleTime" seconds ago
	 * then reset the time, and set ajpp to 1
	 */
	if ((time(NULL) - ci->joinsampletime) > FloodServ.joinsampletime) {
		dlog (DEBUG2, "ChanJoin: SampleTime Expired, Resetting %s", cmdparams->channel->name);
		ci->joinsampletime = time(NULL);
		ci->ajpp = 1;
		return NS_SUCCESS;
	}
		
	/* now check if ajpp has exceeded the threshold */
	
	/* XXX TOTHINK should we have different thresholds for different channel 
	 * sizes? Needs some real life testing I guess 
	 */		
	ci->ajpp++;	

	if ((ci->ajpp > FloodServ.jointhreshold) && (ci->locked > 0)) {
		nlog (LOG_WARNING, "Warning, Possible Flood on %s. Closing Channel. (AJPP: %d/%d Sec, SampleTime %d", ci->c->name, ci->ajpp, (int)(time(NULL) - ci->joinsampletime), FloodServ.joinsampletime);
		irc_chanalert (fs_bot, "Warning, Possible Flood on %s. Closing Channel. (AJPP: %d/%d Sec, SampleTime %d)", ci->c->name, ci->ajpp, (int)(time(NULL) - ci->joinsampletime), FloodServ.joinsampletime);			
		irc_globops (fs_bot, "Warning, Possible Flood on %s. Closing Channel. (AJPP: %d/%d Sec, SampleTime %d)", ci->c->name, ci->ajpp, (int)(time(NULL) - ci->joinsampletime), FloodServ.joinsampletime);			
		irc_chanprivmsg (fs_bot, ci->c->name, "Temporarily closing channel due to possible floodbot attack. Channel will be re-opened in %d seconds", FloodServ.closechantime);
		/* uh oh, channel is under attack. Close it down. */
		irc_cmode (fs_bot, ci->c->name, "+ik", FloodServ.chankey);
		ci->locked = time(NULL);
	}		

	/* just some record keeping */
	if (ci->ajpp > MaxAJPP) {
		dlog (DEBUG1, "New AJPP record on %s at %d Joins in %d Seconds", cmdparams->channel->name, ci->ajpp, (int)(time(NULL) - ci->joinsampletime));
		if (FloodServ.verbose) irc_chanalert (fs_bot, "New AJPP record on %s at %d Joins in %d Seconds", cmdparams->channel->name, ci->ajpp, (int)(time(NULL) - ci->joinsampletime));
		MaxAJPP = ci->ajpp;
		strlcpy(MaxAJPPChan, cmdparams->channel->name, MAXCHANLEN);
	}
	return NS_SUCCESS;
}

/* delete the channel from our hash */
static int fs_event_delchan(CmdParams *cmdparams)
{
	ChanInfo *ci;
	hnode_t *cn;

	SET_SEGV_LOCATION();
	cn = hash_lookup(FC_Chans, cmdparams->channel->name);
	if (cn) {
		ci = hnode_get(cn);
		hash_delete(FC_Chans, cn);
		ns_free (ci);
		hnode_destroy(cn);
#if 0		
	} else {
		/* ignore this, as it just means since we started FloodServ, no one has joined the channel, and now the last person has left. Was just flooding logfiles */
		//nlog (LOG_WARNING, "Can't Find Channel %s in our Hash", cmdparams->channel->name);
#endif
	}
	return NS_SUCCESS;
}

int CheckLockChan() 
{
	hscan_t cs;
	hnode_t *cn;
	ChanInfo *ci;
	
	SET_SEGV_LOCATION();
	/* scan through the channels */
	hash_scan_begin(&cs, FC_Chans);
	while ((cn = hash_scan_next (&cs)) != NULL) {
		ci = hnode_get(cn);
		/* if the locked time plus closechantime is greater than current time, then unlock the channel */
		if ((ci->locked > 0) && (ci->locked + FloodServ.closechantime < time(NULL))) {
			irc_cmode (fs_bot, ci->c->name, "-ik", FloodServ.chankey);
			irc_chanalert (fs_bot, "Unlocking %s after floodprotection timeout", ci->c->name);
			irc_globops (fs_bot, "Unlocking %s after flood protection timeout", ci->c->name);
			irc_chanprivmsg (fs_bot, ci->c->name, "Unlocking the channel now");
			ci->locked = 0;
		}					
	}
	return 1;
}

/* periodically clean up the nickflood hash so it doesn't grow to big */
int CleanNickFlood() 
{
	hscan_t nfscan;
	hnode_t *nfnode;
	nicktrack *nick;

	SET_SEGV_LOCATION();
    hash_scan_begin(&nfscan, nickflood);
    while ((nfnode = hash_scan_next(&nfscan)) != NULL) {
        nick = hnode_get(nfnode);
        if ((time(NULL) - nick->when) > FloodServ.nicksampletime) {
        	/* delete the nickname */
		dlog (DEBUG2, "Deleting %s out of NickFlood Hash", nick->nick);
        	hash_scan_delete(nickflood, nfnode);
        	ns_free (nick);
        }
    }
	return 1;
}       
	                
/* scan nickname changes */
static int fs_event_nick(CmdParams *cmdparams) 
{
	hnode_t *nfnode;
	nicktrack *nick;

	SET_SEGV_LOCATION();
	nfnode = hash_lookup(nickflood, cmdparams->source->name);
	if (nfnode) {
		/* its already there */
		nick = hnode_get(nfnode);
		/* first, remove it from the hash, as the nick has changed */
		hash_delete(nickflood, nfnode);
		/* increment the nflood count */
		nick->changes++;
		dlog (DEBUG2, "NickFlood Check: %d in %s", nick->changes, FloodServ.nicksampletime);
		if ((nick->changes > FloodServ.nickthreshold) && ((time(NULL) - nick->when) <= FloodServ.nicksampletime)) {
			/* its a bad bad bad flood */
			irc_chanalert (fs_bot, "NickFlood Detected on %s", cmdparams->source->name);
			/* XXX Do Something bad !!!! */
			
			/* ns_free the struct */
			hnode_destroy(nfnode);
			ns_free (nick);
		} else if ((time(NULL) - nick->when) > FloodServ.nicksampletime) {
			dlog (DEBUG2, "Resetting NickFlood Count on %s", cmdparams->source->name);
			strlcpy(nick->nick, cmdparams->source->name, MAXNICK);
			nick->changes = 1;
			nick->when = time(NULL);
			hash_insert(nickflood, nfnode, nick->nick);
		} else {			
			/* re-insert it into the hash */
			strlcpy(nick->nick, cmdparams->source->name, MAXNICK);
			hash_insert(nickflood, nfnode, nick->nick);
		}
	} else {
		/* this is because maybe we already have a record from a signoff etc */
		if (!hash_lookup(nickflood, cmdparams->source->name)) {
			/* this is a first */
			nick = ns_malloc (sizeof(nicktrack));
			strlcpy(nick->nick, cmdparams->source->name, MAXNICK);
			nick->changes = 1;
			nick->when = time(NULL);
			nfnode = hnode_create(nick);
			hash_insert(nickflood, nfnode, nick->nick);
			dlog (DEBUG2, "NF: Created New Entry");
		} else {
			dlog (DEBUG2, "Already got a record for %s in NickFlood", cmdparams->source->name);
		}
	}
	return NS_SUCCESS;
}

static int fs_event_quit(CmdParams *cmdparams) 
{
	hnode_t *nfnode;
	nicktrack *nick;

	SET_SEGV_LOCATION();
	dlog (DEBUG2, "fs_event_quit: looking for %s", cmdparams->source->name);
	nfnode = hash_lookup(nickflood, cmdparams->source->name);
	if (nfnode) {
		nick = hnode_get(nfnode);
		hash_delete(nickflood, nfnode);
       		hnode_destroy(nfnode);
		ns_free (nick);
	}
	dlog (DEBUG2, "fs_event_quit: After nickflood Code");
	return NS_SUCCESS;
}

/** Init module
 */
int ModInit (Module *mod_ptr)
{
	SET_SEGV_LOCATION();
	os_memset (&FloodServ, 0, sizeof (FloodServ));
	ModuleConfig (fs_settings);
	/* init the channel hash */	
	FC_Chans = hash_create(-1, 0, 0);
	/* init the nickflood hash */
	nickflood = hash_create(-1, 0, 0);
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
	fs_bot = AddBot (&ss_botinfo);
	if (!fs_bot) {
		return NS_FAILURE;
	}
	/* start cleaning the nickflood list now */
	/* every sixty seconds should keep the list small, and not put *too* much load on NeoStats */
	add_timer (TIMER_TYPE_INTERVAL, CleanNickFlood, "CleanNickFlood", 60);
	add_timer (TIMER_TYPE_INTERVAL, CheckLockChan, "CheckLockChan", 60);
	return NS_SUCCESS;
};

/** Fini module
 * This is required if you need to do cleanup of your module when it ends
 */
void ModFini() 
{
	SET_SEGV_LOCATION();
}
