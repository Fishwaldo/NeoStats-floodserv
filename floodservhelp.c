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

#include "neostats.h"

const char fs_help_status_oneline[] = "Current Status";
const char *fs_help_status[] = {
	"Syntax: \2STATUS\2",
	"",
	"Provide you with the Current Status.",
	NULL
};


const char *fs_help_set[] = {
	"Syntax: \2SET <OPTION> <SETTING>\2",
	"",
	"    \2AKILL <on/off>\2         - Tells SecureServ to never issue an Akill on your Network. A warning message is sent to the operators instead",
	"    \2AKILLTIME <seconds>\2	- Sets the time an AKILL will last for.",
	"    \2DOJOIN <on/off>\2        - Tells SecureServ to never issue a SVSJOIN when a virus is detected. The User is Akilled instead",
	"    \2DOPRIVCHAN <on/off>\2	- Tells SecureServ not to join Private Channels",
	"    \2FLOODPROT <on/off>\2     - Enable Channel Flood Protection Scanning.",
	"    \2CHANKEY <key>\2          - When Flood Protection for a Channel is active, this is the key we will use to lock the channel",
	"    \2CHANLOCKTIME <seconds>\2 - How long (Approx) do we lock a Channel for. Time in seconds",
	"    \2VERBOSE <on/off>\2       - Turn on Verbose Mode. Prepare to be flooded!",
	"    \2SAMPLETIME <seconds> <joins>\2",
	"                               - Sets the threshold for Flood Checking. Read the Readme File for more information",
	"    \2NFCOUNT <number>\2       - Sets the threshold for Nick Floods. <number> is number of changes in 10 seconds.",
	NULL
};

const char *fs_help_set_chankey[] = {
	"\2CHANKEY <key>\2 - Sets the key to use for locking the channel when flood protection is active",
	NULL
};
const char *fs_help_set_floodprot[] = {
	"\2FLOODPROT <on/off>\2 - Enable channel flood protection.",
	NULL
};
const char *fs_help_set_chanlocktime[] = {
	"\2CHANLOCKTIME <seconds>\2 - Set the time to lock a channel for when flood protection is enabled. Time in seconds",
	NULL
};
const char *fs_help_set_nickthreshold[] = {
	"\2NICKTHRESHOLD <number>\2 - Sets the threshold for Nick Floods. <number> is number of changes in 10 seconds.",
	NULL
};
const char *fs_help_set_nicksampletime[] = {
	"\2NICKSAMPLETIME <number>\2 - Sets the threshold for Nick Floods. <number> is number of changes in 10 seconds.",
	NULL
};
const char *fs_help_set_verbose[] = {
	"\2VERBOSE <on/off>\2 - Enable verbose mode. Prepare to be flooded!",
	NULL
};
const char *fs_help_set_joinsampletime[] = {
	"\2JOINSAMPLETIME <seconds> <joins>\2",
	"Sets the threshold for flood checking. Read the Readme file for more information",
	NULL
};
const char *fs_help_set_jointhreshold[] = {
	"\2NICKTHRESHOLD <number>\2 - Sets the threshold for Nick Floods. <number> is number of changes in 10 seconds.",
	NULL
};
