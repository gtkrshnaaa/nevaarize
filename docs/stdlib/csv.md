# Standard Library: `csv`

The `csv` module provides RFC 4180 compliant parsing and serialization for comma-separated tabular records.

```nva
import stdlib csv as csv
```

## Function Reference

- `csv.ParseCSV(filePath)`: Reads a CSV file from `filePath` and parses it into a 2D array of records, where each row is an array of string values. Resolves relative file paths relative to the current source file directory.
- `csv.ParseCSVString(csvContent)`: Deserializes an in-memory CSV string into a 2D array of string values.
- `csv.WriteCSV(filePath, dataArray)`: Serializes a 2D array of records or dictionary entries and writes them to `filePath` as formatted CSV text.

### Parsing Example

```nva
import stdlib csv as csv

csvText = "id,name,role\n1,Alice,Engineer\n2,Bob,Architect"
records = csv.ParseCSVString(csvText)

for row in records {
    print(row[0], row[1], row[2])
}
```
