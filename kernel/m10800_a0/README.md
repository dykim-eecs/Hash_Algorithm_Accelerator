# m10800_a0 - SHA-384 Hash Accelerator (Wordlist Mode)

## Overview

**Algorithm**: SHA-384 (SHA-2 family)
- **Hash Output**: 384-bit (48 bytes)
- **Block Size**: 1024-bit (128 bytes)
- **Mode**: `a0` - Wordlist input mode (processes pre-formatted input messages)
- **Word Size**: 64-bit operations

### Architecture
- **Multi-core**: Configurable 2-4 parallel hash cores
- **Platform**: Xilinx Alveo U280 FPGA
- **Interface**: AXI4-Stream with HLS dataflow optimization
- **Rounds**: 80 compression rounds per block

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
cd kernel/m10800_a0
make build_k3_f280000000_c3
```

This configuration provides optimal throughput for SHA-384 with 3 parallel cores.

## Build Options

### Full Sweep (All Combinations)

Build all kernel/frequency/core combinations sequentially:
```bash
cd kernel/m10800_a0
make all
```

Build all combinations in parallel (36 jobs):
```bash
cd kernel/m10800_a0
make -j 36 all
```

### Specific Build

Build a specific configuration (Kernel 3, 3 cores, 300MHz):
```bash
cd kernel/m10800_a0
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
kernel/m10800_a0/$(BUILD_DIR)/k<KERNEL>/f<FREQ>_c<CORE>/
```

Example: `kernel/m10800_a0/build_1017_1455/k3/f280000000_c3/m10800_a0.xclbin` (Recommended)

## Performance Characteristics

- **Throughput**: ~840 MH/s @ 280MHz with 3 cores
- **Latency**: ~80 rounds per hash
- **Resource Usage**: High (64-bit datapath, 80 rounds)
- **Use Cases**: TLS/SSL, digital certificates, secure communications

## Compiling Host Program

From the sha512 root directory:
```bash
make m10800_a0
```

This creates the executable: `kernel/m10800_a0/host`

## Testing

Run testbench:
```bash
cd kernel/m10800_a0/tb
make run
```

## Usage Examples

**Note**: All commands should be run from the `sha512` root directory.

### Mode 0: Calculate All Hashes (No Salt)

Calculate SHA-384 hashes for all words in a wordlist:
```bash
kernel/m10800_a0/host \
  kernel/m10800_a0/build_1017_1455/k3/f280000000_c3/m10800_a0.xclbin \
  wordlist.txt 0 0
```

**Arguments:**
- `kernel/m10800_a0/host` - Host executable (from sha512 root)
- `kernel/m10800_a0/build_1017_1455/k3/f280000000_c3/m10800_a0.xclbin` - xclbin path
- `wordlist.txt` - Wordlist file (one word per line, max 32 bytes each)
- `0` - Mode 0 (calculate all hashes)
- `0` - No salt

**Output:**
- Prints all hashes to console
- Saves results to `results.txt`

### Mode 0: Calculate All Hashes (With Salt)

Calculate SHA-384 hashes with salt prepended to each word:
```bash
kernel/m10800_a0/host \
  kernel/m10800_a0/build_1017_1455/k3/f280000000_c3/m10800_a0.xclbin \
  wordlist.txt 0 1 Sa1lt123 0
```

**Arguments:**
- `wordlist.txt` - Wordlist file
- `0` - Mode 0 (calculate all hashes)
- `1` - Use salt
- `Sa1lt123` - Salt string (max 8 bytes)
- `0` - Salt position (0=front, 1=back)

**Example:** If wordlist contains "password", it will hash "Sa1lt123password"

### Mode 1: Search for Golden Hash (With Salt)

Search for a specific SHA-384 hash in the wordlist:
```bash
kernel/m10800_a0/host \
  kernel/m10800_a0/build_1017_1455/k3/f280000000_c3/m10800_a0.xclbin \
  wordlist.txt 1 1 Sa1lt123 0 \
  38B060A751AC96384CD9327EB1B1E36A21FDB71114BE07434C0CC7BF63F6E1DA274EDEBFE76F65FBD51AD2F14898B95B
```

**Arguments:**
- `wordlist.txt` - Wordlist file
- `1` - Mode 1 (search for golden hash)
- `1` - Use salt
- `Sa1lt123` - Salt string
- `0` - Salt position (front)
- `5E884898...` - Target SHA-384 hash (96 hex characters)

**Output:**
- Prints matching word index and the word itself if found
- Returns "NOT FOUND" if no match

### Creating a Wordlist

Create a simple wordlist file in the sha512 root directory:
```bash
cat > wordlist.txt << EOF
password
123456
admin
letmein
qwerty
EOF
```

**Wordlist Requirements:**
- One word per line
- Maximum 32 bytes per word
- UTF-8 encoding supported
- Empty lines are skipped

## Complete Example from sha512 Root

```bash
# 1. Setup environment
export XILINX_XRT=/opt/xilinx/xrt
source $XILINX_XRT/setup.sh

# 2. Create wordlist
cat > wordlist.txt << EOF
password
admin
test123
EOF

# 3. Compile host program
make m10800_a0

# 4. Run with salt
kernel/m10800_a0/host \
  kernel/m10800_a0/build_1017_1455/k3/f280000000_c3/m10800_a0.xclbin \
  wordlist.txt 0 1 Sa1lt123 0

# Results will be saved to results.txt in current directory
```

## Clean

Remove all build artifacts:
```bash
cd kernel/m10800_a0
make clean
```

## Related Modules

- `m01400_a3`: SHA-384 with streaming input mode (3 cores default)
- `m01300_a0`: SHA-224 variant (truncated 224-bit output)
