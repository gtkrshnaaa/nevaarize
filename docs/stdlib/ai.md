# Standard Library: `ai`

The `ai` module provides a deep learning runtime for constructing, training, optimizing, and serializing neural network architectures and performing multi-dimensional tensor operations.

```nva
import stdlib ai as nn
```

## 1. Model Lifecycle

### `nn.Sequential(layer1, layer2, ...)`
Instantiates a feed-forward sequential neural network container populated with the specified layers. Returns a model handle.

### Model Object Methods
A model instance provides the following method interfaces:
- `model.predict(input)`: Executes forward inference. Accepts a 1D feature array or 2D batch tensor `[[...], [...]]` and returns output predictions.
- `model.train(xTrain, yTrain, config)`: Executes multi-epoch backpropagation training.
  - `config = {"epochs": 100, "learningRate": 0.01, "optimizer": "adam"}`
- `model.save(filePath)`: Serializes model architecture, weights, and training metrics into a binary `.nmod` file.
- `model.numLayers()`: Returns the total count of configured layers.
- `model.info()`: Returns architecture metadata and parameter counts.

### `nn.SaveModel(modelId, filePath)` / `nn.LoadModel(filePath)`
Saves or restores a trained network from disk:

```nva
import stdlib ai as nn

model = nn.LoadModel("models/classifier.nmod")
output = model.predict([0.5, 1.2, -0.3])
```

## 2. Layer Constructors

- `nn.Linear(inFeatures, outFeatures)`: Fully connected dense projection layer with learnable weights and biases.
- `nn.Layer(type, inFeatures, outFeatures)`: General layer specification (`"dense"`, `"linear"`, `"conv2d"`).
- `nn.Dropout(dropRate)`: Regularization layer applying inverted dropout during training.
- `nn.LayerNorm(normalizedShape)`: Layer normalization stabilizing hidden representation distributions.
- `nn.Embedding(vocabSize, embeddingDim)`: Token embedding lookup table mapping categorical indices to continuous latent vectors.

## 3. Activation Functions

- `nn.ReLU(tensor)`: Rectified Linear Unit, `max(0, x)`.
- `nn.LeakyReLU(tensor, alpha)`: Leaky ReLU with slope `alpha` for negative inputs.
- `nn.GELU(tensor)`: Gaussian Error Linear Unit activation with fast approximation.
- `nn.SiLU(tensor)`: Sigmoid Linear Unit (Swish activation), `x * sigmoid(x)`.
- `nn.Sigmoid(tensor)`: Logistic sigmoid curve, `1 / (1 + exp(-x))`.
- `nn.Tanh(tensor)`: Hyperbolic tangent activation, `tanh(x)`.
- `nn.Softmax(tensor)`: Normalized exponential probability distribution across logits.

## 4. Loss Functions

- `nn.MSELoss(prediction, target)`: Mean Squared Error loss.
- `nn.L1Loss(prediction, target)`: Mean Absolute Error loss.
- `nn.BCELoss(prediction, target)`: Binary Cross Entropy loss for classification.
- `nn.CrossEntropyLoss(logits, targetClass)`: Multi-class negative log-likelihood loss.
- `nn.HuberLoss(prediction, target, delta)`: Smooth L1 / Huber robust regression loss.

## 5. Tensor Algebra & Generative Utilities

- `nn.Zeros(size)` / `nn.Ones(size)`: Initializes tensor filled with 0.0 or 1.0.
- `nn.RandN(size)` / `nn.RandU(size)`: Generates Gaussian normal or uniform random tensors.
- `nn.MatMul(matrixA, matrixB)`: Performs hardware-accelerated 2D matrix multiplication.
- `nn.Transpose(matrix)`: Transposes dimensions of a 2D matrix.
- `nn.Dot(vecA, vecB)`: Computes vector inner dot product.
- `nn.OneHot(classIndex, numClasses)`: Generates a one-hot categorical vector.
- `nn.TopK(probabilities, k)`: Returns the top `k` indices sorted by probability.
- `nn.SampleToken(probabilities, temperature)`: Performs stochastic token sampling using temperature scaling.
- `nn.ClipGradNorm(gradients, maxNorm)`: Clips gradient vector norm to prevent gradient explosion.
