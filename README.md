# README

## Table of Contents

- [Getting Started](#getting-started)
    - [Requirements](#requirements)
    - [Usage](#usage)
- [Project Structure](#project-structure)
    - [Overview](#overview)
    - [Server](#server)
    - [Connection](#connection)
    - [Blacklist](#blacklist)
    - [Logger](#logger)
- [Key Design Aspects](#key-design-aspects)
- [Process Flow](#process-flow)
- [Comparison Between HTTP Versions](#comparison-between-http-versions)

---

## Getting Started
### Requirements
The HTTPS proxy has been tested to compile and run on both XCNE 1 and 2, however listed below are the requirements should you want to run the proxy on another machine.
- Ubuntu (Recommended: LTS 20.04)
- GCC version 9.3.0 or later (Default version shipped with Ubuntu 20.04)
- Boost C++ Libraries (Available at: https://www.boost.org)

### Usage
1. Download the archive.
1. Uncompress the archive using `$ tar -zxvf <archive_name>`
1. The above command should setup the files in the following structure:
    ```
    proxy/
    ├- README.md
    ├- makefile
    ├- images/
    │   └- ...
    └- src/
        ├- main.cpp
        ├- server.hpp
        ├- server.cpp
        ├- ...
        └- logger/
           └- ...
    ```
1. To compile the proxy, you can use the makefile provided in the `proxy` folder: `$ make`
    - If you need to clean up the object files and executable, you can use: `$ make clean`
1. Start the proxy using `./proxy PORT [TELEMETRY_FLAG [PATH_TO_BLACKLIST]]`

---

## Project Structure

### Overview
The HTTPS proxy was implemented in C++ and comprises the following classes:
- [Server](#server)
- [Connection](#connection)
- [Context](#context)
- [Blacklist](#blacklist)
- [Logger](#logger)

Each class is defined in its correspondingly named header file, `class_name.hpp`, and is implemented in the correspondingly named source code file, `class_name.cpp`.

In addition to the aforementioned classes, the `main.cpp` file implements the entry point to the proxy application.

### `Server`
The `Server` class represents an instance of the HTTPS proxy bound to a particular port on the host machine. \
It contains the welcoming TCP socket used for accepting connections from clients as well as a group of threads which are used to serve the proxied connections. \
This class exposes the following methods:
- `Server::create`: Factory method for instantiation.
- `Server::listen`: Start listening for and accepting connections.

### `Connection`
The `Connection` class represents a proxied connection between a client and a web server. \
It contains the two TCP sockets, relevant connection metadata, buffers, and telemetry data.
This class exposes the following methods:
- `Connection::create`: Factory method for instantiation.
- `Connection::handle_connection`: Establishes connection to the server and relays data between endpoints.
- `Connection::shared_ptr`: Returns a shared pointer to the `Connection` instance.

### `Context`
The `Context` structure contains the relevant objects that form the context of the proxy. \
Each `Context` instance includes the following:
- Two `io_context` objects, one for the welcome socket and the other for client/server sockets.
- The hostname `resolver` object and a `mutex` to control access to the resolver.
- The logging facility.
- A `Boolean` flag denoting if telemetry is enabled for the proxy.
- A `Blacklist` object containing the hostnames and substrings that have been blacklisted.

### `Blacklist`
The `Blacklist` class represents a list of hostnames or strings that are blacklisted by the proxy.
This class exposes the following methods:
- `Blacklist::add_entries`: Replaces existing entries with the provided entries.
- `Blacklist::add_entry`: Adds a single entry to the blacklist.
- `Blacklist::is_blocked`: Validates if whole or part of a given hostname matches any entries on the blacklist.

### `Logger`
The `Logger` class is a general purpose thread-safe basic logging facility used for debugging and collecting logs.

---

## Key Design Aspects

The HTTPS proxy was implemented using C++ and heavily relies on the Boost C++ libraries. 
In particular, the asynchronous IO library allows the proxy to maintain and wait on multiple persistent connections while simultaneously serving other active connections. 
This is achieved through the use of a single IO context object which all sockets are bound to. 
This causes the callback function of any asynchronous operation to be executed on any available thread for which the IO context object has been bound to, in this case any of the 8 child threads or parent thread. 
By implementing all reads from the client or server to be asynchronous reads with the callback managing the writing of data to the other side of the connection, and recording telemetry data, we are able to utilize the 8 threads much more efficiently.
We are also able to handle idle persistent connections properly without preventing other connections from being accepted or served.

Due to the relatively unpredictable nature of asynchronous programming, each connection between a client and server is guarded by a unique mutex which prevents any possible race conditions on reading from or writing to the sockets.

Apart from implementing the read operations between the client and server as asynchronous operations, the process of accepting new connections on the welcome socket was also implemented as an asynchronous operation. 
This conversion to use asynchronous operations added ability to capture the interrupt signal and enable graceful shutdown of the proxy.

---

## Process Flow

1. The `main` function serves as the entry point of the whole application.
   The application first initializes the default global context, and updates it with the relevant command line arguments such as telemetry and blacklist data.
1. An instance of the `Server` class is then created with the specified port number, which creates a TCP welcome socket bound to the port number.
1. Thereafter, the application calls the `Server::listen` method which creates 8 child threads and begins listening on the welcoming socket for connection requests.
1. When a client initiates a TCP connection with the proxy, the application accepts the connection and executes the `Server::handle_accept` callback on the parent thread. 
   This callback reads the client socket until a valid HTTP message has been received or an error occurs.
1. Once a valid HTTP message has been received, the message is passed to the constructor of the `Connection` class for validation and parsing. This process validates the syntax of the HTTP request message, HTTP method, HTTP version, hostname and port information. In addition, the hostname of the server is also checked against the blacklist and the proxy request is rejected if a match is found.
   - If the received message cannot be parsed or handled by the proxy, the proxy sends an error message to the client and closes the connection to the client.
1. After the `Connection` object for the received client request has been created, the proxy calls the `Connection::handle_connection` method which performs hostname resolution and attempts to establish a TCP connection to the server.
   - If hostname resolution fails or a connection cannot be established to the server, an error message is sent to the client and the connection is closed.
1. After a TCP connection has been established to the server, a `200 Connection established` message is sent to the client and the application asynchronously reads from both the client and server sockets for data. At this point, the application also starts a timer which tracks the time for which this connection is open.
   - At this point, the `Server::handle_accept` callback terminates and welcome socket is free to accept new connections.
1. When data is received from either of the sockets in a `Connection` object, the `Connection::handle_read` method is called as a callback. This writes the data received into the destination socket and records the amount of bytes transferred if necessary.
1. When either side of a `Connection` closes, either exceptionally or otherwise, the timer for the corresponding `Connection` is stopped and the socket for the other end of the connection is closed. This process also terminates the recursive asynchronous listen loop, allowing the `Connection` object to be destroyed.
1. During the destruction of the `Connection` object, the telemetry data is printed out if necessary.

---

## Comparison Between HTTP Versions

### Difference between HTTPS/1.0 and HTTPS/1.1

Based on collected telemetry data, we observed that when loading the same website, more  HTTP/1.0 connections than HTTP/1.1 connections are established. However, each HTTP/1.0 connection was generally shorter-lived and transmitted less data than HTTP/1.1 connections.

This behaviour is most consistent when loading relatively static websites such as www.comp.nus.edu.sg. During our testing, we found that when HTTP/1.0 was used, 75 connections were established when loading the page, the average size of data transmitted by each connection was 35786 bytes, and the median duration of the connections were 0.0960 seconds. In contrast when HTTP/1.1 was used, only 34 connections were established when loading the page, each with an average transmission size of 78667 bytes and median duration of 115.97 seconds.

This observed behaviour can be explained by the default use of persistent connections in HTTP/1.1. Persistent connections allow multiple HTTP requests and their responses to be served over a single TCP connection to the web server. In contrast, HTTP/1.0 does not default to using persistent connections, and each request is served using a separate TCP connection to the web server. (However, HTTP/1.0 does allow for persistent connections if the `Connection: Keep-alive` header is present in the request and both endpoints support it.) Since in HTTP/1.1 each TCP connection is used to serve multiple HTTP requests, the duration of the connections are significantly longer and more bytes are transferred over a single connection in response to the multiple HTTP requests that are served.

| Title | Screenshot |
| --- | --- |
| Screenshot of Telemetry for HTTP 1.0 | ![HTTP 1.0 Screen Capture](/images/HTTP-1.0.png)|
| Screenshot of Telemetry for HTTP 1.1 | ![HTTP 1.1 Screen Capture](/images/HTTP-1.1.png)|