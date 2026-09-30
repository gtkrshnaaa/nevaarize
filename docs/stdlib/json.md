# Standard Library: `json`

The `json` module implements RFC 8259 compliant parsing and deserialization of JSON text payloads into native Nevaarize maps, arrays, strings, numbers, booleans, and nil primitives.

```nva
import stdlib json as json
```

## Function Reference

- `json.ParseJSON(filePath)`: Reads JSON document from file system path and parses it into native data structure.
- `json.ParseJSONString(jsonText)`: Parses in-memory string of JSON text into corresponding Nevaarize value.

### Usage Example

```nva
import stdlib json as json

payload = "{\"model\": \"LinearClassifier\", \"epochs\": 50, \"accuracy\": 0.98}"
data = json.ParseJSONString(payload)

print("Model:", data["model"])
print("Accuracy:", data["accuracy"])
```
