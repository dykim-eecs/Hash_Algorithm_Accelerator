//--------------------------------------------------------------------------------------------------
// Copyright (c) 2025 Kim, Doh Yon (Sungkyunkwan University)
//
// This source code is provided for portfolio review purposes only.
// Unauthorized copying, modification, or distribution is strictly prohibited.
//
// All Rights Reserved.
//--------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <ap_int.h>
#include <hls_stream.h>
#include <iostream>
#include <iomanip>

#include "m01300_a3.hpp"

// SHA-256 K constants (64 values, 32-bit each)
const ap_uint<32> SHA224_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#ifdef __CSIM__
// Helper function to print ap_uint<512> as ASCII (MSB first order for big endian)
void print_ap_uint_ascii(ap_uint<512> data) {
    char readable_char[MAX_LEN+1];
    for (int i = 0; i < MAX_LEN; i++) {
        // Extract byte from MSB to LSB: bit[511:504], [503:496], [495:488], ...
        char c = data.range(511 - i*8, 511 - i*8 - 7);
        if (c == 0) {
            break;
        }
        else {
            readable_char[i] = c;
        }
    }
    readable_char[MAX_LEN] = '\0'; // Null-terminate the string
    printf("%s | 0x", readable_char);
    // Print hex representation byte by byte
    for (int i = MAX_LEN-1; i >= 0; i--) {
        unsigned char byte_val = data.range(511 - i*8, 511 - i*8 - 7);
        printf("%02x", byte_val);
    }
    printf("\n");
}

// Helper function to print ap_uint<512> as hex (MSB first order for big endian)
void print_ap_uint_hex(ap_uint<512> data) {
    for (int i = 0; i < 64; i++) {
        unsigned char c = data.range(511 - i*8, 511 - i*8 - 7);
        printf("%02x", c);
        if ((i + 1) % 16 == 0) printf("\n");
        else if ((i + 1) % 8 == 0) printf("  ");
        else printf(" ");
    }
    printf("\n");
}
void print_ap_uint_hex(ap_uint<512> data) {
    for (int i = 0; i < 64; i++) {
        unsigned char c = data.range(511 - i*8, 511 - i*8 - 7);
        printf("%02x", c);
        if ((i + 1) % 16 == 0) printf("\n");
        else if ((i + 1) % 8 == 0) printf("  ");
        else printf(" ");
    }
    printf("\n");
}
#endif

#define SHA224_STAGE(i) sha224_core<i>(a[i], b[i], c[i], d[i], e[i], f[i], g[i], h[i], w[i], \
                                       a[i+1], b[i+1], c[i+1], d[i+1], e[i+1], f[i+1], g[i+1], h[i+1], w[i+1]);

// SHA-256 uses 32-bit rotation
inline ap_uint<32> rotr(ap_uint<32> in, int n) {
    return (in >> n) | (in << (32 - n));
}

// SHA-256 sigma functions (different from SHA-512)
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

// Configurable 32-bit adder: DSP48 split path or pure LUT implementation
template<>
inline ap_uint<32> add32<true>(ap_uint<32> a, ap_uint<32> b) {
    #pragma HLS INLINE

    ap_uint<24> a_lo = a.range(23, 0);
    ap_uint<8> a_hi = a.range(31, 24);
    ap_uint<24> b_lo = b.range(23, 0);
    ap_uint<8> b_hi = b.range(31, 24);

    ap_uint<25> sum_lo = a_lo + b_lo;
    #pragma HLS BIND_OP variable=sum_lo op=add impl=dsp

    ap_uint<1> carry = sum_lo[24];
    ap_uint<9> sum_hi = a_hi + b_hi + carry;

    ap_uint<32> result;
    result.range(23, 0) = sum_lo.range(23, 0);
    result.range(31, 24) = sum_hi.range(7, 0);

    return result;
}

template<>
inline ap_uint<32> add32<false>(ap_uint<32> a, ap_uint<32> b) {
    #pragma HLS INLINE

    ap_uint<32> sum = a + b;
    return sum;
}

template<int ROUND>
void wgen(
    ap_uint<32>     cur_w[16],
    ap_uint<32>     nxt_w[16]
) {
    #pragma HLS INLINE
    #pragma HLS ARRAY_PARTITION variable=cur_w complete
    #pragma HLS ARRAY_PARTITION variable=nxt_w complete

    if (ROUND < 15) {
        for (int k = 0; k < 15; k++) {
            #pragma HLS UNROLL
            nxt_w[k] = cur_w[k+1];
        }
        nxt_w[15] = cur_w[0];
    }
    else {
        ap_uint<32> w_level1_a = add32<false>(cur_w[1], sigma0(cur_w[2]));
        ap_uint<32> w_level1_b = add32<false>(cur_w[10], sigma1(cur_w[15]));
        nxt_w[0] = add32<false>(w_level1_a, w_level1_b);
        for (int k = 1; k < 15; k++) {
            #pragma HLS UNROLL
            nxt_w[k] = cur_w[k+1];
        }
        nxt_w[15] = cur_w[0];
    }
}

template<int ROUND>
void dgen(
    ap_uint<32> a, ap_uint<32> b, ap_uint<32> c, ap_uint<32> d,
    ap_uint<32> e, ap_uint<32> f, ap_uint<32> g, ap_uint<32> h,
    ap_uint<32> w_t,
    ap_uint<32> &a_out, ap_uint<32> &b_out, ap_uint<32> &c_out, ap_uint<32> &d_out,
    ap_uint<32> &e_out, ap_uint<32> &f_out, ap_uint<32> &g_out, ap_uint<32> &h_out
) {
    #pragma HLS INLINE
    ap_uint<32> T1;
    ap_uint<32> T2;

    ap_uint<32> t1_level1_a = add32<false>(h, Sigma1(e));
    ap_uint<32> t1_level1_b = add32<false>(Ch(e,f,g), SHA224_K[ROUND]);
    ap_uint<32> t1_level2_a = add32<false>(t1_level1_a, t1_level1_b);
    T1 = add32<false>(t1_level2_a, w_t);

    T2 = add32<false>(Sigma0(a), Maj(a,b,c));

    h_out = g;
    g_out = f;
    f_out = e;
    e_out = add32<false>(d, T1);
    d_out = c;
    c_out = b;
    b_out = a;
    a_out = add32<false>(T1, T2);
}

template<int POS>
void bf_core (
    bool            start,
    bool            active,
    ap_uint<1>      cin,
    ap_uint<1>      &cout,
    ap_uint<7>      &cur_cnt,
    ap_uint<512>    cand_in,
    ap_uint<512>    &cand_out
) {
    #pragma HLS INLINE

    const unsigned char CHARSET[CHARSET_LEN] = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', \
        'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', \
        'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', \
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', \
        'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', \
        '8', '9', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', \
        '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '@', \
        '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~'
    };
    #pragma HLS bind_storage    variable=CHARSET    type=ROM_1P impl=BRAM latency=1

    ap_uint<7>  nxt_cnt;

    if (!active) {
        cur_cnt  = ap_uint<7>(0);
        cout     = ap_uint<1>(0);
        cand_out = cand_in;
        return;
    }

    if (start) {
        cur_cnt  = ap_uint<7>(0);
        nxt_cnt  = ap_uint<7>(0);
        cout     = ap_uint<1>(0);
        cand_out = cand_in;
        cand_out.range(511 - 8*(POS + 1), 511 - 8*(POS + 1) - 7) = CHARSET[0];
    }
    else {
    nxt_cnt  = cur_cnt + cin;
        cout     = (nxt_cnt == CHARSET_LEN);
        nxt_cnt  = (cout) ? ap_uint<7>(0) : nxt_cnt;
        cand_out = cand_in;
        cand_out.range(511 - 8*(POS + 1), 511 - 8*(POS + 1) - 7) = CHARSET[nxt_cnt];
        cur_cnt  = nxt_cnt;
    }
}


template<int ROUND>
void sha224_core(
    ap_uint<32> a, ap_uint<32> b, ap_uint<32> c, ap_uint<32> d,
    ap_uint<32> e, ap_uint<32> f, ap_uint<32> g, ap_uint<32> h,
    ap_uint<32> cur_w[16],
    ap_uint<32> &a_out, ap_uint<32> &b_out, ap_uint<32> &c_out, ap_uint<32> &d_out,
    ap_uint<32> &e_out, ap_uint<32> &f_out, ap_uint<32> &g_out, ap_uint<32> &h_out,
    ap_uint<32> nxt_w[16]
) {
    #pragma HLS INLINE off
    #pragma HLS LATENCY min=1 max=1
    #pragma HLS ARRAY_PARTITION variable=cur_w complete
    #pragma HLS ARRAY_PARTITION variable=nxt_w complete

    wgen<ROUND>(cur_w, nxt_w);
    dgen<ROUND>(a, b, c, d, e, f, g, h, cur_w[0], a_out, b_out, c_out, d_out, e_out, f_out, g_out, h_out);
}

// SHA224 pipeline with independent counter per core
void sha224_pipe(int core_id, ap_uint<48> size, hls::stream<ap_uint<512>>& i_strm, hls::stream<ap_uint<512>>& o_strm) 
{
    #pragma HLS INLINE off

PIPE_SHA224:
    for (ap_uint<48> sk = 0; sk < size; sk++) {
        #pragma HLS PIPELINE II=1
        
        ap_uint<512> cand = i_strm.read();
        
        
        ap_uint<32> a[65], b[65], c[65], d[65], e[65], f[65], g[65], h[65];
        ap_uint<32> w[65][16];
        
        // Physical optimization: Compact array partitioning for better placement
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
            w[0][kk] = cand.range(511 - 32*kk, 480 - 32*kk);
        }

        // Initial digest - SHA-224 uses different initial values (from 9th-16th primes, lower 32 bits)
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
        
        // Use DSP-based 32-bit additions for final hash computation (SHA-224 initial values)
        ap_uint<32> h0 = add32<false>(a[64], ap_uint<32>(0xc1059ed8));
        ap_uint<32> h1 = add32<false>(b[64], ap_uint<32>(0x367cd507));
        ap_uint<32> h2 = add32<false>(c[64], ap_uint<32>(0x3070dd17));
        ap_uint<32> h3 = add32<false>(d[64], ap_uint<32>(0xf70e5939));
        ap_uint<32> h4 = add32<false>(e[64], ap_uint<32>(0xffc00b31));
        ap_uint<32> h5 = add32<false>(f[64], ap_uint<32>(0x68581511));
        ap_uint<32> h6 = add32<false>(g[64], ap_uint<32>(0x64f98fa7));
        // SHA-224 truncates to 224 bits (7 words), so h7 is not used in output

        ap_uint<224> digest;
        digest.range(223-32*0, 224-32*1) = h0;
        digest.range(223-32*1, 224-32*2) = h1;
        digest.range(223-32*2, 224-32*3) = h2;
        digest.range(223-32*3, 224-32*4) = h3;
        digest.range(223-32*4, 224-32*5) = h4;
        digest.range(223-32*5, 224-32*6) = h5;
        digest.range(223-32*6, 224-32*7) = h6;

        ap_uint<512> padded = 0;
        padded.range(223, 0) = digest;

    #if (0)
        printf("DBG: core %d produced digest:\n", core_idx);
        print_ap_uint_hex(padded);
    #endif

        o_strm.write(padded);
    }
}

void in_ctrl(ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, ap_uint<48> size, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt, hls::stream<ap_uint<512>> i_strm[NUM_CORE]) {
    #pragma HLS INLINE off

    ap_uint<7>      cur_cnt[MAX_LEN];
    #pragma HLS ARRAY_PARTITION variable=cur_cnt complete


    for (ap_uint<48> sk = 0; sk < size; sk++) {
        #pragma HLS PIPELINE II=1
        // Generate candidate string from counter
        ap_uint<1>      cout[MAX_LEN];
        ap_uint<512>    block[MAX_LEN]; 
        ap_uint<512>    root;

        #pragma HLS ARRAY_PARTITION variable=cout  complete
        #pragma HLS ARRAY_PARTITION variable=block complete
        
        bf_core<0>(sk == 0, (len > 0), ap_uint<1>(1), cout[0], cur_cnt[0],        0, block[0]);
        bf_core<1>(sk == 0, (len > 1),       cout[0], cout[1], cur_cnt[1], block[0], block[1]);
        bf_core<2>(sk == 0, (len > 2),       cout[1], cout[2], cur_cnt[2], block[1], block[2]);
        bf_core<3>(sk == 0, (len > 3),       cout[2], cout[3], cur_cnt[3], block[2], block[3]);
        bf_core<4>(sk == 0, (len > 4),       cout[3], cout[4], cur_cnt[4], block[3], block[4]);
        bf_core<5>(sk == 0, (len > 5),       cout[4], cout[5], cur_cnt[5], block[4], block[5]);
        bf_core<6>(sk == 0, (len > 6),       cout[5], cout[6], cur_cnt[6], block[5], block[6]);
        
        root = pad_variable_message(block[6], len + 1, salt, salt_position, use_salt);
        
        ap_uint<512>  cand[NUM_CORE];
        #pragma HLS ARRAY_PARTITION variable=cand  complete

        for (int pk = 0; pk < NUM_CORE; ++pk) {
            #pragma HLS UNROLL
            cand[pk] = root;
            // MSB character position depends on salt_position:
            // - salt_position == 0: MSB char goes after salt (447:440) for 512-bit blocks
            // - salt_position == 1 or no salt: MSB char goes at front (511:504)
            if (use_salt == 1 && salt_position == 0) {
                cand[pk].range(447, 440) = msb_chars.range(8*(pk + 1) - 1, 8*pk);
            } else {
                cand[pk].range(511, 511 - 7) = msb_chars.range(8*(pk + 1) - 1, 8*pk);
            }
            i_strm[pk].write(cand[pk]);
        #if (0)
            char dbg_str[MAX_LEN + 2];
            for (int i = 0; i < static_cast<int>(len+1); ++i) {
                int hi = 511 - (i*8);
                dbg_str[i] = static_cast<char>(cand[pk].range(hi, hi - 7));
            }
            dbg_str[static_cast<int>(len+1)] = '\0';
            printf("[CSIM] core %d counter=%llu candidate=%s\n", pk, sk, dbg_str);
        #endif
        }
    }
}

void out_ctrl(ap_uint<512> *output, ap_uint<48> *match_index, hls::stream<ap_uint<512>> o_strm[NUM_CORE], ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden) {
    #pragma HLS INLINE off

    if (mode == 0) {
        // Mode 0: Output all hashes
LOOP_MODE0:
        for (ap_uint<48> sk = 0; sk < size; sk++) {
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

ap_uint<512> pad_variable_message(ap_uint<512> msg_in, int len_bytes, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
    #pragma HLS INLINE

    ap_uint<512> ret = msg_in;

    // Calculate total length = message + (salt if used)
    int total_len_bytes = use_salt ? (len_bytes + 8) : len_bytes;
    int total_len_bits = total_len_bytes * 8;

    // If salt is used, construct salted message
    if (use_salt == 1) {
        // Clear the relevant part
        ret = 0;

        if (salt_position == 0) {
            // Salt at front: [salt 8 bytes][message]
            // Place salt at MSB (first 8 bytes = 64 bits)
            ret.range(511, 448) = salt;
            // Place message after salt
            if (len_bytes > 0) {
                int msg_bits = len_bytes * 8;
                ret.range(447, 448 - msg_bits) = msg_in.range(511, 512 - msg_bits);
            }
        } else {
            // Salt at back: [message][salt 8 bytes]
            // Place message at MSB first
            if (len_bytes > 0) {
                int msg_bits = len_bytes * 8;
                ret.range(511, 512 - msg_bits) = msg_in.range(511, 512 - msg_bits);
                // Place salt after message
                ret.range(511 - msg_bits, 448 - msg_bits) = salt;
            } else {
                // Only salt if message is empty
                ret.range(511, 448) = salt;
            }
        }
    }

    // Insert 0x80 after message (or salted message)
    ret[512 - total_len_bits - 1] = 1;

    // Insert message length in bits at the end (SHA-256 uses 64-bit length field)
    ret.range(63, 0) = total_len_bits;
    
    return ret;
}


extern "C" void m01300_a3(ap_uint<512> *output, ap_uint<48> *match_index, ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem0 depth=2000
    #pragma HLS INTERFACE m_axi port=match_index offset=slave bundle=gmem1 depth=1
    #pragma HLS INTERFACE s_axilite port=output bundle=control
    #pragma HLS INTERFACE s_axilite port=match_index bundle=control
    #pragma HLS INTERFACE s_axilite port=len bundle=control
    #pragma HLS INTERFACE s_axilite port=msb_chars bundle=control
    #pragma HLS INTERFACE s_axilite port=size bundle=control
    #pragma HLS INTERFACE s_axilite port=mode bundle=control
    #pragma HLS INTERFACE s_axilite port=golden bundle=control
    #pragma HLS INTERFACE s_axilite port=salt bundle=control
    #pragma HLS INTERFACE s_axilite port=salt_position bundle=control
    #pragma HLS INTERFACE s_axilite port=use_salt bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    #pragma HLS stable variable=len
    #pragma HLS stable variable=msb_chars
    #pragma HLS stable variable=size
    #pragma HLS stable variable=mode
    #pragma HLS stable variable=golden
    #pragma HLS stable variable=salt
    #pragma HLS stable variable=salt_position
    #pragma HLS stable variable=use_salt
    #pragma HLS DATAFLOW

    hls::stream<ap_uint<512>>  i_strm[NUM_CORE];
    hls::stream<ap_uint<512>>  o_strm[NUM_CORE];
    #pragma HLS STREAM variable=i_strm depth=128
    #pragma HLS STREAM variable=o_strm depth=128
    #pragma HLS ARRAY_PARTITION variable=i_strm complete dim=1
    #pragma HLS ARRAY_PARTITION variable=o_strm complete dim=1
    
    in_ctrl(len, msb_chars, size, salt, salt_position, use_salt, i_strm);

    sha224_pipe(0, size, i_strm[0], o_strm[0]);
    sha224_pipe(1, size, i_strm[1], o_strm[1]);
#if NUM_CORE > 2
    sha224_pipe(2, size, i_strm[2], o_strm[2]);
#endif
#if NUM_CORE > 3
    sha224_pipe(3, size, i_strm[3], o_strm[3]);
#endif

    out_ctrl(output, match_index, o_strm, size, mode, golden);
}
