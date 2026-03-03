/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * HTTP.cpp - HTTP Server Implementation
 *
 * Simple HTTP/1.1 server for model serving.
 */

#include "http.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <vector>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

namespace nevaarize {

// HTTPResponse implementation
void HTTPResponse::setJSON(const std::string& json) {
    body = json;
    headers["Content-Type"] = "application/json";
}

void HTTPResponse::setText(const std::string& text) {
    body = text;
    headers["Content-Type"] = "text/plain";
}

void HTTPResponse::setStatus(int code, const std::string& text) {
    statusCode = code;
    if (!text.empty()) {
        statusText = text;
    } else {
        switch (code) {
            case 200: statusText = "OK"; break;
            case 201: statusText = "Created"; break;
            case 400: statusText = "Bad Request"; break;
            case 404: statusText = "Not Found"; break;
            case 500: statusText = "Internal Server Error"; break;
            default: statusText = "Unknown"; break;
        }
    }
}

std::string HTTPResponse::build() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    
    for (const auto& [key, value] : headers) {
        oss << key << ": " << value << "\r\n";
    }
    
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    
    return oss.str();
}

// HTTPServer implementation
HTTPServer::HTTPServer(int port)
    : port_(port)
    , serverSocket_(-1)
    , running_(false) {}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::get(const std::string& path, HTTPHandler handler) {
    getHandlers_[path] = std::move(handler);
}

void HTTPServer::post(const std::string& path, HTTPHandler handler) {
    postHandlers_[path] = std::move(handler);
}

HTTPRequest HTTPServer::parseRequest(const std::string& raw) {
    HTTPRequest req;
    std::istringstream stream(raw);
    std::string line;

    // Parse request line
    std::getline(stream, line);
    std::istringstream reqLine(line);
    reqLine >> req.method >> req.path;

    // Parse path and query
    auto queryPos = req.path.find('?');
    if (queryPos != std::string::npos) {
        std::string queryStr = req.path.substr(queryPos + 1);
        req.path = req.path.substr(0, queryPos);
        
        // Parse query parameters
        std::istringstream queryStream(queryStr);
        std::string pair;
        while (std::getline(queryStream, pair, '&')) {
            auto eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                req.query[pair.substr(0, eqPos)] = pair.substr(eqPos + 1);
            }
        }
    }

    // Parse headers
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // Trim whitespace
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value = value.substr(1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                value.pop_back();
            }
            req.headers[key] = value;
        }
    }

    // Parse body (simple approach)
    std::ostringstream bodyStream;
    bodyStream << stream.rdbuf();
    req.body = bodyStream.str();
    
    // Trim trailing whitespace
    while (!req.body.empty() && (req.body.back() == '\r' || req.body.back() == '\n')) {
        req.body.pop_back();
    }

    return req;
}

void HTTPServer::handleClient(int clientSocket) {
#ifdef __linux__
    char buffer[8192];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        close(clientSocket);
        return;
    }
    
    buffer[bytesRead] = '\0';
    HTTPRequest req = parseRequest(std::string(buffer));
    
    HTTPResponse res;
    bool handled = false;
    
    if (req.method == "GET") {
        auto it = getHandlers_.find(req.path);
        if (it != getHandlers_.end()) {
            res = it->second(req);
            handled = true;
        }
    } else if (req.method == "POST") {
        auto it = postHandlers_.find(req.path);
        if (it != postHandlers_.end()) {
            res = it->second(req);
            handled = true;
        }
    }
    
    if (!handled) {
        res.setStatus(404);
        res.setJSON("{\"error\": \"Not Found\"}");
    }
    
    std::string response = res.build();
    send(clientSocket, response.c_str(), response.size(), 0);
    close(clientSocket);
#endif
}

void HTTPServer::run() {
#ifdef __linux__
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }
    
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << std::endl;
        close(serverSocket_);
        return;
    }
    
    if (listen(serverSocket_, 128) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        close(serverSocket_);
        return;
    }
    
    std::cout << "Nevaarize HTTP Server running on http://127.0.0.1:" << port_ << std::endl;
    running_ = true;
    
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        
        if (clientSocket >= 0) {
            // Handle in thread for basic concurrency
            std::thread(&HTTPServer::handleClient, this, clientSocket).detach();
        }
    }
    
    close(serverSocket_);
#else
    std::cerr << "HTTP server only supported on Linux" << std::endl;
#endif
}

void HTTPServer::stop() {
    running_ = false;
#ifdef __linux__
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
#endif
}

// JSON utilities
namespace JSON {

std::unordered_map<std::string, std::string> parse(const std::string& json) {
    std::unordered_map<std::string, std::string> result;
    
    // Simple JSON parser (handles flat objects only)
    size_t pos = json.find('{');
    if (pos == std::string::npos) return result;
    
    std::string content = json.substr(pos + 1);
    pos = content.rfind('}');
    if (pos != std::string::npos) {
        content = content.substr(0, pos);
    }
    
    // Parse key-value pairs
    size_t i = 0;
    while (i < content.size()) {
        // Skip whitespace
        while (i < content.size() && (content[i] == ' ' || content[i] == '\n' || content[i] == '\t')) ++i;
        
        // Find key
        if (i >= content.size() || content[i] != '"') break;
        ++i;
        size_t keyStart = i;
        while (i < content.size() && content[i] != '"') ++i;
        std::string key = content.substr(keyStart, i - keyStart);
        ++i;
        
        // Skip colon
        while (i < content.size() && content[i] != ':') ++i;
        ++i;
        while (i < content.size() && content[i] == ' ') ++i;
        
        // Find value
        std::string value;
        if (content[i] == '"') {
            ++i;
            size_t valStart = i;
            while (i < content.size() && content[i] != '"') ++i;
            value = content.substr(valStart, i - valStart);
            ++i;
        } else if (content[i] == '[') {
            // Array - capture as string
            size_t valStart = i;
            int depth = 1;
            ++i;
            while (i < content.size() && depth > 0) {
                if (content[i] == '[') ++depth;
                else if (content[i] == ']') --depth;
                ++i;
            }
            value = content.substr(valStart, i - valStart);
        } else {
            // Number or boolean
            size_t valStart = i;
            while (i < content.size() && content[i] != ',' && content[i] != '}') ++i;
            value = content.substr(valStart, i - valStart);
            // Trim
            while (!value.empty() && (value.back() == ' ' || value.back() == '\n')) {
                value.pop_back();
            }
        }
        
        result[key] = value;
        
        // Skip comma
        while (i < content.size() && content[i] != ',') ++i;
        ++i;
    }
    
    return result;
}

std::string stringify(const std::unordered_map<std::string, std::string>& obj) {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first) oss << ", ";
        first = false;
        oss << "\"" << key << "\": ";
        
        // Check if value looks like a number
        bool isNumber = !value.empty() && (std::isdigit(value[0]) || value[0] == '-');
        if (value == "true" || value == "false" || value == "null" || isNumber) {
            oss << value;
        } else {
            oss << "\"" << value << "\"";
        }
    }
    
    oss << "}";
    return oss.str();
}

std::string object(std::initializer_list<std::pair<std::string, std::string>> pairs) {
    std::unordered_map<std::string, std::string> obj;
    for (const auto& p : pairs) {
        obj[p.first] = p.second;
    }
    return stringify(obj);
}

} // namespace JSON

} // namespace nevaarize
