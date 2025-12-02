//--------------------------------------------------------------------------------------------------
// Copyright (c) 2025 Kim, Doh Yon (Sungkyunkwan University)
//
// This source code is provided for portfolio review purposes only.
// Unauthorized copying, modification, or distribution is strictly prohibited.
//
// All Rights Reserved.
//--------------------------------------------------------------------------------------------------
#define AP_INT_MAX_W 2048
#ifndef NUM_CORE
#define NUM_CORE 2
#endif
#include "m01300_a0.hpp"
#include <stdio.h>
#include <ap_int.h>
#include <hls_stream.h>

#define SHA224_STAGE(i) \
    sha224_core<i>(a[i], b[i], c[i], d[i], e[i], f[i], g[i], h[i], w[i], \
                   a[i + 1], b[i + 1], c[i + 1], d[i + 1], e[i + 1], f[i + 1], g[i + 1], h[i + 1], w[i + 1]);

// SHA-256 uses 32-bit rotation
inline ap_uint<32> rotr(ap_uint<32> in, int n) {
    return (in >> n) | (in << (32 - n));
}

// SHA-256 sigma functions
inline ap_uint<32> sigma0(ap_uint<32> w) {
    return rotr(w, 7) ^ rotr(w, 18) ^ (w >> 3);
}

inline ap_uint<32> sigma1(ap_uint<32> w) {
    return rotr(w, 17) ^ rotr(w, 19) ^ (w >> 10);
}

inline ap_uint<32> Sigma0(ap_uint<32> in) {
    return rotr(in, 2) ^ rotr(in, 13) ^ rotr(in, 22);
}

inline ap_uint<32> Sigma1(ap_uint<32> in) {
    return rotr(in, 6) ^ rotr(in, 11) ^ rotr(in, 25);
}

inline ap_uint<32> Ch(ap_uint<32> x, ap_uint<32> y, ap_uint<32> z) {
    return ((x & y) ^ ((~x) & z));
}

inline ap_uint<32> Maj(ap_uint<32> x, ap_uint<32> y, ap_uint<32> z) {
    return ((x & y) ^ (x & z) ^ (y & z));
}

template <int ROUND>
void wgen(ap_uint<32> cur_w[16], ap_uint<32> nxt_w[16]) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=cur_w complete
#pragma HLS ARRAY_PARTITION variable=nxt_w complete
    if (ROUND < 15) {
        for (int k = 0; k < 15; k++) {
#pragma HLS UNROLL
            nxt_w[k] = cur_w[k + 1];
        }
        nxt_w[15] = cur_w[0];
    } else {
        // w[t-16] + s0(w[t-15]) + w[t-7] + s1(w[t-2])
        nxt_w[0] = cur_w[1] + sigma0(cur_w[2]) + cur_w[10] + sigma1(cur_w[15]);
        for (int k = 1; k < 15; k++) {
#pragma HLS UNROLL
            nxt_w[k] = cur_w[k + 1];
        }
        nxt_w[15] = cur_w[0];
    }
}

template <int ROUND>
void dgen(
    ap_uint<32> a, ap_uint<32> b, ap_uint<32> c, ap_uint<32> d,
    ap_uint<32> e, ap_uint<32> f, ap_uint<32> g, ap_uint<32> h,
    ap_uint<32> w_t,
    ap_uint<32> &a_out, ap_uint<32> &b_out, ap_uint<32> &c_out, ap_uint<32> &d_out,
    ap_uint<32> &e_out, ap_uint<32> &f_out, ap_uint<32> &g_out, ap_uint<32> &h_out
) {
#pragma HLS INLINE
    static const ap_uint<32> K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    ap_uint<32> T1;
    ap_uint<32> T2;

    T1 = h + Sigma1(e) + Ch(e, f, g) + K[ROUND] + w_t;
    T2 = Sigma0(a) + Maj(a, b, c);

    h_out = g;
    g_out = f;
    f_out = e;
    e_out = d + T1;
    d_out = c;
    c_out = b;
    b_out = a;
    a_out = T1 + T2;
}

template <int ROUND>
void sha224_core(
    ap_uint<32> a, ap_uint<32> b, ap_uint<32> c, ap_uint<32> d,
    ap_uint<32> e, ap_uint<32> f, ap_uint<32> g, ap_uint<32> h,
    ap_uint<32> cur_w[16],
    ap_uint<32> &a_out, ap_uint<32> &b_out, ap_uint<32> &c_out, ap_uint<32> &d_out,
    ap_uint<32> &e_out, ap_uint<32> &f_out, ap_uint<32> &g_out, ap_uint<32> &h_out,
    ap_uint<32> nxt_w[16]
) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=cur_w complete
#pragma HLS ARRAY_PARTITION variable=nxt_w complete
    wgen<ROUND>(cur_w, nxt_w);
    dgen<ROUND>(a, b, c, d, e, f, g, h, cur_w[0], a_out, b_out, c_out, d_out, e_out, f_out, g_out, h_out);
}

// Expand 256-bit message to 512-bit SHA224 block with padding, with optional salt
ap_uint<512> expand_to_sha224_block(ap_uint<256> msg_256, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
    #pragma HLS INLINE

    ap_uint<512> block = 0;

    // Calculate actual message length by finding last non-zero byte
    int msg_len_bytes = 0;
    for (int i = 0; i < 32; i++) {
        #pragma HLS UNROLL
        ap_uint<8> byte_val = msg_256.range(255 - 8*i, 248 - 8*i);
        if (byte_val != 0) {
            msg_len_bytes = i + 1;
        }
    }

    // Total length = message + (salt if used)
    int total_len_bytes = use_salt ? (msg_len_bytes + 8) : msg_len_bytes;
    int total_len_bits = total_len_bytes * 8;

    // Construct salted message based on salt_position and use_salt
    ap_uint<256> salted_msg = 0;

    if (use_salt == 0) {
        // No salt: use message only
        if (msg_len_bytes > 0) {
            int msg_bits = msg_len_bytes * 8;
            salted_msg.range(255, 256 - msg_bits) = msg_256.range(255, 256 - msg_bits);
        }
    } else if (salt_position == 0) {
        // Salt at front: [salt 8 bytes][message]
        // Place salt at MSB
        salted_msg.range(255, 192) = salt;
        // Place message after salt
        if (msg_len_bytes > 0) {
            int msg_bits = msg_len_bytes * 8;
            salted_msg.range(191, 192 - msg_bits) = msg_256.range(255, 256 - msg_bits);
        }
    } else {
        // Salt at back: [message][salt 8 bytes]
        // Place message at MSB
        if (msg_len_bytes > 0) {
            int msg_bits = msg_len_bytes * 8;
            salted_msg.range(255, 256 - msg_bits) = msg_256.range(255, 256 - msg_bits);
            // Place salt after message
            salted_msg.range(255 - msg_bits, 192 - msg_bits) = salt;
        } else {
            // Only salt if message is empty
            salted_msg.range(255, 192) = salt;
        }
    }

    // Place salted message at the beginning of block (MSB)
    if (total_len_bits > 0) {
        block.range(511, 512 - total_len_bits) = salted_msg.range(255, 256 - total_len_bits);
    }

    // Insert 0x80 after salted message
    block[512 - total_len_bits - 1] = 1;

    // Insert message length in bits at the end (last 64 bits for SHA-256)
    block.range(63, 0) = total_len_bits;

    return block;
}

void sha224_pipe(int core_id, ap_uint<48> size, hls::stream<ap_uint<256>>& msg_strm, hls::stream<ap_uint<512>>& o_strm, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
#pragma HLS INLINE off
PIPE_SHA224:
    for (ap_uint<48> k = 0; k < size; k++) {
#pragma HLS PIPELINE II=1
        // Read 256-bit message and expand to 512-bit SHA224 block with optional salt
        ap_uint<256> msg_256 = msg_strm.read();
        ap_uint<512> block = expand_to_sha224_block(msg_256, salt, salt_position, use_salt);

        ap_uint<32> a[65], b[65], c[65], d[65], e[65], f[65], g[65], h[65];
        ap_uint<32> w[65][16];
#pragma HLS ARRAY_PARTITION variable=a complete
#pragma HLS ARRAY_PARTITION variable=b complete
#pragma HLS ARRAY_PARTITION variable=c complete
#pragma HLS ARRAY_PARTITION variable=d complete
#pragma HLS ARRAY_PARTITION variable=e complete
#pragma HLS ARRAY_PARTITION variable=f complete
#pragma HLS ARRAY_PARTITION variable=g complete
#pragma HLS ARRAY_PARTITION variable=h complete
#pragma HLS ARRAY_PARTITION variable=w complete dim=1
#pragma HLS ARRAY_PARTITION variable=w complete dim=2

        // Initial W[0] - SHA-256 uses 32-bit words
        for (int kk = 0; kk < 16; kk++) {
#pragma HLS UNROLL
            w[0][kk] = block.range(511 - 32 * kk, 480 - 32 * kk);
        }

        // Initial digest - SHA-256 initial values
        a[0] = 0xc1059ed8;
        b[0] = 0x367cd507;
        c[0] = 0x3070dd17;
        d[0] = 0xf70e5939;
        e[0] = 0xffc00b31;
        f[0] = 0x68581511;
        g[0] = 0x64f98fa7;
        h[0] = 0xbefa4fa4;

        // 64-stage pipeline (SHA-256 has 64 rounds)
        SHA224_STAGE(0)  SHA224_STAGE(1)  SHA224_STAGE(2)  SHA224_STAGE(3)  SHA224_STAGE(4)
        SHA224_STAGE(5)  SHA224_STAGE(6)  SHA224_STAGE(7)  SHA224_STAGE(8)  SHA224_STAGE(9)
        SHA224_STAGE(10) SHA224_STAGE(11) SHA224_STAGE(12) SHA224_STAGE(13) SHA224_STAGE(14)
        SHA224_STAGE(15) SHA224_STAGE(16) SHA224_STAGE(17) SHA224_STAGE(18) SHA224_STAGE(19)
        SHA224_STAGE(20) SHA224_STAGE(21) SHA224_STAGE(22) SHA224_STAGE(23) SHA224_STAGE(24)
        SHA224_STAGE(25) SHA224_STAGE(26) SHA224_STAGE(27) SHA224_STAGE(28) SHA224_STAGE(29)
        SHA224_STAGE(30) SHA224_STAGE(31) SHA224_STAGE(32) SHA224_STAGE(33) SHA224_STAGE(34)
        SHA224_STAGE(35) SHA224_STAGE(36) SHA224_STAGE(37) SHA224_STAGE(38) SHA224_STAGE(39)
        SHA224_STAGE(40) SHA224_STAGE(41) SHA224_STAGE(42) SHA224_STAGE(43) SHA224_STAGE(44)
        SHA224_STAGE(45) SHA224_STAGE(46) SHA224_STAGE(47) SHA224_STAGE(48) SHA224_STAGE(49)
        SHA224_STAGE(50) SHA224_STAGE(51) SHA224_STAGE(52) SHA224_STAGE(53) SHA224_STAGE(54)
        SHA224_STAGE(55) SHA224_STAGE(56) SHA224_STAGE(57) SHA224_STAGE(58) SHA224_STAGE(59)
        SHA224_STAGE(60) SHA224_STAGE(61) SHA224_STAGE(62) SHA224_STAGE(63)

        ap_uint<32> h0 = a[64] + 0xc1059ed8;
        ap_uint<32> h1 = b[64] + 0x367cd507;
        ap_uint<32> h2 = c[64] + 0x3070dd17;
        ap_uint<32> h3 = d[64] + 0xf70e5939;
        ap_uint<32> h4 = e[64] + 0xffc00b31;
        ap_uint<32> h5 = f[64] + 0x68581511;
        ap_uint<32> h6 = g[64] + 0x64f98fa7;

        // Pack into 512-bit output: hash placed in lower 224 bits [223:0], upper bits zero
        ap_uint<512> digest = 0;
        digest.range(223, 192) = h0;
        digest.range(191, 160) = h1;
        digest.range(159, 128) = h2;
        digest.range(127, 96)  = h3;
        digest.range(95, 64)   = h4;
        digest.range(63, 32)   = h5;
        digest.range(31, 0)    = h6;

        o_strm.write(digest);
    }
}

// Simple distribution: split 512-bit input into 2×256-bit messages and distribute to cores
void distribute_messages(
    ap_uint<512> *input,
    hls::stream<ap_uint<256>> msg_strm[NUM_CORE],
    ap_uint<48> size
) {
    #pragma HLS INLINE off

DISTRIBUTE_LOOP:
    for (ap_uint<48> w = 0; w < size; w++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS DEPENDENCE variable=msg_strm inter false

        ap_uint<512> word = input[w];

        // Split into 2 messages of 256 bits each
        // Word layout: [msg0(511:256)][msg1(255:0)]
        ap_uint<256> msg[NUM_CORE];
        #pragma HLS ARRAY_PARTITION variable=msg complete

        for (int k = 0; k < NUM_CORE; k++) {
            #pragma HLS UNROLL
            int bit_high = 511 - (k * 256);
            int bit_low = bit_high - 255;
            msg[k] = word.range(bit_high, bit_low);
        }

        // Write to each core's stream
        for (int k = 0; k < NUM_CORE; k++) {
            #pragma HLS UNROLL
            msg_strm[k].write(msg[k]);
        }
    }
}

void out_ctrl(ap_uint<512> *output, ap_uint<48> *match_index, hls::stream<ap_uint<512>> o_strm[NUM_CORE], ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden) {
    #pragma HLS INLINE off

    if (mode == 0) {
        // Mode 0: Output all hashes
LOOP_MODE0_OUTER:
        for (ap_uint<48> sk = 0; sk < size; sk++) {
LOOP_MODE0_INNER:
            for (int k = 0; k < NUM_CORE; k++) {
                #pragma HLS PIPELINE II=1
                ap_uint<512> d = o_strm[k].read();
                ap_uint<48> idx = sk * NUM_CORE + k;
                output[idx] = d;
            }
        }
        *match_index = ap_uint<48>(-1);  // Not used in mode 0
    } else {
        // Mode 1: Find matching index
        ap_uint<48> found_idx = 0;
        ap_uint<1> found = 0;

LOOP_MODE1:
        for (ap_uint<48> sk = 0; sk < size; sk++) {
            #pragma HLS PIPELINE II=1

            // Read from all cores simultaneously
            ap_uint<512> d[NUM_CORE];
            #pragma HLS ARRAY_PARTITION variable=d complete

            for (int k = 0; k < NUM_CORE; k++) {
                #pragma HLS UNROLL
                d[k] = o_strm[k].read();
            }

            // Check for matches
            for (int k = 0; k < NUM_CORE; k++) {
                #pragma HLS UNROLL
                if (!found && d[k] == golden) {
                    found_idx = sk * NUM_CORE + k;
                    found = 1;
                }
            }
        }

        *match_index = found ? found_idx : ap_uint<48>(-1);
    }
}

extern "C" void m01300_a0(ap_uint<512> *input, ap_uint<512> *output, ap_uint<48> *match_index, ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
#pragma HLS INTERFACE m_axi port=input  offset=slave bundle=gmem0 depth=500
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem2 depth=1000
#pragma HLS INTERFACE m_axi port=match_index offset=slave bundle=gmem1 depth=1
#pragma HLS INTERFACE s_axilite port=input  bundle=control
#pragma HLS INTERFACE s_axilite port=output bundle=control
#pragma HLS INTERFACE s_axilite port=match_index bundle=control
#pragma HLS INTERFACE s_axilite port=size   bundle=control
#pragma HLS INTERFACE s_axilite port=mode   bundle=control
#pragma HLS INTERFACE s_axilite port=golden bundle=control
#pragma HLS INTERFACE s_axilite port=salt bundle=control
#pragma HLS INTERFACE s_axilite port=salt_position bundle=control
#pragma HLS INTERFACE s_axilite port=use_salt bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

#pragma HLS stable variable=size
#pragma HLS stable variable=mode
#pragma HLS stable variable=golden
#pragma HLS stable variable=salt
#pragma HLS stable variable=salt_position
#pragma HLS stable variable=use_salt

#pragma HLS DATAFLOW

    // 256-bit streams for messages
    hls::stream<ap_uint<256>>  msg_strm[NUM_CORE];
    hls::stream<ap_uint<512>>   o_strm[NUM_CORE];
    #pragma HLS STREAM variable=msg_strm depth=128
    #pragma HLS STREAM variable=o_strm depth=128
    #pragma HLS ARRAY_PARTITION variable=msg_strm complete dim=1
    #pragma HLS ARRAY_PARTITION variable=o_strm complete dim=1

    // Simple distribution: split 512-bit words into 2×256-bit messages
    distribute_messages(input, msg_strm, size);

    // Each core processes 'size' messages with optional salt
    sha224_pipe(0, size, msg_strm[0], o_strm[0], salt, salt_position, use_salt);
    sha224_pipe(1, size, msg_strm[1], o_strm[1], salt, salt_position, use_salt);

    out_ctrl(output, match_index, o_strm, size, mode, golden);
}
