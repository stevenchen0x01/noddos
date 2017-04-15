/*
   Copyright 2017 Steven Hessing

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *
 * Config_test.cpp
 *
 *  Created on: Apr 3, 2017
 *      Author: steven
 *
 */

#include <syslog.h>

#include "Config.h"

int main () {
	openlog("Config_test", LOG_NOWAIT | LOG_PID | LOG_PERROR, LOG_UUCP);
	Config c("/etc/noddos/noddos.conf");
	exit(0);
}