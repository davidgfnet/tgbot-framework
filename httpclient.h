
// Implements a HTTPs client that works in async fashion.
// It is able to perform requests and add requests on the fly.
// Uses libcurl as backend.

#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <thread>
#include <functional>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <curl/curl.h>

#define CONNECT_TIMEOUT    30   // We will retry, but that sounds like a lot
#define TRANSFER_TIMEOUT   60   // Abort after a minute, not even uploads are that slow

typedef size_t(*curl_write_function)(char *ptr, size_t size, size_t nmemb, void *userdata);
typedef int (*curl_progress_function)(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                      curl_off_t ultotal, curl_off_t ulnow);

class HttpClient {
public:
// Objects used to upload files (either from buffer or path).
	class UploadFile {
	public:
		UploadFile(std::string name, std::string mimetype)
		 :name(name), mimetype(mimetype) {}
		virtual ~UploadFile() {}
		virtual std::string read(unsigned amount) = 0;
		virtual size_t size() = 0;
		std::string name, mimetype;
	};
	class DiskFile : public UploadFile {
	public:
		~DiskFile() {
			if (fd)
				fclose(fd);
		}
		DiskFile(std::string name, std::string filepath, std::string mimetype)
			: UploadFile(name, mimetype), fsize(0)
		{
			fd = fopen(filepath.c_str(), "rb");
			if (fd) {
				fseek(fd, 0L, SEEK_END);
				fsize = ftell(fd);
				fseek(fd, 0L, SEEK_SET);
			}
		}
		virtual std::string read(unsigned amount) {
			if (fd) {
				char tmp[8192];
				unsigned toread = std::min(amount, 8192U);
				size_t bgot = fread(tmp, 1, toread, fd);
				if (bgot > 0)
					return std::string(&tmp[0], bgot);
			}
			return {};
		}
		virtual size_t size() { return fsize; }
		FILE * fd;
		unsigned fsize;
	};
	class MemFile : public UploadFile {
	public:
		~MemFile() {}
		MemFile(std::string name, std::string content, std::string mimetype)
			: UploadFile(name, mimetype), content(content), offset(0) {}
		virtual std::string read(unsigned amount) {
			std::string ret = content.substr(offset, amount);
			offset += ret.size();
			return ret;
		}
		virtual size_t size() { return content.size(); }
		std::string content;
		unsigned offset;
	};

private:
	class t_query {
	public:
		t_query() : form(NULL) {}
		~t_query() {
			if (form)
				curl_formfree(form);
		}
		std::function<bool(std::string)> wrcb;  // Write callback (data download)
		std::function<void(bool)>      donecb;  // End callback with result
		std::function<void(unsigned, unsigned)> pgcb;  // Upload/Download % progress callback
		struct curl_httppost *form;             // Any form post data
		std::vector<std::unique_ptr<UploadFile>> files;  // Keep files around
	};
	// Client thread
	std::thread worker;
	// Queue of pending requests to be performed
	std::map<CURL*, std::unique_ptr<t_query>> rqueue;
	std::mutex rqueue_mutex;
	// Multi handlers that is in charge of doing requests.
	CURLM *multi_handle;
	std::map<CURL*, std::unique_ptr<t_query>> request_set;
	// Signal end of thread
	bool end;
	// Timeouts
	unsigned connto, tranfto;
	// Pipe used to signal select() end, so we can add new requests
	int pipefd[2];

public:

	HttpClient(unsigned connto = CONNECT_TIMEOUT, unsigned tranfto = TRANSFER_TIMEOUT)
		: multi_handle(curl_multi_init()), end(false), connto(connto), tranfto(tranfto) {
		// Create a new pipe, make both ends non-blocking
		if (pipe(pipefd) < 0)
			throw std::system_error();

		for (unsigned i = 0; i < 2; i++) {		
			int flags = fcntl(pipefd[i], F_GETFL);
			if (flags < 0)
				throw std::system_error();
			if (fcntl(pipefd[i], F_SETFL, flags | O_NONBLOCK) < 0)
				throw std::system_error();
		}

		// Start thread
		worker = std::thread(&HttpClient::work, this);
	}

	~HttpClient() {
		// Mark it as done
		end = true;

		// Unblock the thread
		write(pipefd[1], "\0", 1);

		// Now detroy the thread
		worker.join();

		// Close the signaling pipe
		close(pipefd[0]);
		close(pipefd[1]);

		// Manually cleanup any easy handles inflight or pending
		for (const auto & req: request_set) {
			curl_multi_remove_handle(multi_handle, req.first);
			curl_easy_cleanup(req.first);
		}
		for (const auto & req: rqueue) {
			curl_multi_remove_handle(multi_handle, req.first);
			curl_easy_cleanup(req.first);
		}

		// Wipe multi
		curl_multi_cleanup(multi_handle);
	}

	static std::string urlescape(std::string q) {
		char *ret = curl_escape(q.c_str(), q.size());
		std::string r(ret);
		curl_free(ret);
		return r;
	}

	static std::string urlunescape(std::string q) {
		char *ret = curl_unescape(q.c_str(), q.size());
		std::string r(ret);
		curl_free(ret);
		return r;
	}

	void doGET(const std::string &url,
		std::unordered_multimap<std::string, std::string> args,
		std::function<bool(std::string)> wrcb = nullptr,
		std::function<void(bool)> donecb = nullptr,
		std::function<void(unsigned, unsigned)> progcb = nullptr) {
		// Process args
		std::string argstr;
		for (auto arg : args)
			argstr += "&" + arg.first + "=" + urlescape(arg.second);
		if (!argstr.empty())
			argstr[0] = '?';

		CURL *req = curl_easy_init();
		auto userq = std::make_unique<t_query>();
		userq->wrcb = std::move(wrcb);
		userq->donecb = std::move(donecb);
		userq->pgcb = std::move(progcb);
		curl_easy_setopt(req, CURLOPT_URL, (url + argstr).c_str());
		curl_easy_setopt(req, CURLOPT_CONNECTTIMEOUT, connto);
		curl_easy_setopt(req, CURLOPT_TIMEOUT, tranfto);
		curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(req, CURLOPT_AUTOREFERER, 1L);
		curl_easy_setopt(req, CURLOPT_MAXREDIRS, 5);

		this->doQuery(req, std::move(userq));
	}

	void doPOST(const std::string &url,
		std::unordered_multimap<std::string, std::string> args,
		std::unordered_multimap<std::string, UploadFile*> files = {},
		std::function<bool(std::string)> wrcb = nullptr,
		std::function<void(bool)> donecb = nullptr,
		std::string userpass = "") {

		struct curl_httppost *lastptr = NULL;
		auto userq = std::make_unique<t_query>();
		userq->donecb = std::move(donecb);
		userq->wrcb = std::move(wrcb);

		// Add args to forms
		for (const auto & a : args)
			curl_formadd(&userq->form, &lastptr,
				         CURLFORM_COPYNAME, a.first.c_str(),
				         CURLFORM_COPYCONTENTS, a.second.c_str(), CURLFORM_END);
		// Add files (diskfiles and memfiles)
		for (auto & f : files) {
			curl_formadd(&userq->form, &lastptr,
				         CURLFORM_COPYNAME, f.first.c_str(),
				         CURLFORM_CONTENTTYPE, f.second->mimetype.c_str(),
				         CURLFORM_CONTENTLEN, f.second->size(),
			             CURLFORM_STREAM, (void*)f.second,
				         CURLFORM_FILENAME, f.second->name.c_str(), CURLFORM_END);
			userq->files.emplace_back(f.second);
		}

		curl_write_function wrapperfn{[]
			(char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
				// Just keep calling read() with the right amount and memcpy it
				UploadFile *f = static_cast<UploadFile*>(userdata);
				std::string data = f->read(size * nmemb);
				memcpy(ptr, data.c_str(), data.size());
				return data.size();
			}
		};

		CURL *req = curl_easy_init();
		curl_easy_setopt(req, CURLOPT_POST, 1);
		curl_easy_setopt(req, CURLOPT_URL, url.c_str());
		curl_easy_setopt(req, CURLOPT_CONNECTTIMEOUT, connto);
		curl_easy_setopt(req, CURLOPT_TIMEOUT, tranfto);
		curl_easy_setopt(req, CURLOPT_READFUNCTION, wrapperfn);
		curl_easy_setopt(req, CURLOPT_HTTPPOST, userq->form);
		if (!userpass.empty())
			curl_easy_setopt(req, CURLOPT_USERPWD, userpass.c_str());

		this->doQuery(req, std::move(userq));
	}

	void doQuery(CURL *req, std::unique_ptr<t_query> userq) {
		curl_write_function wrapperfn{[]
			(char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
				// Push data to the user-defined callback if any
				t_query *q = static_cast<t_query*>(userdata);
				if (q->wrcb && !q->wrcb(std::string(ptr, size*nmemb)))
					return 0;
				return size * nmemb;
			}
		};

		curl_progress_function progfn{[]
			(void *ptr, curl_off_t dltotal, curl_off_t dlnow,
			 curl_off_t ultotal, curl_off_t ulnow) -> int {
				t_query *q = static_cast<t_query*>(ptr);
				if (q->pgcb)
					q->pgcb((dlnow * 1000) / (dltotal ?: 1), (ulnow * 1000) / (ultotal ?: 1));
				return 0;
			}
		};

		curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, wrapperfn);
		curl_easy_setopt(req, CURLOPT_WRITEDATA, userq.get());
		if (progfn) {
			curl_easy_setopt(req, CURLOPT_XFERINFOFUNCTION, progfn);
			curl_easy_setopt(req, CURLOPT_XFERINFODATA, userq.get());
			curl_easy_setopt(req, CURLOPT_NOPROGRESS, 0);
		}

		// Enqueues a query in the pending queue
		{
			std::lock_guard<std::mutex> guard(rqueue_mutex);
			rqueue[req] = std::move(userq);
		}

		// Use self-pipe trick to make select return immediately
		write(pipefd[1], "\0", 1);
	}

	// Will process http client requests
	void work() {
		while (!end) {
			// Process input queue to add new requests
			{
				std::lock_guard<std::mutex> guard(rqueue_mutex);
				for (auto & req: rqueue) {
					// Add to the Multi client
					curl_multi_add_handle(multi_handle, req.first);
					// Add it to the req_set
					request_set[req.first] = std::move(req.second);
				}
				rqueue.clear();
			}

			// Work a bit, non blocking fashion
			int nbLeft;
			while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi_handle, &nbLeft));

			struct timeval timeout;
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			int maxfd;
			fd_set rd, wr, er;
			FD_ZERO(&rd); FD_ZERO(&wr); FD_ZERO(&er);
			FD_SET(pipefd[0], &rd);

			curl_multi_fdset(multi_handle, &rd, &wr, &er, &maxfd);
			maxfd = std::max(maxfd, pipefd[0]);

			// Wait for something to happen, or the pipe to unblock us
			// or just timeout after some time, just in case.
			select(maxfd+1, &rd, &wr, &er, &timeout);

			char tmp[1024];
			read(pipefd[0], tmp, sizeof(tmp));

			while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi_handle, &nbLeft));

			// Retrieve events to care about (cleanup of finished reqs)
			int msgs_left = 0;
			CURLMsg *msg;
			while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
				if (msg->msg == CURLMSG_DONE) {
					CURL *h = msg->easy_handle;
					bool okcode = (msg->data.result == CURLE_OK);
					curl_multi_remove_handle(multi_handle, h);
					curl_easy_cleanup(h);

					// Call completion callback
					if (request_set.count(h)) {
						const auto &uq = request_set.at(h);
						if (uq->donecb)
							uq->donecb(okcode);
						request_set.erase(h);
					}
				}
			}
		}
	}
};

#endif

