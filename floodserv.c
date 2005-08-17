/* NeoStats - IRC Statistical Services 
** Copyright (c) 1999-2005 Adam Rutter, Justin Hammond, Mark Hetherington
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
#include MODULECONFIG
#include "floodserv.h"

struct fscfg 
{
	int verbose;
	int topicflood;
	int topicfloodact;
	int topicthreshold;
	int topicsampletime;
	int nickflood;
	int nickfloodact;
	int nickthreshold;
	int nicksampletime;
	int joinflood;
	int joinfloodact;
	int jointhreshold;
	int joinsampletime;
	int chanlocktime;
	char chanlockkey[KEYLEN];
} fscfg;

/* channel flood tracking
 * ( ajpp = average joins per period )
 * ( ctc = channel topic changes )
 */
typedef struct chantrack 
{
	Channel *c;
	int ajpp;
	time_t ts_lastjoin;
	time_t locked;
	int ctc;
	time_t ts_lasttopic;
	time_t topic_locked;
}chantrack;

/* nick flood tracking */
typedef struct usertrack 
{
	Client *u;
	int changes;
	time_t ts_lastchange;
}usertrack;

/* prototypes */
static int fs_event_signon( CmdParams *cmdparams );
static int fs_event_quit( CmdParams *cmdparams );
static int fs_event_kill( CmdParams *cmdparams );
static int fs_event_nick( CmdParams *cmdparams );
static int fs_event_newchan( CmdParams *cmdparams );
static int fs_event_delchan( CmdParams *cmdparams );
static int fs_event_joinchan( CmdParams *cmdparams );
static int fs_event_topicchange( CmdParams *cmdparams );
static int fs_cmd_status( CmdParams *cmdparams );

/* Bot pointer */
Bot *fs_bot;

/* the hash that contains the channels we are tracking */
static hash_t *joinfloodhash;

/* the hash that contains the nicks we are tracking */
static hash_t *nickfloodhash;

/* Max AJPP reporting variables */
static int MaxAJPP = 0;
static char MaxAJPPChan[MAXCHANLEN];

/* Max CTC reporting variables */
static int MaxCTC = 0;
static char MaxCTCChan[MAXCHANLEN];

/** about info */
const char *fs_about[] = 
{
	"Flood protection service",
	NULL
};

/** copyright info */
const char *fs_copyright[] = 
{
	"Copyright (c) 1999-2005, NeoStats",
	"http://www.neostats.net/",
	NULL
};

/** Module Info definition 
 */
ModuleInfo module_info = 
{
	"FloodServ",
	"Flood protection service",
	fs_copyright,
	fs_about,
	NEOSTATS_VERSION,
	MODULE_VERSION,
	__DATE__,
	__TIME__,
	0,
	0,
};

static bot_setting fs_settings[]=
{
	{"VERBOSE",			&fscfg.verbose,			SET_TYPE_BOOLEAN,	0,	0,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_verbose,		NULL,	( void * )1 },
	{"TOPICFLOOD",		&fscfg.topicflood,		SET_TYPE_BOOLEAN,	0,	0,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_topicflood,		NULL,	( void * )1 },
	{"TOPICFLOODACT",	&fscfg.topicfloodact,	SET_TYPE_INT,		0,	1,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_topicfloodact,	NULL,	( void * )0 },
	{"TOPICSAMPLETIME",	&fscfg.topicsampletime,	SET_TYPE_INT,		0,	100,	NS_ULEVEL_ADMIN,"seconds",	fs_help_set_topicsampletime,NULL,	( void * )5 },
	{"TOPICTHRESHOLD",	&fscfg.topicthreshold,	SET_TYPE_INT,		0,	100,	NS_ULEVEL_ADMIN,NULL,		fs_help_set_topicthreshold,	NULL,	( void * )5 },
	{"NICKFLOOD",		&fscfg.nickflood,		SET_TYPE_BOOLEAN,	0,	0,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_nickflood,		NULL,	( void * )1 },
	{"NICKFLOODACT",	&fscfg.nickfloodact,	SET_TYPE_INT,		0,	0,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_nickfloodact,	NULL,	( void * )0 },
	{"NICKSAMPLETIME",	&fscfg.nicksampletime,	SET_TYPE_INT,		0,	100,	NS_ULEVEL_ADMIN,"seconds",	fs_help_set_nicksampletime,	NULL,	( void * )5 },
	{"NICKTHRESHOLD",	&fscfg.nickthreshold,	SET_TYPE_INT,		0,	100,	NS_ULEVEL_ADMIN,NULL,		fs_help_set_nickthreshold,	NULL,	( void * )5 },
	{"JOINFLOOD",		&fscfg.joinflood,		SET_TYPE_BOOLEAN,	0,	0,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_joinflood,		NULL,	( void * )1 },
	{"JOINFLOODACT",	&fscfg.joinfloodact,	SET_TYPE_INT,		0,	1,		NS_ULEVEL_ADMIN,NULL,		fs_help_set_joinfloodact,	NULL,	( void * )0 },
	{"JOINSAMPLETIME",	&fscfg.joinsampletime,	SET_TYPE_INT,		1,	1000,	NS_ULEVEL_ADMIN,"seconds",	fs_help_set_joinsampletime,	NULL,	( void * )5 },
	{"JOINTHRESHOLD",	&fscfg.jointhreshold,	SET_TYPE_INT,		1,	1000,	NS_ULEVEL_ADMIN,NULL,		fs_help_set_jointhreshold,	NULL,	( void * )5 },
	{"CHANLOCKKEY",		&fscfg.chanlockkey,		SET_TYPE_STRING,	0,	KEYLEN,	NS_ULEVEL_ADMIN,NULL,		fs_help_set_chanlockkey,	NULL,	( void * )"random" },
	{"CHANLOCKTIME",	&fscfg.chanlocktime,	SET_TYPE_INT,		0,	600,	NS_ULEVEL_ADMIN,NULL,		fs_help_set_chanlocktime,	NULL,	( void * )30 },
	NS_SETTING_END()
};

static bot_cmd fs_commands[]=
{
	{"STATUS",	fs_cmd_status,	0,	NS_ULEVEL_OPER, fs_help_status},
	NS_CMD_END()
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

ModuleEvent module_events[] = 
{
	{ EVENT_NICK,		fs_event_nick},
	{ EVENT_SIGNON, 	fs_event_signon,	EVENT_FLAG_IGNORE_SYNCH},
	{ EVENT_QUIT, 		fs_event_quit},
	{ EVENT_KILL, 		fs_event_kill},
	{ EVENT_JOIN, 		fs_event_joinchan},
	{ EVENT_NEWCHAN,	fs_event_newchan,	EVENT_FLAG_IGNORE_SYNCH},
	{ EVENT_DELCHAN,	fs_event_delchan},
	{ EVENT_TOPIC,		fs_event_topicchange},
	{ EVENT_NULL, 		NULL}
};

/** @brief fs_cmd_status
 *  
 *  STATUS command processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_cmd_status( CmdParams *cmdparams )
{
	SET_SEGV_LOCATION();
	irc_prefmsg( fs_bot, cmdparams->source, "Current top AJPP %d (in %d seconds) in channel %s",
		MaxAJPP, fscfg.joinsampletime, MaxAJPPChan );
	irc_prefmsg( fs_bot, cmdparams->source, "Current top CTC %d (in %d seconds) in channel %s",
		MaxCTC, fscfg.topicsampletime, MaxCTCChan );
	return NS_SUCCESS;
}

/** @brief fs_new_channel
 *  
 *  create new channel hash
 *  
 *  @param channel structure pointer
 *  
 *  @return hash created
 */

static chantrack *fs_new_channel( Channel* channel )
{
	chantrack *ci;

	dlog( DEBUG2, "Creating channel record for %s", channel->name );
	ci = ( chantrack * ) AllocChannelModPtr( channel, sizeof( chantrack ) );
	ci->c = channel;
	hnode_create_insert( joinfloodhash, ci, channel->name );	
	return ci;
}

/** @brief fs_event_joinchan
 *  
 *  JOIN event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_joinchan( CmdParams *cmdparams )
{
	chantrack *ci;
	time_t period;

	SET_SEGV_LOCATION();
	/* if channel flood protection is disabled, return here */
	if( fscfg.joinflood == 0 ) 
	{
		return NS_SUCCESS;
	}
	if( IsNetSplit( cmdparams->source ) ) 
	{
		dlog( DEBUG1, "Ignoring netsplit nick %s", cmdparams->source->name );
		return NS_SUCCESS;
	}
	ci = ( chantrack * ) GetChannelModPtr( cmdparams->channel );
	if( !ci )
	{
		ci = fs_new_channel( cmdparams->channel );
	}
	/* if the last join was "SampleTime" seconds ago reset the time and ajpp */
	period = me.now - ci->ts_lastjoin;
	if( period > fscfg.joinsampletime ) 
	{
		dlog( DEBUG2, "ChanJoin: SampleTime expired, resetting %s", cmdparams->channel->name );
		ci->ts_lastjoin = me.now;
		ci->ajpp = 1;
		return NS_SUCCESS;
	}		
	/* check if ajpp has exceeded the threshold */	
	/* should we have different thresholds for different channel sizes? */		
	ci->ajpp++;	
	dlog( DEBUG2, "check join flood: %d %d", ci->ajpp,  fscfg.jointhreshold );
	if( ( ci->ajpp > fscfg.jointhreshold ) && ( ci->locked == 0 ) ) 
	{
		switch( fscfg.joinfloodact ) 
		{
			case 0:
				/* warn only */
				nlog( LOG_WARNING, "Warning, possible flood on %s. AJPP: %d/%d sec, SampleTime %d", ci->c->name, ci->ajpp, (int)period, fscfg.joinsampletime );
				irc_chanalert( fs_bot, "Warning, possible flood on %s. AJPP: %d/%d sec, SampleTime %d", ci->c->name, ci->ajpp, (int)period, fscfg.joinsampletime );
				irc_globops( fs_bot, "Warning, possible flood on %s. AJPP: %d/%d Sec, SampleTime %d", ci->c->name, ci->ajpp, (int)period, fscfg.joinsampletime );
				break;
			case 1:
				/* close flood channel */
				nlog( LOG_WARNING, "Warning, possible flood on %s. Closing channel. AJPP: %d/%d sec, SampleTime %d", ci->c->name, ci->ajpp, (int)period, fscfg.joinsampletime );
				irc_chanalert( fs_bot, "Warning, possible flood on %s. Closing channel. AJPP: %d/%d sec, SampleTime %d", ci->c->name, ci->ajpp, (int)period, fscfg.joinsampletime );
				irc_globops( fs_bot, "Warning, possible flood on %s. Closing channel. AJPP: %d/%d Sec, SampleTime %d", ci->c->name, ci->ajpp, (int)period, fscfg.joinsampletime );
				irc_chanprivmsg( fs_bot, ci->c->name, "Temporarily closing channel due to possible floodbot attack. Channel will be re-opened in %d seconds", fscfg.chanlocktime );
				if( !ircstrcasecmp( fscfg.chanlockkey, "random" ) )
				{
					char *key;

					key = GetRandomChannelKey( 9 );
					irc_cmode( fs_bot, ci->c->name, "+ik", key );
					ns_free( key );
				} 
				else
				{
					irc_cmode( fs_bot, ci->c->name, "+ik", fscfg.chanlockkey );
				}
				ci->locked = me.now;
				break;
			default:
				break;
		}
	}
	/* just some record keeping */
	if( ci->ajpp > MaxAJPP ) 
	{
		dlog( DEBUG1, "New AJPP record on %s with %d joins in %d seconds", cmdparams->channel->name, ci->ajpp, (int)period );
		if( fscfg.verbose ) 
		{
			irc_chanalert( fs_bot, "New AJPP record on %s with %d joins in %d seconds", cmdparams->channel->name, ci->ajpp, (int)period );
		}
		MaxAJPP = ci->ajpp;
		strlcpy( MaxAJPPChan, cmdparams->channel->name, MAXCHANLEN );
	}
	return NS_SUCCESS;
}

/** @brief fs_event_topicchange
 *  
 *  TOPIC event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_topicchange( CmdParams *cmdparams )
{
	chantrack *ci;
	time_t period;

	SET_SEGV_LOCATION();
	/* if topic flood protection is disabled, return here */
	if( fscfg.topicflood == 0 ) 
	{
		return NS_SUCCESS;
	}
	/* if topic already locked, nothing we can do, return */
	if (test_cmode(cmdparams->channel, CMODE_TOPICLIMIT))
	{
		return NS_SUCCESS;
	}
	ci = ( chantrack * ) GetChannelModPtr( cmdparams->channel );
	if( !ci )
	{
		ci = fs_new_channel( cmdparams->channel );
	}
	/* if the last topic was "SampleTime" seconds ago reset the time and ctc */
	period = me.now - ci->ts_lasttopic;
	if( period > fscfg.topicsampletime ) 
	{
		dlog( DEBUG2, "TopicChange: SampleTime expired, resetting %s", cmdparams->channel->name );
		ci->ts_lasttopic = me.now;
		ci->ctc = 1;
		return NS_SUCCESS;
	}		
	/* check if ctc has exceeded the threshold */	
	/* should we have different thresholds for different channel sizes? */		
	ci->ctc++;	
	dlog( DEBUG2, "check topic flood: %d %d", ci->ctc,  fscfg.topicthreshold );
	if( ( ci->ctc > fscfg.topicthreshold ) && ( ci->topic_locked == 0 ) ) 
	{
		switch( fscfg.topicfloodact ) 
		{
			case 0:
				/* warn only */
				nlog( LOG_WARNING, "Warning, possible topic flood on %s. CTC: %d/%d sec, SampleTime %d", ci->c->name, ci->ctc, (int)period, fscfg.topicsampletime );
				irc_chanalert( fs_bot, "Warning, possible flood on %s. CTC: %d/%d sec, SampleTime %d", ci->c->name, ci->ctc, (int)period, fscfg.topicsampletime );
				irc_globops( fs_bot, "Warning, possible flood on %s. CTC: %d/%d Sec, SampleTime %d", ci->c->name, ci->ctc, (int)period, fscfg.topicsampletime );
				break;
			case 1:
				/* close flood channel */
				nlog( LOG_WARNING, "Warning, possible flood on %s. Closing channel. CTC: %d/%d sec, SampleTime %d", ci->c->name, ci->ctc, (int)period, fscfg.topicsampletime );
				irc_chanalert( fs_bot, "Warning, possible flood on %s. Closing channel. CTC: %d/%d sec, SampleTime %d", ci->c->name, ci->ctc, (int)period, fscfg.topicsampletime );
				irc_globops( fs_bot, "Warning, possible flood on %s. Closing channel. CTC: %d/%d Sec, SampleTime %d", ci->c->name, ci->ctc, (int)period, fscfg.topicsampletime );
				irc_chanprivmsg( fs_bot, ci->c->name, "Temporarily locking channel topic due to possible attack. Channel Topic will be re-opened in %d seconds", fscfg.chanlocktime );
				irc_cmode( fs_bot, ci->c->name, "+t", NULL );
				ci->topic_locked = me.now;
				break;
			default:
				break;
		}
	}
	/* just some record keeping */
	if( ci->ctc > MaxCTC ) 
	{
		dlog( DEBUG1, "New CTC record on %s with %d Topics in %d seconds", cmdparams->channel->name, ci->ctc, (int)period );
		if( fscfg.verbose ) 
		{
			irc_chanalert( fs_bot, "New CTC record on %s with %d joins in %d seconds", cmdparams->channel->name, ci->ctc, (int)period );
		}
		MaxCTC = ci->ctc;
		strlcpy( MaxCTCChan, cmdparams->channel->name, MAXCHANLEN );
	}
	return NS_SUCCESS;
}

/** @brief fs_event_newchan
 *  
 *  NEWCHAN event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_newchan( CmdParams *cmdparams )
{
	SET_SEGV_LOCATION();
	fs_new_channel( cmdparams->channel );
	return NS_SUCCESS;
}

/** @brief fs_event_delchan
 *  
 *  DELCHAN event processing
 *  deletes channel from hash
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_delchan( CmdParams *cmdparams )
{
	chantrack *ci;
	hnode_t *cn;

	SET_SEGV_LOCATION();
	cn = hash_lookup( joinfloodhash, cmdparams->channel->name );
	if( cn ) 
	{
		ci = hnode_get( cn );
		hash_delete( joinfloodhash, cn );
		FreeChannelModPtr( ci->c );
		hnode_destroy( cn );
	}
	return NS_SUCCESS;
}

/** @brief CheckLockChan
 *  
 *  Timer callback to unlock channels
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

int CheckLockChan( void *userptr ) 
{
	hscan_t cs;
	hnode_t *cn;
	chantrack *ci;
	
	SET_SEGV_LOCATION();
	/* scan through the channels */
	hash_scan_begin( &cs, joinfloodhash );
	while( ( cn = hash_scan_next( &cs ) ) != NULL ) 
	{
		ci = hnode_get( cn );
		/* if the locked time plus chanlocktime is greater than current time, then unlock the channel */
		if( ( ci->locked > 0 ) && ( ci->locked + fscfg.chanlocktime < me.now ) ) 
		{
			if( fscfg.joinfloodact == 1 ) 
			{
				irc_cmode( fs_bot, ci->c->name, "-ik", ci->c->key );
				nlog( LOG_NOTICE, "Unlocking %s after flood protection timeout", ci->c->name );
				irc_chanalert( fs_bot, "Unlocking %s after flood protection timeout", ci->c->name );
				irc_globops( fs_bot, "Unlocking %s after flood protection timeout", ci->c->name );
				irc_chanprivmsg( fs_bot, ci->c->name, "Unlocking the channel now" );
			}
			ci->locked = 0;
		}					
		/* if the topic locked time plus chanlocktime is greater than current time, then unlock the channel topic */
		if( ( ci->topic_locked > 0 ) && ( ci->topic_locked + fscfg.chanlocktime < me.now ) ) 
		{
			if( fscfg.topicfloodact == 1 ) 
			{
				irc_cmode( fs_bot, ci->c->name, "-t", NULL );
				nlog( LOG_NOTICE, "Unlocking %s topic after flood protection timeout", ci->c->name );
				irc_chanalert( fs_bot, "Unlocking %s topic after flood protection timeout", ci->c->name );
				irc_globops( fs_bot, "Unlocking %s topic after flood protection timeout", ci->c->name );
				irc_chanprivmsg( fs_bot, ci->c->name, "Unlocking the channel topic now" );
			}
			ci->topic_locked = 0;
		}					
	}
	return NS_SUCCESS;
}

/** @brief fs_event_nick
 *  
 *  NICK event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_nick( CmdParams *cmdparams ) 
{
	hnode_t *nfnode;
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	nfnode = hash_lookup( nickfloodhash, cmdparams->param );
	if( nfnode ) 
	{
		time_t period;

		flooduser = hnode_get( nfnode );
		/* remove it from the hash, as the nick has changed */
		hash_delete( nickfloodhash, nfnode );
		/* increment the nflood count */
		flooduser->changes++;
		period = me.now - flooduser->ts_lastchange;		
		dlog( DEBUG2, "NickFlood check: %d in %d", flooduser->changes, fscfg.nicksampletime );
		if( ( flooduser->changes > fscfg.nickthreshold ) && ( period <= fscfg.nicksampletime ) ) 
		{
			/* nick change flood */
			irc_chanalert( fs_bot, "NickFlood detected on %s", cmdparams->source->name );
			/* TODO: React to nick flood */	
		} 
		else if( period > fscfg.nicksampletime ) 
		{
			dlog( DEBUG2, "Resetting nickflood count on %s", cmdparams->source->name );
			flooduser->changes = 1;
			flooduser->ts_lastchange = me.now;
		} 
		/* re-insert it into the hash */
		hash_insert( nickfloodhash, nfnode, flooduser->u->name );
	}
	return NS_SUCCESS;
}

/** @brief fs_event_signon
 *  
 *  SIGNON event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_signon( CmdParams *cmdparams )
{
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	flooduser = ( usertrack * ) AllocUserModPtr( cmdparams->source, sizeof( usertrack ) );
	flooduser->changes = 1;
	flooduser->ts_lastchange = me.now;
	flooduser->u = cmdparams->source;
	hnode_create_insert( nickfloodhash, flooduser, flooduser->u->name );
	dlog( DEBUG2, "Created new nickflood entry" );
	return NS_SUCCESS;
}

/** @brief fs_event_quit
 *  
 *  QUIT event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_quit( CmdParams *cmdparams ) 
{
	hnode_t *nfnode;
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	dlog( DEBUG2, "fs_event_quit: looking for %s", cmdparams->source->name );
	nfnode = hash_lookup( nickfloodhash, cmdparams->source->name );
	if( nfnode ) 
	{
		flooduser = hnode_get( nfnode );
		hash_delete( nickfloodhash, nfnode );
		FreeUserModPtr( flooduser->u );
       	hnode_destroy( nfnode );
	}
	return NS_SUCCESS;
}

/** @brief fs_event_kill
 *  
 *  KILL event processing
 *  
 *  @param cmdparams
 *  
 *  @return NS_SUCCESS if succeeds, NS_FAILURE if not 
 */

static int fs_event_kill( CmdParams *cmdparams ) 
{
	hnode_t *nfnode;
	usertrack *flooduser;

	SET_SEGV_LOCATION();
	dlog( DEBUG2, "fs_event_kill: looking for %s", cmdparams->target->name );
	nfnode = hash_lookup( nickfloodhash, cmdparams->target->name );
	if( nfnode ) 
	{
		flooduser = hnode_get( nfnode );
		hash_delete( nickfloodhash, nfnode );
		FreeUserModPtr( flooduser->u );
       	hnode_destroy( nfnode );
	}
	return NS_SUCCESS;
}

/** @brief FiniJoinFlood
 *
 *  Clean up join flood hash
 *
 *  @param none
 *
 *  @return none
 */
void FiniJoinFlood( void )
{
	hscan_t scan;
	hnode_t *node;
	chantrack *ci;

	/* scan through the channels */
	hash_scan_begin( &scan, joinfloodhash );
	while( ( node = hash_scan_next( &scan ) ) != NULL ) 
	{
		ci = hnode_get( node );
		hash_scan_delete( joinfloodhash, node );
		FreeChannelModPtr( ci->c );
		hnode_destroy( node );
	}
	hash_destroy( joinfloodhash );
}

/** @brief FiniNickFlood
 *
 *  Clean up nick flood hash
 *
 *  @param none
 *
 *  @return none
 */
void FiniNickFlood( void )
{
	hscan_t scan;
	hnode_t *node;
	usertrack *flooduser;

	/* scan through the nicks */
	hash_scan_begin( &scan, nickfloodhash );
	while( ( node = hash_scan_next( &scan ) ) != NULL ) 
	{
		flooduser = hnode_get( node );
		hash_scan_delete( nickfloodhash, node );
		FreeUserModPtr( flooduser->u );
       	hnode_destroy( node );
	}
	hash_destroy( nickfloodhash );
}

/** @brief ModInit
 *
 *  Init handler
 *
 *  @param none
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

int ModInit( void )
{
	SET_SEGV_LOCATION();
	os_memset( &fscfg, 0, sizeof( fscfg ) );
	ModuleConfig( fs_settings );
	/* init the channel hash */	
	joinfloodhash = hash_create( -1, 0, 0 );
	/* init the nickfloodhash hash */
	nickfloodhash = hash_create( -1, 0, 0 );
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

int ModSynch( void )
{
	SET_SEGV_LOCATION();
	fs_bot = AddBot( &fs_botinfo );
	if( !fs_bot ) 
	{
		return NS_FAILURE;
	}
	AddTimer( TIMER_TYPE_INTERVAL, CheckLockChan, "CheckLockChan", TS_ONE_MINUTE, NULL );
	return NS_SUCCESS;
}

/** @brief ModFini
 *
 *  Fini handler
 *
 *  @param none
 *
 *  @return NS_SUCCESS if suceeds else NS_FAILURE
 */

int ModFini( void )
{
	SET_SEGV_LOCATION();
	FiniJoinFlood();
	FiniNickFlood();
	return NS_SUCCESS;
}
