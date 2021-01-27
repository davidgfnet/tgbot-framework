
#include "executor.h"
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>

int main() {
	rmdir("/tmp/tmplocket99");

	Executor te1(4, 1);
	Executor te2(4, 1);
	Executor te3(4, 1);
	assert(te1.queue_size() == 0 && te2.queue_size() == 0);
	for (unsigned i = 0; i < 8; i++) {
		te1.execute("bash", {"bash", "-c", "until [ -d /tmp/tmplocket99 ]; do sleep 0.1; done"}, [] (int code) {} );
		te1.execute("bash", {"bash", "-c", "until [ -d /tmp/tmplocket99 ]; do sleep 0.1; done"}, NULL);
		te2.execute("bash", {"bash", "-c", "until [ -d /tmp/tmplocket99 ]; do sleep 0.1; done"}, NULL);
	}

	// Wait for it to schedule the processes
	while(te1.queue_size() != 12);
	assert(te1.queue_size() == 12);

	while(te2.queue_size() != 4);
	assert(te2.queue_size() == 4);

	// Release the lock
	te3.execute("mkdir", {"mkdir", "/tmp/tmplocket99"}, NULL);

	// Wait for it to drain
	while(te1.queue_size() > 0);
	assert(te1.queue_size() == 0);
}


