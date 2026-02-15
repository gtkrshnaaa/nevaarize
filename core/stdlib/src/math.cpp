/**
 * Math.cpp - Nevaarize Math Standard Library Implementation
 *
 * All mathematical functions.
 */

#include "math.hpp"
#include <cmath>
#include <random>
#include <limits>

namespace nevaarize {
namespace stdlib {

std::unordered_map<std::string, NativeFunction> getMathLibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;

    funcs["Abs"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        double val = args[0].asDouble();
        return Value::fromFloat(std::abs(val));
    };

    funcs["Sqrt"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        double val = args[0].asDouble();
        if (val < 0) return Value::fromFloat(std::numeric_limits<double>::quiet_NaN());
        return Value::fromFloat(std::sqrt(val));
    };

    funcs["Pow"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        double base = args[0].asDouble();
        double exp = args[1].asDouble();
        return Value::fromFloat(std::pow(base, exp));
    };

    funcs["Floor"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::floor(args[0].asDouble()));
    };

    funcs["Ceil"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::ceil(args[0].asDouble()));
    };

    funcs["Round"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::round(args[0].asDouble()));
    };

    funcs["Sin"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::sin(args[0].asDouble()));
    };

    funcs["Cos"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::cos(args[0].asDouble()));
    };

    funcs["Tan"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::tan(args[0].asDouble()));
    };

    funcs["Asin"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::asin(args[0].asDouble()));
    };

    funcs["Acos"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::acos(args[0].asDouble()));
    };

    funcs["Atan"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::atan(args[0].asDouble()));
    };

    funcs["Atan2"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        return Value::fromFloat(std::atan2(args[0].asDouble(), args[1].asDouble()));
    };

    funcs["Log"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::log(args[0].asDouble()));
    };

    funcs["Log10"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::log10(args[0].asDouble()));
    };

    funcs["Exp"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
        return Value::fromFloat(std::exp(args[0].asDouble()));
    };

    funcs["Min"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        return Value::fromFloat(std::min(args[0].asDouble(), args[1].asDouble()));
    };

    funcs["Max"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        return Value::fromFloat(std::max(args[0].asDouble(), args[1].asDouble()));
    };

    funcs["Random"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<double> dis(0.0, 1.0);
        return Value::fromFloat(dis(gen));
    };

    funcs["RandomInt"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        int64_t minVal = 0;
        int64_t maxVal = 100;
        
        if (args.size() >= 2) {
            minVal = static_cast<int64_t>(args[0].asDouble());
            maxVal = static_cast<int64_t>(args[1].asDouble());
        } else if (args.size() == 1) {
            maxVal = static_cast<int64_t>(args[0].asDouble());
        }
        
        std::uniform_int_distribution<int64_t> dis(minVal, maxVal);
        return Value::fromInt(dis(gen));
    };

    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
