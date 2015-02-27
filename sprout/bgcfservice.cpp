/**
 * @file bgcfservice.cpp class implementation for an BGCF service provider
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */


#include <sys/stat.h>
#include <json/reader.h>
#include <fstream>
#include <stdlib.h>

#include "bgcfservice.h"
#include "log.h"
#include "sas.h"
#include "sproutsasevent.h"

const boost::regex BgcfService::CHARS_TO_STRIP = boost::regex("[.)(-]");

BgcfService::BgcfService(std::string configuration) :
  _configuration(configuration),
  _updater(NULL)
{
  // Create an updater to keep the bgcf routes configured appropriately.
  _updater = new Updater<void, BgcfService>(this, std::mem_fun(&BgcfService::update_routes));
}

void BgcfService::update_routes()
{
  Json::Value root;
  Json::Reader reader;

  std::string jsonData;
  std::ifstream file;

  // Check whether the file exists.
  struct stat s;
  LOG_DEBUG("stat(%s) returns %d", _configuration.c_str(), stat(_configuration.c_str(), &s));
  if ((stat(_configuration.c_str(), &s) != 0) &&
      (errno == ENOENT))
  {
    LOG_STATUS("No BGCF configuration (file %s does not exist)",
               _configuration.c_str());
    return;
  }

  LOG_STATUS("Loading BGCF configuration from %s", _configuration.c_str());

  std::map<std::string, std::vector<std::string>> new_domain_routes;
  std::map<std::string, std::vector<std::string>> new_number_routes;

  file.open(_configuration.c_str());
  if (file.is_open())
  {
    if (!reader.parse(file, root))
    {
      LOG_WARNING("Failed to read BGCF configuration data, %s",
                  reader.getFormattedErrorMessages().c_str());
      return;
    }

    file.close();

    if (root["routes"].isArray())
    {
      Json::Value routes = root["routes"];

      for (size_t ii = 0; ii < routes.size(); ++ii)
      {
        Json::Value route = routes[(int)ii];

        // An entry is valid if it has either a domain (string) OR a 
        // number (string) AND an array of routes
        if (((((route.isMember("domain")) && 
               (route["domain"].isString()))   && 
              (!route.isMember("number"))) || 
             ((!route.isMember("domain"))  && 
              ((route.isMember("number")) && 
               (route["number"].isString())))) &&
            (route["route"].isArray()) )
        {
          std::vector<std::string> route_vec;
          Json::Value route_vals = route["route"];
          std::string routing_value;

          if (route["domain"].isString())
          {
            routing_value = route["domain"].asString();
          }
          else
          {
            routing_value = route["number"].asString();
          }

          LOG_DEBUG("Add route for %s", routing_value.c_str());

          for (size_t jj = 0; jj < route_vals.size(); ++jj)
          {
            Json::Value route_val = route_vals[(int)jj];
            std::string route_uri = route_val.asString();
            LOG_DEBUG("  %s", route_uri.c_str());
            route_vec.push_back(route_uri);
          }

          if (route["domain"].isString())
          {
            new_domain_routes.insert(std::make_pair(routing_value, route_vec));
          }
          else
          {
            new_number_routes.insert(
                      std::make_pair(remove_visual_separators(routing_value), 
                                     route_vec));
          }

          route_vec.clear();
        }
        else
        {
          LOG_WARNING("Badly formed BGCF route entry %s", route.toStyledString().c_str());
        }
      }

      _domain_routes = new_domain_routes;
      _number_routes = new_number_routes;
    }
    else
    {
      LOG_WARNING("Badly formed BGCF configuration file - missing routes object");
    }
  }
  else
  {
    //LCOV_EXCL_START
    LOG_WARNING("Failed to read BGCF configuration data %d", file.rdstate());
    //LCOV_EXCL_STOP
  }
}

BgcfService::~BgcfService()
{
  // Destroy the updater (if it was created).
  delete _updater;
  _updater = NULL;
}

std::vector<std::string> BgcfService::get_route_from_domain(
                                                const std::string &domain,
                                                SAS::TrailId trail) const
{
  LOG_DEBUG("Getting route for URI domain %s via BGCF lookup", domain.c_str());

  // First try the specified domain.
  std::map<std::string, std::vector<std::string>>::const_iterator i = 
                                                    _domain_routes.find(domain);
  if (i != _domain_routes.end())
  {
    LOG_INFO("Found route to domain %s", domain.c_str());

    SAS::Event event(trail, SASEvent::BGCF_FOUND_ROUTE_DOMAIN, 0);
    event.add_var_param(domain);
    std::string route_string;

    for (std::vector<std::string>::const_iterator ii = i->second.begin(); ii != i->second.end(); ++ii)
    {
      route_string = route_string + *ii + ";";
    }

    event.add_var_param(route_string);
    SAS::report_event(event);

    return i->second;
  }

  // Then try the default domain (*).
  i = _domain_routes.find("*");
  if (i != _domain_routes.end())
  {
    LOG_INFO("Found default route");

    SAS::Event event(trail, SASEvent::BGCF_DEFAULT_ROUTE_DOMAIN, 0);
    event.add_var_param(domain);
    std::string route_string;

    for (std::vector<std::string>::const_iterator ii = i->second.begin(); ii != i->second.end(); ++ii)
    {
      route_string = route_string + *ii + ";";
    }

    event.add_var_param(route_string);
    SAS::report_event(event);

    return i->second;
  }

  SAS::Event event(trail, SASEvent::BGCF_NO_ROUTE_DOMAIN, 0);
  event.add_var_param(domain);
  SAS::report_event(event);

  return std::vector<std::string>();
}

std::vector<std::string> BgcfService::get_route_from_number(
                                                const std::string &number,
                                                SAS::TrailId trail) const
{
  // The number routes map is ordered by length of key. Start from the end of 
  // the map to get the longest prefixes first. 
  for (std::map<std::string, std::vector<std::string>>::const_reverse_iterator it =
        _number_routes.rbegin();
       it != _number_routes.rend();
       it++)
  {
    int len = std::min(number.size(), (*it).first.size());

    if (remove_visual_separators(number).compare(0, 
                                                 len, 
                                                 (*it).first, 
                                                 0, 
                                                 len) == 0)
    {
      // Found a match, so return it
      LOG_DEBUG("Match found. Number: %s, prefix: %s",
                number.c_str(), (*it).first.c_str());

      SAS::Event event(trail, SASEvent::BGCF_FOUND_ROUTE_NUMBER, 0);
      event.add_var_param(number);
      std::string route_string;

      for (std::vector<std::string>::const_iterator ii = (*it).second.begin(); 
                                                    ii != (*it).second.end(); 
                                                    ++ii)
      {
        route_string = route_string + *ii + ";";
      }

      event.add_var_param(route_string);
      SAS::report_event(event);

      return (*it).second;
    }
  }

  SAS::Event event(trail, SASEvent::BGCF_NO_ROUTE_NUMBER, 0);
  event.add_var_param(number);
  SAS::report_event(event);

  return std::vector<std::string>();
}
