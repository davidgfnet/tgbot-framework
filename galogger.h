
// Created by David Guillen Fandos <david@davidgf.net> 2019

// Implements a Google Analytics logger so that you can track
// user interactions in real time.
// Uses the GET api for simplicity

#ifndef _GA_LOGGER_HH__
#define _GA_LOGGER_HH__

#include <mutex>
#include <map>
#include <list>
#include <thread>
#include <condition_variable>

#include "httpclient.h"

#define BASE_GA_URL "https://www.google-analytics.com/collect"

// Event to push:
// Exceptions have a type, to track errors reported to the user (or internal errors too).
// Pageviews, which have location (dl), hostname (dh), path (dp), title (dt)
// Events, which have category (ec), action (ea), value (ev), label (el)
class GAUpdate {
protected:
	uint64_t t;           // Keeps the time logged (ms), since the push might happen some seconds later
	uint64_t userid;      // User id, to track users

	GAUpdate(uint64_t userid) : t(ptimems()), userid(userid), attempts(0) {}

	static uint64_t ptimems() {
		struct timespec spec;
		clock_gettime(CLOCK_REALTIME, &spec);

		uint64_t ret = spec.tv_sec;
		ret = ret * 1000 + (spec.tv_nsec / 1000000);
		return ret;
	}

public:
	unsigned attempts;

	virtual ~GAUpdate() {}

	virtual std::map<std::string, std::string> serialize() const {
		return {
			{"v",   "1"},
			{"uid", std::to_string(userid)},
			{"qt",  std::to_string(ptimems() - t)},
		};
	}
};

class GAPageView : public GAUpdate {
protected:
	std::string dt, dh, dp;
public:
	GAPageView(uint64_t userid, std::string title, std::string host, std::string path)
	 : GAUpdate(userid), dt(title), dh(host), dp(path) { }

	std::map<std::string, std::string> serialize() const {
		auto ret = GAUpdate::serialize();
		ret["t"] = "pageview";
		ret["dt"] = dt;
		ret["dh"] = dh;
		ret["dp"] = dp;
		return ret;
	}
};

class GAEvent : public GAUpdate {
protected:
	std::string ecat, eaction;
public:
	GAEvent(uint64_t userid, std::string category, std::string action)
	 : GAUpdate(userid), ecat(category), eaction(action) { }

	std::map<std::string, std::string> serialize() const {
		auto ret = GAUpdate::serialize();
		ret["t"] = "event";
		ret["ec"] = ecat;
		ret["ea"] = eaction;
		return ret;
	}
};

class GoogleAnalyticsLogger {
public:
	GoogleAnalyticsLogger(std::string trackingid) {
		// Spawn a thread to push events, hopefully enough
		writerth = std::thread(&GoogleAnalyticsLogger::pusher_thread, this);
	}

	~GoogleAnalyticsLogger() {
		// Set end and wake flush thread
		{
			std::unique_lock<std::mutex> lock(waitmu);
			end = true;
		}
		waitcond.notify_all();

		// Wait for thread
		writerth.join();
	}

	void push_event(GAUpdate *event) {
		// Add line to memory buffer
		std::lock_guard<std::mutex> guard(mu);
		event_queue.push_back(event);

		// Tell flusher to flush this (lazily)
		waitcond.notify_all();
	}

private:

	void pusher_thread() {
		// Keeps pushing data to the GA frontend as long as there's some in the queue
		while (true) {
			std::unique_lock<std::mutex> lock(waitmu);
			waitcond.wait(lock);

			bool empty;
			{
				std::lock_guard<std::mutex> guard(mu);
				empty = event_queue.empty();
			}

			while (!empty) {
				// Read buffer we want ot write
				GAUpdate* upd;
				{
					std::lock_guard<std::mutex> guard(mu);
					upd = event_queue.front();
					event_queue.pop_front();
				}

				// Push via GET request
				auto args = upd->serialize();
				client->doGET(BASE_GA_URL, args, nullptr,
					[upd, this] (bool ok) mutable {
						if (!ok && upd->attempts < 5) {
							// Failed! Let's retry, to simplify things just add to queue again
							std::lock_guard<std::mutex> guard(this->mu);
							upd->attempts++;
							this->event_queue.push_back(upd);
						}
						else {
							// Completed or too many retries, delete req
							delete upd;
						}
					}
				);
			}

			if (end)
				break;
		}
	}

	// Mutex protected queue and http client
	std::list<GAUpdate*> event_queue;
	HttpClient *client;
	std::mutex mu;

	// Thread that sits in the background flushing stuff
	std::thread writerth;
	std::mutex waitmu;
	std::condition_variable waitcond;
	bool end = false;
};

#endif

