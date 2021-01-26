
#include "executor.h"
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>

int main() {
	rmdir("/tmp/tmplocket99");

	Executor *te = new Executor(4, 1);
	assert(te->size() == 0);
	for (unsigned i = 0; i < 8; i++) {
		te->execute("bash", {"bash", "-c", "until [ -d /tmp/tmplocket99 ]; do sleep 0.1; done"}, [] (int code) {} );
		te->execute("bash", {"bash", "-c", "until [ -d /tmp/tmplocket99 ]; do sleep 0.1; done"}, NULL);
	}
	assert(te->size() == 16);

	// Wait for it to schedule the processes
	while(te->queue_size() != 12);
	assert(te->queue_size() == 12);

	// Release the lock
	mkdir("/tmp/tmplocket99", S_IRWXU);

	// Wait for it to drain
	while(te->size() > 0);
	assert(te->size() == 0);

	delete te;
}


