# Standard Library: `http`

The `http` module provides server-side HTTP socket listening, route dispatching, and client HTTP request execution.

```nva
import stdlib http as http
```

## Function Reference

- `http.Server()`: Instantiates and initializes an HTTP server instance handle.
- `http.Route(serverHandle, method, path, handlerFunc)`: Registers endpoint route matching specified HTTP method (`"GET"`, `"POST"`, `"PUT"`, `"DELETE"`) and URL path.
- `http.Listen(serverHandle, port)`: Binds server socket to specified TCP port and starts processing incoming client connections.
- `http.Get(url)`: Executes an HTTP GET request to specified remote URL and returns response map containing status code and response body string.
- `http.Post(url, payloadJson)`: Sends an HTTP POST request carrying JSON payload and returns response map.

### Web Server Example

```nva
import stdlib http as http

server = http.Server()

func handleRoot() {
    return "Nevaarize Microservice Running"
}

http.Route(server, "GET", "/", handleRoot)
print("Listening on http://localhost:8080")
http.Listen(server, 8080)
```
