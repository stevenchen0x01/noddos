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

 * HostCache.cxx
 *
 *  Created on: Mar 11, 2017
 *      Author: steven
 */

#include <iostream>
#include <unordered_set>
#include <string>
#include <cstring>
#include <fstream>
#include <memory>

#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <syslog.h>
// #include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>

#include <json.hpp>
using nlohmann::json;

#include "cpr/cpr.h"

#include "HostCache.h"


uint32_t HostCache::Prune (bool Force) {
	uint32_t prunedhosts = 0;
	for (auto it : hC) {
		if (it.second->Prune(Force)) {
			prunedhosts++;
		}
	}
	syslog(LOG_INFO, "Pruned %u hosts", prunedhosts);
	return prunedhosts;
}

uint32_t HostCache::Match() {
	uint32_t matched = 0;
	for (auto it : hC) {
		if (it.second->Match(dpMap)) {
			matched++;
		}
	}
	return matched;
}

bool HostCache::MatchByMac(const std::string inMacAddress) {
	if (hC.find(inMacAddress) != hC.end()) {
		auto &h = *(hC[inMacAddress]);
		return h.Match(dpMap);
	}

	return false;
}

bool HostCache::MatchByIpAddress(const std::string inIpAddress) {
	if (Ip2MacMap.find(inIpAddress) != Ip2MacMap.end()) {
		std::string mac = Ip2MacMap[inIpAddress];
		return MatchByMac(mac);
	}
	return false;
}

std::shared_ptr<Host> HostCache::FindOrCreateHostByMac (const std::string mac, const std::string Uuid) {
	if (WhitelistedNodes.find(mac) != WhitelistedNodes.end())
		return nullptr;
	if (mac == "") {
		syslog(LOG_WARNING, "empty Mac Address provided");
		return nullptr;
	}
	if (hC.find(mac) == hC.end()) {
		syslog(LOG_DEBUG, "Adding new Host with MAC address %s", mac.c_str());
		auto h = std::make_shared<Host>(mac, Uuid);
		hC[mac] = h;
		return h;
	}
	return hC[mac];
}

std::shared_ptr<Host> HostCache::FindOrCreateHostByIp (const std::string ip, const std::string Uuid) {
	if (WhitelistedNodes.find(ip) != WhitelistedNodes.end())
		return nullptr;

	std::string MacAddress;
	auto it = Ip2MacMap.find(ip);
	if ( it == Ip2MacMap.end()) {
		MacAddress = MacLookup(ip);
		if (MacAddress == "") {
			syslog(LOG_DEBUG, "Couldn't find ARP entry for %s", ip.c_str());
			return nullptr;
		}
		Ip2MacMap[ip] = MacAddress;
	} else {
		MacAddress = it->second;
	}
	return FindOrCreateHostByMac (MacAddress, Uuid);
}

bool HostCache::AddByMac (const std::string inMacAddress, const std::string inIpAddress) {
	if (hC.find(inMacAddress) != hC.end())
		return false;

	auto h = std::make_shared<Host>(inMacAddress);
	h->IpAddress_set (inIpAddress);
	hC[inMacAddress] = h;
	Ip2MacMap[inIpAddress] = inMacAddress;
	return true;
}

bool HostCache::AddFlow (const std::string srcip, const uint16_t srcport, const std::string dstip, const uint16_t dstport, const uint8_t protocol, const uint32_t expiration) {
	if (WhitelistedNodes.find(srcip) != WhitelistedNodes.end())
			return false;

	std::shared_ptr<Host> h = FindOrCreateHostByIp(srcip);
	if (h) {
		h->FlowEntry_set(srcport, dstip, dstport, protocol, expiration);
		return true;
	}
	return false;
}

bool HostCache::AddDnsQueryIp (const std::string clientip, const std::string fqdn, const std::string ip, const uint32_t expire) {
	if (WhitelistedNodes.find(clientip) != WhitelistedNodes.end())
		return false;

	std::shared_ptr<Host> h = FindOrCreateHostByIp(clientip);
	if (h) {
		h->DnsLogEntry_set(fqdn, ip);
		return true;
	}
	return false;
}

bool HostCache::AddDhcpRequest (const std::shared_ptr<DhcpRequest> inDhcpRequest_sptr) {
	if (inDhcpRequest_sptr->IpAddress == "" && inDhcpRequest_sptr->MacAddress == "") {
		syslog(LOG_WARNING, "No IpAdddress or Macaddress in DHCP request");
		return false;

	}
	if (isWhitelisted(inDhcpRequest_sptr->IpAddress) || isWhitelisted(inDhcpRequest_sptr->MacAddress))
		return false;

	std::shared_ptr<Host> h;
	if (inDhcpRequest_sptr->MacAddress != "") {
		h = FindOrCreateHostByMac(inDhcpRequest_sptr->MacAddress);
	} else {
		h = FindOrCreateHostByIp(inDhcpRequest_sptr->IpAddress);
	}

	if (h) {
		h->Dhcp_set(inDhcpRequest_sptr);
		return true;
	}
	return false;
}

bool HostCache::AddDhcpRequest (const DhcpRequest &inDhcpRequest) {
	if (inDhcpRequest.IpAddress == "" && inDhcpRequest.MacAddress == "") {
		syslog(LOG_WARNING, "No IpAdddress or Macaddress in DHCP request");
		return false;

	}
	if (isWhitelisted(inDhcpRequest.IpAddress) || isWhitelisted(inDhcpRequest.MacAddress))
		return false;

	std::shared_ptr<Host> h;
	if (inDhcpRequest.MacAddress != "") {
		h = FindOrCreateHostByMac(inDhcpRequest.MacAddress);
	} else {
		h = FindOrCreateHostByIp(inDhcpRequest.IpAddress);
	}

	if (h) {
		h->Dhcp_set(inDhcpRequest);
		return true;
	}
	return false;
}

bool HostCache::AddDhcpRequest (const std::string IpAddress, const std::string MacAddress, const std::string Hostname, const std::string DhcpHostname, const std::string DhcpVendor) {
	if (IpAddress == "" && MacAddress == "") {
		syslog(LOG_WARNING, "No IpAdddress or Macaddress in DHCP request");
		return false;

	}
	if (isWhitelisted(IpAddress) || isWhitelisted(MacAddress))
		return false;

	std::shared_ptr<Host> h;
	if (MacAddress != "") {
		h = FindOrCreateHostByMac(MacAddress);
	} else {
		h = FindOrCreateHostByIp(IpAddress);
	}

	if (h) {
		h->Dhcp_set(IpAddress, MacAddress, Hostname, DhcpHostname, DhcpVendor);
		return true;
	}
	return false;

}

bool HostCache::AddSsdpInfo (const std::shared_ptr<SsdpHost> sHost) {
	if (sHost->IpAddress == "") {
		syslog(LOG_WARNING, "AddSsdpInfo: no IP address provided");
		return false;
	}
	if (WhitelistedNodes.find(sHost->IpAddress) != WhitelistedNodes.end())
		return false;

	std::shared_ptr<Host> h = FindOrCreateHostByIp(sHost->IpAddress);
	if (h) {
		h->SsdpInfo_set(sHost);
		return true;
	}
	return false;
}

// TODO: Lookup MAC addresses in ARP table using IOCTL now works but you need to specify the Ethernet interface and we don't have code for that yet
// so for now we'll continue doing lookups in /proc/net/arp even though that is slower
std::string HostCache::MacLookup (const std::string inIpAddress, std::string inInterface, const int retries) {
	int domain;
	struct arpreq areq;
	memset(&areq, 0, sizeof(areq));
	void *addr_ptr;

	if (inIpAddress.find(":") == std::string::npos ) {
		struct sockaddr_in *sin = (struct sockaddr_in *) &areq.arp_pa;
	    sin->sin_family = domain = AF_INET;
	    addr_ptr = &(sin->sin_addr);
	} else {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &areq.arp_pa;
		sin6->sin6_family = domain = AF_INET6;
	    addr_ptr = &(sin6->sin6_addr);
	}
    if (! inet_pton(domain, inIpAddress.c_str(), addr_ptr)) {
		syslog (LOG_ERR, "inet_pton failed for %s", inIpAddress.c_str());
		return "";
	}

	struct sockaddr_in *sin = (struct sockaddr_in *) &areq.arp_ha;
    sin->sin_family = ARPHRD_ETHER;
    strncpy(areq.arp_dev, inInterface.c_str(), 15);

    /* Get an internet domain socket. */
    int s;
	if ((s = socket(domain, SOCK_DGRAM, 0)) == -1) {
        syslog(LOG_ERR, "Can't open socket for ARP table lookup");
        return "";
    }

    if (-1 == ioctl(s,SIOCGARP , (caddr_t) &areq)) {
		syslog (LOG_ERR, "ARP lookup failure for %s", inIpAddress.c_str());
		if (retries > 0) {
			syslog(LOG_DEBUG, "Additional ARP lookup for %s", inIpAddress.c_str());
			if (SendUdpPing (inIpAddress, 1900)) {
				usleep(5000);
				return MacLookup (inIpAddress, inInterface, retries - 1);
			}
		}
		return "";
	}
	char mA[18];
	unsigned char *ptr = (unsigned char *) areq.arp_ha.sa_data;
	sprintf(mA, "%02X:%02X:%02X:%02X:%02X:%02X",
	        (ptr[0] & 0xff), (ptr[1] & 0xff), (ptr[2] & 0xff),
	        (ptr[3] & 0xff), (ptr[4] & 0xff), (ptr[5] & 0xff));
	std::string MacAddress(mA);
	return MacAddress;
}
std::string HostCache::MacLookup (const std::string inIpAddress, const int retries) {
	// TODO: we should cache ARP table and only refresh it if a MAC lookup fails
	std::ifstream ifs("/proc/net/arp");
	std::string line;
	while (std::getline(ifs, line)) {
		std::smatch m;
		if(std::regex_match(line, m, arp_rx)) {
			std::string ip = m.str(1);
			std::string mac = m.str(2);
			if (ip == inIpAddress && mac != "00:00:00:00:00:00") {
				ifs.close();
				return mac;
			}
		}

	}
	ifs.close();
	if (retries > 0) {
		syslog(LOG_DEBUG, "Additional ARP lookup for %s", inIpAddress.c_str());
		if (SendUdpPing (inIpAddress, 1900)) {
			usleep(5000);
			return MacLookup (inIpAddress, retries - 1);
		}
	}
	return "";
}

bool HostCache::SendUdpPing (const std::string DstIpAddress, const uint16_t DstPort) {
	//Structure for address of server
	struct sockaddr_in myaddr;
	int sock;

	//Construct the server sockaddr_ structure
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family=AF_INET;
	myaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	myaddr.sin_port=htons(0);

	//Create the socket
	if((sock=socket(AF_INET, SOCK_DGRAM, 0))<0) {
		syslog(LOG_ERR, "Failed to create socket");
		return false;
	}

	if(bind(sock,( struct sockaddr *) &myaddr, sizeof(myaddr))<0) {
		syslog(LOG_ERR, "bind failed");
	    return false;
	}
	inet_pton(AF_INET,DstIpAddress.c_str(),&myaddr.sin_addr.s_addr);
	myaddr.sin_port=htons(1900);

	std::string s("12345678910:5/15:300.00:Visa");

	//send the message to server
	if(sendto(sock, s.c_str(), s.size(), 0, (struct sockaddr *)&myaddr, sizeof(myaddr))!=s.size()) {
		syslog(LOG_ERR, "Mismatch in number of bytes sent");
		close (sock);
	    return false;
	}
	close (sock);
	return true;
}

bool HostCache::ExportDeviceProfileMatches(const std::string filename, bool detailed) {
	std::ofstream ofs(filename);
	json j;
	for (auto it : hC) {
		if (! isWhitelisted(*(it.second))) {
			it.second->ExportDeviceInfo(j, detailed);
		}
	}

	if (!ofs.is_open()) {
		syslog(LOG_WARNING, "Couldn't open %s", filename.c_str());
		return false;
	}
	ofs << std::setw(4) << j << std::endl;
	ofs.close();
	return true;
}

uint32_t HostCache::RestApiCall (const std::string api, const json &j, const std::string ClientCertFingerprint) {
    auto r = cpr::Post(
    	cpr::Url{"https://api.noddos.io" + api},
    	cpr::Header{{"Content-Type", "application/json"},{"X-Fingerprint", ClientCertFingerprint}},
		cpr::Body{j.dump()}
    );

    return r.status_code;
}

uint32_t HostCache::UploadDeviceStats(const std::string ClientCertFingerprint) {
	uint32_t uploads = 0;
	json j;
	for (auto it : hC) {
		if ( (! isWhitelisted(*(it.second))) && ! it.second->isMatched()) {
			json h;
			it.second->DeviceStats(h, 604800, false, false);
			uploads++;
			j.push_back(h);
		}
	}
	auto r = RestApiCall ("/v1/uploaddevices", j, ClientCertFingerprint);
	syslog(LOG_INFO, "Called v1/uploaddevices API with status_code %u", r);
	return uploads;
}

bool HostCache::UploadTrafficStats(const time_t interval, const std::string ClientCertFingerprint) {
	uint32_t uploads = 0;
	json j;
	for (auto it : hC) {
		if ( (! isWhitelisted(*(it.second))) && it.second->isMatched()) {
			json h;
			it.second->TrafficStats(h, interval, false);
			uploads++;
			j.push_back(h);
		}
	}
	auto r = RestApiCall ("/v1/uploadstats", j, ClientCertFingerprint);
	syslog(LOG_INFO, "Called v1/uploadstats API with status_code %u", r);
	return uploads;
}

bool HostCache::ImportDeviceProfileMatches(const std::string filename) {
	syslog(LOG_DEBUG, "Opening & reading %s", filename.c_str());
	std::ifstream ifs(filename);
	if (!ifs.is_open()) {
		syslog(LOG_WARNING, "Couldn't open %s", filename.c_str());
		return false;
	}
	json j;
	ifs >> j;

	uint32_t matches = 0;
	for (auto it = j.begin(); it != j.end(); ++it) {
	   ImportDeviceInfo(*it);
	   matches++;
	}
	ifs.close();
	syslog(LOG_INFO, "DeviceMatches read: %u", matches);
	return true;
}

bool HostCache::ImportDeviceInfo (json &j) {
	std::string DeviceProfileUuid;
	if (j.find("DeviceProfileUuid") == j.end()) {
		syslog(LOG_ERR, "No DeviceProfileUuid set, ignoring this Object");
		return false;
	}
	if (! j["DeviceProfileUuid"].is_string()) {
		syslog(LOG_ERR, "DeviceProfileUuid is not a string, ignoring this Object");
		return false;
	}
	if (DeviceProfileUuid != j["DeviceProfileUuid"].get<std::string>())
		return false;

	std::string MacAddress;
	if (j.find("MacAddress") == j.end()) {
		syslog(LOG_ERR, "No MacAddress set, ignoring this Object");
		return false;
	}
	if (! j["MacAddress"].is_string()) {
		syslog(LOG_ERR, "MacAddress is not a string, ignoring this Object");
		return false;
	}
	if (MacAddress != j["MacAddress"].get<std::string>())
		return false;

	syslog(LOG_DEBUG, "Importing Device Profile for UUID %s with MacAddress %s", DeviceProfileUuid.c_str(), MacAddress.c_str());

	auto hit = hC.find(MacAddress);
	if (hit != hC.end()) {
		std::string uuid = hit->second->Uuid_get();
		if (uuid != DeviceProfileUuid) {
			syslog(LOG_WARNING, "Conflicting Uuid for imported device with existing Host Cache");
			return false;
		}
	}
	if (! FindOrCreateHostByMac(MacAddress, DeviceProfileUuid)) {
		syslog(LOG_WARNING, "Failed to create Host with MacAddress %s and uuid %s", MacAddress.c_str(), DeviceProfileUuid.c_str());
		return false;
	}
	return true;
}
uint32_t HostCache::DeviceProfiles_load(const std::string filename) {
	syslog(LOG_DEBUG, "Opening & reading %s", filename.c_str());
	// Read the DeviceProfiles file
	std::ifstream ifs(filename);
	if (!ifs.is_open()) {
		syslog(LOG_WARNING, "Couldn't open %s", filename.c_str());
		return 0;
	}
	json j;
	ifs >> j;

	// Track which DeviceProfileUuids were read from the file
	std::unordered_set<std::string> uuids;

	// for (json::iterator it = j.begin(); it != j.end(); ++it) {
	for (auto it = j.begin(); it != j.end(); ++it) {
	  std::string uuid = (*it)["DeviceProfileUuid"].get<std::string>();
	  dpMap[uuid] = std::make_shared<DeviceProfile>(*it);
	  uuids.insert (uuid);
	}
	ifs.close();

	for (auto &kv : dpMap) {
		if (uuids.find(kv.first) == uuids.end()) {
			syslog(LOG_DEBUG, "Profile no longer in DeviceProfiles file: %s", kv.first.c_str());
			dpMap.erase(kv.first);
		}
	}
	auto s = uuids.size();
	syslog(LOG_DEBUG, "Profiles imported %lu", s);
	return s;
}

uint32_t HostCache::Whitelists_set (const std::unordered_set<std::string>& inIpv4Addresses,
		const std::unordered_set<std::string>& inIpv6Addresses,
		const std::unordered_set<std::string>& inMacAddresses) {
	WhitelistedNodes.empty();
	WhitelistedNodes.insert(inIpv4Addresses.begin(), inIpv4Addresses.end());
	WhitelistedNodes.insert(inIpv6Addresses.begin(), inIpv6Addresses.end());
	WhitelistedNodes.insert(inMacAddresses.begin(), inMacAddresses.end());
	return WhitelistedNodes.size();
}

uint32_t HostCache::HostDnsQueryCount (std::string IpAddress) {
	auto it = Ip2MacMap.find(IpAddress);
	if ( it == Ip2MacMap.end()) {
		return 0;
	}
	auto Mac = it->second;
	auto &h = hC[Mac];
	return h->DnsLogEntryCount();

}
