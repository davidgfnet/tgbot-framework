
TelegramBot C++ Framework
=========================

Here lives a bunch of helper files, framework-like classes and all sorts of
helpers and utilities used by some Telegram bots created in C++

Functionality
-------------

 - userdata.h: Helper class that can be used to hold user data, such as last
   user query and some preferences. It is implemented to be thread safe and
   very fast.
 - logger.h: Implements a logging facility that allows user to log data with
   a timestamp to disk in a safe manner. The class is thread safe and should
   be non-blocking most of the time (has a buffer and a flusher thread).
   It will rotate files daily by default.
 - executor.h: Helper class that can execute programs in the background and
   report back via callback. It takes care of process reaping and ensuring
   a maximum number of processes are running concurrently.
 - httpclient.h: Implementation of HTTP/S client on top of libcurl using the
   mutli interface (so only one thread per class is used).


Utility/Helper classes
----------------------

 - cqueue.h: Contains a thread safe queue that can be used in a worker-manager
   paradigm. Typically used for a main thread to push queries to worker threads
   so the server can be parallelized.
 - lrucache.h: Class that implements an LRU cache, very useful to keep data in
   memory for an efficient lookup and defer its flushing to evictions.
 - util.h: Misc functions around strings.

