//--------------------------------------------------------------------------------------------------
// Copyright (c) 2025 Kim, Doh Yon (Sungkyunkwan University)
//
// This source code is provided for portfolio review purposes only.
// Unauthorized copying, modification, or distribution is strictly prohibited.
//
// All Rights Reserved.
//--------------------------------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <xcl2.hpp>
#include <ap_int.h>
#include <CL/cl_ext_xilinx.h>
#include "m01300_a3.hpp"
// ============================================================================
// Kernel Interface Constants
// ============================================================================
#define NUM_CUS 3
#define WORD_SIZE 28 // SHA-224 digest = 28 bytes
#define MSG_SIZE 32 // 256 bits = 32 bytes per message
#define SALT_MAX_SIZE 8
#define GOLDEN_HEX_SIZE 56 // 512 bits = 128 hex characters
// NUM_CORE and MAX_LEN are now defined in m01300_a3.hpp
// ============================================================================
// Utility Functions
// ============================================================================
/**
 * Convert ap_uint<224> hash to uppercase hexadecimal string
 */
static inline std::string to_sha224_hex(const ap_uint<224>& hash) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 28; ++i) {
        unsigned char byte = static_cast<unsigned char>(
            hash.range(223 - i * 8, 223 - i * 8 - 7).to_uint());
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}
/**
 * Convert salt string to uint64_t (8 bytes, MSB-first)
 */
uint64_t string_to_salt(const std::string& salt_str) {
    uint64_t salt = 0;
    size_t len = std::min(salt_str.size(), size_t(SALT_MAX_SIZE));
   
    for (size_t i = 0; i < len; i++) {
        salt |= (static_cast<uint64_t>(salt_str[i]) << (56 - i * 8));
    }
   
    return salt;
}
/**
 * Parse hexadecimal string to ap_uint<224>
 * Input must be exactly 56 hex characters (224 bits)
 */
ap_uint<224> parse_hex_to_512(const std::string& hex_str) {
    if (hex_str.size() != GOLDEN_HEX_SIZE) {
        throw std::runtime_error(
            "Hex string must be " + std::to_string(GOLDEN_HEX_SIZE) +
            " characters (224 bits)");
    }
   
    ap_uint<224> result;
    for (int i = 0; i < 28; ++i) {
        std::string byte_str = hex_str.substr(i * 2, 2);
        uint8_t byte_val = static_cast<uint8_t>(
            std::stoul(byte_str, nullptr, 16));
        // MSB first: byte 0 is at bits 511:504
        result.range(223 - i * 8, 216 - i * 8) = byte_val;
    }
    return result;
}
static inline ap_uint<224> extract_sha224(const ap_uint<512>& word) {
    return word.range(223, 0);
}

static inline ap_uint<512> pack_sha224(const ap_uint<224>& digest) {
    ap_uint<512> word = 0;
    word.range(223, 0) = digest;
    return word;
}
/**
 * Generate candidate string from index and parameters
 * This is a helper to reconstruct what was tested (for display purposes)
 */
std::string generate_candidate_string(uint64_t index, ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, int core_id) {
    // Note: This is a simplified reconstruction for display
    // The actual generation happens in the kernel
    std::ostringstream oss;
    
    // Extract MSB character for this core
    uint8_t msb_char = static_cast<uint8_t>(msb_chars.range(8 * (core_id + 1) - 1, 8 * core_id).to_uint());
    if (msb_char != 0) {
        oss << static_cast<char>(msb_char);
    }
    
    // For the rest, we can't fully reconstruct without the counter state
    // So we'll just show a placeholder
    oss << "[...]";
    
    return oss.str();
}
// ============================================================================
// Command-line Argument Parsing
// ============================================================================
void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " <xclbin> <len> <msb_chars> <size> <mode> <use_salt> [salt] [position] [golden_hex]\n"
              << " xclbin: Path to .xclbin file\n"
              << " len: Password length (0-7, meaning 1-8 characters)\n"
              << " msb_chars: MSB characters for each core (hex string, " << (NUM_CORE * 2) << " hex chars)\n"
              << " size: Number of candidates to test (bruteforce iterations)\n"
              << " mode: 0 = calculate all hashes, 1 = search for golden hash\n"
              << " use_salt: 0 = no salt, 1 = use salt\n"
              << " salt: Salt string (max " << SALT_MAX_SIZE
              << " bytes, required if use_salt=1)\n"
              << " position: 0 = front, 1 = back (optional, default=0)\n"
              << " golden_hex: SHA224 hex string (" << GOLDEN_HEX_SIZE
              << " chars, required if mode=1)\n"
              << "\n"
              << "Example (Mode 0 - calculate all):\n"
              << " " << program_name << " kernel.xclbin 3 41424344 1000 0 1 Sa1lt123 0\n"
              << " " << program_name << " kernel.xclbin 2 61626364 500 0 0\n"
              << "\n"
              << "Example (Mode 1 - search):\n"
              << " " << program_name << " kernel.xclbin 3 41424344 10000 1 1 Sa1lt123 0 "
              << "44BD950E1B30C9BE510DDA81A9E58752A47734DBA8933135D02F7FCC6CE11AD34C8D0FFD17B80C41035F76B22D892C96B345A45576A76A66901619C20D2D390B\n";
}
struct ProgramArgs {
    std::string xclbin_path;
    ap_uint<6> len;
    ap_uint<8*NUM_CORE> msb_chars;
    ap_uint<48> size;
    int mode; // 0 = calculate all, 1 = search
    bool use_salt;
    std::string salt_str;
    uint32_t salt_position; // 0 = front, 1 = back
    std::string golden_hex;
};
int parse_arguments(int argc, char** argv, ProgramArgs& args) {
    if (argc < 7) {
        print_usage(argv[0]);
        return 1;
    }
   
    args.xclbin_path = argv[1];
    args.len = static_cast<ap_uint<6>>(std::stoi(argv[2]));
    if (args.len.to_uint() > MAX_LEN) {
        std::cerr << "Error: len must be 0-" << MAX_LEN << "\n";
        return 1;
    }
    
    // Parse msb_chars as hex string
    std::string msb_hex = argv[3];
    if (msb_hex.size() != NUM_CORE * 2) {
        std::cerr << "Error: msb_chars must be " << (NUM_CORE * 2) << " hex characters\n";
        return 1;
    }
    ap_uint<8*NUM_CORE> msb_chars_val = 0;
    for (int i = 0; i < NUM_CORE; ++i) {
        std::string byte_str = msb_hex.substr(i * 2, 2);
        uint8_t byte_val = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        msb_chars_val.range(8 * (i + 1) - 1, 8 * i) = byte_val;
    }
    args.msb_chars = msb_chars_val;
    
    args.size = static_cast<ap_uint<48>>(std::stoull(argv[4]));
    args.mode = std::stoi(argv[5]);
    args.use_salt = (std::stoi(argv[6]) != 0);
   
    if (args.mode != 0 && args.mode != 1) {
        std::cerr << "Error: mode must be 0 or 1\n";
        return 1;
    }
   
    // Initialize optional arguments
    args.salt_str = "";
    args.salt_position = 0;
    args.golden_hex = "";
   
    // Parse arguments based on use_salt and mode
    int arg_idx = 7;
   
    if (args.use_salt) {
        // Salt string required
        if (argc <= arg_idx) {
            std::cerr << "Error: salt string required when use_salt=1\n";
            return 1;
        }
        args.salt_str = argv[arg_idx++];
        if (args.salt_str.size() > SALT_MAX_SIZE) {
            std::cerr << "Warning: salt longer than " << SALT_MAX_SIZE
                      << " bytes, truncating\n";
            args.salt_str = args.salt_str.substr(0, SALT_MAX_SIZE);
        }
       
        // Optional position
        if (argc > arg_idx) {
            std::string next_arg = argv[arg_idx];
            // Check if next arg is position (single digit) or golden_hex (128 chars)
            if (args.mode == 0 || next_arg.size() == 1) {
                args.salt_position = std::stoi(next_arg);
                arg_idx++;
            }
        }
    }
   
    // Golden hash required for mode 1
    if (args.mode == 1) {
        if (argc <= arg_idx) {
            std::cerr << "Error: golden_hex required when mode=1\n";
            return 1;
        }
        args.golden_hex = argv[arg_idx];
        if (args.golden_hex.size() != GOLDEN_HEX_SIZE) {
            std::cerr << "Error: golden_hex must be " << GOLDEN_HEX_SIZE
                      << " characters (512-bit hex)\n";
            return 1;
        }
    }
   
    return 0;
}
// ============================================================================
// OpenCL Device Initialization
// ============================================================================
cl::Context create_context(cl::Device& device, cl_int& err) {
    auto devices = xcl::get_xil_devices();
    if (devices.empty()) {
        throw std::runtime_error("No Xilinx devices found");
    }
    device = devices[0];
   
    cl::Context context(device, nullptr, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create context: " + std::to_string(err));
    }
   
    return context;
}
cl::CommandQueue create_command_queue(cl::Context& context,
                                       cl::Device& device,
                                       cl_int& err) {
    cl::CommandQueue q(context, device,
                       CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE |
                       CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create command queue: " +
                                std::to_string(err));
    }
    return q;
}
cl::Kernel create_kernel(cl::Context& context,
                         cl::Device& device,
                         const std::string& xclbin_path,
                         const std::string& kernel_name,
                         cl_int& err) {
    std::cout << "Loading xclbin: " << xclbin_path << std::endl;
    cl::Program::Binaries bins = xcl::import_binary_file(xclbin_path);
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create program from binary: " +
                                std::to_string(err));
    }
   
    std::cout << "Creating kernel: " << kernel_name << std::endl;
    cl::Kernel kernel = cl::Kernel(program, kernel_name.c_str(), &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Failed to create kernel " + kernel_name +
                                ": " + std::to_string(err));
    }
   
    return kernel;
}
// ============================================================================
// Buffer Management
// ============================================================================
cl::Buffer create_buffer(cl::Context& context,
                         cl_mem_flags flags,
                         size_t size,
                         cl_int& err,
                         int bank) {
    cl_mem_ext_ptr_t ext;
    ext.flags = bank | XCL_MEM_TOPOLOGY;
    ext.obj = nullptr;
    ext.param = 0;
   
    cl::Buffer buffer(context, flags | CL_MEM_EXT_PTR_XILINX, size, &ext, &err);
    if (err != CL_SUCCESS) {
        throw std::runtime_error("Buffer allocation error: " +
                                std::to_string(err));
    }
   
    return buffer;
}
// ============================================================================
// Result Processing
// ============================================================================
void print_results_mode0(const std::vector<std::vector<ap_uint<224>>>& output_datas,
                         const std::vector<size_t>& part_starts,
                         size_t total_candidates) {
    const size_t LARGE_DATASET_THRESHOLD = 10000;
    const size_t PREVIEW_COUNT = 5;
   
    std::cout << "\n=== Results ===" << std::endl;
   
    if (total_candidates <= LARGE_DATASET_THRESHOLD) {
        // Small dataset: print all results
        std::cout << "Printing all " << total_candidates << " results..." << std::endl;
        for (size_t cu = 0; cu < NUM_CUS; ++cu) {
            const auto& part_hashes = output_datas[cu];
            for (size_t j = 0; j < part_hashes.size(); ++j) {
                size_t global_i = part_starts[cu] + j;
                std::cout << "[" << std::setw(6) << global_i << "] "
                          << " -> 0x" << to_sha224_hex(part_hashes[j]) << std::endl;
            }
        }
    } else {
        // Large dataset: print preview only
        std::cout << "Large dataset (" << total_candidates << " results). Showing preview only..." << std::endl;
        std::cout << "First " << PREVIEW_COUNT << " results:" << std::endl;
        size_t printed = 0;
        for (size_t cu = 0; cu < NUM_CUS && printed < PREVIEW_COUNT; ++cu) {
            const auto& part_hashes = output_datas[cu];
            for (size_t j = 0; j < part_hashes.size() && printed < PREVIEW_COUNT; ++j) {
                size_t global_i = part_starts[cu] + j;
                std::cout << "[" << std::setw(6) << global_i << "] "
                          << " -> 0x" << to_sha224_hex(part_hashes[j]) << std::endl;
                ++printed;
            }
        }
        if (total_candidates > PREVIEW_COUNT * 2) {
            std::cout << "..." << std::endl;
            std::cout << "Last " << PREVIEW_COUNT << " results:" << std::endl;
            printed = 0;
            for (int cu = NUM_CUS - 1; cu >= 0 && printed < PREVIEW_COUNT; --cu) {
                const auto& part_hashes = output_datas[cu];
                for (int j = part_hashes.size() - 1; j >= 0 && printed < PREVIEW_COUNT; --j) {
                    size_t global_i = part_starts[cu] + j;
                    std::cout << "[" << std::setw(6) << global_i << "] "
                              << " -> 0x" << to_sha224_hex(part_hashes[j]) << std::endl;
                    ++printed;
                }
            }
        }
        std::cout << "All results will be saved to results.txt" << std::endl;
    }
    std::cout.flush(); // Ensure output is flushed
}
void save_results_to_file(const std::vector<std::vector<ap_uint<224>>>& output_datas,
                          const std::string& filename) {
    const size_t PROGRESS_INTERVAL = 100000; // Show progress every 100k entries
   
    std::ofstream out_file(filename);
    if (!out_file.is_open()) {
        std::cerr << "Warning: Failed to open output file: " << filename << std::endl;
        return;
    }
   
    size_t total_candidates = 0;
    for (const auto& part : output_datas) {
        total_candidates += part.size();
    }
   
    std::cout << "Saving " << total_candidates << " results to " << filename << "..." << std::endl;
    std::cout.flush();
   
    size_t processed = 0;
    for (size_t cu = 0; cu < NUM_CUS; ++cu) {
        const auto& part_hashes = output_datas[cu];
        for (size_t j = 0; j < part_hashes.size(); ++j) {
            out_file << "candidate_" << (processed) << " 0x" << to_sha224_hex(part_hashes[j]) << "\n";
           
            ++processed;
            // Show progress for large datasets
            if (processed % PROGRESS_INTERVAL == 0 || processed == total_candidates) {
                double progress = 100.0 * processed / total_candidates;
                std::cout << "\rProgress: " << std::fixed << std::setprecision(1)
                          << progress << "% (" << processed << "/" << total_candidates << ")"
                          << std::flush;
            }
        }
    }
   
    out_file.close();
    std::cout << "\nAll results saved to: " << filename << std::endl;
    std::cout.flush();
}
void print_results_mode1(ap_uint<48> match_index,
                         ap_uint<224> golden) {
    std::cout << "\n=== Search Result ===" << std::endl;
   
    if (match_index == ap_uint<48>(-1)) {
        std::cout << "NOT FOUND: No matching hash found" << std::endl;
    } else {
        uint64_t idx = match_index.to_uint64();
        std::cout << "FOUND at index: " << idx << std::endl;
        std::cout << "Golden hash: 0x" << to_sha224_hex(golden) << std::endl;
    }
}
void print_performance_metrics(int mode,
                                size_t num_candidates,
                                double kernel_time_us) {
    std::cout << "\n=== Performance ===" << std::endl;
   
    if (mode == 0) {
        double hash_rate = static_cast<double>(num_candidates) /
                          (kernel_time_us / 1e6) / 1e6; // MH/s
        std::cout << "Total hashes: " << num_candidates << std::endl;
        std::cout << "Kernel time (us): " << std::fixed << std::setprecision(2)
                  << kernel_time_us << std::endl;
        std::cout << "Hash rate (MH/s): " << hash_rate << std::endl;
    } else {
        double search_rate = static_cast<double>(num_candidates) /
                            (kernel_time_us / 1e6) / 1e6; // MH/s
        std::cout << "Searched through: " << num_candidates << " candidates" << std::endl;
        std::cout << "Kernel time (us): " << std::fixed << std::setprecision(2)
                  << kernel_time_us << std::endl;
        std::cout << "Search rate (MH/s): " << search_rate << std::endl;
    }
}
// ============================================================================
// Main Function
// ============================================================================
int main(int argc, char** argv) {
    ProgramArgs args;
    if (parse_arguments(argc, argv, args) != 0) {
        return 1;
    }
   
    cl_int err = CL_SUCCESS;
   
    try {
        // ====================================================================
        // Step 1: Prepare parameters
        // ====================================================================
        std::cout << "=== Bruteforce Parameters ===" << std::endl;
        std::cout << "Length: " << args.len.to_uint() << " (meaning " << (args.len.to_uint() + 1) << " characters)" << std::endl;
        std::cout << "MSB chars: 0x";
        for (int i = 0; i < NUM_CORE; ++i) {
            uint8_t c = static_cast<uint8_t>(args.msb_chars.range(8 * (i + 1) - 1, 8 * i).to_uint());
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
        std::cout << std::dec << std::endl;
        std::cout << "Size: " << args.size.to_uint64() << " candidates" << std::endl;
       
        // ====================================================================
        // Step 2: Prepare salt and golden hash
        // ====================================================================
        ap_uint<64> salt = 0;
        if (args.use_salt) {
            salt = string_to_salt(args.salt_str);
            std::cout << "Salt: " << args.salt_str
                      << " (0x" << std::hex << salt.to_uint64() << std::dec << ")"
                      << std::endl;
            std::cout << "Salt position: "
                      << (args.salt_position == 0 ? "FRONT" : "BACK") << std::endl;
        } else {
            std::cout << "Salt: NOT USED" << std::endl;
        }
       
        ap_uint<224> golden = 0;
        if (args.mode == 1) {
            golden = parse_hex_to_512(args.golden_hex);
            std::cout << "Golden hash: 0x" << to_sha224_hex(golden) << std::endl;
        }
        ap_uint<512> golden_word = pack_sha224(golden);
       
        // ====================================================================
        // Step 3: Initialize OpenCL device
        // ====================================================================
        std::cout << "\n=== Initializing device ===" << std::endl;
        cl::Device device;
        cl::Context context = create_context(device, err);
        cl::CommandQueue q = create_command_queue(context, device, err);
       
        // ====================================================================
        // Step 4: Load kernel
        // ====================================================================
        std::vector<std::string> cu_instance_names = {"m01300_a3_1", "m01300_a3_2", "m01300_a3_3"};
        std::vector<int> banks = {0, 2, 3};
        std::vector<cl::Kernel> kernels(NUM_CUS);
        for (int i = 0; i < NUM_CUS; ++i) {
            std::string full_kernel_name = "m01300_a3:{" + cu_instance_names[i] + "}";
            kernels[i] = create_kernel(context, device, args.xclbin_path, full_kernel_name, err);
        }
       
        // ====================================================================
        // Step 5: Allocate buffers
        // ====================================================================
        std::cout << "\n=== Allocating buffers ===" << std::endl;
       
        std::vector<cl::Buffer> outputBufs(NUM_CUS);
        std::vector<cl::Buffer> matchIndexBufs(NUM_CUS);
        std::vector<ap_uint<48>> sizes(NUM_CUS);
        std::vector<size_t> num_output_words_list(NUM_CUS);
        size_t total_output_size = 0;
       
        // Distribute size across CUs
        size_t total_size = args.size.to_uint64();
        size_t size_per_cu = total_size / NUM_CUS;
        size_t remainder = total_size % NUM_CUS;
       
        std::vector<size_t> part_starts(NUM_CUS, 0);
        std::vector<size_t> part_sizes(NUM_CUS);
        for (int i = 0; i < NUM_CUS; ++i) {
            part_sizes[i] = size_per_cu + (i < remainder ? 1 : 0);
            if (i > 0) {
                part_starts[i] = part_starts[i - 1] + part_sizes[i - 1];
            }
            sizes[i] = ap_uint<48>(part_sizes[i]);
            num_output_words_list[i] = (args.mode == 0) ? part_sizes[i] * NUM_CORE : 1;
           
            size_t output_size_i = num_output_words_list[i] * sizeof(ap_uint<512>);
            size_t match_index_size = sizeof(ap_uint<48>);
           
            outputBufs[i] = create_buffer(context, CL_MEM_WRITE_ONLY, output_size_i, err, banks[i]);
            matchIndexBufs[i] = create_buffer(context, CL_MEM_WRITE_ONLY, match_index_size, err, banks[i]);
           
            total_output_size += output_size_i;
        }
       
        std::cout << "Output buffer: " << total_output_size << " bytes (" << (args.mode == 0 ? total_size * NUM_CORE : NUM_CUS) << " words)" << std::endl;
       
        // ====================================================================
        // Step 6: Initialize match_index buffers
        // ====================================================================
        std::cout << "\n=== Initializing buffers ===" << std::endl;
        for (int i = 0; i < NUM_CUS; ++i) {
            ap_uint<48> match_index_init = ap_uint<48>(-1);
            q.enqueueWriteBuffer(matchIndexBufs[i], CL_FALSE, 0, sizeof(ap_uint<48>), &match_index_init, nullptr, nullptr);
        }
        q.finish();
       
        // ====================================================================
        // Step 7: Set kernel arguments
        // ====================================================================
        std::cout << "\n=== Setting kernel arguments ===" << std::endl;
        for (int i = 0; i < NUM_CUS; ++i) {
            kernels[i].setArg(0, outputBufs[i]); // output
            kernels[i].setArg(1, matchIndexBufs[i]); // match_index
            kernels[i].setArg(2, args.len); // len
            kernels[i].setArg(3, args.msb_chars); // msb_chars
            kernels[i].setArg(4, sizes[i]); // size
            kernels[i].setArg(5, ap_uint<1>(args.mode)); // mode
            kernels[i].setArg(6, golden_word); // golden (padded)
            kernels[i].setArg(7, salt); // salt
            kernels[i].setArg(8, ap_uint<1>(args.salt_position)); // salt_position
            kernels[i].setArg(9, ap_uint<1>(args.use_salt)); // use_salt
        }
       
        std::cout << "Size: " << total_size << " candidates per CU" << std::endl;
        std::cout << "Mode: " << args.mode
                  << (args.mode == 0 ? " (calculate all hashes)"
                      : " (search for golden hash)") << std::endl;
       
        // ====================================================================
        // Step 8: Execute kernel
        // ====================================================================
        std::cout << "\n=== Executing kernel ===" << std::endl;
        std::vector<cl::Event> kernel_events(NUM_CUS);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_CUS; ++i) {
            q.enqueueTask(kernels[i], nullptr, &kernel_events[i]);
        }
        q.finish();
        auto end = std::chrono::high_resolution_clock::now();
       
        std::vector<double> kernel_times_us(NUM_CUS);
        for (int i = 0; i < NUM_CUS; ++i) {
            cl_ulong start_time = 0, end_time = 0;
            kernel_events[i].getProfilingInfo(CL_PROFILING_COMMAND_START, &start_time);
            kernel_events[i].getProfilingInfo(CL_PROFILING_COMMAND_END, &end_time);
            kernel_times_us[i] = (end_time - start_time) / 1000.0;
        }
        double max_kernel_time_us = *std::max_element(kernel_times_us.begin(), kernel_times_us.end());
       
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
       
        std::cout << "Kernel execution time: " << max_kernel_time_us
                  << " us (profiling, max among CUs)" << std::endl;
        std::cout << "Wall clock time: " << duration.count() << " us" << std::endl;
       
        // ====================================================================
        // Step 9: Read and process results
        // ====================================================================
        std::cout << "\n=== Reading results ===" << std::endl;
       
        if (args.mode == 0) {
            // Mode 0: Read all hashes
            std::vector<std::vector<ap_uint<224>>> output_datas(NUM_CUS);
            for (int i = 0; i < NUM_CUS; ++i) {
                size_t word_count = num_output_words_list[i];
                output_datas[i].resize(word_count);
                std::vector<ap_uint<512>> raw(word_count);
                size_t output_size_i = word_count * sizeof(ap_uint<512>);
                q.enqueueReadBuffer(outputBufs[i], CL_TRUE, 0, output_size_i, raw.data(), nullptr, nullptr);
                for (size_t j = 0; j < word_count; ++j) {
                    output_datas[i][j] = extract_sha224(raw[j]);
                }
            }
           
            print_results_mode0(output_datas, part_starts, total_size * NUM_CORE);
            save_results_to_file(output_datas, "results.txt");
        } else {
            // Mode 1: Read match_index
            std::vector<ap_uint<48>> match_indices(NUM_CUS);
            for (int i = 0; i < NUM_CUS; ++i) {
                q.enqueueReadBuffer(matchIndexBufs[i], CL_FALSE, 0, sizeof(ap_uint<48>), &match_indices[i], nullptr, nullptr);
            }
            q.finish();
           
            ap_uint<48> found_index = ap_uint<48>(-1);
            for (int i = 0; i < NUM_CUS; ++i) {
                if (match_indices[i] != ap_uint<48>(-1)) {
                    found_index = match_indices[i] + ap_uint<48>(part_starts[i] * NUM_CORE);
                    break;
                }
            }
           
            print_results_mode1(found_index, golden);
        }
       
        // ====================================================================
        // Step 10: Print performance metrics
        // ====================================================================
        print_performance_metrics(args.mode, total_size * NUM_CORE, max_kernel_time_us);
       
        std::cout << "\n=== Done ===" << std::endl;
       
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
   
    return 0;
}

