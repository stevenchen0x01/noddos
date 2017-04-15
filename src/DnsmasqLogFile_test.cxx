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

 * DnsmasqLogFile_test.cxx
 *
 *  Created on: Apr 1, 2017
 *      Author: steven
 */

#include <iostream>
#include <sstream>

#include "DnsmasqLogFile.h"
#include "HostCache.h"

int main () {
	bool testfail = false;
	HostCache hc;
	// To test we have to add dummy Host entries to make sure
	// the client IP address translates to a MAC address as
	// the client ip address in the test colleteral may no longer
	// be present.
	hc.AddByMac ("00:00:00:00:00:01", "192.168.1.232");
	hc.AddByMac ("00:00:00:00:00:02", "192.168.1.98");
	hc.AddByMac ("00:00:00:00:00:03", "192.168.1.99");
	hc.AddByMac ("00:00:00:00:00:04", "192.168.1.235");
	hc.AddByMac ("00:00:00:00:00:05", "192.168.1.241");
	hc.AddByMac ("00:00:00:00:00:05", "192.168.1.251");
	hc.AddByMac ("00:00:00:00:00:05", "192.168.1.234");

	DnsmasqLogFile d ("tests/dnsmasqdnsdata.log", hc, 86400);
	auto lp = d.LinesParsed();
	std::cout << "Lines parsed: " << lp << std::endl;
	if (lp != 216)
		testfail = true;

	auto c = hc.HostCount();
	std::cout << "Hosts found: " << c << std::endl;
	if (c != 8)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.232");
	std::cout << "DNS queries by 192.168.1.232: " << c << std::endl;
	if (c != 4)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.98");
	std::cout << "DNS queries by 192.168.1.98: " << c << std::endl;
	if (c != 4)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.241");
	std::cout << "DNS queries by 192.168.1.241: " << c << std::endl;
	if (c != 21)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.99");
	std::cout << "DNS queries by 192.168.1.99: " << c << std::endl;
	if (c != 1)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.234");
	std::cout << "DNS queries by 192.168.1.234: " << c << std::endl;
	if (c != 1)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.235");
	std::cout << "DNS queries by 192.168.1.235: " << c << std::endl;
	if (c != 3)
		testfail = true;

	c = hc.HostDnsQueryCount ("192.168.1.251");
	std::cout << "DNS queries by 192.168.1.251: " << c << std::endl;
	if (c != 2)
		testfail = true;

	if(testfail)
		exit(1);
	exit(0);
}



