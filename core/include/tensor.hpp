/**
 * Tensor.hpp - N-Dimensional Array for AI/ML
 *
 * High-performance tensor operations with SIMD acceleration.
 */

#ifndef NEVAARIZE_TENSOR_HPP
#define NEVAARIZE_TENSOR_HPP

#include "simd.hpp"
#include "vectorOps.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <initializer_list>

namespace nevaarize {

/**
 * Data type for tensor elements.
 */
enum class DType : uint8_t {
    FLOAT32 = 0,
    FLOAT64 = 1,
    INT32 = 2,
    INT64 = 3
};

/**
 * Get size of dtype in bytes.
 */
inline size_t dtypeSize(DType dtype) {
    switch (dtype) {
        case DType::FLOAT32: return 4;
        case DType::FLOAT64: return 8;
        case DType::INT32: return 4;
        case DType::INT64: return 8;
        default: return 0;
    }
}

/**
 * N-dimensional tensor optimized for AI/ML operations.
 */
class Tensor {
public:
    /**
     * Create empty tensor.
     */
    Tensor();

    /**
     * Create tensor with given shape (initialized to zero).
     */
    explicit Tensor(const std::vector<int64_t>& shape, DType dtype = DType::FLOAT32);

    /**
     * Create 1D tensor from vector.
     */
    static Tensor fromVector(const std::vector<float>& data);

    /**
     * Create 2D matrix.
     */
    static Tensor matrix(int64_t rows, int64_t cols, DType dtype = DType::FLOAT32);

    /**
     * Create tensor filled with value.
     */
    static Tensor full(const std::vector<int64_t>& shape, float value, DType dtype = DType::FLOAT32);

    /**
     * Create tensor filled with zeros.
     */
    static Tensor zeros(const std::vector<int64_t>& shape, DType dtype = DType::FLOAT32);

    /**
     * Create tensor filled with ones.
     */
    static Tensor ones(const std::vector<int64_t>& shape, DType dtype = DType::FLOAT32);

    /**
     * Create identity matrix.
     */
    static Tensor eye(int64_t n, DType dtype = DType::FLOAT32);

    // Accessors
    const std::vector<int64_t>& shape() const { return shape_; }
    size_t ndim() const { return shape_.size(); }
    size_t size() const { return size_; }
    DType dtype() const { return dtype_; }
    float* data() { return data_.get(); }
    const float* data() const { return data_.get(); }

    // Element access
    float& at(int64_t i);
    float at(int64_t i) const;
    float& at(int64_t i, int64_t j);
    float at(int64_t i, int64_t j) const;

    // Operations (return new tensors)
    Tensor add(const Tensor& other) const;
    Tensor sub(const Tensor& other) const;
    Tensor mul(const Tensor& other) const;  // Element-wise
    Tensor matmul(const Tensor& other) const;  // Matrix multiply
    Tensor transpose() const;

    // Scalar operations
    Tensor add(float scalar) const;
    Tensor mul(float scalar) const;

    // Reductions
    float sum() const;
    float mean() const;
    float max() const;
    float min() const;

    // Activation functions (in-place efficient versions available)
    Tensor relu() const;
    Tensor sigmoid() const;
    Tensor tanh() const;
    Tensor softmax() const;  // Along last dimension

    // Reshape (returns view if possible)
    Tensor reshape(const std::vector<int64_t>& newShape) const;

    // Print for debugging
    void print() const;

private:
    std::shared_ptr<float[]> data_;
    std::vector<int64_t> shape_;
    std::vector<int64_t> strides_;
    size_t size_;
    DType dtype_;

    void computeStrides();
    size_t index(int64_t i, int64_t j) const;
};

/**
 * Matrix multiplication with cache blocking and SIMD.
 * C = A @ B where A is MxK and B is KxN, resulting in MxN.
 */
void matmul_blocked(float* C, const float* A, const float* B,
                    int M, int N, int K, int blockSize = 32);

/**
 * Matrix multiplication benchmark.
 */
struct MatmulResult {
    double gflops;
    double seconds;
};

MatmulResult benchmarkMatmul(int M, int N, int K);

} // namespace nevaarize

#endif // NEVAARIZE_TENSOR_HPP
