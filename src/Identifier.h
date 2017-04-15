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

 * Identifier.h
 *
 *  Created on: Mar 25, 2017
 *      Author: steven
 */

#ifndef IDENTIFIER_H_
#define IDENTIFIER_H_

#include <vector>
#include <map>
#include <memory>

#include <syslog.h>

#include <json.hpp>
using nlohmann::json;

#include "noddos.h"
#include "MatchCondition.h"

class Identifier {
private:
	ConfidenceLevel IdentifyConfidenceLevel;
	ConfidenceLevel EnforceConfidenceLevel;
	std::vector<std::shared_ptr<MatchCondition>> MatchConditions;

public:
	Identifier(json &j) {
		if (j.find("IdentifyConfidenceLevel") == j.end()) {
			syslog(LOG_INFO, "No IdentifyConfidenceLevel set, defaulting to `Low'");
			IdentifyConfidenceLevel = ConfidenceLevel::Low;
		} else {
			if (! j["IdentifyConfidenceLevel"].is_string()) {
				syslog(LOG_ERR, "IdentifyConfidenceLevel is not a string, defaulting to `Low'");
				IdentifyConfidenceLevel = ConfidenceLevel::Low;;
			} else {
				// TODO *sigh* surely there is a better way!
				std::string cl = j["IdentifyConfidenceLevel"].get<std::string>();
				for(unsigned int i = 0; i < cl.length(); ++i)
				    cl[i] = tolower(cl[i]);

				if(cl == "low")
					IdentifyConfidenceLevel = ConfidenceLevel::Low;
				else if (cl == "medium")
					IdentifyConfidenceLevel = ConfidenceLevel::Medium;
				else if (cl == "high")
					IdentifyConfidenceLevel = ConfidenceLevel::High;
			}
		}
		if (j.find("EnforceConfidenceLevel") == j.end()) {
			syslog(LOG_INFO, "No EnforceConfidenceLevel set, defaulting to `None'");
			EnforceConfidenceLevel = ConfidenceLevel::None;
		} else {
			if (! j["EnforceConfidenceLevel"].is_string()) {
				syslog(LOG_ERR, "EnforceConfidenceLevel is not a string, defaulting to `None'");
				EnforceConfidenceLevel = ConfidenceLevel::None;
			} else {
				// TODO *sigh* surely there must be a better way!
				std::string cl = j["EnforceConfidenceLevel"].get<std::string>();
				for(unsigned int i = 0; i < cl.length(); ++i)
				    cl[i] = tolower(cl[i]);
				EnforceConfidenceLevel = ConfidenceLevel::None;
				if(cl == "low")
					EnforceConfidenceLevel = ConfidenceLevel::Low;
				else if (cl == "medium")
					EnforceConfidenceLevel = ConfidenceLevel::Medium;
				else if (cl == "high")
					EnforceConfidenceLevel = ConfidenceLevel::High;
			}
		}
		if (j.find("MustMatch") == j.end() && j.find("MustContain") == j.end()) {
			syslog(LOG_ERR, "Identifier has no MustMatch and no MustContain restrctions");
			return;
		}
		if (j.find("MustMatch") != j.end()) {
			if (!j["MustMatch"].is_object()) {
				syslog(LOG_ERR, "MustMatch condition is not a JSON Object");
			} else {
				for (json::iterator it = j["MustMatch"].begin(); it != j["MustMatch"].end(); ++it ) {
					auto mc = std::make_shared<MatchCondition>(it.key(), it.value(), false);
					MatchConditions.push_back(mc);
				}
			}
		}
		if (j.find("MustContain") != j.end()) {
			if (!j["MustContain"].is_object()) {
				syslog(LOG_ERR, "MustContain condition is not a JSON Object");
			} else {
				for (json::iterator it = j["MustContain"].begin(); it != j["MustContain"].end(); ++it) {
					auto mc = std::make_shared<MatchCondition>(it.key(), it.value(), true);
					MatchConditions.push_back(mc);
				}
			}
		}
	}
	~Identifier() {
		syslog (LOG_DEBUG, "Destroying Identifier instance");
	};
	ConfidenceLevel IdentifyConfidenceLevel_get () const { return IdentifyConfidenceLevel; }
	const std::vector<std::shared_ptr<MatchCondition>>& MatchConditions_get() const { return MatchConditions; }
};

#endif /* IDENTIFIER_H_ */
