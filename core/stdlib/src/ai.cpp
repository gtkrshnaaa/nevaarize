/**
 * AI.cpp - Nevaarize AI Engineering Standard Library Implementation
 *
 * Comprehensive AI/ML primitives with SIMD acceleration.
 */

#include "ai.hpp"
#include "jit.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <filesystem>
#include <iostream>
#include <immintrin.h>

namespace fs = std::filesystem;

namespace nevaarize {
namespace stdlib {

namespace ai_internal {

std::vector<float> toFloatVector(const Value& val) {
    std::vector<float> result;
    if (val.isArray() && val.arrayVal) {
        const auto& arr = *val.arrayVal;
        result.reserve(arr.size());
        for (const auto& v : arr) {
            result.push_back(static_cast<float>(v.asDouble()));
        }
    }
    return result;
}

Value fromFloatVector(const std::vector<float>& vec) {
    std::vector<Value> arr;
    arr.reserve(vec.size());
    for (float f : vec) {
        arr.push_back(Value::fromFloat(static_cast<double>(f)));
    }
    return Value::fromArray(std::move(arr));
}

float simdDotProduct(const float* a, const float* b, size_t n) {
    float sum = 0.0f;
    size_t i = 0;
    
#ifdef __AVX2__
    __m256 acc = _mm256_setzero_ps();
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        acc = _mm256_fmadd_ps(va, vb, acc);
    }
    float temp[8];
    _mm256_storeu_ps(temp, acc);
    sum = temp[0] + temp[1] + temp[2] + temp[3] + 
          temp[4] + temp[5] + temp[6] + temp[7];
#endif
    
    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

void simdAdd(float* dst, const float* a, const float* b, size_t n) {
    size_t i = 0;
    
#ifdef __AVX2__
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(dst + i, vc);
    }
#endif
    
    for (; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
}

void simdMul(float* dst, const float* a, const float* b, size_t n) {
    size_t i = 0;
    
#ifdef __AVX2__
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_mul_ps(va, vb);
        _mm256_storeu_ps(dst + i, vc);
    }
#endif
    
    for (; i < n; ++i) {
        dst[i] = a[i] * b[i];
    }
}

float simdSum(const float* data, size_t n) {
    float sum = 0.0f;
    size_t i = 0;
    
#ifdef __AVX2__
    __m256 acc = _mm256_setzero_ps();
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(data + i);
        acc = _mm256_add_ps(acc, v);
    }
    float temp[8];
    _mm256_storeu_ps(temp, acc);
    sum = temp[0] + temp[1] + temp[2] + temp[3] + 
          temp[4] + temp[5] + temp[6] + temp[7];
#endif
    
    for (; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

void simdMatMul(float* C, const float* A, const float* B, 
                size_t m, size_t k, size_t n) {
    constexpr size_t BLOCK = 64;
    
    std::fill(C, C + m * n, 0.0f);
    
    for (size_t ii = 0; ii < m; ii += BLOCK) {
        for (size_t jj = 0; jj < n; jj += BLOCK) {
            for (size_t kk = 0; kk < k; kk += BLOCK) {
                size_t iEnd = std::min(ii + BLOCK, m);
                size_t jEnd = std::min(jj + BLOCK, n);
                size_t kEnd = std::min(kk + BLOCK, k);
                
                for (size_t i = ii; i < iEnd; ++i) {
                    for (size_t kIdx = kk; kIdx < kEnd; ++kIdx) {
                        float a_ik = A[i * k + kIdx];
#ifdef __AVX2__
                        __m256 va = _mm256_set1_ps(a_ik);
                        size_t j = jj;
                        for (; j + 8 <= jEnd; j += 8) {
                            __m256 vb = _mm256_loadu_ps(&B[kIdx * n + j]);
                            __m256 vc = _mm256_loadu_ps(&C[i * n + j]);
                            vc = _mm256_fmadd_ps(va, vb, vc);
                            _mm256_storeu_ps(&C[i * n + j], vc);
                        }
                        for (; j < jEnd; ++j) {
                            C[i * n + j] += a_ik * B[kIdx * n + j];
                        }
#else
                        for (size_t j = jj; j < jEnd; ++j) {
                            C[i * n + j] += a_ik * B[kIdx * n + j];
                        }
#endif
                    }
                }
            }
        }
    }
}

void simdReLU(float* dst, const float* src, size_t n) {
    size_t i = 0;
    
#ifdef __AVX2__
    __m256 zero = _mm256_setzero_ps();
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m256 r = _mm256_max_ps(v, zero);
        _mm256_storeu_ps(dst + i, r);
    }
#endif
    
    for (; i < n; ++i) {
        dst[i] = std::max(0.0f, src[i]);
    }
}

void simdSigmoid(float* dst, const float* src, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = 1.0f / (1.0f + std::exp(-src[i]));
    }
}

void stableSoftmax(float* dst, const float* src, size_t n) {
    if (n == 0) return;
    
    float maxVal = *std::max_element(src, src + n);
    float sum = 0.0f;
    
    for (size_t i = 0; i < n; ++i) {
        dst[i] = std::exp(src[i] - maxVal);
        sum += dst[i];
    }
    
    for (size_t i = 0; i < n; ++i) {
        dst[i] /= sum;
    }
}

} // namespace ai_internal

std::unordered_map<std::string, NativeFunction> getAILibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;
    
    // ========================================
    // TENSOR CREATION
    // ========================================
    
    funcs["Zeros"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromArray({});
        size_t n = static_cast<size_t>(args[0].asDouble());
        std::vector<Value> arr(n, Value::fromFloat(0.0));
        return Value::fromArray(std::move(arr));
    };
    
    funcs["Ones"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromArray({});
        size_t n = static_cast<size_t>(args[0].asDouble());
        std::vector<Value> arr(n, Value::fromFloat(1.0));
        return Value::fromArray(std::move(arr));
    };
    
    funcs["RandN"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromArray({});
        size_t n = static_cast<size_t>(args[0].asDouble());
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::normal_distribution<float> dis(0.0f, 1.0f);
        
        std::vector<Value> arr;
        arr.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            arr.push_back(Value::fromFloat(dis(gen)));
        }
        return Value::fromArray(std::move(arr));
    };
    
    funcs["RandU"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        size_t n = 1;
        float minVal = 0.0f, maxVal = 1.0f;
        
        if (args.size() >= 1) n = static_cast<size_t>(args[0].asDouble());
        if (args.size() >= 2) minVal = static_cast<float>(args[1].asDouble());
        if (args.size() >= 3) maxVal = static_cast<float>(args[2].asDouble());
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(minVal, maxVal);
        
        std::vector<Value> arr;
        arr.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            arr.push_back(Value::fromFloat(dis(gen)));
        }
        return Value::fromArray(std::move(arr));
    };
    
    // ========================================
    // ELEMENT-WISE OPERATIONS
    // ========================================
    
    funcs["Add"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        auto b = ai_internal::toFloatVector(args[1]);
        if (a.size() != b.size()) return Value::nil();
        
        std::vector<float> result(a.size());
        ai_internal::simdAdd(result.data(), a.data(), b.data(), a.size());
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["Sub"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        auto b = ai_internal::toFloatVector(args[1]);
        if (a.size() != b.size()) return Value::nil();
        
        std::vector<float> result(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            result[i] = a[i] - b[i];
        }
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["Mul"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        auto b = ai_internal::toFloatVector(args[1]);
        if (a.size() != b.size()) return Value::nil();
        
        std::vector<float> result(a.size());
        ai_internal::simdMul(result.data(), a.data(), b.data(), a.size());
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["Div"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        auto b = ai_internal::toFloatVector(args[1]);
        if (a.size() != b.size()) return Value::nil();
        
        std::vector<float> result(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            result[i] = (b[i] != 0.0f) ? a[i] / b[i] : 0.0f;
        }
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["Scale"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        float scalar = static_cast<float>(args[1].asDouble());
        
        for (auto& v : a) v *= scalar;
        return ai_internal::fromFloatVector(a);
    };
    
    // ========================================
    // REDUCTION OPERATIONS
    // ========================================
    
    funcs["Sum"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromFloat(0.0);
        auto a = ai_internal::toFloatVector(args[0]);
        return Value::fromFloat(ai_internal::simdSum(a.data(), a.size()));
    };
    
    funcs["Mean"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromFloat(0.0);
        auto a = ai_internal::toFloatVector(args[0]);
        if (a.empty()) return Value::fromFloat(0.0);
        return Value::fromFloat(ai_internal::simdSum(a.data(), a.size()) / a.size());
    };
    
    funcs["Max"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        if (a.empty()) return Value::nil();
        return Value::fromFloat(*std::max_element(a.begin(), a.end()));
    };
    
    funcs["Min"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto a = ai_internal::toFloatVector(args[0]);
        if (a.empty()) return Value::nil();
        return Value::fromFloat(*std::min_element(a.begin(), a.end()));
    };
    
    funcs["Argmax"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromInt(0);
        auto a = ai_internal::toFloatVector(args[0]);
        if (a.empty()) return Value::fromInt(0);
        auto it = std::max_element(a.begin(), a.end());
        return Value::fromInt(static_cast<int64_t>(it - a.begin()));
    };
    
    funcs["Argmin"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromInt(0);
        auto a = ai_internal::toFloatVector(args[0]);
        if (a.empty()) return Value::fromInt(0);
        auto it = std::min_element(a.begin(), a.end());
        return Value::fromInt(static_cast<int64_t>(it - a.begin()));
    };
    
    // ========================================
    // MATRIX/VECTOR OPERATIONS
    // ========================================
    
    funcs["Dot"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        auto a = ai_internal::toFloatVector(args[0]);
        auto b = ai_internal::toFloatVector(args[1]);
        if (a.size() != b.size()) return Value::fromFloat(0.0);
        return Value::fromFloat(ai_internal::simdDotProduct(a.data(), b.data(), a.size()));
    };
    
    funcs["MatMul"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 5) return Value::nil();
        
        auto A = ai_internal::toFloatVector(args[0]);
        auto B = ai_internal::toFloatVector(args[1]);
        size_t m = static_cast<size_t>(args[2].asDouble());
        size_t k = static_cast<size_t>(args[3].asDouble());
        size_t n = static_cast<size_t>(args[4].asDouble());
        
        if (A.size() != m * k || B.size() != k * n) return Value::nil();
        
        std::vector<float> C(m * n);
        ai_internal::simdMatMul(C.data(), A.data(), B.data(), m, k, n);
        return ai_internal::fromFloatVector(C);
    };
    
    funcs["Transpose"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 3) return Value::nil();
        
        auto M = ai_internal::toFloatVector(args[0]);
        size_t rows = static_cast<size_t>(args[1].asDouble());
        size_t cols = static_cast<size_t>(args[2].asDouble());
        
        if (M.size() != rows * cols) return Value::nil();
        
        std::vector<float> T(cols * rows);
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                T[j * rows + i] = M[i * cols + j];
            }
        }
        return ai_internal::fromFloatVector(T);
    };
    
    // ========================================
    // ACTIVATION FUNCTIONS
    // ========================================
    
    funcs["ReLU"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        std::vector<float> result(x.size());
        ai_internal::simdReLU(result.data(), x.data(), x.size());
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["LeakyReLU"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        float alpha = (args.size() > 1) ? static_cast<float>(args[1].asDouble()) : 0.01f;
        
        for (auto& v : x) {
            v = (v > 0) ? v : alpha * v;
        }
        return ai_internal::fromFloatVector(x);
    };
    
    funcs["Sigmoid"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        std::vector<float> result(x.size());
        ai_internal::simdSigmoid(result.data(), x.data(), x.size());
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["Tanh"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        for (auto& v : x) v = std::tanh(v);
        return ai_internal::fromFloatVector(x);
    };
    
    funcs["Softmax"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        std::vector<float> result(x.size());
        ai_internal::stableSoftmax(result.data(), x.data(), x.size());
        return ai_internal::fromFloatVector(result);
    };
    
    funcs["GELU"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        constexpr float sqrt2_pi = 0.7978845608f;
        constexpr float coef = 0.044715f;
        
        for (auto& v : x) {
            float x3 = v * v * v;
            v = 0.5f * v * (1.0f + std::tanh(sqrt2_pi * (v + coef * x3)));
        }
        return ai_internal::fromFloatVector(x);
    };
    
    funcs["SiLU"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        for (auto& v : x) {
            v = v / (1.0f + std::exp(-v));
        }
        return ai_internal::fromFloatVector(x);
    };
    
    // ========================================
    // LOSS FUNCTIONS
    // ========================================
    
    funcs["MSELoss"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        auto pred = ai_internal::toFloatVector(args[0]);
        auto target = ai_internal::toFloatVector(args[1]);
        if (pred.size() != target.size()) return Value::fromFloat(0.0);
        
        float sum = 0.0f;
        for (size_t i = 0; i < pred.size(); ++i) {
            float diff = pred[i] - target[i];
            sum += diff * diff;
        }
        return Value::fromFloat(sum / pred.size());
    };
    
    funcs["L1Loss"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        auto pred = ai_internal::toFloatVector(args[0]);
        auto target = ai_internal::toFloatVector(args[1]);
        if (pred.size() != target.size()) return Value::fromFloat(0.0);
        
        float sum = 0.0f;
        for (size_t i = 0; i < pred.size(); ++i) {
            sum += std::abs(pred[i] - target[i]);
        }
        return Value::fromFloat(sum / pred.size());
    };
    
    funcs["BCELoss"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        auto pred = ai_internal::toFloatVector(args[0]);
        auto target = ai_internal::toFloatVector(args[1]);
        if (pred.size() != target.size()) return Value::fromFloat(0.0);
        
        float sum = 0.0f;
        constexpr float eps = 1e-7f;
        for (size_t i = 0; i < pred.size(); ++i) {
            float p = std::clamp(pred[i], eps, 1.0f - eps);
            sum -= target[i] * std::log(p) + (1.0f - target[i]) * std::log(1.0f - p);
        }
        return Value::fromFloat(sum / pred.size());
    };
    
    funcs["CrossEntropyLoss"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        auto pred = ai_internal::toFloatVector(args[0]);
        size_t targetIdx = static_cast<size_t>(args[1].asDouble());
        
        if (targetIdx >= pred.size()) return Value::fromFloat(0.0);
        
        std::vector<float> softmaxed(pred.size());
        ai_internal::stableSoftmax(softmaxed.data(), pred.data(), pred.size());
        
        constexpr float eps = 1e-7f;
        return Value::fromFloat(-std::log(std::max(softmaxed[targetIdx], eps)));
    };
    
    funcs["HuberLoss"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromFloat(0.0);
        auto pred = ai_internal::toFloatVector(args[0]);
        auto target = ai_internal::toFloatVector(args[1]);
        float delta = (args.size() > 2) ? static_cast<float>(args[2].asDouble()) : 1.0f;
        
        if (pred.size() != target.size()) return Value::fromFloat(0.0);
        
        float sum = 0.0f;
        for (size_t i = 0; i < pred.size(); ++i) {
            float diff = std::abs(pred[i] - target[i]);
            if (diff <= delta) {
                sum += 0.5f * diff * diff;
            } else {
                sum += delta * (diff - 0.5f * delta);
            }
        }
        return Value::fromFloat(sum / pred.size());
    };
    
    // ========================================
    // NEURAL NETWORK LAYERS
    // ========================================
    
    funcs["Linear"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 4) return Value::nil();
        
        auto input = ai_internal::toFloatVector(args[0]);
        auto weights = ai_internal::toFloatVector(args[1]);
        auto bias = ai_internal::toFloatVector(args[2]);
        size_t outFeatures = static_cast<size_t>(args[3].asDouble());
        
        size_t inFeatures = input.size();
        if (weights.size() != inFeatures * outFeatures) return Value::nil();
        if (bias.size() != outFeatures) return Value::nil();
        
        std::vector<float> output(outFeatures);
        for (size_t o = 0; o < outFeatures; ++o) {
            output[o] = bias[o];
            for (size_t i = 0; i < inFeatures; ++i) {
                output[o] += input[i] * weights[i * outFeatures + o];
            }
        }
        return ai_internal::fromFloatVector(output);
    };
    
    funcs["Dropout"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        auto x = ai_internal::toFloatVector(args[0]);
        float p = (args.size() > 1) ? static_cast<float>(args[1].asDouble()) : 0.5f;
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::bernoulli_distribution drop(p);
        
        float scale = 1.0f / (1.0f - p);
        for (auto& v : x) {
            v = drop(gen) ? 0.0f : v * scale;
        }
        return ai_internal::fromFloatVector(x);
    };
    
    funcs["LayerNorm"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 3) return Value::nil();
        
        auto x = ai_internal::toFloatVector(args[0]);
        auto gamma = ai_internal::toFloatVector(args[1]);
        auto beta = ai_internal::toFloatVector(args[2]);
        float eps = (args.size() > 3) ? static_cast<float>(args[3].asDouble()) : 1e-5f;
        
        if (x.empty() || x.size() != gamma.size() || x.size() != beta.size()) {
            return Value::nil();
        }
        
        float mean = ai_internal::simdSum(x.data(), x.size()) / x.size();
        float var = 0.0f;
        for (float v : x) {
            float diff = v - mean;
            var += diff * diff;
        }
        var /= x.size();
        
        float invStd = 1.0f / std::sqrt(var + eps);
        std::vector<float> result(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            result[i] = gamma[i] * (x[i] - mean) * invStd + beta[i];
        }
        return ai_internal::fromFloatVector(result);
    };
    
    // ========================================
    // OPTIMIZER UPDATES
    // ========================================
    
    funcs["SGDUpdate"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 3) return Value::nil();
        
        auto weight = ai_internal::toFloatVector(args[0]);
        auto grad = ai_internal::toFloatVector(args[1]);
        float lr = static_cast<float>(args[2].asDouble());
        
        if (weight.size() != grad.size()) return Value::nil();
        
        for (size_t i = 0; i < weight.size(); ++i) {
            weight[i] -= lr * grad[i];
        }
        return ai_internal::fromFloatVector(weight);
    };
    
    funcs["AdamUpdate"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 7) return Value::nil();
        
        auto weight = ai_internal::toFloatVector(args[0]);
        auto grad = ai_internal::toFloatVector(args[1]);
        auto m = ai_internal::toFloatVector(args[2]);
        auto v = ai_internal::toFloatVector(args[3]);
        float lr = static_cast<float>(args[4].asDouble());
        float beta1 = static_cast<float>(args[5].asDouble());
        float beta2 = static_cast<float>(args[6].asDouble());
        int64_t t = (args.size() > 7) ? static_cast<int64_t>(args[7].asDouble()) : 1;
        float eps = (args.size() > 8) ? static_cast<float>(args[8].asDouble()) : 1e-8f;
        
        size_t n = weight.size();
        if (n != grad.size() || n != m.size() || n != v.size()) return Value::nil();
        
        float beta1_t = std::pow(beta1, static_cast<float>(t));
        float beta2_t = std::pow(beta2, static_cast<float>(t));
        
        for (size_t i = 0; i < n; ++i) {
            m[i] = beta1 * m[i] + (1.0f - beta1) * grad[i];
            v[i] = beta2 * v[i] + (1.0f - beta2) * grad[i] * grad[i];
            
            float m_hat = m[i] / (1.0f - beta1_t);
            float v_hat = v[i] / (1.0f - beta2_t);
            
            weight[i] -= lr * m_hat / (std::sqrt(v_hat) + eps);
        }
        
        std::vector<Value> result;
        result.push_back(ai_internal::fromFloatVector(weight));
        result.push_back(ai_internal::fromFloatVector(m));
        result.push_back(ai_internal::fromFloatVector(v));
        return Value::fromArray(std::move(result));
    };
    
    funcs["ClipGradNorm"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        
        auto grad = ai_internal::toFloatVector(args[0]);
        float maxNorm = static_cast<float>(args[1].asDouble());
        
        float norm = std::sqrt(ai_internal::simdDotProduct(grad.data(), grad.data(), grad.size()));
        
        if (norm > maxNorm) {
            float scale = maxNorm / norm;
            for (auto& g : grad) g *= scale;
        }
        return ai_internal::fromFloatVector(grad);
    };
    
    // ========================================
    // INITIALIZATION
    // ========================================
    
    funcs["XavierInit"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromArray({});
        
        size_t fanIn = static_cast<size_t>(args[0].asDouble());
        size_t fanOut = static_cast<size_t>(args[1].asDouble());
        
        float std = std::sqrt(2.0f / (fanIn + fanOut));
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::normal_distribution<float> dis(0.0f, std);
        
        std::vector<Value> arr;
        arr.reserve(fanIn * fanOut);
        for (size_t i = 0; i < fanIn * fanOut; ++i) {
            arr.push_back(Value::fromFloat(dis(gen)));
        }
        return Value::fromArray(std::move(arr));
    };
    
    funcs["HeInit"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromArray({});
        
        size_t fanIn = static_cast<size_t>(args[0].asDouble());
        size_t fanOut = static_cast<size_t>(args[1].asDouble());
        
        float std = std::sqrt(2.0f / fanIn);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::normal_distribution<float> dis(0.0f, std);
        
        std::vector<Value> arr;
        arr.reserve(fanIn * fanOut);
        for (size_t i = 0; i < fanIn * fanOut; ++i) {
            arr.push_back(Value::fromFloat(dis(gen)));
        }
        return Value::fromArray(std::move(arr));
    };
    
    // ========================================
    // UTILITY
    // ========================================
    
    funcs["Shape"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isArray() || !args[0].arrayVal) return Value::fromInt(0);
        return Value::fromInt(static_cast<int64_t>(args[0].arrayVal->size()));
    };
    
    funcs["Flatten"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromArray({});
        
        std::vector<Value> result;
        std::function<void(const Value&)> flatten = [&](const Value& v) {
            if (v.isArray() && v.arrayVal) {
                for (const auto& elem : *v.arrayVal) {
                    flatten(elem);
                }
            } else {
                result.push_back(v);
            }
        };
        
        flatten(args[0]);
        return Value::fromArray(std::move(result));
    };
    
    funcs["OneHot"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromArray({});
        
        size_t idx = static_cast<size_t>(args[0].asDouble());
        size_t numClasses = static_cast<size_t>(args[1].asDouble());
        
        std::vector<Value> arr(numClasses, Value::fromFloat(0.0));
        if (idx < numClasses) {
            arr[idx] = Value::fromFloat(1.0);
        }
        return Value::fromArray(std::move(arr));
    };
    
    // ========================================
    // MODEL FUNCTIONS
    // ========================================
    
    // Global model storage
    static std::unordered_map<int, std::shared_ptr<Model>> modelRegistry;
    static int nextModelId = 1;
    
    funcs["Sequential"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        auto model = std::make_shared<Model>();
        
        // Parse layers from array
        if (!args.empty() && args[0].isArray() && args[0].arrayVal) {
            for (const auto& layerVal : *args[0].arrayVal) {
                if (layerVal.isArray() && layerVal.arrayVal && !layerVal.arrayVal->empty()) {
                    const auto& layerDef = *layerVal.arrayVal;
                    
                    if (layerDef[0].isString() && layerDef[0].stringVal) {
                        std::string layerType = *layerDef[0].stringVal;
                        Layer layer;
                        layer.type = stringToLayerType(layerType);
                        
                        if (layerType == "linear" && layerDef.size() >= 3) {
                            layer.inputSize = static_cast<size_t>(layerDef[1].asDouble());
                            layer.outputSize = static_cast<size_t>(layerDef[2].asDouble());
                        } else if (layerType == "leakyrelu" && layerDef.size() >= 2) {
                            layer.param = static_cast<float>(layerDef[1].asDouble());
                        } else if (layerType == "dropout" && layerDef.size() >= 2) {
                            layer.param = static_cast<float>(layerDef[1].asDouble());
                        }
                        
                        model->addLayer(layer);
                    }
                }
            }
        }
        
        int modelId = nextModelId++;
        modelRegistry[modelId] = model;
        
        return Value::fromInt(modelId);
    };
    
    funcs["Layer"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromArray({});
        
        std::vector<Value> result;
        
        // First arg is layer type string
        if (args[0].isString() && args[0].stringVal) {
            result.push_back(args[0]);
            
            // Copy additional params
            for (size_t i = 1; i < args.size(); ++i) {
                result.push_back(args[i]);
            }
        }
        
        return Value::fromArray(std::move(result));
    };
    
    funcs["train"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 4) return Value::nil();
        
        int modelId = static_cast<int>(args[0].asDouble());
        auto it = modelRegistry.find(modelId);
        if (it == modelRegistry.end()) return Value::nil();
        
        auto& model = it->second;
        
        // Parse training data
        std::vector<std::vector<float>> xData;
        std::vector<int> yData;
        
        if (args[1].isArray() && args[1].arrayVal) {
            for (const auto& sample : *args[1].arrayVal) {
                xData.push_back(ai_internal::toFloatVector(sample));
            }
        }
        
        if (args[2].isArray() && args[2].arrayVal) {
            for (const auto& label : *args[2].arrayVal) {
                yData.push_back(static_cast<int>(label.asDouble()));
            }
        }
        
        // Parse config
        int epochs = 100;
        float lr = 0.001f;
        std::string optimizer = "adam";
        std::string loss = "crossentropy";
        
        if (args[3].isArray() && args[3].arrayVal) {
            const auto& config = *args[3].arrayVal;
            if (config.size() >= 1) epochs = static_cast<int>(config[0].asDouble());
            if (config.size() >= 2) lr = static_cast<float>(config[1].asDouble());
            if (config.size() >= 3 && config[2].isString() && config[2].stringVal) {
                optimizer = *config[2].stringVal;
            }
            if (config.size() >= 4 && config[3].isString() && config[3].stringVal) {
                loss = *config[3].stringVal;
            }
        }
        
        // Train
        model->train(xData, yData, epochs, lr, optimizer, loss, true);
        
        return Value::fromFloat(model->getFinalLoss());
    };
    
    funcs["predict"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::nil();
        
        int modelId = static_cast<int>(args[0].asDouble());
        auto it = modelRegistry.find(modelId);
        if (it == modelRegistry.end()) return Value::nil();
        
        auto input = ai_internal::toFloatVector(args[1]);
        auto output = it->second->predict(input);
        
        return ai_internal::fromFloatVector(output);
    };
    
    funcs["saveModel"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::fromBool(false);
        
        int modelId = static_cast<int>(args[0].asDouble());
        auto it = modelRegistry.find(modelId);
        if (it == modelRegistry.end()) return Value::fromBool(false);
        
        if (!args[1].isString() || !args[1].stringVal) return Value::fromBool(false);
        
        // Resolve path relative to current working directory
        std::string pathStr = *args[1].stringVal;
        fs::path savePath;
        
        if (fs::path(pathStr).is_absolute()) {
            savePath = pathStr;
        } else {
            savePath = fs::current_path() / pathStr;
        }
        
        // Create parent directories if needed
        if (savePath.has_parent_path()) {
            fs::create_directories(savePath.parent_path());
        }
        
        bool success = it->second->save(savePath.string());
        return Value::fromBool(success);
    };
    
    funcs["loadModel"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) {
            return Value::fromInt(-1);
        }
        
        // Resolve path relative to current working directory
        std::string pathStr = *args[0].stringVal;
        fs::path loadPath;
        
        if (fs::path(pathStr).is_absolute()) {
            loadPath = pathStr;
        } else {
            loadPath = fs::current_path() / pathStr;
        }
        
        if (!fs::exists(loadPath)) {
            std::cerr << "Error: Model file not found: " << loadPath.string() << std::endl;
            return Value::fromInt(-1);
        }
        
        auto model = Model::load(loadPath.string());
        if (!model) return Value::fromInt(-1);
        
        int modelId = nextModelId++;
        modelRegistry[modelId] = model;
        
        return Value::fromInt(modelId);
    };
    
    funcs["getModelInfo"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        
        int modelId = static_cast<int>(args[0].asDouble());
        auto it = modelRegistry.find(modelId);
        if (it == modelRegistry.end()) return Value::nil();
        
        auto& model = it->second;
        
        std::vector<Value> info;
        info.push_back(Value::fromInt(static_cast<int64_t>(model->getInputSize())));
        info.push_back(Value::fromInt(static_cast<int64_t>(model->getOutputSize())));
        info.push_back(Value::fromInt(model->getEpoch()));
        info.push_back(Value::fromFloat(model->getFinalLoss()));
        info.push_back(Value::fromBool(model->isTrained()));
        
        return Value::fromArray(std::move(info));
    };
    
    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
