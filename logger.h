
// Created by David Guillen Fandos <david@davidgf.net> 2019

// Implements a log-on-disk that supports rotation
// and compression and flushing.

#include <mutex>
#include <ctime>

static std::time_t last_midnight() {
	std::time_t t = std::time(NULL);
	std::tm *tm = ::gmtime(&t);
	tm->tm_hour = 0;
	tm->tm_min  = 0;
	tm->tm_sec  = 0;
    return ::mktime(tm);
}

static std::string logts(bool date = false) {
	char fmtime[128];
	auto tp = std::chrono::system_clock::now();
	const std::time_t t = std::chrono::system_clock::to_time_t(tp);
	std::strftime(fmtime, sizeof(fmtime), date ? "%Y%m%d" : "%Y%m%d-%H%M%S", std::localtime(&t));
	return fmtime;
}

class Logger {
public:
	Logger(std::string logfile) : logfile(logfile) {
		// Create/append first log file
		rotatelog();

		// Create thread and start
		flusher = std::thread(&Logger::flushthread, this);
	}

	~Logger() {
		// Set end and wake flush thread
		{
			std::unique_lock<std::mutex> lock(waitmu);
			end = true;
		}
		waitcond.notify_all();

		// Wait for thread
		flusher.join();
	}

	void log(std::string line) {
		// Add line to memory buffer
		std::lock_guard<std::mutex> guard(mu);
		logbuffer += logts() + " " + line + "\n";

		// Tell flusher to flush this (lazily)
		waitcond.notify_all();
	}

private:

	void rotatelog() {
		// Try to rotate the log
		std::string localtime = logts(true);
		if (this->logdate == localtime && logfd >= 0)
			return;   // Already using that log

		this->logdate = localtime;
		if (logfd >= 0)
			close(logfd);

		std::string fn = logfile + "_" + this->logdate;
		logfd = open(fn.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);

		// Next run
		next_rotation = last_midnight() + 24*60*60;
	}

	void flushthread() {
		// Keeps flushing logs to disk periodically
		while (true) {
			std::unique_lock<std::mutex> lock(waitmu);
			waitcond.wait(lock);

			bool empty = false;
			do {
				// Read buffer we want ot write
				std::string chunk;
				{
					std::lock_guard<std::mutex> guard(mu);
					chunk = logbuffer.substr(0, 256*1024);
				}

				// Try to write as much as possible
				int writtenbytes = 0;
				while (!chunk.empty()) {
					int w = write(logfd, &chunk[writtenbytes], chunk.size() - writtenbytes);
					if (w > 0)
						writtenbytes += w;
					else
						break;
				}

				// Remove the written bits
				{
					std::lock_guard<std::mutex> guard(mu);
					logbuffer = logbuffer.substr(writtenbytes);
					empty = logbuffer.empty();
				}

				// Check log rotation
				if (time(NULL) > next_rotation && empty)
					rotatelog();
			} while (!empty);

			if (end)
				break;
		}
	}

	// List of stuff to be flushed
	std::string logbuffer;
	bool forceflush;
	std::mutex mu;

	// Thread that sits in the background flushing stuff
	std::thread flusher;
	std::mutex waitmu;
	std::condition_variable waitcond;
	std::string logdate;
	bool end = false;

	// Log management
	std::string logfile;
	int logfd = -1;
	std::time_t next_rotation = 0;
};


