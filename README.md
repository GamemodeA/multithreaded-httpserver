# multithreaded-httpserver
A server which can recieve file I/O requests from multiple clients. The requests are handled in a thread-pool in which a dispatcher thread queues requests onto a list for the worker threads to execute. Each command is executed in the order it's recieved while still running efficiently. A server-side audit log is also included.

This project is still in development and will be updated biweekly.
