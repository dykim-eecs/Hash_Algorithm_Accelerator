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
#include "m10800_a0.hpp"
#include <stdio.h>
#include <ap_int.h>
#include <hls_stream.h>

#define SHA384_STAGE(i) \
    sha384_core<i>(a[i], b[i], c[i], d[i], e[i], f[i], g[i], h[i], w[i], \
                   a[i + 1], b[i + 1], c[i + 1], d[i + 1], e[i + 1], f[i + 1], g[i + 1], h[i + 1], w[i + 1]);

inline ap_uint<64> rotr(ap_uint<64> in, int n) {
    return (in >> n) | (in << (64 - n));
}

inline ap_uint<64> sigma0(ap_uint<64> w) {
    return rotr(w, 1) ^ rotr(w, 8) ^ (w >> 7);
}

inline ap_uint<64> sigma1(ap_uint<64> w) {
    return rotr(w, 19) ^ rotr(w, 61) ^ (w >> 6);
}

inline ap_uint<64> Sigma0(ap_uint<64> in) {
    return rotr(in, 28) ^ rotr(in, 34) ^ rotr(in, 39);
}

inline ap_uint<64> Sigma1(ap_uint<64> in) {
    return rotr(in, 14) ^ rotr(in, 18) ^ rotr(in, 41);
}

inline ap_uint<64> Ch(ap_uint<64> x, ap_uint<64> y, ap_uint<64> z) {
    return ((x & y) ^ ((~x) & z));
}

inline ap_uint<64> Maj(ap_uint<64> x, ap_uint<64> y, ap_uint<64> z) {
    return ((x & y) ^ (x & z) ^ (y & z));
}

template <int ROUND>
void wgen(ap_uint<64> cur_w[16], ap_uint<64> nxt_w[16]) {
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
    ap_uint<64> a, ap_uint<64> b, ap_uint<64> c, ap_uint<64> d,
    ap_uint<64> e, ap_uint<64> f, ap_uint<64> g, ap_uint<64> h,
    ap_uint<64> w_t,
    ap_uint<64> &a_out, ap_uint<64> &b_out, ap_uint<64> &c_out, ap_uint<64> &d_out,
    ap_uint<64> &e_out, ap_uint<64> &f_out, ap_uint<64> &g_out, ap_uint<64> &h_out
) {
#pragma HLS INLINE
    static const ap_uint<64> K[80] = {
        0x428a2f98d728ae22UL, 0x7137449123ef65cdUL, 0xb5c0fbcfec4d3b2fUL, 0xe9b5dba58189dbbcUL, 0x3956c25bf348b538UL,
        0x59f111f1b605d019UL, 0x923f82a4af194f9bUL, 0xab1c5ed5da6d8118UL, 0xd807aa98a3030242UL, 0x12835b0145706fbeUL,
        0x243185be4ee4b28cUL, 0x550c7dc3d5ffb4e2UL, 0x72be5d74f27b896fUL, 0x80deb1fe3b1696b1UL, 0x9bdc06a725c71235UL,
        0xc19bf174cf692694UL, 0xe49b69c19ef14ad2UL, 0xefbe4786384f25e3UL, 0x0fc19dc68b8cd5b5UL, 0x240ca1cc77ac9c65UL,
        0x2de92c6f592b0275UL, 0x4a7484aa6ea6e483UL, 0x5cb0a9dcbd41fbd4UL, 0x76f988da831153b5UL, 0x983e5152ee66dfabUL,
        0xa831c66d2db43210UL, 0xb00327c898fb213fUL, 0xbf597fc7beef0ee4UL, 0xc6e00bf33da88fc2UL, 0xd5a79147930aa725UL,
        0x06ca6351e003826fUL, 0x142929670a0e6e70UL, 0x27b70a8546d22ffcUL, 0x2e1b21385c26c926UL, 0x4d2c6dfc5ac42aedUL,
        0x53380d139d95b3dfUL, 0x650a73548baf63deUL, 0x766a0abb3c77b2a8UL, 0x81c2c92e47edaee6UL, 0x92722c851482353bUL,
        0xa2bfe8a14cf10364UL, 0xa81a664bbc423001UL, 0xc24b8b70d0f89791UL, 0xc76c51a30654be30UL, 0xd192e819d6ef5218UL,
        0xd69906245565a910UL, 0xf40e35855771202aUL, 0x106aa07032bbd1b8UL, 0x19a4c116b8d2d0c8UL, 0x1e376c085141ab53UL,
        0x2748774cdf8eeb99UL, 0x34b0bcb5e19b48a8UL, 0x391c0cb3c5c95a63UL, 0x4ed8aa4ae3418acbUL, 0x5b9cca4f7763e373UL,
        0x682e6ff3d6b2b8a3UL, 0x748f82ee5defb2fcUL, 0x78a5636f43172f60UL, 0x84c87814a1f0ab72UL, 0x8cc702081a6439ecUL,
        0x90befffa23631e28UL, 0xa4506cebde82bde9UL, 0xbef9a3f7b2c67915UL, 0xc67178f2e372532bUL, 0xca273eceea26619cUL,
        0xd186b8c721c0c207UL, 0xeada7dd6cde0eb1eUL, 0xf57d4f7fee6ed178UL, 0x06f067aa72176fbaUL, 0x0a637dc5a2c898a6UL,
        0x113f9804bef90daeUL, 0x1b710b35131c471bUL, 0x28db77f523047d84UL, 0x32caab7b40c72493UL, 0x3c9ebe0a15c9bebcUL,
        0x431d67c49c100d4cUL, 0x4cc5d4becb3e42b6UL, 0x597f299cfc657e2aUL, 0x5fcb6fab3ad6faecUL, 0x6c44198c4a475817UL,
    };

    ap_uint<64> T1;
    ap_uint<64> T2;

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
void sha384_core(
    ap_uint<64> a, ap_uint<64> b, ap_uint<64> c, ap_uint<64> d,
    ap_uint<64> e, ap_uint<64> f, ap_uint<64> g, ap_uint<64> h,
    ap_uint<64> cur_w[16],
    ap_uint<64> &a_out, ap_uint<64> &b_out, ap_uint<64> &c_out, ap_uint<64> &d_out,
    ap_uint<64> &e_out, ap_uint<64> &f_out, ap_uint<64> &g_out, ap_uint<64> &h_out,
    ap_uint<64> nxt_w[16]
) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=cur_w complete
#pragma HLS ARRAY_PARTITION variable=nxt_w complete
    wgen<ROUND>(cur_w, nxt_w);
    dgen<ROUND>(a, b, c, d, e, f, g, h, cur_w[0], a_out, b_out, c_out, d_out, e_out, f_out, g_out, h_out);
}

// Expand 256-bit message to 1024-bit SHA384 block with padding, with optional salt
ap_uint<1024> expand_to_sha384_block(ap_uint<256> msg_256, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
    #pragma HLS INLINE

    ap_uint<1024> block = 0;

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
        block.range(1023, 1024 - total_len_bits) = salted_msg.range(255, 256 - total_len_bits);
    }

    // Insert 0x80 after salted message
    block[1024 - total_len_bits - 1] = 1;

    // Insert message length in bits at the end (last 128 bits)
    block.range(127, 0) = total_len_bits;

    return block;
}

void sha384_pipe(int core_id, ap_uint<48> size, hls::stream<ap_uint<256>> &msg_strm, hls::stream<ap_uint<512>> &o_strm, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
#pragma HLS INLINE off
PIPE_SHA384:
    for (ap_uint<48> k = 0; k < size; k++) {
#pragma HLS PIPELINE II=1
        // Read 256-bit message and expand to 1024-bit SHA384 block with optional salt
        ap_uint<256> msg_256 = msg_strm.read();
        ap_uint<1024> block = expand_to_sha384_block(msg_256, salt, salt_position, use_salt);

        ap_uint<64> a[81], b[81], c[81], d[81], e[81], f[81], g[81], h[81];
        ap_uint<64> w[81][16];
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

        // Initial W[0]
        for (int kk = 0; kk < 16; kk++) {
#pragma HLS UNROLL
            w[0][kk] = block.range(1023 - 64 * kk, 960 - 64 * kk);
        }

        // Initial digest
        a[0] = 0xcbbb9d5dc1059ed8UL;
        b[0] = 0x629a292a367cd507UL;
        c[0] = 0x9159015a3070dd17UL;
        d[0] = 0x152fecd8f70e5939UL;
        e[0] = 0x67332667ffc00b31UL;
        f[0] = 0x8eb44a8768581511UL;
        g[0] = 0xdb0c2e0d64f98fa7UL;
        h[0] = 0x47b5481dbefa4fa4UL;

        // 80-stage pipeline
        SHA384_STAGE(0)  SHA384_STAGE(1)  SHA384_STAGE(2)  SHA384_STAGE(3)  SHA384_STAGE(4)
        SHA384_STAGE(5)  SHA384_STAGE(6)  SHA384_STAGE(7)  SHA384_STAGE(8)  SHA384_STAGE(9)
        SHA384_STAGE(10) SHA384_STAGE(11) SHA384_STAGE(12) SHA384_STAGE(13) SHA384_STAGE(14)
        SHA384_STAGE(15) SHA384_STAGE(16) SHA384_STAGE(17) SHA384_STAGE(18) SHA384_STAGE(19)
        SHA384_STAGE(20) SHA384_STAGE(21) SHA384_STAGE(22) SHA384_STAGE(23) SHA384_STAGE(24)
        SHA384_STAGE(25) SHA384_STAGE(26) SHA384_STAGE(27) SHA384_STAGE(28) SHA384_STAGE(29)
        SHA384_STAGE(30) SHA384_STAGE(31) SHA384_STAGE(32) SHA384_STAGE(33) SHA384_STAGE(34)
        SHA384_STAGE(35) SHA384_STAGE(36) SHA384_STAGE(37) SHA384_STAGE(38) SHA384_STAGE(39)
        SHA384_STAGE(40) SHA384_STAGE(41) SHA384_STAGE(42) SHA384_STAGE(43) SHA384_STAGE(44)
        SHA384_STAGE(45) SHA384_STAGE(46) SHA384_STAGE(47) SHA384_STAGE(48) SHA384_STAGE(49)
        SHA384_STAGE(50) SHA384_STAGE(51) SHA384_STAGE(52) SHA384_STAGE(53) SHA384_STAGE(54)
        SHA384_STAGE(55) SHA384_STAGE(56) SHA384_STAGE(57) SHA384_STAGE(58) SHA384_STAGE(59)
        SHA384_STAGE(60) SHA384_STAGE(61) SHA384_STAGE(62) SHA384_STAGE(63) SHA384_STAGE(64)
        SHA384_STAGE(65) SHA384_STAGE(66) SHA384_STAGE(67) SHA384_STAGE(68) SHA384_STAGE(69)
        SHA384_STAGE(70) SHA384_STAGE(71) SHA384_STAGE(72) SHA384_STAGE(73) SHA384_STAGE(74)
        SHA384_STAGE(75) SHA384_STAGE(76) SHA384_STAGE(77) SHA384_STAGE(78) SHA384_STAGE(79)

        ap_uint<64> h0 = a[80] + 0xcbbb9d5dc1059ed8UL;
        ap_uint<64> h1 = b[80] + 0x629a292a367cd507UL;
        ap_uint<64> h2 = c[80] + 0x9159015a3070dd17UL;
        ap_uint<64> h3 = d[80] + 0x152fecd8f70e5939UL;
        ap_uint<64> h4 = e[80] + 0x67332667ffc00b31UL;
        ap_uint<64> h5 = f[80] + 0x8eb44a8768581511UL;

        ap_uint<512> digest = 0;  // Initialize to zero
        // Pack 384-bit hash into lower bits [383:0], upper bits zero
        digest.range(383, 320) = h0;
        digest.range(319, 256) = h1;
        digest.range(255, 192) = h2;
        digest.range(191, 128) = h3;
        digest.range(127, 64)  = h4;
        digest.range(63, 0)    = h5;

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

extern "C" void m10800_a0(ap_uint<512> *input, ap_uint<512> *output, ap_uint<48> *match_index, ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
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
    sha384_pipe(0, size, msg_strm[0], o_strm[0], salt, salt_position, use_salt);
    sha384_pipe(1, size, msg_strm[1], o_strm[1], salt, salt_position, use_salt);

    out_ctrl(output, match_index, o_strm, size, mode, golden);
}
