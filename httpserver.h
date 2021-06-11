
// Created by David Guillen Fandos <david@davidgf.net> 2021

// Super-thin wrapper around microhttpd library
// Mainly single-threaded.

#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <microhttpd.h>
#include <iostream>

#if MHD_VERSION < 0x00097002
#define MHD_Result int
#endif

class HTTPReq {
public:
	HTTPReq(std::string method, std::string url)
		:method(method), url(url), truncated(false) {}
	std::string method;
	std::string url;
	std::string body;
	bool truncated;
};

class HTTPResp {
public:
	HTTPResp(unsigned status, std::string content_type, std::string data)
		:status(status), content_type(content_type), data(data) {}
	unsigned status;
	std::string content_type;
	std::string data;
};


class HTTPServer {
public:
	HTTPServer(unsigned port, std::function<void(const HTTPReq*)> usercb,
	           unsigned max_req_size = 8*1024) {
		this->daemon = NULL;
		this->usercb = usercb;
		this->port = port;
		this->max_req_size = max_req_size;
		this->drained = false;
	}

	bool serve() {
		// Create server and start listening
		std::lock_guard<std::mutex> g(mtx);
		daemon = MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY | MHD_ALLOW_SUSPEND_RESUME, port, NULL, NULL,
			&http_callback, this, MHD_OPTION_END);
		return daemon;
	}

	void stop() {
		{
			// Stop accepting new connections and clean up everything
			std::lock_guard<std::mutex> g(mtx);
			if (daemon)
				MHD_quiesce_daemon(daemon);
			// Ensure all callbacks will do nothing but return
			drained = true;
			
			// Now proceed to clean up any ongoing reqs
			for (auto it : ongoing)
				delete it.second;
			for (auto it : toreply) {
				// This is required to call stop()
				MHD_resume_connection(it.second);
				delete it.first;
			}
			ongoing.clear();
			toreply.clear();

		}
		// Now we can kill the daemon more or less cleanly
		if (daemon)
			MHD_stop_daemon(daemon);
		daemon = NULL;
	}

	bool respond(const HTTPReq *req, const HTTPResp *resp) {
		// Check whether we have a pending request for this at all!
		std::lock_guard<std::mutex> g(mtx);
		if (!daemon || drained)
			return false;

		struct MHD_Connection *connection = this->toreply[req];
		if (!connection)
			return false;

		// Queue a response
		struct MHD_Response *mresp = MHD_create_response_from_buffer(
			resp->data.size(), (char*)resp->data.c_str(), MHD_RESPMEM_MUST_COPY);
		MHD_add_response_header(mresp, MHD_HTTP_HEADER_CONTENT_TYPE, resp->content_type.c_str());
		MHD_queue_response(connection, resp->status, mresp);
		MHD_destroy_response(mresp);

		// Cleanup
		toreply.erase(req);
		delete req;

		// Can start to respond now that the connection has some response
		MHD_resume_connection(connection);
		
		return true;
	}

private:

	static MHD_Result http_callback(
		void *cls, struct MHD_Connection *connection,
		const char *url, const char *method,
		const char *version, const char *upload_data,
		size_t *upload_data_size, void **con_cls) {

		HTTPServer *tptr = (HTTPServer*)cls;
		std::unique_lock<std::mutex> g(tptr->mtx);
		if (tptr->drained)
			return MHD_NO;

		// Initialize the connection the first time this gets called, nothing else to do!
		if (!tptr->ongoing.count(connection)) {
			tptr->ongoing.emplace(connection, new HTTPReq(method, url));
			return MHD_YES;
		}

		HTTPReq *req = tptr->ongoing[connection];
		
		// Add data to the request if any
		if (*upload_data_size) {
			// If over the limit, mark it as truncated and stop buffering.
			if (req->body.size() > tptr->max_req_size)
				req->truncated = true;
			else
				req->body += std::string(upload_data, *upload_data_size);
		}
		else {
			// Move the reference to the right queue, before the callback!
			tptr->ongoing.erase(connection);
			tptr->toreply.emplace(req, connection);

			// Since we do not plan to respond now, ensure we do not get any more callbacks
			MHD_suspend_connection(connection);

			// User callback, pass request. Can unlock already to prevent deadlocks
			g.unlock();
			tptr->usercb(req);
		}

		// Keep asking for more data!
		*upload_data_size = 0;
		return MHD_YES;
	}

	// Daemon itself and configs
	struct MHD_Daemon *daemon;
	unsigned port, max_req_size;
	// User callback
	std::function<void(const HTTPReq*)> usercb;
	// Requests being processed, by connection
	std::unordered_map<void*, HTTPReq*> ongoing;
	// Requests indexed by request, for the purpose of replying
	std::unordered_map<const HTTPReq*, struct MHD_Connection*> toreply;
	// Multithread protections
	mutable std::mutex mtx;
	bool drained;
};

