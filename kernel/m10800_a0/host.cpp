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
#include "m10800_a0.hpp"

// ============================================================================
// Kernel Interface Constants
// ============================================================================
// NUM_CORE and MAX_LEN are now defined in m10800_a0.hpp
#define NUM_CUS 3  // Maximum number of compute units (kernel 3 config)
#define WORD_SIZE 64  // 512 bits = 64 bytes per word
#define MSG_SIZE 32   // 256 bits = 32 bytes per message
#define SALT_MAX_SIZE 8
#define GOLDEN_HEX_SIZE 96  // 512 bits = 128 hex characters

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert ap_uint<512> hash to uppercase hexadecimal string
 */
static inline std::string to_sha384_hex(const ap_uint<512>& hash) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 48; ++i) {
        unsigned char byte = static_cast<unsigned char>(
            hash.range(383 - i * 8, 383 - i * 8 - 7).to_uint());
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

/**
 * Read wordlist from text file
 * Each line should contain one word (max 32 bytes)
 */
std::vector<std::string> read_wordlist(const std::string& filename) {
    std::vector<std::string> words;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open wordlist file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove trailing whitespace (CR, LF, spaces)
        while (!line.empty() && 
               (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        
        // Add non-empty words within size limit
        if (!line.empty() && line.size() <= MSG_SIZE) {
            words.push_back(line);
        }
    }
    
    file.close();
    std::cout << "Read " << words.size() << " words from " << filename << std::endl;
    return words;
}

/**
 * Pack wordlist into 512-bit words
 * Each 512-bit word contains 2 messages (256 bits each)
 * - msg0: high 256 bits (bytes 32-63)
 * - msg1: low 256 bits (bytes 0-31)
 */
void pack_wordlist(const std::vector<std::string>& words, 
                   std::vector<ap_uint<512>>& buffer) {
    size_t num_words = (words.size() + NUM_CORE - 1) / NUM_CORE;  // Round up
    buffer.resize(num_words, ap_uint<512>(0));
    
    for (size_t i = 0; i < words.size(); i++) {
        size_t word_idx = i / NUM_CORE;
        size_t msg_idx = i % NUM_CORE;
        
        const std::string& word = words[i];
        ap_uint<512>& word_buf = buffer[word_idx];
        
        size_t copy_len = std::min(word.size(), size_t(MSG_SIZE));
        
        // Create message buffer: copy string + zero padding
        uint8_t msg_buf[MSG_SIZE] = {0};
        memcpy(msg_buf, word.c_str(), copy_len);
        
        // Reverse byte order for MSB-first alignment (padding goes to low bits)
        std::reverse(msg_buf, msg_buf + MSG_SIZE);
        
        // Cast ap_uint<512> to uint8_t* (memory layout: byte[0]=LSB ~ byte[63]=MSB)
        uint8_t* word_bytes = reinterpret_cast<uint8_t*>(&word_buf);
        
        // Calculate offset: msg0 (high 256 bits) = bytes 32-63, msg1 (low 256 bits) = bytes 0-31
        size_t offset = (msg_idx == 0) ? 32 : 0;
        
        // Copy message buffer to word buffer (optimized memcpy)
        memcpy(word_bytes + offset, msg_buf, MSG_SIZE);
    }
    
    std::cout << "Packed " << words.size() << " words into " << num_words
              << " x 512-bit words" << std::endl;
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
 * Parse hexadecimal string to ap_uint<512>
 * Input must be exactly 128 hex characters (512 bits)
 */
ap_uint<512> parse_hex_to_512(const std::string& hex_str) {
    if (hex_str.size() != GOLDEN_HEX_SIZE) {
        throw std::runtime_error(
            "Hex string must be " + std::to_string(GOLDEN_HEX_SIZE) + 
            " characters (512 bits)");
    }
    
    ap_uint<512> result;
    for (int i = 0; i < 48; ++i) {
        std::string byte_str = hex_str.substr(i * 2, 2);
        uint8_t byte_val = static_cast<uint8_t>(
            std::stoul(byte_str, nullptr, 16));
        // MSB first: byte 0 is at bits 511:504
        result.range(511 - i * 8, 504 - i * 8) = byte_val;
    }
    return result;
}

// ============================================================================
// Command-line Argument Parsing
// ============================================================================

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name 
              << " <xclbin> <wordlist.txt> <mode> <use_salt> [salt] [position] [golden_hex]\n"
              << "  xclbin:        Path to .xclbin file\n"
              << "  wordlist.txt:  Path to wordlist text file\n"
              << "  mode:          0 = calculate all hashes, 1 = search for golden hash\n"
              << "  use_salt:      0 = no salt, 1 = use salt\n"
              << "  salt:          Salt string (max " << SALT_MAX_SIZE 
              << " bytes, required if use_salt=1)\n"
              << "  position:      0 = front, 1 = back (optional, default=0)\n"
              << "  golden_hex:    SHA384 hex string (" << GOLDEN_HEX_SIZE 
              << " chars, required if mode=1)\n"
              << "\n"
              << "Example (Mode 0 - calculate all):\n"
              << "  " << program_name << " kernel.xclbin wordlist.txt 0 1 Sa1lt123 0\n"
              << "  " << program_name << " kernel.xclbin wordlist.txt 0 0\n"
              << "\n"
              << "Example (Mode 1 - search):\n"
              << "  " << program_name << " kernel.xclbin wordlist.txt 1 1 Sa1lt123 0 "
              << "44BD950E1B30C9BE510DDA81A9E58752A47734DBA8933135D02F7FCC6CE11AD34C8D0FFD17B80C41035F76B22D892C96B345A45576A76A66901619C20D2D390B\n";
}

struct ProgramArgs {
    std::string xclbin_path;
    std::string wordlist_path;
    int mode;                    // 0 = calculate all, 1 = search
    bool use_salt;
    std::string salt_str;
    uint32_t salt_position;      // 0 = front, 1 = back
    std::string golden_hex;
};

int parse_arguments(int argc, char** argv, ProgramArgs& args) {
    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }
    
    args.xclbin_path = argv[1];
    args.wordlist_path = argv[2];
    args.mode = std::stoi(argv[3]);
    args.use_salt = (std::stoi(argv[4]) != 0);
    
    if (args.mode != 0 && args.mode != 1) {
        std::cerr << "Error: mode must be 0 or 1\n";
        return 1;
    }
    
    // Initialize optional arguments
    args.salt_str = "";
    args.salt_position = 0;
    args.golden_hex = "";
    
    // Parse arguments based on use_salt and mode
    int arg_idx = 5;
    
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
                         int bank = 0) {
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

void print_results_mode0(const std::vector<std::string>& words,
                         const std::vector<ap_uint<512>>& output_data) {
    const size_t LARGE_DATASET_THRESHOLD = 10000;
    const size_t PREVIEW_COUNT = 5;
    
    std::cout << "\n=== Results ===" << std::endl;
    
    if (words.size() <= LARGE_DATASET_THRESHOLD) {
        // Small dataset: print all results
        std::cout << "Printing all " << words.size() << " results..." << std::endl;
        for (size_t i = 0; i < words.size(); i++) {
            ap_uint<512> hash = output_data[i];
            std::cout << "[" << std::setw(6) << i << "] " << words[i] 
                      << " -> 0x" << to_sha384_hex(hash) << std::endl;
        }
    } else {
        // Large dataset: print preview only
        std::cout << "Large dataset (" << words.size() << " results). Showing preview only..." << std::endl;
        std::cout << "First " << PREVIEW_COUNT << " results:" << std::endl;
        for (size_t i = 0; i < std::min(PREVIEW_COUNT, words.size()); i++) {
            ap_uint<512> hash = output_data[i];
            std::cout << "[" << std::setw(6) << i << "] " << words[i] 
                      << " -> 0x" << to_sha384_hex(hash) << std::endl;
        }
        if (words.size() > PREVIEW_COUNT * 2) {
            std::cout << "..." << std::endl;
            std::cout << "Last " << PREVIEW_COUNT << " results:" << std::endl;
            for (size_t i = words.size() - PREVIEW_COUNT; i < words.size(); i++) {
                ap_uint<512> hash = output_data[i];
                std::cout << "[" << std::setw(6) << i << "] " << words[i] 
                          << " -> 0x" << to_sha384_hex(hash) << std::endl;
            }
        }
        std::cout << "All results will be saved to results.txt" << std::endl;
    }
    std::cout.flush();  // Ensure output is flushed
}

void save_results_to_file(const std::vector<std::string>& words,
                          const std::vector<ap_uint<512>>& output_data,
                          const std::string& filename) {
    const size_t PROGRESS_INTERVAL = 100000;  // Show progress every 100k entries
    
    std::ofstream out_file(filename);
    if (!out_file.is_open()) {
        std::cerr << "Warning: Failed to open output file: " << filename << std::endl;
        return;
    }
    
    // File stream uses default buffering (optimized by C++ runtime)
    
    std::cout << "Saving " << words.size() << " results to " << filename << "..." << std::endl;
    std::cout.flush();
    
    for (size_t i = 0; i < words.size(); i++) {
        ap_uint<512> hash = output_data[i];
        out_file << words[i] << " 0x" << to_sha384_hex(hash) << "\n";
        
        // Show progress for large datasets
        if ((i + 1) % PROGRESS_INTERVAL == 0 || (i + 1) == words.size()) {
            double progress = 100.0 * (i + 1) / words.size();
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                      << progress << "% (" << (i + 1) << "/" << words.size() << ")" 
                      << std::flush;
        }
    }
    
    out_file.close();
    std::cout << "\nAll results saved to: " << filename << std::endl;
    std::cout.flush();
}

void print_results_mode1(const std::vector<std::string>& words,
                         ap_uint<48> match_index,
                         ap_uint<512> golden) {
    std::cout << "\n=== Search Result ===" << std::endl;
    
    if (match_index == ap_uint<48>(-1)) {
        std::cout << "NOT FOUND: No matching hash found in wordlist" << std::endl;
    } else {
        uint64_t idx = match_index.to_uint64();
        std::cout << "FOUND at index: " << idx << std::endl;
        
        if (idx < words.size()) {
            std::cout << "Matching word: \"" << words[idx] << "\"" << std::endl;
            std::cout << "Golden hash: 0x" << to_sha384_hex(golden) << std::endl;
        } else {
            std::cout << "Warning: Index " << idx 
                      << " is out of range (wordlist size: " << words.size() << ")" 
                      << std::endl;
        }
    }
}

void print_performance_metrics(int mode,
                                size_t num_words,
                                double kernel_time_us) {
    std::cout << "\n=== Performance ===" << std::endl;
    
    if (mode == 0) {
        double hash_rate = static_cast<double>(num_words) / 
                          (kernel_time_us / 1e6) / 1e6;  // MH/s
        std::cout << "Total hashes: " << num_words << std::endl;
        std::cout << "Kernel time (us): " << std::fixed << std::setprecision(2) 
                  << kernel_time_us << std::endl;
        std::cout << "Hash rate (MH/s): " << hash_rate << std::endl;
    } else {
        double search_rate = static_cast<double>(num_words) / 
                            (kernel_time_us / 1e6) / 1e6;  // MH/s
        std::cout << "Searched through: " << num_words << " words" << std::endl;
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
        // Step 1: Read and prepare wordlist
        // ====================================================================
        std::cout << "=== Reading wordlist ===" << std::endl;
        auto words = read_wordlist(args.wordlist_path);
        if (words.empty()) {
            std::cerr << "Error: No words found in wordlist file\n";
            return 1;
        }
        
        std::vector<ap_uint<512>> input_data;
        pack_wordlist(words, input_data);
        size_t num_input_words = input_data.size();
        ap_uint<48> size = static_cast<ap_uint<48>>(num_input_words);
        
        // Calculate buffer sizes
        size_t num_output_words = (args.mode == 0) ? words.size() : 1;
        size_t output_size = num_output_words * sizeof(ap_uint<512>);
        size_t input_size = num_input_words * sizeof(ap_uint<512>);
        
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
        
        ap_uint<512> golden = 0;
        if (args.mode == 1) {
            golden = parse_hex_to_512(args.golden_hex);
            std::cout << "Golden hash: 0x" << to_sha384_hex(golden) << std::endl;
        }
        
        // ====================================================================
        // Step 3: Initialize OpenCL device
        // ====================================================================
        std::cout << "\n=== Initializing device ===" << std::endl;
        cl::Device device;
        cl::Context context = create_context(device, err);
        cl::CommandQueue q = create_command_queue(context, device, err);
        
        // ====================================================================
        // Step 4: Load kernels for each compute unit
        // ====================================================================
        std::vector<std::string> cu_instance_names = {"m10800_a0_1", "m10800_a0_2", "m10800_a0_3"};
        std::vector<int> banks = {0, 2, 3};  // DDR[0], DDR[2], DDR[3]
        std::vector<cl::Kernel> kernels;
        int num_active_cus = 0;
        
        // Try to create kernels for each CU (some may not exist in xclbin)
        for (int i = 0; i < NUM_CUS; ++i) {
            try {
                std::string full_kernel_name = "m10800_a0:{" + cu_instance_names[i] + "}";
                cl::Kernel kernel = create_kernel(context, device, args.xclbin_path, full_kernel_name, err);
                if (err == CL_SUCCESS) {
                    kernels.push_back(kernel);
                    num_active_cus++;
                    std::cout << "Created kernel: " << full_kernel_name << std::endl;
                } else {
                    break;  // Stop if kernel creation fails (no more CUs)
                }
            } catch (...) {
                break;  // Stop if exception occurs
            }
        }
        
        if (kernels.empty()) {
            throw std::runtime_error("No compute units found in xclbin");
        }
        
        std::cout << "Using " << num_active_cus << " compute unit(s)" << std::endl;
        
        // ====================================================================
        // Step 5: Distribute wordlist across CUs
        // ====================================================================
        std::vector<std::vector<std::string>> words_per_cu(num_active_cus);
        std::vector<std::vector<ap_uint<512>>> input_data_per_cu(num_active_cus);
        std::vector<size_t> num_input_words_per_cu(num_active_cus);
        std::vector<ap_uint<48>> sizes_per_cu(num_active_cus);
        
        // Distribute words across CUs
        size_t words_per_cu_base = words.size() / num_active_cus;
        size_t words_remainder = words.size() % num_active_cus;
        
        size_t word_offset = 0;
        for (int i = 0; i < num_active_cus; ++i) {
            size_t words_for_cu = words_per_cu_base + (i < words_remainder ? 1 : 0);
            words_per_cu[i].assign(words.begin() + word_offset, 
                                   words.begin() + word_offset + words_for_cu);
            word_offset += words_for_cu;
            
            pack_wordlist(words_per_cu[i], input_data_per_cu[i]);
            num_input_words_per_cu[i] = input_data_per_cu[i].size();
            sizes_per_cu[i] = static_cast<ap_uint<48>>(num_input_words_per_cu[i]);
        }
        
        // ====================================================================
        // Step 6: Allocate buffers for each CU
        // ====================================================================
        std::cout << "\n=== Allocating buffers ===" << std::endl;
        
        std::vector<cl::Buffer> inputBufs(num_active_cus);
        std::vector<cl::Buffer> outputBufs(num_active_cus);
        std::vector<cl::Buffer> matchIndexBufs(num_active_cus);
        std::vector<size_t> input_sizes(num_active_cus);
        std::vector<size_t> output_sizes(num_active_cus);
        std::vector<size_t> num_output_words_per_cu(num_active_cus);
        
        for (int i = 0; i < num_active_cus; ++i) {
            input_sizes[i] = num_input_words_per_cu[i] * sizeof(ap_uint<512>);
            num_output_words_per_cu[i] = (args.mode == 0) ? words_per_cu[i].size() : 1;
            output_sizes[i] = num_output_words_per_cu[i] * sizeof(ap_uint<512>);
            size_t match_index_size = sizeof(ap_uint<48>);
            
            inputBufs[i] = create_buffer(context, CL_MEM_READ_ONLY, input_sizes[i], err, banks[i]);
            outputBufs[i] = create_buffer(context, CL_MEM_WRITE_ONLY, output_sizes[i], err, banks[i]);
            matchIndexBufs[i] = create_buffer(context, CL_MEM_WRITE_ONLY, match_index_size, err, banks[i]);
        }
        
        size_t total_input_size = 0;
        size_t total_output_size = 0;
        for (int i = 0; i < num_active_cus; ++i) {
            total_input_size += input_sizes[i];
            total_output_size += output_sizes[i];
        }
        
        std::cout << "Total input buffer: " << total_input_size << " bytes" << std::endl;
        std::cout << "Total output buffer: " << total_output_size << " bytes" << std::endl;
        
        // ====================================================================
        // Step 7: Write input data
        // ====================================================================
        std::cout << "\n=== Writing input data ===" << std::endl;
        for (int i = 0; i < num_active_cus; ++i) {
            q.enqueueWriteBuffer(inputBufs[i], CL_FALSE, 0, input_sizes[i], 
                                input_data_per_cu[i].data(), nullptr, nullptr);
            
            ap_uint<48> match_index_init = ap_uint<48>(-1);
            q.enqueueWriteBuffer(matchIndexBufs[i], CL_FALSE, 0, sizeof(ap_uint<48>), 
                                &match_index_init, nullptr, nullptr);
        }
        q.finish();
        
        // ====================================================================
        // Step 8: Set kernel arguments
        // ====================================================================
        std::cout << "\n=== Setting kernel arguments ===" << std::endl;
        for (int i = 0; i < num_active_cus; ++i) {
            kernels[i].setArg(0, inputBufs[i]);           // input
            kernels[i].setArg(1, outputBufs[i]);          // output
            kernels[i].setArg(2, matchIndexBufs[i]);       // match_index
            kernels[i].setArg(3, sizes_per_cu[i]);         // size
            kernels[i].setArg(4, ap_uint<1>(args.mode));  // mode
            kernels[i].setArg(5, golden);                  // golden
            kernels[i].setArg(6, salt);                     // salt
            kernels[i].setArg(7, ap_uint<1>(args.salt_position));  // salt_position
            kernels[i].setArg(8, ap_uint<1>(args.use_salt));      // use_salt
        }
        
        std::cout << "Total size: " << words.size() << " words across " << num_active_cus << " CU(s)" << std::endl;
        std::cout << "Mode: " << args.mode 
                  << (args.mode == 0 ? " (calculate all hashes)" 
                      : " (search for golden hash)") << std::endl;
        
        // ====================================================================
        // Step 9: Execute kernels
        // ====================================================================
        std::cout << "\n=== Executing kernels ===" << std::endl;
        std::vector<cl::Event> kernel_events(num_active_cus);
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_active_cus; ++i) {
            q.enqueueTask(kernels[i], nullptr, &kernel_events[i]);
        }
        q.finish();
        auto end = std::chrono::high_resolution_clock::now();
        
        // Calculate total kernel time (max of all CUs)
        double kernel_time_us = 0.0;
        for (int i = 0; i < num_active_cus; ++i) {
            cl_ulong start_time = 0, end_time = 0;
            kernel_events[i].getProfilingInfo(CL_PROFILING_COMMAND_START, &start_time);
            kernel_events[i].getProfilingInfo(CL_PROFILING_COMMAND_END, &end_time);
            double cu_time_us = (end_time - start_time) / 1000.0;
            if (cu_time_us > kernel_time_us) {
                kernel_time_us = cu_time_us;
            }
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start);
        
        std::cout << "Kernel execution time: " << kernel_time_us 
                  << " us (profiling)" << std::endl;
        std::cout << "Wall clock time: " << duration.count() << " us" << std::endl;
        
        // ====================================================================
        // Step 10: Read and process results
        // ====================================================================
        std::cout << "\n=== Reading results ===" << std::endl;
        
        if (args.mode == 0) {
            // Mode 0: Read all hashes from all CUs and merge
            std::vector<ap_uint<512>> output_data;
            output_data.reserve(words.size());
            std::vector<std::string> all_words;
            
            for (int i = 0; i < num_active_cus; ++i) {
                // Kernel outputs words.size() hashes (one per word)
                std::vector<ap_uint<512>> cu_output(words_per_cu[i].size());
                q.enqueueReadBuffer(outputBufs[i], CL_TRUE, 0, 
                                   words_per_cu[i].size() * sizeof(ap_uint<512>), 
                                   cu_output.data(), nullptr, nullptr);
                
                // Add words and hashes in order
                all_words.insert(all_words.end(), words_per_cu[i].begin(), words_per_cu[i].end());
                output_data.insert(output_data.end(), cu_output.begin(), cu_output.end());
            }
            
            print_results_mode0(all_words, output_data);
            save_results_to_file(all_words, output_data, "results.txt");
        } else {
            // Mode 1: Read match_index from all CUs and find first match
            ap_uint<48> best_match_index = ap_uint<48>(-1);
            int best_cu = -1;
            
            for (int i = 0; i < num_active_cus; ++i) {
                ap_uint<48> match_index;
                q.enqueueReadBuffer(matchIndexBufs[i], CL_TRUE, 0, sizeof(ap_uint<48>), 
                                   &match_index, nullptr, nullptr);
                
                if (match_index != ap_uint<48>(-1)) {
                    // Convert to global index
                    size_t global_offset = 0;
                    for (int j = 0; j < i; ++j) {
                        global_offset += words_per_cu[j].size();
                    }
                    ap_uint<48> global_index = ap_uint<48>(global_offset + match_index.to_uint64());
                    
                    if (best_match_index == ap_uint<48>(-1) || global_index < best_match_index) {
                        best_match_index = global_index;
                        best_cu = i;
                    }
                }
            }
            
            print_results_mode1(words, best_match_index, golden);
        }
        
        q.finish();
        
        // ====================================================================
        // Step 10: Print performance metrics
        // ====================================================================
        print_performance_metrics(args.mode, words.size(), kernel_time_us);
        
        std::cout << "\n=== Done ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}