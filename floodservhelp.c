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

const char *fs_help_set_verbose[] = {
	"\2VERBOSE <on/off>\2 - Enable verbose mode. Prepare to be flooded!",
	NULL
};

const char *fs_help_set_nickflood[] = {
	"\2NICKFLOOD <on/off>\2 - Enable nick flood protection.",
	NULL
};

const char *fs_help_set_nickfloodact[] = {
	"\2NICKFLOODACT <type>\2 - Nick flood action.",
	"<type> is",
	"0 : Warn only",
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

const char *fs_help_set_joinflood[] = {
	"\2JOINFLOOD <on/off>\2 - Enable channel flood protection.",
	NULL
};

const char *fs_help_set_joinfloodact[] = {
	"\2JOINFLOODACT <type>\2 - Join flood action.",
	"<type> is",
	"0 : Warn only",
	"1 : Lock channel",
	NULL
};

const char *fs_help_set_joinsampletime[] = {
	"\2JOINSAMPLETIME <seconds> <joins>\2",
	"Sets the threshold for flood checking. Read the Readme file for more information",
	NULL
};

const char *fs_help_set_jointhreshold[] = {
	"\2JOINTHRESHOLD <number>\2 - Sets the threshold for Nick Floods. <number> is number of changes in 10 seconds.",
	NULL
};

const char *fs_help_set_chanlockkey[] = {
	"\2CHANLOCKKEY <key>\2 - Sets the key to use for locking the channel when flood protection is active",
	NULL
};

const char *fs_help_set_chanlocktime[] = {
	"\2CHANLOCKTIME <seconds>\2 - Set the time to lock a channel for when flood protection is enabled. Time in seconds",
	NULL
};
