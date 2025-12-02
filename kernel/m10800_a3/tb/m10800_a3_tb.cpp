//--------------------------------------------------------------------------------------------------
// Copyright (c) 2025 Kim, Doh Yon (Sungkyunkwan University)
//
// This source code is provided for portfolio review purposes only.
// Unauthorized copying, modification, or distribution is strictly prohibited.
//
// All Rights Reserved.
//--------------------------------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <ap_int.h>
#include <iomanip>
#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <cmath>
#ifndef MAX_LEN
#define MAX_LEN 7
#endif
#ifndef NUM_CORE
#define NUM_CORE 3
#endif
extern "C" void m10800_a3(ap_uint<512> *output, ap_uint<48> *match_index, ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt);

// Helper function to generate message for given index (same as kernel logic)
std::string generate_message(int index, int core, int len) {
    const char CHARSET[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    const int CHARSET_LEN = 94;
    
    std::string msg = "";
    char msb_char = 'a' + core;
    msg += msb_char;
    
    // Generate remaining characters based on counter (odometer style)
    int counter = index;
    for (int pos = len - 1; pos >= 0; --pos) {
        int char_idx = counter % CHARSET_LEN;
        counter /= CHARSET_LEN;
        msg += CHARSET[char_idx];
    }
    
    return msg;
}
static inline ap_uint<384> extract_sha384(const ap_uint<512>& word) {
    return word.range(383, 0);
}

static inline ap_uint<512> pack_sha384(const ap_uint<384>& digest) {
    ap_uint<512> word = 0;
    word.range(383, 0) = digest;
    return word;
}
void dump_result(ap_uint<512> *output) {
    // Output only the last message digests for each channel (messages like "a~~", "b~~", etc.)
    std::cout << "Displaying SHA384 digests for the last message of each channel:\n";
    ap_uint<384> digest[NUM_CORE];
    for (int k = 0; k < NUM_CORE; k++) {
        digest[k] = extract_sha384(output[k]);
    }
    // Helper to print digest with channel - generates odometer style message
    auto print_digest = [](int channel, char msb_char, ap_uint<384>& digest) {
        // Generate odometer style message for this index
        std::string msg(1, msb_char);
        msg += std::string(MAX_LEN - 1, '~');  // e.g., "a~~" for MAX_LEN=3
        std::ostringstream hexDigest;
        hexDigest << std::hex << std::noshowbase << std::setfill('0');
        for (int byte = 0; byte < 48; ++byte) {
            unsigned int byteVal = digest.range((47 - byte) * 8 + 7, (47 - byte) * 8);
            hexDigest << std::setw(2) << byteVal;
        }
        std::cout << "Channel " << channel
                  << " [ \"" << msg << "\" ] -> SHA384 : 0x" << hexDigest.str() << std::endl;
    };
    for (int k = 0; k < NUM_CORE; ++k) {
        char msb_char = 'a' + k;
        print_digest(k, msb_char, digest[k]);
    }
}
std::string print_hash(ap_uint<384> hash_val) {
    std::ostringstream hexStr;
    hexStr << std::uppercase << std::hex << std::noshowbase << std::setfill('0');
    for (int byte = 0; byte < 48; ++byte) {
        unsigned int byteVal = hash_val.range((47 - byte) * 8 + 7, (47 - byte) * 8);
        hexStr << std::setw(2) << byteVal;
    }
    return hexStr.str();
}

int main() {
    // Allocate input and output arrays
    ap_uint<6> len = MAX_LEN - 1; // Length excluding MSB chars
    ap_uint<48> size = 1000;
    ap_uint<8*NUM_CORE> msb_chars = 0;
    const int OUTPUT_SIZE = 3000;
    ap_uint<512>* output_hw = new ap_uint<512>[OUTPUT_SIZE];
    ap_uint<48>* match_index = new ap_uint<48>[1];
    
    // Set MSB chars for each core
    for (int i = 0; i < NUM_CORE; ++i) {
        msb_chars.range(8 * (i + 1) - 1, 8 * i) = 'a' + i;
    }

    // Salt configuration
    ap_uint<64> salt = 0x5361316C74313233ULL;  // "Sa1lt123" in ASCII
    ap_uint<1> salt_position = 0;  // 0 = front, 1 = back
    ap_uint<1> use_salt = 1;  // 0 = no salt, 1 = use salt
    
    std::cout << "\n========== Salt Configuration ==========\n";
    std::cout << "Use salt: " << (use_salt == 1 ? "YES" : "NO") << "\n";
    if (use_salt == 1) {
        std::cout << "Salt: 0x" << std::hex << std::setw(16) << std::setfill('0') << salt.to_uint64() << std::dec << "\n";
        std::cout << "Salt position: " << (salt_position == 0 ? "FRONT" : "BACK") << " of message\n";
        std::cout << "Salt text: ";
        for (int i = 0; i < 8; i++) {
            uint8_t byte = (salt.to_uint64() >> (56 - i*8)) & 0xFF;
            if (byte >= 32 && byte < 127) std::cout << (char)byte;
            else std::cout << ".";
        }
        std::cout << "\n";
    }
    
    // ========== CASE 1: MODE 0 with salt (FRONT) ==========
    std::cout << "\n========== CASE 1: MODE 0 - Calculate all hashes (with salt FRONT) ==========\n";
    std::cout << "Size: " << (int)size << " | Cores: " << NUM_CORE << " | Total: " << NUM_CORE * (int)size << " passwords\n";
    
    ap_uint<1> mode = 0;
    ap_uint<512> golden_dummy = 0;  // Not used in mode 0
    use_salt = 1;
    salt_position = 0;  // FRONT

    m10800_a3(output_hw, match_index, len, msb_chars, size, mode, golden_dummy, salt, salt_position, use_salt);

    // Print first few hashes
    std::cout << "\nFirst 3 hashes:\n";
    for (int i = 0; i < 3; ++i) {
        std::string msg = generate_message(i / NUM_CORE, i % NUM_CORE, len);
        std::cout << "  [" << i << "] " << msg << " | 0x" << print_hash(extract_sha384(output_hw[i])) << "\n";
    }

    // ========== CASE 2: MODE 1 with salt (FRONT) - Search for specific hash ==========
    std::cout << "\n========== CASE 2: MODE 1 - Search for specific hash (with salt FRONT) ==========\n";

    // Use hash of message at index 500 as golden
    int target_idx = 500;
    ap_uint<384> golden_digest = extract_sha384(output_hw[target_idx]);
    ap_uint<512> golden = pack_sha384(golden_digest);
    std::string target_msg = generate_message(target_idx / NUM_CORE, target_idx % NUM_CORE, len);
    
    std::cout << "Target message: " << target_msg << " (index " << target_idx << ")\n";
    std::cout << "Target hash (with salt): 0x" << print_hash(golden_digest) << "\n";
    
    // Run kernel in search mode
    mode = 1;
    m10800_a3(output_hw, match_index, len, msb_chars, size, mode, golden, salt, salt_position, use_salt);
    
    // Check result
    ap_uint<48> found_idx = match_index[0];
    if (found_idx == ap_uint<48>(-1)) {
        std::cout << "Result: NOT FOUND (ERROR!)\n";
        std::cout << "✗ FAIL: Should have found index " << target_idx << "\n";
    } else {
        std::cout << "Result: FOUND at index " << found_idx.to_uint() << "\n";
        std::string found_msg = generate_message(found_idx.to_uint() / NUM_CORE, found_idx.to_uint() % NUM_CORE, len);
        std::cout << "Message: " << found_msg << "\n";
        
        if (found_idx.to_uint() == target_idx) {
            std::cout << "✓ PASS: Found correct message!\n";
        } else {
            std::cout << "✗ FAIL: Found wrong index (expected " << target_idx << ")\n";
        }
    }
    
    // ========== CASE 3: MODE 0 with salt (BACK) ==========
    std::cout << "\n========== CASE 3: MODE 0 - Calculate all hashes (with salt BACK) ==========\n";
    salt_position = 1;  // Change to BACK
    std::cout << "Salt position: BACK of message\n";

    // Recalculate with salt at back
    mode = 0;
    m10800_a3(output_hw, match_index, len, msb_chars, size, mode, golden_dummy, salt, salt_position, use_salt);

    std::cout << "First hash with BACK salt: [0] " << generate_message(0, 0, len) << " | 0x" << print_hash(extract_sha384(output_hw[0])) << "\n";
    std::cout << "Compare with FRONT salt - should be different!\n";

    // ========== CASE 4: MODE 0 without salt ==========
    std::cout << "\n========== CASE 4: MODE 0 - Calculate all hashes (WITHOUT salt) ==========\n";
    use_salt = 0;  // Disable salt
    std::cout << "Use salt: NO\n";

    // Recalculate without salt
    mode = 0;
    m10800_a3(output_hw, match_index, len, msb_chars, size, mode, golden_dummy, salt, salt_position, use_salt);

    std::cout << "First hash WITHOUT salt: [0] " << generate_message(0, 0, len) << " | 0x" << print_hash(extract_sha384(output_hw[0])) << "\n";
    std::cout << "Compare with salt hashes - should be different!\n";

    std::cout << "\n========== All test cases completed ==========\n";

    // Cleanup
    delete[] output_hw;
    delete[] match_index;
    
    return 0;
}
