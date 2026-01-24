/**
 * Model.cpp - Nevaarize Neural Network Model Implementation
 *
 * Complete implementation of forward pass, backpropagation, training,
 * and .nmod serialization.
 */

#include "Model.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>
#include <immintrin.h>

namespace nevaarize {

// SIMD helpers
namespace {

float simdDot(const float* a, const float* b, size_t n) {
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
    for (; i < n; ++i) sum += a[i] * b[i];
    return sum;
}

} // anonymous namespace

void Model::addLayer(const Layer& layer) {
    layers.push_back(layer);
}

void Model::initializeWeights() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    for (auto& layer : layers) {
        if (layer.type == LayerType::LINEAR) {
            // He initialization for ReLU networks
            float std = std::sqrt(2.0f / layer.inputSize);
            std::normal_distribution<float> dist(0.0f, std);
            
            layer.weights.resize(layer.inputSize * layer.outputSize);
            layer.bias.resize(layer.outputSize, 0.0f);
            
            for (auto& w : layer.weights) {
                w = dist(gen);
            }
            
            // Initialize Adam momentum and velocity
            layer.mWeights.resize(layer.weights.size(), 0.0f);
            layer.vWeights.resize(layer.weights.size(), 0.0f);
            layer.mBias.resize(layer.bias.size(), 0.0f);
            layer.vBias.resize(layer.bias.size(), 0.0f);
        }
    }
}

std::vector<float> Model::applyActivation(const std::vector<float>& input, 
                                          LayerType type, float param) {
    std::vector<float> output(input.size());
    
    switch (type) {
        case LayerType::RELU:
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = std::max(0.0f, input[i]);
            }
            break;
            
        case LayerType::LEAKY_RELU:
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = input[i] > 0 ? input[i] : param * input[i];
            }
            break;
            
        case LayerType::SIGMOID:
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = 1.0f / (1.0f + std::exp(-input[i]));
            }
            break;
            
        case LayerType::TANH:
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = std::tanh(input[i]);
            }
            break;
            
        case LayerType::SOFTMAX: {
            float maxVal = *std::max_element(input.begin(), input.end());
            float sum = 0.0f;
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = std::exp(input[i] - maxVal);
                sum += output[i];
            }
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] /= sum;
            }
            break;
        }
            
        case LayerType::GELU: {
            constexpr float sqrt2pi = 0.7978845608f;
            constexpr float coef = 0.044715f;
            for (size_t i = 0; i < input.size(); ++i) {
                float x = input[i];
                float x3 = x * x * x;
                output[i] = 0.5f * x * (1.0f + std::tanh(sqrt2pi * (x + coef * x3)));
            }
            break;
        }
            
        case LayerType::SILU:
            for (size_t i = 0; i < input.size(); ++i) {
                output[i] = input[i] / (1.0f + std::exp(-input[i]));
            }
            break;
            
        default:
            output = input;
    }
    
    return output;
}

std::vector<float> Model::activationGradient(const std::vector<float>& output,
                                             LayerType type, float param) {
    std::vector<float> grad(output.size());
    
    switch (type) {
        case LayerType::RELU:
            for (size_t i = 0; i < output.size(); ++i) {
                grad[i] = output[i] > 0 ? 1.0f : 0.0f;
            }
            break;
            
        case LayerType::LEAKY_RELU:
            for (size_t i = 0; i < output.size(); ++i) {
                grad[i] = output[i] > 0 ? 1.0f : param;
            }
            break;
            
        case LayerType::SIGMOID:
            for (size_t i = 0; i < output.size(); ++i) {
                grad[i] = output[i] * (1.0f - output[i]);
            }
            break;
            
        case LayerType::TANH:
            for (size_t i = 0; i < output.size(); ++i) {
                grad[i] = 1.0f - output[i] * output[i];
            }
            break;
            
        default:
            std::fill(grad.begin(), grad.end(), 1.0f);
    }
    
    return grad;
}

std::vector<float> Model::forward(const std::vector<float>& input, bool training) {
    activations.clear();
    activations.push_back(input);
    
    std::vector<float> current = input;
    
    for (auto& layer : layers) {
        if (layer.type == LayerType::LINEAR) {
            std::vector<float> output(layer.outputSize, 0.0f);
            
            for (size_t o = 0; o < layer.outputSize; ++o) {
                output[o] = layer.bias[o];
                output[o] += simdDot(current.data(), 
                                     &layer.weights[o * layer.inputSize],
                                     layer.inputSize);
            }
            
            current = output;
        } else if (layer.type == LayerType::DROPOUT && training) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::bernoulli_distribution drop(layer.param);
            float scale = 1.0f / (1.0f - layer.param);
            
            for (auto& v : current) {
                v = drop(gen) ? 0.0f : v * scale;
            }
        } else {
            current = applyActivation(current, layer.type, layer.param);
        }
        
        activations.push_back(current);
    }
    
    return current;
}

float Model::computeLoss(const std::vector<float>& output, int target,
                         const std::string& lossType) {
    if (lossType == "crossentropy") {
        constexpr float eps = 1e-7f;
        if (target >= 0 && target < static_cast<int>(output.size())) {
            return -std::log(std::max(output[target], eps));
        }
    } else if (lossType == "mse") {
        float sum = 0.0f;
        for (size_t i = 0; i < output.size(); ++i) {
            float t = (static_cast<int>(i) == target) ? 1.0f : 0.0f;
            float diff = output[i] - t;
            sum += diff * diff;
        }
        return sum / output.size();
    }
    return 0.0f;
}

std::vector<float> Model::lossGradient(const std::vector<float>& output, int target,
                                       const std::string& lossType) {
    std::vector<float> grad(output.size());
    
    if (lossType == "crossentropy") {
        // For softmax + crossentropy: grad = output - oneHot(target)
        for (size_t i = 0; i < output.size(); ++i) {
            float t = (static_cast<int>(i) == target) ? 1.0f : 0.0f;
            grad[i] = output[i] - t;
        }
    } else if (lossType == "mse") {
        for (size_t i = 0; i < output.size(); ++i) {
            float t = (static_cast<int>(i) == target) ? 1.0f : 0.0f;
            grad[i] = 2.0f * (output[i] - t) / output.size();
        }
    }
    
    return grad;
}

void Model::backward(const std::vector<float>& target, const std::string& lossType) {
    gradients.clear();
    gradients.resize(layers.size());
    
    // Get output gradient from loss
    int targetIdx = static_cast<int>(std::max_element(target.begin(), target.end()) - target.begin());
    std::vector<float> delta = lossGradient(activations.back(), targetIdx, lossType);
    
    // Backpropagate through layers
    for (int l = static_cast<int>(layers.size()) - 1; l >= 0; --l) {
        auto& layer = layers[l];
        const auto& actIn = activations[l];
        
        if (layer.type == LayerType::LINEAR) {
            gradients[l] = delta;
            
            // Compute upstream gradient
            std::vector<float> newDelta(layer.inputSize, 0.0f);
            for (size_t i = 0; i < layer.inputSize; ++i) {
                for (size_t o = 0; o < layer.outputSize; ++o) {
                    newDelta[i] += delta[o] * layer.weights[o * layer.inputSize + i];
                }
            }
            delta = newDelta;
        } else if (layer.type != LayerType::SOFTMAX) {
            // Apply activation gradient
            auto actGrad = activationGradient(activations[l + 1], layer.type, layer.param);
            for (size_t i = 0; i < delta.size(); ++i) {
                delta[i] *= actGrad[i];
            }
        }
    }
}

void Model::updateWeights(const std::string& optimizer, float lr,
                          float beta1, float beta2, int step) {
    constexpr float eps = 1e-8f;
    
    for (size_t l = 0; l < layers.size(); ++l) {
        auto& layer = layers[l];
        
        if (layer.type != LayerType::LINEAR || gradients[l].empty()) continue;
        
        const auto& actIn = activations[l];
        const auto& delta = gradients[l];
        
        if (optimizer == "adam") {
            float beta1t = std::pow(beta1, static_cast<float>(step));
            float beta2t = std::pow(beta2, static_cast<float>(step));
            
            for (size_t o = 0; o < layer.outputSize; ++o) {
                // Bias gradient
                float gB = delta[o];
                layer.mBias[o] = beta1 * layer.mBias[o] + (1.0f - beta1) * gB;
                layer.vBias[o] = beta2 * layer.vBias[o] + (1.0f - beta2) * gB * gB;
                float mHat = layer.mBias[o] / (1.0f - beta1t);
                float vHat = layer.vBias[o] / (1.0f - beta2t);
                layer.bias[o] -= lr * mHat / (std::sqrt(vHat) + eps);
                
                for (size_t i = 0; i < layer.inputSize; ++i) {
                    size_t idx = o * layer.inputSize + i;
                    float gW = delta[o] * actIn[i];
                    
                    layer.mWeights[idx] = beta1 * layer.mWeights[idx] + (1.0f - beta1) * gW;
                    layer.vWeights[idx] = beta2 * layer.vWeights[idx] + (1.0f - beta2) * gW * gW;
                    
                    mHat = layer.mWeights[idx] / (1.0f - beta1t);
                    vHat = layer.vWeights[idx] / (1.0f - beta2t);
                    
                    layer.weights[idx] -= lr * mHat / (std::sqrt(vHat) + eps);
                }
            }
        } else {
            // SGD
            for (size_t o = 0; o < layer.outputSize; ++o) {
                layer.bias[o] -= lr * delta[o];
                for (size_t i = 0; i < layer.inputSize; ++i) {
                    size_t idx = o * layer.inputSize + i;
                    layer.weights[idx] -= lr * delta[o] * actIn[i];
                }
            }
        }
    }
}

void Model::train(const std::vector<std::vector<float>>& xData,
                  const std::vector<int>& yData,
                  int epochs, float lr, const std::string& optimizer,
                  const std::string& lossType, bool verbose) {
    if (xData.empty() || xData.size() != yData.size()) {
        std::cerr << "Error: Invalid training data" << std::endl;
        return;
    }
    
    initializeWeights();
    
    int step = 0;
    
    for (int e = 0; e < epochs; ++e) {
        float totalLoss = 0.0f;
        int correct = 0;
        
        for (size_t s = 0; s < xData.size(); ++s) {
            ++step;
            
            // Forward pass
            auto output = forward(xData[s], true);
            
            // Compute loss
            totalLoss += computeLoss(output, yData[s], lossType);
            
            // Check accuracy
            int pred = static_cast<int>(std::max_element(output.begin(), output.end()) - output.begin());
            if (pred == yData[s]) ++correct;
            
            // Backward pass
            std::vector<float> target(output.size(), 0.0f);
            target[yData[s]] = 1.0f;
            backward(target, lossType);
            
            // Update weights
            updateWeights(optimizer, lr, 0.9f, 0.999f, step);
        }
        
        float avgLoss = totalLoss / xData.size();
        float accuracy = static_cast<float>(correct) / xData.size() * 100.0f;
        
        if (verbose && (e == 0 || (e + 1) % 10 == 0 || e == epochs - 1)) {
            std::cout << "Epoch " << std::setw(4) << (e + 1) 
                      << " | Loss: " << std::fixed << std::setprecision(4) << avgLoss
                      << " | Accuracy: " << std::setprecision(1) << accuracy << "%"
                      << std::endl;
        }
        
        finalLoss = avgLoss;
        epoch = e + 1;
    }
    
    trained = true;
}

std::vector<float> Model::predict(const std::vector<float>& input) {
    return forward(input, false);
}

size_t Model::getInputSize() const {
    for (const auto& layer : layers) {
        if (layer.type == LayerType::LINEAR) {
            return layer.inputSize;
        }
    }
    return 0;
}

size_t Model::getOutputSize() const {
    for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
        if (it->type == LayerType::LINEAR) {
            return it->outputSize;
        }
    }
    return 0;
}

bool Model::save(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
        return false;
    }
    
    // Write header
    NmodHeader header;
    header.magic = NMOD_MAGIC;
    header.version = NMOD_VERSION;
    header.numLayers = static_cast<uint16_t>(layers.size());
    header.weightsOffset = sizeof(NmodHeader);
    header.metadataOffset = 0; // Will be updated
    
    file.write(reinterpret_cast<char*>(&header), sizeof(header));
    
    // Write layer info and weights
    for (const auto& layer : layers) {
        uint8_t type = static_cast<uint8_t>(layer.type);
        file.write(reinterpret_cast<char*>(&type), sizeof(type));
        
        uint32_t inSize = static_cast<uint32_t>(layer.inputSize);
        uint32_t outSize = static_cast<uint32_t>(layer.outputSize);
        file.write(reinterpret_cast<char*>(&inSize), sizeof(inSize));
        file.write(reinterpret_cast<char*>(&outSize), sizeof(outSize));
        file.write(reinterpret_cast<const char*>(&layer.param), sizeof(layer.param));
        
        if (layer.type == LayerType::LINEAR) {
            uint32_t numWeights = static_cast<uint32_t>(layer.weights.size());
            file.write(reinterpret_cast<char*>(&numWeights), sizeof(numWeights));
            file.write(reinterpret_cast<const char*>(layer.weights.data()), 
                       layer.weights.size() * sizeof(float));
            
            uint32_t numBias = static_cast<uint32_t>(layer.bias.size());
            file.write(reinterpret_cast<char*>(&numBias), sizeof(numBias));
            file.write(reinterpret_cast<const char*>(layer.bias.data()),
                       layer.bias.size() * sizeof(float));
        }
    }
    
    // Write metadata
    file.write(reinterpret_cast<const char*>(&epoch), sizeof(epoch));
    file.write(reinterpret_cast<const char*>(&finalLoss), sizeof(finalLoss));
    
    std::cout << "Model saved to: " << path << std::endl;
    return true;
}

std::shared_ptr<Model> Model::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << path << std::endl;
        return nullptr;
    }
    
    // Read header
    NmodHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (header.magic != NMOD_MAGIC) {
        std::cerr << "Error: Invalid .nmod file format" << std::endl;
        return nullptr;
    }
    
    auto model = std::make_shared<Model>();
    
    // Read layers
    for (uint16_t i = 0; i < header.numLayers; ++i) {
        Layer layer;
        
        uint8_t type;
        file.read(reinterpret_cast<char*>(&type), sizeof(type));
        layer.type = static_cast<LayerType>(type);
        
        uint32_t inSize, outSize;
        file.read(reinterpret_cast<char*>(&inSize), sizeof(inSize));
        file.read(reinterpret_cast<char*>(&outSize), sizeof(outSize));
        layer.inputSize = inSize;
        layer.outputSize = outSize;
        
        file.read(reinterpret_cast<char*>(&layer.param), sizeof(layer.param));
        
        if (layer.type == LayerType::LINEAR) {
            uint32_t numWeights;
            file.read(reinterpret_cast<char*>(&numWeights), sizeof(numWeights));
            layer.weights.resize(numWeights);
            file.read(reinterpret_cast<char*>(layer.weights.data()),
                      numWeights * sizeof(float));
            
            uint32_t numBias;
            file.read(reinterpret_cast<char*>(&numBias), sizeof(numBias));
            layer.bias.resize(numBias);
            file.read(reinterpret_cast<char*>(layer.bias.data()),
                      numBias * sizeof(float));
        }
        
        model->addLayer(layer);
    }
    
    // Read metadata
    file.read(reinterpret_cast<char*>(&model->epoch), sizeof(model->epoch));
    file.read(reinterpret_cast<char*>(&model->finalLoss), sizeof(model->finalLoss));
    model->trained = true;
    
    std::cout << "Model loaded from: " << path << std::endl;
    std::cout << "  Epochs trained: " << model->epoch << std::endl;
    std::cout << "  Final loss: " << model->finalLoss << std::endl;
    
    return model;
}

} // namespace nevaarize
