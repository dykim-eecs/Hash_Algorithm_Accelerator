# ============================================================================
# Compiler Configuration
# ============================================================================
CXX := g++
CXXFLAGS := -std=c++17 -pthread

# ============================================================================
# Directory Paths
# ============================================================================
SRC_DIR := src
INCLUDE_DIR := include

# Kernel directories
KERNEL_M0100_A0_DIR := kernel/m0100_a0
KERNEL_M0100_A3_DIR := kernel/m0100_a3
KERNEL_M01300_A0_DIR := kernel/m01300_a0
KERNEL_M01300_A3_DIR := kernel/m01300_a3
KERNEL_M01400_A0_DIR := kernel/m01400_a0
KERNEL_M01400_A3_DIR := kernel/m01400_a3
KERNEL_M01700_A0_DIR := kernel/m01700_a0
KERNEL_M01700_A3_DIR := kernel/m01700_a3
KERNEL_M10800_A0_DIR := kernel/m10800_a0
KERNEL_M10800_A3_DIR := kernel/m10800_a3

# ============================================================================
# Xilinx Toolchain Paths
# ============================================================================
# Modify these paths if your Xilinx installation is in a different location
VITIS_HLS_INCLUDE := /tools/Xilinx/Vitis_HLS/2022.2/include
XRT_INCLUDE := /opt/xilinx/xrt/include
XRT_LIB := /opt/xilinx/xrt/lib

# ============================================================================
# Include Paths
# ============================================================================
INCLUDES := -I$(INCLUDE_DIR)/utils \
            -I$(INCLUDE_DIR) \
            -I$(KERNEL_M0100_A0_DIR) \
            -I$(KERNEL_M0100_A3_DIR) \
            -I$(KERNEL_M01300_A0_DIR) \
            -I$(KERNEL_M01300_A3_DIR) \
            -I$(KERNEL_M01400_A0_DIR) \
            -I$(KERNEL_M01400_A3_DIR) \
            -I$(KERNEL_M01700_A0_DIR) \
            -I$(KERNEL_M01700_A3_DIR) \
            -I$(KERNEL_M10800_A0_DIR) \
            -I$(KERNEL_M10800_A3_DIR) \
            -I$(VITIS_HLS_INCLUDE) \
            -I$(XRT_INCLUDE)

# ============================================================================
# Library Paths and Linking
# ============================================================================
LIBDIRS := -L$(XRT_LIB)
LIBS := -lOpenCL -lxrt_core -lxrt_coreutil

# ============================================================================
# Source Files
# ============================================================================
XCL2_CPP := $(SRC_DIR)/xcl2.cpp

# Host source files
HOST_M0100_A0_CPP := $(KERNEL_M0100_A0_DIR)/host.cpp
HOST_M0100_A3_CPP := $(KERNEL_M0100_A3_DIR)/host.cpp
HOST_M01300_A0_CPP := $(KERNEL_M01300_A0_DIR)/host.cpp
HOST_M01300_A3_CPP := $(KERNEL_M01300_A3_DIR)/host.cpp
HOST_M01400_A0_CPP := $(KERNEL_M01400_A0_DIR)/host.cpp
HOST_M01400_A3_CPP := $(KERNEL_M01400_A3_DIR)/host.cpp
HOST_M01700_A0_CPP := $(KERNEL_M01700_A0_DIR)/host.cpp
HOST_M01700_A3_CPP := $(KERNEL_M01700_A3_DIR)/host.cpp
HOST_M10800_A0_CPP := $(KERNEL_M10800_A0_DIR)/host.cpp
HOST_M10800_A3_CPP := $(KERNEL_M10800_A3_DIR)/host.cpp

# ============================================================================
# Output Files
# ============================================================================
HOST_M0100_A0 := $(KERNEL_M0100_A0_DIR)/host
HOST_M0100_A3 := $(KERNEL_M0100_A3_DIR)/host
HOST_M01300_A0 := $(KERNEL_M01300_A0_DIR)/host
HOST_M01300_A3 := $(KERNEL_M01300_A3_DIR)/host
HOST_M01400_A0 := $(KERNEL_M01400_A0_DIR)/host
HOST_M01400_A3 := $(KERNEL_M01400_A3_DIR)/host
HOST_M01700_A0 := $(KERNEL_M01700_A0_DIR)/host
HOST_M01700_A3 := $(KERNEL_M01700_A3_DIR)/host
HOST_M10800_A0 := $(KERNEL_M10800_A0_DIR)/host
HOST_M10800_A3 := $(KERNEL_M10800_A3_DIR)/host

# ============================================================================
# Build Targets
# ============================================================================
.PHONY: all clean \
        m0100_a0 m0100_a3 \
        m01300_a0 m01300_a3 \
        m01400_a0 m01400_a3 \
        m01700_a0 m01700_a3 \
        m10800_a0 m10800_a3

all: m0100_a0 m0100_a3 \
     m01300_a0 m01300_a3 \
     m01400_a0 m01400_a3 \
     m01700_a0 m01700_a3 \
     m10800_a0 m10800_a3

# ============================================================================
# m0100_a0 (SHA-1 Wordlist) Host Compilation
# ============================================================================
m0100_a0: $(HOST_M0100_A0)

$(HOST_M0100_A0): $(HOST_M0100_A0_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m0100_a0 (SHA-1 Wordlist) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m0100_a0.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M0100_A0_CPP) $(XCL2_CPP) -o $(HOST_M0100_A0) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m0100_a3 (SHA-1 Streaming) Host Compilation
# ============================================================================
m0100_a3: $(HOST_M0100_A3)

$(HOST_M0100_A3): $(HOST_M0100_A3_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m0100_a3 (SHA-1 Streaming) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m0100_a3.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M0100_A3_CPP) $(XCL2_CPP) -o $(HOST_M0100_A3) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m01300_a0 (SHA-224 Wordlist) Host Compilation
# ============================================================================
m01300_a0: $(HOST_M01300_A0)

$(HOST_M01300_A0): $(HOST_M01300_A0_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m01300_a0 (SHA-224 Wordlist) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m01300_a0.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M01300_A0_CPP) $(XCL2_CPP) -o $(HOST_M01300_A0) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m01300_a3 (SHA-224 Streaming) Host Compilation
# ============================================================================
m01300_a3: $(HOST_M01300_A3)

$(HOST_M01300_A3): $(HOST_M01300_A3_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m01300_a3 (SHA-224 Streaming) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m01300_a3.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M01300_A3_CPP) $(XCL2_CPP) -o $(HOST_M01300_A3) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m01400_a0 (SHA-256 Wordlist) Host Compilation
# ============================================================================
m01400_a0: $(HOST_M01400_A0)

$(HOST_M01400_A0): $(HOST_M01400_A0_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m01400_a0 (SHA-256 Wordlist) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m01400_a0.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M01400_A0_CPP) $(XCL2_CPP) -o $(HOST_M01400_A0) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m01400_a3 (SHA-256 Streaming) Host Compilation
# ============================================================================
m01400_a3: $(HOST_M01400_A3)

$(HOST_M01400_A3): $(HOST_M01400_A3_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m01400_a3 (SHA-256 Streaming) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m01400_a3.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M01400_A3_CPP) $(XCL2_CPP) -o $(HOST_M01400_A3) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m01700_a0 (SHA-512 Wordlist) Host Compilation
# ============================================================================
m01700_a0: $(HOST_M01700_A0)

$(HOST_M01700_A0): $(HOST_M01700_A0_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m01700_a0 (SHA-512 Wordlist) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m01700_a0.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M01700_A0_CPP) $(XCL2_CPP) -o $(HOST_M01700_A0) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m01700_a3 (SHA-512 Streaming) Host Compilation
# ============================================================================
m01700_a3: $(HOST_M01700_A3)

$(HOST_M01700_A3): $(HOST_M01700_A3_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m01700_a3 (SHA-512 Streaming) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m01700_a3.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M01700_A3_CPP) $(XCL2_CPP) -o $(HOST_M01700_A3) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m10800_a0 (SHA-384 Wordlist) Host Compilation
# ============================================================================
m10800_a0: $(HOST_M10800_A0)

$(HOST_M10800_A0): $(HOST_M10800_A0_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m10800_a0 (SHA-384 Wordlist) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m10800_a0.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M10800_A0_CPP) $(XCL2_CPP) -o $(HOST_M10800_A0) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# m10800_a3 (SHA-384 Streaming) Host Compilation
# ============================================================================
m10800_a3: $(HOST_M10800_A3)

$(HOST_M10800_A3): $(HOST_M10800_A3_CPP) $(XCL2_CPP)
	@echo "========================================================"
	@echo "==> Compiling: m10800_a3 (SHA-384 Streaming) host"
	@echo "========================================================"
	@echo "Note: NUM_CORE is automatically read from m10800_a3.hpp"
	$(CXX) $(CXXFLAGS) $(HOST_M10800_A3_CPP) $(XCL2_CPP) -o $(HOST_M10800_A3) \
		$(INCLUDES) $(LIBDIRS) $(LIBS)

# ============================================================================
# Clean Target
# ============================================================================
clean:
	rm -f $(HOST_M0100_A0) $(HOST_M0100_A3) \
	      $(HOST_M01300_A0) $(HOST_M01300_A3) \
	      $(HOST_M01400_A0) $(HOST_M01400_A3) \
	      $(HOST_M01700_A0) $(HOST_M01700_A3) \
	      $(HOST_M10800_A0) $(HOST_M10800_A3)
	@echo "Clean completed - all host executables removed"

# ============================================================================
# Help Target
# ============================================================================
help:
	@echo "========================================================"
	@echo "SHA Hash Accelerator Host Compilation Makefile"
	@echo "========================================================"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build all host programs (10 modules)"
	@echo ""
	@echo "SHA-1 (160-bit):"
	@echo "  m0100_a0     - SHA-1 Wordlist mode"
	@echo "  m0100_a3     - SHA-1 Streaming mode"
	@echo ""
	@echo "SHA-224 (224-bit):"
	@echo "  m01300_a0    - SHA-224 Wordlist mode"
	@echo "  m01300_a3    - SHA-224 Streaming mode"
	@echo ""
	@echo "SHA-256 (256-bit):"
	@echo "  m01400_a0    - SHA-256 Wordlist mode"
	@echo "  m01400_a3    - SHA-256 Streaming mode"
	@echo ""
	@echo "SHA-512 (512-bit):"
	@echo "  m01700_a0    - SHA-512 Wordlist mode"
	@echo "  m01700_a3    - SHA-512 Streaming mode"
	@echo ""
	@echo "SHA-384 (384-bit):"
	@echo "  m10800_a0    - SHA-384 Wordlist mode"
	@echo "  m10800_a3    - SHA-384 Streaming mode"
	@echo ""
	@echo "  clean        - Remove all compiled host executables"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make all              # Build all host programs"
	@echo "  make m01700_a0        # Build only SHA-512 wordlist host"
	@echo "  make m01400_a3        # Build only SHA-256 streaming host"
	@echo "  make clean            # Clean all builds"
	@echo "========================================================"
