/**
 * @file associated_uris.h Definitions for AssociatedURIs class.
 *
 * Copyright (C) Metaswitch Networks
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef ASSOCIATED_URIS_H_
#define ASSOCIATED_URIS_H_

#include <string>
#include <vector>
#include <map>

struct AssociatedURIs
{
public:
  /// Gets sthe default IMPU from an implicit registration set.
  bool get_default_impu(std::string& uri,
                        bool emergency);

  /// Checks if a URI is in the list of assiated URIs.
  bool contains_uri(std::string uri);

  /// Adds to the list of associated URIs.
  void add_uri(std::string uri, bool barred);

  /// Adds the barring status of a URI.
  void add_barring_status(std::string uri, bool barred);

  /// Clears this structure.
  void clear_uris();

  /// Returns whether a URI is barred or not.
  bool is_impu_barred(std::string uri);

  /// Returns all the unbarred URIs.
  std::vector<std::string> get_unbarred_uris();

  /// Returns all the barred URIs.
  std::vector<std::string> get_barred_uris();

  /// Returns all URIs.
  std::vector<std::string> get_all_uris();

  /// Add a mapping between a distinct IMPU and the wildcard it belongs to
  void add_wildcard_mapping(std::string wildcard, std::string distinct);

private:
  /// A vector of associated URIs.
  std::vector<std::string> _associated_uris;

  /// A map from the associated URIs to their barring state.
  std::map<std::string, bool> _barred_map;

  /// A map of distinct IMPUs to their wildcards
  std::map<std::string, std::string> _distinct_to_wildcard;
};

#endif
