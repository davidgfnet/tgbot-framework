
#ifndef _UTIL__HH___
#define _UTIL__HH___

#include <stdint.h>
#include <string>
#include <vector>

// Creates a human-readable file size from byte count
std::string hsize(uint64_t size);

// Splits a string into chunks
std::vector<std::string> strsplit(const std::string &s, char c);

// Convert to and from base63
std::string to63(uint64_t n);
std::string to63(uint64_t n, unsigned digits);
uint64_t from63(std::string s);

// Trims spaces and stuff from both sides
std::string trim(const std::string &str);

// Remove repeated spaces
std::string tr(std::string s);

// URI-encodes a string
std::string urienc(std::string s);

// Escapes Markdown strings, telegram is a bit picky :)
std::string mdescape(std::string s);
std::string charescape(std::string s, char r);
// HTML escaping
std::string htmlescape(std::string s);

// Get the filename given a path
std::string basename(std::string fn);

// Make a short summary, kind of word based.
// TODO: Make it work in non-indoeuropean languages perhaps?
std::string makeshort(std::string msg, unsigned maxlen);

// Hex conversion functions
std::string tohex(const std::string & buf);
std::string fromhex(const std::string &h);

#endif

