# mserv

**mserv** is a lightweight high-performance asynchronous HTTP/HTTPS server written in C.

The server is built around Linux native technologies such as **epoll**, non-blocking sockets, and an event-driven architecture. It provides HTTP parsing, TLS support, dynamic API modules, custom memory management utilities, and a modular routing system.

The main goal of mserv is to provide a simple but powerful server core that can be extended dynamically without restarting the server.

---

# Features

## Event-driven architecture

mserv uses:

* Linux `epoll`
* Edge-triggered event handling (`EPOLLET`)
* Non-blocking sockets
* Asynchronous connection processing

The server is designed to efficiently handle many simultaneous connections using a single event loop.

---

## HTTP/HTTPS support

Supported HTTP features:

* HTTP request parsing using **llhttp**
* HTTP methods:

  * GET
  * HEAD
  * POST
  * PUT
  * DELETE
* Keep-Alive connections
* Static file serving
* MIME type detection
* Custom API responses
* HTTP status handling

Supported response content types:

* HTML
* CSS
* JavaScript
* JSON
* XML
* Images
* PDF
* ZIP
* Binary files

---

## TLS support

mserv supports HTTPS using **OpenSSL**.

The server provides:

* TLS handshake handling
* Encrypted client communication
* OpenSSL SSL objects per connection
* Optional Linux Kernel TLS (KTLS) support

For maximum performance, running mserv with a Linux kernel that supports **KTLS** is recommended.

KTLS allows TLS record processing to be partially offloaded into the Linux kernel, reducing userspace overhead during encrypted data transmission.

---

# Dependencies

Required libraries:

* Linux kernel with epoll support
* OpenSSL
* llhttp
* pthread
* dl (dynamic loading support)

Example packages on Arch Linux:

```bash
sudo pacman -S openssl llhttp
```

---

# Kernel requirements

Recommended:

```
Linux Kernel >= 7.x
```

KTLS support is recommended for HTTPS performance improvements.

Check KTLS support:

```bash
cat /proc/sys/net/ipv4/tcp_available_congestion_control
```

or check OpenSSL KTLS availability:

```bash
openssl version -f
```

---

# Architecture

The server consists of several main subsystems:

```
mserv
 |
 +-- event loop (epoll)
 |
 +-- connection manager
 |
 +-- HTTP parser (llhttp)
 |
 +-- HTTP response engine
 |
 +-- TLS layer (OpenSSL)
 |
 +-- route manager
 |
 +-- dynamic module loader
 |
 +-- memory pools
 |
 +-- hashmap
```

---

# Dynamic API Modules

mserv supports dynamically loaded API modules.

A module is a shared object:

```
example.so
```

placed inside the configured modules directory.

The server automatically watches the module directory and can:

* Load new modules
* Remove deleted modules
* Reload modified modules

No server restart is required.

---

# Creating an API

A module can register a new HTTP endpoint dynamically.

Example:

```
GET https://localhost/api/test
```

The module registers:

```c
MSERV_DEFINE_API_MODULE(
    "/api/test",
    api_test_handler
)
```

Handler example:

```c
int api_test_handler(
        const mhttp_msg_t *request,
        mhttp_route_response_t *response,
        mhttp_method_t method)
{
        response->status_code = MHTTP_SUCCESS;

        response->content_type =
            "application/json";

        response->body.bytes =
            "{\"message\":\"hello\"}";

        response->content_len =
            strlen(response->body.bytes);

        return 0;
}
```

After loading the module, the route becomes available:

```
https://server/api/test
```

---

# Dynamic module workflow

Example:

```
modules/

 ├── users.so
 ├── auth.so
 └── statistics.so
```

When `users.so` appears:

```
filesystem event
        |
        v
module watcher
        |
        v
dlopen(users.so)
        |
        v
register routes
        |
        v
/api/users becomes available
```

When the file is removed:

```
unlink(users.so)

        |
        v

module unload

        |
        v

routes removed
```

When the module changes:

```
users.so modified

        |
        v

reload module

        |
        v

update API handlers
```

---

# Internal components

## Hash map

Custom hashmap implementation:

* Route lookup
* Dynamic API registration
* Fast URL resolving

---

## Object pools

mserv contains custom object pools for:

* HTTP requests
* HTTP responses
* Modules

This reduces allocation overhead during heavy traffic.

---

## Logging system

Built-in logging:

```c
mserv_log()
mserv_warn()
mserv_err()
```

Example output:

```
[LOG] [1720000000] Client connected
[WARN] connection.c:120 SSL error
[ERR] parser.c:54 Invalid request
```

---

# Building

Example:

```bash
git clone https://github.com/example/mserv

cd mserv

make
```

---

# Configuration

Server configuration includes:

* Listening port
* Maximum connections
* TLS certificate path
* Static file directories
* Module directory
* Main HTML page

---

# Example project structure

```
project/

├── mserv
├── config/
│
├── modules/
│   ├── api.so
│   └── auth.so
│
├── html/
│   ├── index.html
│   └── style.css
│
└── certificates/
    ├── server.crt
    └── server.key
```

---
