# Test Data for t35_tool

This directory contains test files for validating the t35_tool implementation.

## Test Files Overview

### GenericJsonSource Test Files
- `test_manifest.json` - JSON manifest file with metadata items
- `meta_001.bin` - Binary metadata payload for frames 0-23 (16 bytes)
- `meta_002.bin` - Binary metadata payload for frames 24-47 (16 bytes)
- `meta_003.bin` - Binary metadata payload for frames 48-71 (16 bytes)

### SMPTE Folder Source Test Files
- `ExampleJson/` - Directory containing SMPTE ST 2094-50 metadata in JSON format
  - `ST2094-50_ReferenceWhiteOnly_metadataItems.json` - Reference white level only
  - `ST2094-50_DefaultToneMapping_metadataItems.json` - Default tone mapping parameters
  - `ST2094-50_CustomToneMapping_metadataItems.json` - Custom tone mapping parameters
- `ST2094-50_LightDetector.mov` - Sample HEVC video file for testing injection
- `test_t35_tool.sh` - Automated test script for all injection/extraction methods

## JSON Schemas

### GenericJsonSource Schema

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

### SMPTE Folder Source Schema

SMPTE folder source uses individual JSON files per metadata item with SMPTE ST 2094-50 specific fields:

```json
{
  "hdrReferenceWhite": 203,
  "frame_start": 1,
  "frame_duration": 1
}
```

## Testing

### Test GenericJsonSource

```bash
cd mybuild
../bin/t35_tool inject input.mp4 output.mp4 \
  --source generic-json:../IsoLib/t35_tool/test_data/test_manifest.json \
  --method mebx-me4c \
  --t35-prefix B500900001:SMPTE-ST2094-50
```

Expected output:
- Validation passes
- 3 metadata items loaded
- Each item has frame_start, frame_duration, and payload_size=16

### Test SMPTE Folder Source

Run the automated test script from the test_data directory:

```bash
cd IsoLib/t35_tool/test_data
../../bin/t35_tool inject ST2094-50_LightDetector.mov output.mov \
  --source smpte-folder:./ExampleJson \
  --method mebx-me4c \
  --t35-prefix B500900001:SMPTE-ST2094-50
```

Or use the comprehensive test script:

```bash
cd IsoLib/t35_tool/test_data
./test_t35_tool.sh
```

The test script validates all injection methods:
- **ME4C**: MEBX track with me4c namespace (default)
- **Dedicated Track**: Dedicated IT35 metadata track
- **Sample Group**: Video track sample groups (sgpd/sbgp)

Each test performs injection followed by extraction using auto-detection.

## Binary File Contents

Each binary file contains 16 bytes of sequential test data:
- `meta_001.bin`: 0x00-0x0F
- `meta_002.bin`: 0x10-0x1F
- `meta_003.bin`: 0x20-0x2F
