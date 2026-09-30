# Standard Library: `claw`

The `claw` module provides high-performance HTML parsing, CSS-like element selection, and structured data extraction pipelines.

```nva
import stdlib claw as claw
```

## Function Reference

- `claw.CrawlFile(filePath)`: Reads and parses an HTML document from the filesystem, returning an internal DOM element collection.
- `claw.CrawlString(htmlContent)`: Parses raw HTML string content into a DOM element collection.
- `claw.Select(elements, selector)`: Queries the DOM tree using CSS selector syntax (e.g. tag names, class identifiers, ID specifiers) and returns matching sub-elements.
- `claw.SaveCSV(filePath, elements)`: Formats and exports extracted element text or structured attributes into a CSV file.
- `claw.SaveJSON(filePath, elements)`: Formats and exports extracted element records into an RFC 8259 JSON file.

### Web Scraping Example

```nva
import stdlib claw as claw

html = "<html><body><div class='item'>Product A</div><div class='item'>Product B</div></body></html>"
dom = claw.CrawlString(html)
items = claw.Select(dom, ".item")

claw.SaveJSON("output.json", items)
```
