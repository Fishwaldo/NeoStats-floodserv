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

#ifndef _FLOODSERV_H_
#define _FLOODSERV_H_

extern const char fs_help_status_oneline[];
extern const char *fs_help_status[];


const char *fs_help_set_chankey[];
const char *fs_help_set_floodprot[];
const char *fs_help_set_chanlocktime[];
const char *fs_help_set_nickthreshold[];
const char *fs_help_set_nicksampletime[];
const char *fs_help_set_dojoin[];
const char *fs_help_set_verbose[];
const char *fs_help_set_joinsampletime[];
const char *fs_help_set_jointhreshold[];

#endif /* _FLOODSERV_H_ */
