#pragma once

#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

namespace bell {
class URLParser {
 public:
  std::string host;
  int port = -1;

  std::string schema = "http";
  std::string path;
  std::regex urlParseRegex = std::regex(
      "^(?:([^:/?#]+):)?(?://([^/?#]*))?([^?#]*)(\\?(?:[^#]*))?(#(?:.*))?");

  static URLParser parse(const std::string& url) {
    URLParser parser;

    // apply parser.urlParseRegex to url
    std::cmatch match;

    std::regex_match(url.c_str(), match, parser.urlParseRegex);

    if (match.size() < 3) {
      throw std::invalid_argument("Invalid URL");
    }

    parser.schema = match[1];
    parser.host = match[2];
    parser.path = match[3];

    if (match[4] != "") {
      parser.path += match[4];
    }

    // check if parser.host contains ':'
    if (parser.host.find(':') != std::string::npos) {
      auto port = std::stoi(parser.host.substr(parser.host.find(':') + 1, parser.host.size()));
      auto host = parser.host.substr(0, parser.host.find(':'));
      parser.port = port;
      parser.host = host;
    }

    if (parser.port == -1 ) {
      parser.port = parser.schema == "http" ? 80 : 443;
    }

    return parser;
  }
};
}  // namespace bell