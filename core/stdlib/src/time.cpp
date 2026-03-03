/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Time.cpp - Nevaarize Time Standard Library Implementation
 *
 * Time and timing functions.
 */

#include "time.hpp"
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace nevaarize {
namespace stdlib {

std::unordered_map<std::string, NativeFunction> getTimeLibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;

    funcs["clock"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::high_resolution_clock::now();
        auto epoch = now.time_since_epoch();
        auto seconds = std::chrono::duration<double>(epoch).count();
        return Value::fromFloat(seconds);
    };

    funcs["sleep"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (!args.empty() && args[0].isNumber()) {
            int ms = static_cast<int>(args[0].asDouble());
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return Value::nil();
    };

    funcs["millis"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return Value::fromInt(ms);
    };

    funcs["nanos"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::steady_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        return Value::fromInt(ns);
    };

    funcs["timestamp"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        return Value::fromInt(epoch);
    };

    funcs["format"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        std::string format = "%Y-%m-%d %H:%M:%S";
        if (!args.empty() && args[0].isString()) {
            format = *args[0].stringVal;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        
        std::ostringstream oss;
        oss << std::put_time(tm, format.c_str());
        return Value::fromString(oss.str());
    };

    funcs["year"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        return Value::fromInt(tm->tm_year + 1900);
    };

    funcs["month"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        return Value::fromInt(tm->tm_mon + 1);
    };

    funcs["day"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        return Value::fromInt(tm->tm_mday);
    };

    funcs["hour"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        return Value::fromInt(tm->tm_hour);
    };

    funcs["minute"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        return Value::fromInt(tm->tm_min);
    };

    funcs["second"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time);
        return Value::fromInt(tm->tm_sec);
    };

    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
