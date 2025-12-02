# m10800_a3 - SHA-384 Hash Accelerator (Streaming Mode)

## Overview

**Algorithm**: SHA-384 (SHA-2 family)
- **Hash Output**: 384-bit (48 bytes)
- **Block Size**: 1024-bit (128 bytes)
- **Mode**: `a3` - Streaming input mode (3 cores default, processes raw input streams)
- **Word Size**: 64-bit operations

### Architecture
- **Multi-core**: Configurable 2-4 parallel hash cores (default: 3)
- **Platform**: Xilinx Alveo U280 FPGA
- **Interface**: AXI4-Stream with HLS dataflow optimization
- **Algorithm**: SHA-512 variant with truncated output
- **Rounds**: 80 compression rounds per block
- **Datapath**: 64-bit wide operations (higher resource usage than SHA-256)
- **Input Processing**: Handles raw message streams with automatic padding

## Quick Start

### 0. Setup Environment

Before building or running, set up Xilinx XRT environment:
```bash
export XILINX_XRT=/opt/xilinx/xrt
source $XILINX_XRT/setup.sh
```

## Configuration

Before building, configure the following variables in `Makefile`:

```makefile
PLATFORM_REPO_PATH ?= /home/hskim/platforms  # Change to your platform path
BUILD_DIR ?= build_1017_1455                  # Change to your build directory
```

## Recommended Configuration

**Recommended Build**: Kernel 3 (3SLR), 3 cores, 280MHz

Build the recommended configuration:
```bash
make build_k3_f280000000_c3
```

This configuration leverages 3 cores for optimal streaming throughput with SHA-384.

## Build Options

### Full Sweep (All Combinations)

Build all kernel/frequency/core combinations sequentially:
```bash
make all
```

Build all combinations in parallel (36 jobs):
```bash
make -j 36 all
```

### Specific Build

Build a specific configuration (Kernel 3, 3 cores, 300MHz):
```bash
make build_k3_f300000000_c3
```

## Build Configuration

- **Kernels**: 1, 2, 3, 4 (SLR configurations)
- **Cores**: 2, 3, 4 (parallel hash engines)
- **Frequencies**: 300MHz, 280MHz, 260MHz

Total combinations: 4 kernels × 3 cores × 3 frequencies = 36 builds

## Output

Build outputs are stored in:
```
$(BUILD_DIR)/k<KERNEL>/f<FREQ>_c<CORE>/
```

Example: `build_1017_1455/k3/f280000000_c3/m10800_a3.xclbin` (Recommended)

## Performance Characteristics

- **Throughput**: ~840 MH/s @ 280MHz with 3 cores
- **Latency**: ~80 rounds per hash
- **Resource Usage**: High (64-bit datapath, 80 rounds)
- **Use Cases**: TLS/SSL, digital certificates, secure communications

## Testing

Run testbench:
```bash
cd tb
make run
```

## Usage Examples

### Mode 0: Bruteforce Hash Calculation (No Salt)

Generate and hash candidates using bruteforce:
```bash
./host build_1017_1455/k3/f280000000_c3/m10800_a3.xclbin 3 616263 1000 0 0
```

**Arguments:**
- `build_1017_1455/k3/f280000000_c3/m10800_a3.xclbin` - Path to xclbin file
- `3` - Password length (0-7, meaning 1-8 characters). Here: 3 = 4 characters
- `616263` - MSB characters for each core in hex (3 cores = 6 hex chars). Here: 'abc'
- `1000` - Number of candidates to test (bruteforce iterations)
- `0` - Mode 0 (calculate all hashes)
- `0` - No salt

**Output:**
- Prints all generated hashes to console
- Saves results to `results.txt`

### Mode 0: Bruteforce with Salt

Generate hashes with salt prepended:
```bash
./host build_1017_1455/k3/f280000000_c3/m10800_a3.xclbin 3 414243 1000 0 1 Sa1lt123 0
```

**Arguments:**
- `3` - Length (4 characters)
- `414243` - MSB chars 'ABC' in hex
- `1000` - Number of candidates
- `0` - Mode 0 (calculate all)
- `1` - Use salt
- `Sa1lt123` - Salt string (max 8 bytes)
- `0` - Salt position (0=front, 1=back)

**Example:** Generates candidates like "ABCD", "ABCE", etc., and hashes "Sa1lt123ABCD", "Sa1lt123ABCE", etc.

### Mode 1: Bruteforce Search for Golden Hash

Search for a specific hash using bruteforce:
```bash
./host build_1017_1455/k3/f280000000_c3/m10800_a3.xclbin 3 414243 10000 1 1 Sa1lt123 0 38B060A751AC96384CD9327EB1B1E36A21FDB71114BE07434C0CC7BF63F6E1DA274EDEBFE76F65FBD51AD2F14898B95B
```

**Arguments:**
- `3` - Length (4 characters)
- `414243` - MSB chars 'ABC'
- `10000` - Search space size
- `1` - Mode 1 (search for golden hash)
- `1` - Use salt
- `Sa1lt123` - Salt string
- `0` - Salt position (front)
- `38B060A7...` - Target SHA-384 hash (96 hex characters)

**Output:**
- Prints matching candidate index if found
- Returns "NOT FOUND" if no match

### Understanding MSB Characters

The `msb_chars` parameter specifies the most significant byte for each core:
- **3 cores** = 6 hex characters (2 per core)
- Example: `414243` = 'A' (0x41), 'B' (0x42), 'C' (0x43)
- Each core generates candidates starting with its assigned character

### Understanding Length Parameter

Length parameter is 0-indexed:
- `0` = 1 character
- `1` = 2 characters
- `2` = 3 characters
- `3` = 4 characters
- ...
- `7` = 8 characters (maximum)

## Clean

Remove all build artifacts:
```bash
make clean
```

## Related Modules

- `m10800_a0`: SHA-384 with wordlist input mode
- `m01700_a3`: SHA-512 streaming variant (full 512-bit output)
