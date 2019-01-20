
#include <string>
#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "util.h"

static const char *cset = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";

std::string to63(uint64_t n) {
	std::string ret;
	do {
		ret += cset[n % 63];
		n /= 63;
	} while (n);
	return ret;
}

uint64_t from63(std::string s) {
	uint64_t ret = 0;
	for (int i = s.size()-1; i >= 0; i--) {
		ret *= 63;
		const char *p = strchr(cset, s[i]);
		if (p)
			ret += (p - cset);
	}
	return ret;
}

std::string hsize(uint64_t size) {
	if (size < (1ULL<<10))
		return std::to_string(size) + "B";
	if (size < (1ULL<<20))
		return std::to_string(size >> 10) + "KB";
	if (size < (1ULL<<30))
		return std::to_string(size >> 20) + "MB";
	return std::to_string(size >> 30) + "GB";
}

std::string trim(const std::string &str) {
	auto first = str.find_first_not_of(' ');
    if (std::string::npos == first)
		return {};
	auto last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}

std::string tr(std::string s) {
	std::string output;
	unique_copy(s.begin(), s.end(), std::back_insert_iterator<std::string>(output),
	            [](char a, char b) { return std::isspace(a) && std::isspace(b);});
	return output;
}

static const char *hcharset = "0123456789abcdef";
std::string urienc(std::string s) {
	std::string ret;
	for (char c : s) {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			ret += c;
		else {
			ret.push_back('%');
			ret.push_back(hcharset[(*(uint8_t*)&c) >> 4]);
			ret.push_back(hcharset[(*(uint8_t*)&c) & 15]);
		}
	}
	return ret;
}

std::string mdescape(std::string s) {
	std::string ret;
	for (char c : s) {
		if (c == '_' || c == '*' || c == '[' || c == ']' || c == '`')
			ret += '\\';
		ret += c;
	}
	return ret;
}

std::string charescape(std::string s, char r) {
	std::string ret;
	for (char c : s) {
		if (c == r)
			ret += '\\';
		ret += c;
	}
	return ret;
}

std::string basename(std::string fn) {
	auto pos = fn.find_last_of('/');
	if (pos == std::string::npos)
		return fn;  // No path, just a file name
	return fn.substr(pos+1);
}

std::string makeshort(std::string msg, unsigned maxlen) {
	assert(maxlen > 10);
	if (msg.size() > maxlen) {
		while (!msg.empty() && msg.size() > maxlen - 3) {
			auto pos = msg.find_last_of(' ');
			if (pos != std::string::npos)
				msg.resize(pos);
			else
				msg.resize(maxlen - 3);
		}
		return msg + "...";
	}
	return msg;
}
