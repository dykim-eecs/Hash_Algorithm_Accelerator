# SHA Hash Accelerator Suite for Xilinx Alveo U250

A comprehensive collection of hardware-accelerated SHA hash implementations optimized for Xilinx Alveo U250 FPGAs using Vitis HLS.

## Overview

This repository contains high-performance FPGA implementations of the SHA-1 and SHA-2 family of cryptographic hash functions. Each implementation features multi-core parallel processing, configurable SLR distribution, and optimized dataflow architectures.

## Supported Algorithms

| Module | Algorithm | Hash Size | Block Size | Word Size | Rounds | Use Cases |
|--------|-----------|-----------|------------|-----------|--------|-----------|
| **m0100** | SHA-1 | 160-bit | 512-bit | 32-bit | 80 | Legacy systems, Git |
| **m01300** | SHA-224 | 224-bit | 512-bit | 32-bit | 64 | Truncated security |
| **m01400** | SHA-256 | 256-bit | 512-bit | 32-bit | 64 | Bitcoin, SSL/TLS |
| **m01700** | SHA-512 | 512-bit | 1024-bit | 64-bit | 80 | High security, TLS |
| **m10800** | SHA-384 | 384-bit | 1024-bit | 64-bit | 80 | Certificates, SSL |

## Architecture Variants

Each algorithm is available in two architectural variants:

### `a0` - Wordlist Mode
- **Input**: Pre-formatted wordlist (packed 512-bit messages)
- **Default Cores**: 2 cores
- **Use Case**: Password cracking, dictionary attacks
- **Optimization**: Minimal input processing overhead

### `a3` - Streaming Mode
- **Input**: Raw message streams
- **Default Cores**: 3 cores
- **Use Case**: General-purpose hashing, file integrity
- **Features**: Automatic padding and message scheduling

## Directory Structure

```
sha512/
├── README.md                    # This file
└── kernel/
    ├── m0100_a0/               # SHA-1 Wordlist
    ├── m0100_a3/               # SHA-1 Streaming
    ├── m01300_a0/              # SHA-224 Wordlist
    ├── m01300_a3/              # SHA-224 Streaming
    ├── m01400_a0/              # SHA-256 Wordlist
    ├── m01400_a3/              # SHA-256 Streaming
    ├── m01700_a0/              # SHA-512 Wordlist
    ├── m01700_a3/              # SHA-512 Streaming
    ├── m10800_a0/              # SHA-384 Wordlist
    └── m10800_a3/              # SHA-384 Streaming
```

Each kernel directory contains:
- `*.cpp`, `*.hpp` - HLS kernel source code
- `host.cpp` - Host application code
- `Makefile` - Build configuration
- `README.md` - Module-specific documentation
- `config/` - Vitis connectivity configurations
- `tb/` - Testbench files

## Quick Start

### Prerequisites

- Xilinx Vitis 2023.1 or later
- Xilinx Alveo U280 FPGA
- Platform files installed in `PLATFORM_REPO_PATH`

### Building a Kernel

1. Navigate to the desired kernel directory:
```bash
cd kernel/m01400_a0  # Example: SHA-256 Wordlist mode
```

2. Configure the Makefile (edit `PLATFORM_REPO_PATH` and `BUILD_DIR`)

3. Build the recommended configuration:
```bash
make build_k3_f280000000_c3
```

4. Or build all configurations:
```bash
make -j 36 all
```

### Running Tests

Each module includes a testbench:
```bash
cd kernel/m01400_a0/tb
make run
```

## Performance Summary

### Recommended Configurations

| Algorithm | Variant | Kernels | Cores | Frequency | Throughput* |
|-----------|---------|---------|-------|-----------|-------------|
| SHA-1 | a0 | 3 | 2 | 300MHz | ~600 MH/s |
| SHA-1 | a3 | 3 | 3 | 300MHz | ~900 MH/s |
| SHA-224 | a0/a3 | 3 | 3 | 280MHz | ~840 MH/s |
| SHA-256 | a0/a3 | 3 | 3 | 280MHz | ~840 MH/s |
| SHA-384 | a0/a3 | 3 | 3 | 280MHz | ~840 MH/s |
| SHA-512 | a0 | 3 | 2 | 300MHz | ~600 MH/s |
| SHA-512 | a3 | 3 | 3 | 280MHz | ~840 MH/s |

*Throughput estimates based on theoretical calculations. Actual performance may vary.

## Build Configuration Options

All kernels support the following build parameters:

- **Kernels (SLR Distribution)**: 1, 2, 3, 4
  - Kernel 3 (3 SLRs) recommended for best performance
  
- **Cores (Parallel Hash Engines)**: 2, 3, 4
  - More cores = higher throughput but increased resource usage
  - SHA-512/384: 2-3 cores recommended (64-bit datapath)
  - SHA-256/224/1: 3-4 cores recommended (32-bit datapath)
  
- **Frequencies**: 300MHz, 280MHz, 260MHz
  - 300MHz: Maximum performance (may not meet timing for all configs)
  - 280MHz: Recommended balance
  - 260MHz: Conservative timing closure

Total combinations per module: 36 builds (4 × 3 × 3)

## Resource Utilization

### Relative Resource Usage

- **SHA-1**: Low (32-bit, 80 rounds, simple operations)
- **SHA-224/256**: Moderate (32-bit, 64 rounds)
- **SHA-384/512**: High (64-bit, 80 rounds, complex operations)

### SLR Distribution

The U280 has 3 SLRs. Kernel 3 configuration distributes compute across all SLRs for optimal resource utilization and thermal distribution.

## Module Selection Guide

### Choose SHA-1 (m0100) if:
- Working with legacy systems
- Git repository verification
- Lower security requirements acceptable

### Choose SHA-224 (m01300) if:
- Need truncated SHA-256 output
- Bandwidth-constrained applications
- 224-bit security sufficient

### Choose SHA-256 (m01400) if:
- Bitcoin mining or cryptocurrency
- General-purpose security applications
- Industry-standard compatibility required

### Choose SHA-384 (m10800) if:
- TLS/SSL with 384-bit security
- Digital certificates
- Moderate security requirements

### Choose SHA-512 (m01700) if:
- Maximum security required
- High-security applications
- TLS 1.3, SSH, IPsec
- File integrity verification

### Choose `a0` (Wordlist) if:
- Password cracking
- Dictionary attacks
- Pre-formatted input available
- Minimal latency required

### Choose `a3` (Streaming) if:
- General-purpose hashing
- File hashing
- Variable-length messages
- Automatic padding needed

## Development

### Adding a New Configuration

1. Copy an existing module as a template
2. Modify the algorithm-specific constants
3. Update the Makefile with new kernel name
4. Create appropriate config files in `config/`
5. Update testbench for new hash size

### Debugging

- Use C-simulation: `vitis_hls -f run_csim.tcl`
- Check synthesis reports in `build_*/reports/`
- Monitor resource utilization in Vivado reports

## Known Limitations

- Maximum message length: Defined by `MAX_LEN` in each module (default: 8 words)
- Input alignment: Messages must be properly aligned for `a0` variants
- Platform-specific: Optimized for U280, may need adjustments for other platforms

## Contributing

When adding new modules or optimizations:
1. Follow existing naming conventions (`mXXXXX_aY`)
2. Include comprehensive README in module directory
3. Provide testbench with known-good test vectors
4. Document performance characteristics


## References

- NIST FIPS 180-4: Secure Hash Standard (SHS)
- Xilinx Vitis HLS User Guide (UG1399)
- Alveo U250 Data Sheet

---

**Last Updated**: 2025-11-24
**Vitis Version**: 2022.2+
**Platform**: Xilinx Alveo U250
