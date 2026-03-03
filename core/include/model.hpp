/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Model.hpp - Nevaarize Neural Network Model System
 *
 * Provides model definition, training, serialization, and inference.
 */

#ifndef NEVAARIZE_MODEL_HPP
#define NEVAARIZE_MODEL_HPP

#include "value.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <cstdint>

namespace nevaarize {

/**
 * Layer type enumeration.
 */
enum class LayerType : uint8_t {
    LINEAR,
    RELU,
    LEAKY_RELU,
    SIGMOID,
    TANH,
    SOFTMAX,
    GELU,
    SILU,
    DROPOUT,
    LAYER_NORM
};

/**
 * Single layer in a neural network.
 */
struct Layer {
    LayerType type;
    size_t inputSize;
    size_t outputSize;
    float param;  // For LeakyReLU alpha, Dropout rate, etc.
    
    // Weights and biases (for Linear layers)
    std::vector<float> weights;
    std::vector<float> bias;
    
    // Momentum and velocity for Adam optimizer
    std::vector<float> mWeights;
    std::vector<float> vWeights;
    std::vector<float> mBias;
    std::vector<float> vBias;
    
    Layer() : type(LayerType::RELU), inputSize(0), outputSize(0), param(0.0f) {}
    
    Layer(LayerType t, size_t inSize = 0, size_t outSize = 0, float p = 0.0f)
        : type(t), inputSize(inSize), outputSize(outSize), param(p) {}
};

/**
 * Neural network model.
 */
class Model {
public:
    Model() : trained(false), epoch(0), finalLoss(0.0f) {}
    
    /**
     * Add a layer to the model.
     */
    void addLayer(const Layer& layer);
    
    /**
     * Initialize weights using He initialization for ReLU networks.
     */
    void initializeWeights();
    
    /**
     * Forward pass.
     */
    std::vector<float> forward(const std::vector<float>& input, bool training = false);
    
    /**
     * Backward pass (compute gradients).
     */
    void backward(const std::vector<float>& target, const std::string& lossType);
    
    /**
     * Update weights using specified optimizer.
     */
    void updateWeights(const std::string& optimizer, float lr, 
                       float beta1 = 0.9f, float beta2 = 0.999f, int step = 1);
    
    /**
     * Train the model.
     */
    void train(const std::vector<std::vector<float>>& xData,
               const std::vector<int>& yData,
               int epochs, float lr, const std::string& optimizer,
               const std::string& lossType, bool verbose = true);
    
    /**
     * Predict output for single input.
     */
    std::vector<float> predict(const std::vector<float>& input);
    
    /**
     * Save model to .nmod file.
     */
    bool save(const std::string& path);
    
    /**
     * Load model from .nmod file.
     */
    static std::shared_ptr<Model> load(const std::string& path);
    
    /**
     * Get number of layers.
     */
    size_t numLayers() const { return layers.size(); }
    
    /**
     * Get input size (first layer's input).
     */
    size_t getInputSize() const;
    
    /**
     * Get output size (last layer's output).
     */
    size_t getOutputSize() const;
    
    /**
     * Check if model is trained.
     */
    bool isTrained() const { return trained; }
    
    /**
     * Get training info.
     */
    int getEpoch() const { return epoch; }
    float getFinalLoss() const { return finalLoss; }
    
private:
    std::vector<Layer> layers;
    bool trained;
    int epoch;
    float finalLoss;
    
    // Cached activations for backprop
    std::vector<std::vector<float>> activations;
    std::vector<std::vector<float>> gradients;
    
    // Helper functions
    std::vector<float> applyActivation(const std::vector<float>& input, LayerType type, float param = 0.0f);
    std::vector<float> activationGradient(const std::vector<float>& output, LayerType type, float param = 0.0f);
    float computeLoss(const std::vector<float>& output, int target, const std::string& lossType);
    std::vector<float> lossGradient(const std::vector<float>& output, int target, const std::string& lossType);
};

/**
 * .nmod file magic number.
 */
constexpr uint32_t NMOD_MAGIC = 0x444F4D4E; // "NMOD"

/**
 * .nmod file version.
 */
constexpr uint16_t NMOD_VERSION = 1;

/**
 * .nmod file header structure.
 */
struct NmodHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t numLayers;
    uint32_t weightsOffset;
    uint32_t metadataOffset;
};

/**
 * Convert layer type to string.
 */
inline const char* layerTypeToString(LayerType type) {
    switch (type) {
        case LayerType::LINEAR: return "linear";
        case LayerType::RELU: return "relu";
        case LayerType::LEAKY_RELU: return "leakyrelu";
        case LayerType::SIGMOID: return "sigmoid";
        case LayerType::TANH: return "tanh";
        case LayerType::SOFTMAX: return "softmax";
        case LayerType::GELU: return "gelu";
        case LayerType::SILU: return "silu";
        case LayerType::DROPOUT: return "dropout";
        case LayerType::LAYER_NORM: return "layernorm";
        default: return "unknown";
    }
}

/**
 * Convert string to layer type.
 */
inline LayerType stringToLayerType(const std::string& str) {
    if (str == "linear") return LayerType::LINEAR;
    if (str == "relu") return LayerType::RELU;
    if (str == "leakyrelu") return LayerType::LEAKY_RELU;
    if (str == "sigmoid") return LayerType::SIGMOID;
    if (str == "tanh") return LayerType::TANH;
    if (str == "softmax") return LayerType::SOFTMAX;
    if (str == "gelu") return LayerType::GELU;
    if (str == "silu") return LayerType::SILU;
    if (str == "dropout") return LayerType::DROPOUT;
    if (str == "layernorm") return LayerType::LAYER_NORM;
    return LayerType::RELU;
}

} // namespace nevaarize

#endif // NEVAARIZE_MODEL_HPP
