/**
 * HTTP.hpp - Nevaarize HTTP Server
 *
 * High-performance HTTP server for model serving.
 */

#ifndef NEVAARIZE_HTTP_HPP
#define NEVAARIZE_HTTP_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <memory>

namespace nevaarize {

/**
 * HTTP request.
 */
struct HTTPRequest {
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query;
};

/**
 * HTTP response.
 */
struct HTTPResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    void setJSON(const std::string& json);
    void setText(const std::string& text);
    void setStatus(int code, const std::string& text = "");
    std::string build() const;
};

/**
 * HTTP request handler.
 */
using HTTPHandler = std::function<HTTPResponse(const HTTPRequest&)>;

/**
 * Simple HTTP server.
 */
class HTTPServer {
public:
    HTTPServer(int port = 8080);
    ~HTTPServer();

    /**
     * Register a GET handler.
     */
    void get(const std::string& path, HTTPHandler handler);

    /**
     * Register a POST handler.
     */
    void post(const std::string& path, HTTPHandler handler);

    /**
     * Start the server (blocking).
     */
    void run();

    /**
     * Stop the server.
     */
    void stop();

    /**
     * Check if server is running.
     */
    bool isRunning() const { return running_; }

private:
    int port_;
    int serverSocket_;
    bool running_;
    std::unordered_map<std::string, HTTPHandler> getHandlers_;
    std::unordered_map<std::string, HTTPHandler> postHandlers_;

    void handleClient(int clientSocket);
    HTTPRequest parseRequest(const std::string& raw);
};

/**
 * JSON utilities.
 */
namespace JSON {
    /**
     * Parse JSON string to map.
     */
    std::unordered_map<std::string, std::string> parse(const std::string& json);

    /**
     * Stringify map to JSON.
     */
    std::string stringify(const std::unordered_map<std::string, std::string>& obj);

    /**
     * Build JSON object string.
     */
    std::string object(std::initializer_list<std::pair<std::string, std::string>> pairs);
}

} // namespace nevaarize

#endif // NEVAARIZE_HTTP_HPP
