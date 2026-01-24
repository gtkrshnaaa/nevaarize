# Phase 3: HTTP & Networking

## Priority: HIGH
## Target: High-performance HTTP server for model serving

---

## Overview

AI model deployment requires serving predictions via HTTP. This phase adds a native HTTP server optimized for low-latency inference.

---

## Target Usage

```nva
import stdlib http as h

// Define prediction endpoint
func predict(request) {
    input = request.json()["data"]
    result = model.forward(input)
    return h.json({"prediction": result})
}

// Start server
server = h.Server(8080)
server.post("/predict", predict)
server.run()
```

---

## Architecture

```
┌──────────────────────────────────────────────┐
│                 HTTP Server                   │
├──────────────────────────────────────────────┤
│  Async I/O (epoll/kqueue)                    │
├──────────────────────────────────────────────┤
│  Request Parser (zero-copy)                  │
├──────────────────────────────────────────────┤
│  Router (radix tree)                         │
├──────────────────────────────────────────────┤
│  Handler Pool (thread pool)                  │
├──────────────────────────────────────────────┤
│  JSON Parser/Serializer                      │
└──────────────────────────────────────────────┘
```

---

## Implementation Plan

### Files to Create

```
stdlib/include/HTTP.hpp      - HTTP server interface
stdlib/src/HTTP.cpp          - Server implementation
stdlib/include/JSON.hpp      - JSON parser
stdlib/src/JSON.cpp          - JSON implementation
stdlib/include/Socket.hpp    - Low-level networking
stdlib/src/Socket.cpp        - Socket implementation
```

### 3.1 HTTP Parser (Zero-Copy)
```cpp
struct HTTPRequest {
    std::string_view method;    // GET, POST, etc
    std::string_view path;      // /predict
    std::string_view body;      // Raw body (no copy)
    // Headers as string_views pointing to original buffer
};
```

### 3.2 Async I/O
```cpp
// Linux: epoll
// macOS: kqueue
// This enables handling 100K+ connections

class EventLoop {
    void addSocket(int fd, Callback cb);
    void run();  // Non-blocking event loop
};
```

### 3.3 Thread Pool for Handlers
```cpp
class ThreadPool {
    void submit(std::function<void()> task);
    // Lock-free queue for minimal contention
};
```

---

## JSON Support

```nva
// Parse JSON
data = JSON.parse('{"x": [1,2,3]}')
print(data["x"][0])  // 1

// Serialize
output = JSON.stringify({"prediction": 0.95})
```

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Requests/sec | 100,000+ | With keep-alive |
| Latency (p99) | < 1ms | For small payloads |
| Connections | 10,000+ | Concurrent |
| JSON Parse | 500 MB/s | For request bodies |

---

## API Design

```nva
// Server
server = http.Server(port)
server.get(path, handler)
server.post(path, handler)
server.run()

// Request object
request.method      // "GET", "POST"
request.path        // "/predict"
request.json()      // Parsed JSON body
request.headers     // Map of headers

// Response helpers
http.json(data)     // JSON response
http.text(str)      // Plain text
http.status(code)   // Set status code
```

---

## Success Criteria

- [ ] Basic HTTP/1.1 server working
- [ ] GET and POST handlers
- [ ] JSON parse/serialize
- [ ] Thread pool for parallel handlers
- [ ] Benchmark: 50K+ req/sec
- [ ] Model serving demo working
