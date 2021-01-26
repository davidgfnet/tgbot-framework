
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

// Forker, with maximum number of children

class Executor {
private:
	struct t_exec {
		std::string exec;
		std::vector<std::string> args;
		std::function<void(int)> cb;
	};

	void work() {
		while (!end) {
			// Check whether we can add new children!
			while (inflight < maxinflight) {
				// Mutex acquire
				std::lock_guard<std::mutex> guard(queue_mutex);
				if (queue.empty())
					break;
				launch(queue.front());
				queue.pop_front();
				inflight++;
			}

			// Check for finished child
			int status;
			pid_t ret = waitpid(-1, &status, WNOHANG);

			if (ret <= 0)   // Just wait a bit, not too long
				usleep(100000);
			else {
				// Lookup the pid and call the callback
				if (ongoing.count(ret)) {
					// Callback could be null
					auto & cb = ongoing[ret].cb;
					if (cb)
						cb(status);
					ongoing.erase(ret);
					{
						std::lock_guard<std::mutex> guard(queue_mutex);
						inflight--;
					}
				}
			}
		}
	}

	void launch(t_exec ex) {
		pid_t child = fork();
		if (child) {
			// Add the t_exec in the map, indexed by PID
			ongoing[child] = ex;
		}
		else {
			// Adjust niceness
			nice(this->niceadj);

			// We are the children, execv!
			char const *args[ex.args.size()+1] = {0};
			for (unsigned i = 0; i < ex.args.size(); i++)
				args[i] = (char*)ex.args[i].c_str();

			execvp(ex.exec.c_str(), (char* const*)args);
			exit(1);
		}
	}

	std::list<t_exec> queue;
	std::unordered_map<pid_t, t_exec> ongoing;
	unsigned inflight, maxinflight;
	unsigned niceadj;
	std::atomic<bool> end;
	mutable std::mutex queue_mutex;
	std::thread worker;

public:
	Executor(unsigned maxinflight, unsigned niceadj = 0)
	:inflight(0), maxinflight(maxinflight), niceadj(niceadj), end(false) {
		worker = std::thread(&Executor::work, this);
	}

	~Executor() {
		end = true;
		worker.join();
	}

	void execute(std::string executable,
	             std::vector<std::string> args,
	             std::function<void(int)> cb) {
		// Fork ourselves and execute the binary.
		std::lock_guard<std::mutex> guard(queue_mutex);
		queue.push_back(
			t_exec{.exec = executable, .args = args, .cb = cb});
	}

	unsigned size() const {
		std::lock_guard<std::mutex> guard(queue_mutex);
		return queue.size() + inflight;
	}

	unsigned queue_size() const {
		std::lock_guard<std::mutex> guard(queue_mutex);
		return queue.size();
	}
};

#endif

