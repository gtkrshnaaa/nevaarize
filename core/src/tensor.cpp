/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Tensor.cpp - Tensor Implementation
 *
 * SIMD-accelerated tensor operations.
 */

#include "tensor.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace nevaarize {

Tensor::Tensor() : size_(0), dtype_(DType::FLOAT32) {}

Tensor::Tensor(const std::vector<int64_t>& shape, DType dtype)
    : shape_(shape), dtype_(dtype) {
    size_ = 1;
    for (int64_t dim : shape) {
        size_ *= static_cast<size_t>(dim);
    }
    
    data_ = std::shared_ptr<float[]>(
        static_cast<float*>(simdAlloc(size_ * sizeof(float))),
        simdFree
    );
    
    std::memset(data_.get(), 0, size_ * sizeof(float));
    computeStrides();
}

void Tensor::computeStrides() {
    strides_.resize(shape_.size());
    if (shape_.empty()) return;
    
    strides_.back() = 1;
    for (int i = static_cast<int>(shape_.size()) - 2; i >= 0; --i) {
        strides_[i] = strides_[i + 1] * shape_[i + 1];
    }
}

Tensor Tensor::fromVector(const std::vector<float>& data) {
    Tensor t({static_cast<int64_t>(data.size())});
    std::memcpy(t.data_.get(), data.data(), data.size() * sizeof(float));
    return t;
}

Tensor Tensor::matrix(int64_t rows, int64_t cols, DType dtype) {
    return Tensor({rows, cols}, dtype);
}

Tensor Tensor::full(const std::vector<int64_t>& shape, float value, DType dtype) {
    Tensor t(shape, dtype);
    for (size_t i = 0; i < t.size_; ++i) {
        t.data_[i] = value;
    }
    return t;
}

Tensor Tensor::zeros(const std::vector<int64_t>& shape, DType dtype) {
    return Tensor(shape, dtype);
}

Tensor Tensor::ones(const std::vector<int64_t>& shape, DType dtype) {
    return full(shape, 1.0f, dtype);
}

Tensor Tensor::eye(int64_t n, DType dtype) {
    Tensor t({n, n}, dtype);
    for (int64_t i = 0; i < n; ++i) {
        t.at(i, i) = 1.0f;
    }
    return t;
}

float& Tensor::at(int64_t i) {
    return data_[i];
}

float Tensor::at(int64_t i) const {
    return data_[i];
}

size_t Tensor::index(int64_t i, int64_t j) const {
    return static_cast<size_t>(i * strides_[0] + j * strides_[1]);
}

float& Tensor::at(int64_t i, int64_t j) {
    return data_[index(i, j)];
}

float Tensor::at(int64_t i, int64_t j) const {
    return data_[index(i, j)];
}

Tensor Tensor::add(const Tensor& other) const {
    Tensor result(shape_);
    vecAdd_f32(result.data_.get(), data_.get(), other.data_.get(), size_);
    return result;
}

Tensor Tensor::sub(const Tensor& other) const {
    Tensor result(shape_);
    vecSub_f32(result.data_.get(), data_.get(), other.data_.get(), size_);
    return result;
}

Tensor Tensor::mul(const Tensor& other) const {
    Tensor result(shape_);
    vecMul_f32(result.data_.get(), data_.get(), other.data_.get(), size_);
    return result;
}

Tensor Tensor::add(float scalar) const {
    Tensor result(shape_);
    vecScalarAdd_f32(result.data_.get(), data_.get(), scalar, size_);
    return result;
}

Tensor Tensor::mul(float scalar) const {
    Tensor result(shape_);
    vecScalarMul_f32(result.data_.get(), data_.get(), scalar, size_);
    return result;
}

float Tensor::sum() const {
    return vecSum_f32(data_.get(), size_);
}

float Tensor::mean() const {
    return sum() / static_cast<float>(size_);
}

float Tensor::max() const {
    float maxVal = data_[0];
    for (size_t i = 1; i < size_; ++i) {
        if (data_[i] > maxVal) maxVal = data_[i];
    }
    return maxVal;
}

float Tensor::min() const {
    float minVal = data_[0];
    for (size_t i = 1; i < size_; ++i) {
        if (data_[i] < minVal) minVal = data_[i];
    }
    return minVal;
}

Tensor Tensor::relu() const {
    Tensor result(shape_);
    vecReLU_f32(result.data_.get(), data_.get(), size_);
    return result;
}

Tensor Tensor::sigmoid() const {
    Tensor result(shape_);
    vecSigmoid_f32(result.data_.get(), data_.get(), size_);
    return result;
}

Tensor Tensor::tanh() const {
    Tensor result(shape_);
    vecTanh_f32(result.data_.get(), data_.get(), size_);
    return result;
}

Tensor Tensor::softmax() const {
    Tensor result(shape_);
    
    if (ndim() == 1) {
        // 1D softmax
        float maxVal = max();
        float sum = 0.0f;
        for (size_t i = 0; i < size_; ++i) {
            result.data_[i] = std::exp(data_[i] - maxVal);
            sum += result.data_[i];
        }
        for (size_t i = 0; i < size_; ++i) {
            result.data_[i] /= sum;
        }
    } else if (ndim() == 2) {
        // 2D softmax along last dimension
        int64_t rows = shape_[0];
        int64_t cols = shape_[1];
        
        for (int64_t i = 0; i < rows; ++i) {
            float maxVal = at(i, 0);
            for (int64_t j = 1; j < cols; ++j) {
                if (at(i, j) > maxVal) maxVal = at(i, j);
            }
            
            float sum = 0.0f;
            for (int64_t j = 0; j < cols; ++j) {
                result.at(i, j) = std::exp(at(i, j) - maxVal);
                sum += result.at(i, j);
            }
            for (int64_t j = 0; j < cols; ++j) {
                result.at(i, j) /= sum;
            }
        }
    }
    
    return result;
}

Tensor Tensor::transpose() const {
    if (ndim() != 2) return *this;
    
    Tensor result({shape_[1], shape_[0]});
    int64_t rows = shape_[0];
    int64_t cols = shape_[1];
    
    for (int64_t i = 0; i < rows; ++i) {
        for (int64_t j = 0; j < cols; ++j) {
            result.at(j, i) = at(i, j);
        }
    }
    
    return result;
}

Tensor Tensor::reshape(const std::vector<int64_t>& newShape) const {
    size_t newSize = 1;
    for (int64_t dim : newShape) {
        newSize *= static_cast<size_t>(dim);
    }
    
    if (newSize != size_) {
        // Cannot reshape
        return *this;
    }
    
    Tensor result;
    result.data_ = data_;  // Share data
    result.shape_ = newShape;
    result.size_ = size_;
    result.dtype_ = dtype_;
    result.computeStrides();
    
    return result;
}

Tensor Tensor::matmul(const Tensor& other) const {
    if (ndim() != 2 || other.ndim() != 2) {
        return Tensor();
    }
    
    int M = static_cast<int>(shape_[0]);
    int K = static_cast<int>(shape_[1]);
    int N = static_cast<int>(other.shape_[1]);
    
    if (K != static_cast<int>(other.shape_[0])) {
        return Tensor();
    }
    
    Tensor result({shape_[0], other.shape_[1]});
    matmul_blocked(result.data_.get(), data_.get(), other.data_.get(), M, N, K);
    
    return result;
}

void Tensor::print() const {
    std::cout << "Tensor(shape=[";
    for (size_t i = 0; i < shape_.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << shape_[i];
    }
    std::cout << "], dtype=float32)" << std::endl;
    
    if (size_ <= 20) {
        std::cout << "[";
        for (size_t i = 0; i < size_; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << std::fixed << std::setprecision(4) << data_[i];
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "[" << data_[0] << ", " << data_[1] << ", ... , " 
                  << data_[size_-2] << ", " << data_[size_-1] << "]" << std::endl;
    }
}

// Cache-blocked matrix multiplication with SIMD and multi-threading
void matmul_blocked(float* C, const float* A, const float* B,
                    int M, int N, int K, int blockSize) {
    // Initialize C to zero
    std::memset(C, 0, M * N * sizeof(float));

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    if (M < static_cast<int>(numThreads * 4)) numThreads = 1;

    if (numThreads == 1) {
        // Single-threaded path for small matrices
        for (int i0 = 0; i0 < M; i0 += blockSize) {
            for (int j0 = 0; j0 < N; j0 += blockSize) {
                for (int k0 = 0; k0 < K; k0 += blockSize) {
                    int iMax = std::min(i0 + blockSize, M);
                    int jMax = std::min(j0 + blockSize, N);
                    int kMax = std::min(k0 + blockSize, K);

                    for (int i = i0; i < iMax; ++i) {
                        for (int k = k0; k < kMax; ++k) {
                            float aik = A[i * K + k];
#if defined(__AVX2__)
                            __m256 va = _mm256_set1_ps(aik);
                            int j = j0;
                            for (; j + 8 <= jMax; j += 8) {
                                __m256 vb = _mm256_loadu_ps(&B[k * N + j]);
                                __m256 vc = _mm256_loadu_ps(&C[i * N + j]);
#if defined(__FMA__)
                                vc = _mm256_fmadd_ps(va, vb, vc);
#else
                                vc = _mm256_add_ps(vc, _mm256_mul_ps(va, vb));
#endif
                                _mm256_storeu_ps(&C[i * N + j], vc);
                            }
                            for (; j < jMax; ++j) {
                                C[i * N + j] += aik * B[k * N + j];
                            }
#else
                            for (int j = j0; j < jMax; ++j) {
                                C[i * N + j] += aik * B[k * N + j];
                            }
#endif
                        }
                    }
                }
            }
        }
        return;
    }

    // Multi-threaded path: partition rows across threads
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    int rowsPerThread = M / static_cast<int>(numThreads);
    int remainder = M % static_cast<int>(numThreads);

    int rowStart = 0;
    for (unsigned int t = 0; t < numThreads; ++t) {
        int rowEnd = rowStart + rowsPerThread + (static_cast<int>(t) < remainder ? 1 : 0);

        threads.emplace_back([=]() {
            for (int i0 = rowStart; i0 < rowEnd; i0 += blockSize) {
                for (int j0 = 0; j0 < N; j0 += blockSize) {
                    for (int k0 = 0; k0 < K; k0 += blockSize) {
                        int iMax = std::min(i0 + blockSize, rowEnd);
                        int jMax = std::min(j0 + blockSize, N);
                        int kMax = std::min(k0 + blockSize, K);

                        for (int i = i0; i < iMax; ++i) {
                            for (int k = k0; k < kMax; ++k) {
                                float aik = A[i * K + k];
#if defined(__AVX2__)
                                __m256 va = _mm256_set1_ps(aik);
                                int j = j0;
                                for (; j + 8 <= jMax; j += 8) {
                                    __m256 vb = _mm256_loadu_ps(&B[k * N + j]);
                                    __m256 vc = _mm256_loadu_ps(&C[i * N + j]);
#if defined(__FMA__)
                                    vc = _mm256_fmadd_ps(va, vb, vc);
#else
                                    vc = _mm256_add_ps(vc, _mm256_mul_ps(va, vb));
#endif
                                    _mm256_storeu_ps(&C[i * N + j], vc);
                                }
                                for (; j < jMax; ++j) {
                                    C[i * N + j] += aik * B[k * N + j];
                                }
#else
                                for (int j = j0; j < jMax; ++j) {
                                    C[i * N + j] += aik * B[k * N + j];
                                }
#endif
                            }
                        }
                    }
                }
            }
        });

        rowStart = rowEnd;
    }

    for (auto& th : threads) {
        th.join();
    }
}


MatmulResult benchmarkMatmul(int M, int N, int K) {
    Tensor A = Tensor::ones({M, K});
    Tensor B = Tensor::ones({K, N});
    
    // Warm up
    Tensor C = A.matmul(B);
    
    auto start = std::chrono::high_resolution_clock::now();
    C = A.matmul(B);
    auto end = std::chrono::high_resolution_clock::now();
    
    double seconds = std::chrono::duration<double>(end - start).count();
    double flops = 2.0 * M * N * K;  // 2 ops per multiply-add
    double gflops = flops / seconds / 1e9;
    
    return {gflops, seconds};
}

} // namespace nevaarize
