---
id: multi-source-sink-rules
title: Multi-Source, Multi-Sink Rules
sidebar_label: Multi-Source, Multi-Sink Rules
---
### Multi-Source, Multi-Sink Rules

Multi-source multi-sink rules are used to track the flow of taint from multiple sources to multiple sinks. This can, for example, be useful if you want to track both the source types "SensitiveData" and "WorldReadableFileLocation" to an IO operation as displayed in the code below.

```java
File externalDir = context.getExternalFilesDir() // source WorldReadableFileLocation
String sensitiveData = getUserToken() // source SensitiveData

File outputFile = new File(externalDir, "file.txt");
try (FileOutputStream fos = new FileOutputStream(outputFile)) {
  fos.write(sensitiveData.getBytes()); // sink Argument(0) and Argument(1)
}
```

Such a rule can be defined as follows:

1. Define the sources as usual (see documentation above).
2. Define sinks on `FileOutputStream::write` as follows:

```json
{
  "model_generators": [
    {
      "find": "methods",
      "where": [ /* name = write */ ... ],
      "model": {
        "sink": [
          {
            "kind": "PartialExternalFileWrite",
            "partial_label": "outputStream",
            "port": "Argument(0)"
          },
          {
            "kind": "PartialExternalFileWrite",
            "partial_label": "outputBytes",
            "port": "Argument(1)"
          }
        ]
      }
    }
}
```

Here, there is **one sink** method catching **two sources** flowing into it, turning it into a **partial_sink**.

To define each source that flows into the `partial_sink`:

- `kind` must be the same
- `partial_label` must be unique - this will map to the source in the rule definition
- `port` must be unique - see [port documentation](../models/#access-path-format)
- There must be as many `partial_labels` are there are `multi_sources`

>**NOTE:** Multi-source/sink rules currently support exactly 2 sources/sinks only.

3. Define rules as follows:

```json
  {
    "name": "Experimental: Some name",
    "code": 9001,
    "description": "More description here.",
    "multi_sources": {
      "outputBytes": [
        "SensitiveData"
      ],
      "outputStream": [
        "WorldReadableFileLocation"
      ]
    },
    "partial_sinks": [
      "PartialExternalFileWrite"
    ]
}
```

Pay attention to how the labels and partial sink kinds match what is defined in the sinks above.
