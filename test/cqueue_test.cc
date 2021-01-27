
#include "cqueue.h"
#include <cassert>

int main() {
	ConcurrentQueue<unsigned> q;
	assert(q.empty());
	q.push(1);
	q.push(2);
	q.push(3);
	assert(q.size() == 3);

	unsigned v;
	assert(q.pop(&v));
	assert(v == 1);
	assert(q.size() == 2);

	q.close();
	assert(q.size() == 2);
	assert(!q.pop(&v));
	assert(!q.empty());
}


