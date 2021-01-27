
// Created by David Guillen Fandos <david@davidgf.net> 2020

#ifndef __PROC_EXECUTOR_H__
#define __PROC_EXECUTOR_H__

#include <list>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <unordered_map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "cqueue.h"

// Forker, with maximum number of children

class Executor {
private:
	struct t_exec {
		std::string exec;
		std::vector<std::string> args;
		std::function<void(int)> cb;
	};

	void work() {
		t_exec elem;
		while (!end && queue.pop(&elem)) {
			pid_t child = fork();
			if (!child) {
				// Adjust niceness
				nice(this->niceadj);

				// We are the children, execv!
				char const *args[elem.args.size()+1] = {0};
				for (unsigned i = 0; i < elem.args.size(); i++)
					args[i] = (char*)elem.args[i].c_str();

				execvp(elem.exec.c_str(), (char* const*)args);
				exit(1);
			}

			// Wait for child to finish
			while (!end) {
				int status;
				pid_t ret = waitpid(child, &status, WNOHANG);
				if (ret <= 0)
					usleep(100000);  // Wait a bit to poll again
				else {
					if (elem.cb)
						elem.cb(status);
					break;
				}
			}
		}
	}

	ConcurrentQueue<t_exec> queue;
	unsigned niceadj;
	std::atomic<bool> end;
	std::vector<std::thread> workers;

public:
	Executor(unsigned maxinflight, unsigned niceadj = 0)
	: niceadj(niceadj), end(false) {
		for (unsigned i = 0; i < maxinflight; i++)
			workers.emplace_back(&Executor::work, this);
	}

	~Executor() {
		end = true;
		queue.close();
		for (auto & worker : workers)
			worker.join();
	}

	void execute(std::string executable,
	             std::vector<std::string> args,
	             std::function<void(int)> cb) {
		queue.push(
			t_exec{.exec = executable, .args = args, .cb = cb});
	}

	unsigned queue_size() const {
		return queue.size();
	}
};

#endif

