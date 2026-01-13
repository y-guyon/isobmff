# Test Data for GenericJsonSource

This directory contains test files for validating the GenericJsonSource implementation.

## Files

- `test_manifest.json` - JSON manifest file with metadata items
- `meta_001.bin` - Binary metadata payload for frames 0-23 (16 bytes)
- `meta_002.bin` - Binary metadata payload for frames 24-47 (16 bytes)
- `meta_003.bin` - Binary metadata payload for frames 48-71 (16 bytes)

## JSON Schema

```json
{
  "t35_prefix": "B500900001",
  "items": [
    {
      "frame_start": 0,
      "frame_duration": 24,
      "binary_file": "meta_001.bin"
    }
  ]
}
```

## Testing

Test the GenericJsonSource with the CLI:

```bash
cd mybuild
../bin/t35_tool --verbose 3 inject input.mp4 output.mp4 \
  --source generic-json:../IsoLib/t35_tool/test_data/test_manifest.json \
  --method mebx-it35 \
  --t35-prefix B500900001:SMPTE-ST2094-50
```

Expected output should show:
- Validation passes
- 3 metadata items loaded
- Each item has frame_start, frame_duration, and payload_size=16

## Binary File Contents

Each binary file contains 16 bytes of sequential test data:
- `meta_001.bin`: 0x00-0x0F
- `meta_002.bin`: 0x10-0x1F
- `meta_003.bin`: 0x20-0x2F
