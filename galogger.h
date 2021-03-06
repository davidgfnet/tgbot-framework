
// Created by David Guillen Fandos <david@davidgf.net> 2019

// Implements a Google Analytics logger so that you can track
// user interactions in real time.
// Uses the GET api for simplicity

#ifndef _GA_LOGGER_HH__
#define _GA_LOGGER_HH__

#include <mutex>
#include <unordered_map>
#include <list>
#include <thread>
#include <memory>
#include <iterator>
#include <condition_variable>

#include "httpclient.h"

#define PUSH_TIMEOUT  std::chrono::seconds(10)
#define PUSH_COUNT    size_t(20)
#define BASE_GA_URL   "https://www.google-analytics.com/batch"

// Event to push:
// Exceptions have a type, to track errors reported to the user (or internal errors too).
// Pageviews, which have location (dl), hostname (dh), path (dp), title (dt)
// Events, which have category (ec), action (ea), value (ev), label (el)
class GAUpdate {
protected:
	uint64_t t;           // Keeps the time logged (ms), since the push might happen some seconds later
	uint64_t userid;      // User id, to track users
	std::string lang;     // Language (if set)

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

	void set_lang(const std::string &lang) {
		this->lang = lang;
	}

	virtual ~GAUpdate() {}

	virtual std::unordered_multimap<std::string, std::string> serialize() const {
		return {
			{"v",   "1"},
			{"cid", std::to_string(userid)},
			{"qt",  std::to_string(ptimems() - t)},
			{"ul",  lang},
		};
	}
};

class GAPageView : public GAUpdate {
protected:
	std::string dt, dh, dp;
public:
	GAPageView(uint64_t userid, std::string title, std::string host, std::string path)
	 : GAUpdate(userid), dt(title), dh(host), dp(path) { }

	std::unordered_multimap<std::string, std::string> serialize() const {
		auto ret = GAUpdate::serialize();
		ret.emplace("t", "pageview");
		ret.emplace("dt", dt);
		ret.emplace("dh", dh);
		ret.emplace("dp", dp);
		return ret;
	}
};

class GAEvent : public GAUpdate {
protected:
	std::string ecat, eaction;
public:
	GAEvent(uint64_t userid, std::string category, std::string action)
	 : GAUpdate(userid), ecat(category), eaction(action) { }

	std::unordered_multimap<std::string, std::string> serialize() const {
		auto ret = GAUpdate::serialize();
		ret.emplace("t", "event");
		ret.emplace("ec", ecat);
		ret.emplace("ea", eaction);
		return ret;
	}
};

class GoogleAnalyticsLogger {
public:
	GoogleAnalyticsLogger(std::string trackingid)
	 : trackingid(trackingid), successful_hits(0), failed_hits(0) {
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
		event_queue.emplace_back(event);

		// Tell flusher to flush this (lazily)
		waitcond.notify_all();
	}

	unsigned getSuccessfulHits() const {
		std::lock_guard<std::mutex> guard(this->mu);
		return successful_hits;
	}

	unsigned getFailedHits() const {
		std::lock_guard<std::mutex> guard(this->mu);
		return failed_hits;
	}

private:

	std::string serialize_hit(std::unordered_multimap<std::string, std::string> args) {
		std::string ret;
		for (auto it : args)
			ret += it.first + "=" + HttpClient::urlescape(it.second) + "&";
		ret.pop_back();
		return ret;
	}

	void pusher_thread() {
		// Keeps pushing data to the GA frontend as long as there's some in the queue
		while (!end) {
			std::unique_lock<std::mutex> lock(waitmu);
			bool tout = std::cv_status::timeout == waitcond.wait_until(
				lock, std::chrono::system_clock::now() + PUSH_TIMEOUT);

			std::list<std::shared_ptr<GAUpdate>> upd;
			do {
				upd.clear();
				{
					std::lock_guard<std::mutex> guard(mu);
					if (event_queue.size() > PUSH_COUNT || tout) {
						// Extract up to PUSH_COUNT elements to be pushed
						upd.splice(upd.begin(), event_queue, event_queue.begin(),
							std::next(event_queue.begin(), std::min(PUSH_COUNT, event_queue.size())));
					}
				}

				if (!upd.empty()) {
					// Push via batch POST request
					std::string payload;
					for (auto hit : upd) {
						auto args = hit->serialize();
						args.emplace("tid", trackingid);
						payload += serialize_hit(args) + "\n";
					}
					client.doPOST(BASE_GA_URL, payload, nullptr,
						[upd, this] (bool ok) mutable {
							std::lock_guard<std::mutex> guard(this->mu);
							if (ok)
								successful_hits++;
							else {
								for (auto elem : upd) {
									if (elem->attempts >= 5)
										failed_hits++;    // Just drop it!
									else {
										// Failed! Let's retry, to simplify things just add to queue again
										elem->attempts++;
										this->event_queue.push_back(elem);
									}
								}
							}
						}
					);
				}
			} while (!upd.empty() && !end);
		}
	}

	std::string trackingid;
	unsigned successful_hits, failed_hits;

	// Mutex protected queue and http client
	std::list<std::shared_ptr<GAUpdate>> event_queue;
	HttpClient client;
	mutable std::mutex mu;

	// Thread that sits in the background flushing stuff
	std::thread writerth;
	mutable std::mutex waitmu;
	std::condition_variable waitcond;
	bool end = false;
};

#endif

