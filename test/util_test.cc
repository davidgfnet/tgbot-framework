
#include "util.h"
#include <cassert>

int main() {
	// Test split function
	auto r1 = strsplit("a,b,c,d", ',');
	assert(r1.size() == 4);
	assert(r1[0] == "a" && r1[1] == "b" && r1[2] == "c" && r1[3] == "d");

	auto r2 = strsplit(",b,", ',');
	assert(r2.size() == 3);
	assert(r2[0] == "" && r2[1] == "b" && r2[2] == "");

	// Trim
	assert(trim("   ab cd ") == "ab cd");
	assert(trim("abcd") == "abcd");
	assert(trim("   ") == "" && trim(" ") == "");

	// To/From63
	for (uint64_t n = 0; n < (1 << 16); n++)
		assert(from63(to63(n)) == n);
	for (uint64_t n = 0; n < (1 << 16); n++)
		assert(from63(to63(n, 3)) == n);
	assert(to63(0) == "0");
	assert(from63("0") == 0);
	assert(to63(0, 4) == "0000");

	// Some other str functions
	assert(basename("/foo/bar/lol.pdf") == "lol.pdf");
	assert(basename("lol.pdf") == "lol.pdf");
	assert(basename("/lol.pdf") == "lol.pdf");

	// Hex functions
	assert(fromhex(tohex("123")) == "123");
	assert(fromhex("ff") == "\xff");
	assert(fromhex("F0") == "\xf0");
	assert(fromhex("41424344454647484142434445464748") == "ABCDEFGHABCDEFGH");
	assert(fromhex("1") == "");

	// Char escaping
	assert(charescape("foo*bar", '$') == "foo*bar");
	assert(charescape("foo*bar", '*') == "foo\\*bar");

	// Other str functions
	assert(tr("  foo      bar    ") == " foo bar ");
	assert(makeshort("hello there foo bar lol", 99) == "hello there foo bar lol");
	assert(makeshort("hello there foo bar lol", 13) == "hello...");
	assert(makeshort("hello there foo bar lol", 15) == "hello there...");
	assert(makeshort("hello there foo bar lol", 17) == "hello there...");
	assert(makeshort("hello there foo bar lol", 18) == "hello there foo...");

	// URL encoding
	assert(urienc("https://foobar?a=123+456") == "https%3a%2f%2ffoobar%3fa%3d123%2b456");

	// Markdown escape
	assert(mdescape("foo bar") == "foo bar");
	assert(mdescape("_foo_ [bar]") == "\\_foo\\_ \\[bar]");

	// HTML escape
	assert(htmlescape("<a> foo & \"lol\"") == "&lt;a&gt; foo &amp; &quot;lol&quot;");

	// Size repr
	assert(hsize(1) == "1B");
	assert(hsize(1580) == "1.5KiB");
	assert(hsize(1024*1024*2ULL) == "2.0MiB");
	assert(hsize(1024ULL*1024*1024*3) == "3.0GiB");
	assert(hsize(1024ULL*1024*1024*1024*5) == "5.0TiB");
}

