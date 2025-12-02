#--------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Kim, Doh Yon (Sungkyunkwan University)
#
# This source code is provided for portfolio review purposes only.
# Unauthorized copying, modification, or distribution is strictly prohibited.
//
// All Rights Reserved.
//--------------------------------------------------------------------------------------------------
# ============================================================================
# Build Configuration
# ============================================================================
TARGET ?= hw
PLATFORM ?= xilinx_u250_gen3x16_xdma_4_1_202210_1
PLATFORM_REPO_PATH ?= /opt/xilinx/platforms
PFM := $(PLATFORM_REPO_PATH)/$(PLATFORM)/$(PLATFORM).xpfm
BUILD_DIR ?= build_1017_1455

# ============================================================================
# Kernel Configuration
# ============================================================================
KERNEL_NAME := m10800_a3
KERNELS := 1 2 3 4
NUM_CORE_LIST := 2 3 4
FREQ_HZ_LIST := 300000000 280000000 260000000

# ============================================================================
# Build Parallelism
# ============================================================================
PARALLEL = 16

# ============================================================================
# Build Targets
# ============================================================================
.PHONY: all clean

all: $(foreach K,$(KERNELS),$(foreach F,$(FREQ_HZ_LIST),$(foreach C,$(NUM_CORE_LIST),build_k$(K)_f$(F)_c$(C))))

# ============================================================================
# Kernel Build Rule
# ============================================================================
define MAKE_KERNEL_RULE
build_k$(1)_f$(2)_c$(3):
	@echo "========================================================"
	@echo "==> Building Kernels=$(1), FREQ=$(2)Hz, CORE=$(3)"
	@echo "========================================================"

	@mkdir -p $(BUILD_DIR)/k$(1)/f$(2)_c$(3)

	@# Compile to .xo (continue even if fail)
	- v++ -c -k $(KERNEL_NAME) \
	--hls.jobs $(PARALLEL) \
	--vivado.impl.jobs $(PARALLEL) \
	--vivado.synth.jobs $(PARALLEL) \
	-o $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/$(KERNEL_NAME).xo \
	--platform $(PFM) \
	--target $(TARGET) \
	--hls.clock $(2):$(KERNEL_NAME) \
	--config config/$(KERNEL_NAME)_$(1).ini \
	--save-temps \
	--log_dir $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/compile/log \
	--temp_dir $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/compile/temp \
	--report_dir $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/compile/report \
	-Ikernel \
	-I. \
	-DNUM_CORE=$(3) \
	./$(KERNEL_NAME).cpp

	@# Link to .xclbin (continue even if fail)
	- v++ -l \
	--hls.jobs $(PARALLEL) \
	--vivado.impl.jobs $(PARALLEL) \
	--vivado.synth.jobs $(PARALLEL) \
	-o $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/$(KERNEL_NAME).xclbin \
	$(BUILD_DIR)/k$(1)/f$(2)_c$(3)/$(KERNEL_NAME).xo \
	--platform $(PFM) \
	--target $(TARGET) \
	--kernel_frequency $(shell echo $(2) | sed 's/000000$$//') \
	--config config/$(KERNEL_NAME)_$(1).ini \
	--save-temps \
	--log_dir $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/link/log \
	--temp_dir $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/link/temp \
	--report_dir $(BUILD_DIR)/k$(1)/f$(2)_c$(3)/link/report
endef

# ============================================================================
# Expand Rules for Each Kernel/Frequency/Core Combination
# ============================================================================
$(foreach K,$(KERNELS),$(foreach F,$(FREQ_HZ_LIST),$(foreach C,$(NUM_CORE_LIST),$(eval $(call MAKE_KERNEL_RULE,$(K),$(F),$(C))))))

# ============================================================================
# Clean Target
# ============================================================================
clean:
	rm -rf build tmp* *~ .Xil *.log *.jou *.xml *.json *.html *.ipcache
