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

#include "m10800_a3.hpp"

const ap_uint<64> SHA384_K[80] = {
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

#ifdef __CSIM__
// Helper function to print ap_uint<1024> as ASCII (MSB first order for big endian)
void print_ap_uint_ascii(ap_uint<1024> data) {
    char readable_char[MAX_LEN+1];
    for (int i = 0; i < MAX_LEN; i++) {
        // Extract byte from MSB to LSB: bit[1023:1016], [1015:1008], [1007:1000], ...
        char c = data.range(1023 - i*8, 1023 - i*8 - 7);
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
        unsigned char byte_val = data.range(1023 - i*8, 1023 - i*8 - 7);
        printf("%02x", byte_val);
    }
    printf("\n");
}

// Helper function to print ap_uint<1024> as hex (MSB first order for big endian)
void print_ap_uint_hex(ap_uint<1024> data) {
    for (int i = 0; i < 128; i++) {
        unsigned char c = data.range(1023 - i*8, 1023 - i*8 - 7);
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

#define SHA384_STAGE(i) sha384_core<i>(a[i], b[i], c[i], d[i], e[i], f[i], g[i], h[i], w[i], \
                                       a[i+1], b[i+1], c[i+1], d[i+1], e[i+1], f[i+1], g[i+1], h[i+1], w[i+1]);

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

// Configurable 64-bit adder: DSP48 split path or pure LUT implementation
template<>
inline ap_uint<64> add64<true>(ap_uint<64> a, ap_uint<64> b) {
    #pragma HLS INLINE

    ap_uint<48> a_lo = a.range(47, 0);
    ap_uint<16> a_hi = a.range(63, 48);
    ap_uint<48> b_lo = b.range(47, 0);
    ap_uint<16> b_hi = b.range(63, 48);

    ap_uint<49> sum_lo = a_lo + b_lo;
    #pragma HLS BIND_OP variable=sum_lo op=add impl=dsp

    ap_uint<1> carry = sum_lo[48];
    ap_uint<17> sum_hi = a_hi + b_hi + carry;

    ap_uint<64> result;
    result.range(47, 0) = sum_lo.range(47, 0);
    result.range(63, 48) = sum_hi.range(15, 0);

    return result;
}

template<>
inline ap_uint<64> add64<false>(ap_uint<64> a, ap_uint<64> b) {
    #pragma HLS INLINE

    ap_uint<64> sum = a + b;
    return sum;
}

template<int ROUND>
void wgen(
    ap_uint<64>     cur_w[16],
    ap_uint<64>     nxt_w[16]
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
        ap_uint<64> w_level1_a = add64<false>(cur_w[1], sigma0(cur_w[2]));
        ap_uint<64> w_level1_b = add64<false>(cur_w[10], sigma1(cur_w[15]));
        nxt_w[0] = add64<false>(w_level1_a, w_level1_b);
        for (int k = 1; k < 15; k++) {
            #pragma HLS UNROLL
            nxt_w[k] = cur_w[k+1];
        }
        nxt_w[15] = cur_w[0];
    }
}

template<int ROUND>
void dgen(
    ap_uint<64> a, ap_uint<64> b, ap_uint<64> c, ap_uint<64> d,
    ap_uint<64> e, ap_uint<64> f, ap_uint<64> g, ap_uint<64> h,
    ap_uint<64> w_t,
    ap_uint<64> &a_out, ap_uint<64> &b_out, ap_uint<64> &c_out, ap_uint<64> &d_out,
    ap_uint<64> &e_out, ap_uint<64> &f_out, ap_uint<64> &g_out, ap_uint<64> &h_out
) {
    #pragma HLS INLINE
    ap_uint<64> T1;
    ap_uint<64> T2;

    ap_uint<64> t1_level1_a = add64<false>(h, Sigma1(e));
    ap_uint<64> t1_level1_b = add64<false>(Ch(e,f,g), SHA384_K[ROUND]);
    ap_uint<64> t1_level2_a = add64<false>(t1_level1_a, t1_level1_b);
    T1 = add64<false>(t1_level2_a, w_t);

    T2 = add64<false>(Sigma0(a), Maj(a,b,c));

    h_out = g;
    g_out = f;
    f_out = e;
    e_out = add64<false>(d, T1);
    d_out = c;
    c_out = b;
    b_out = a;
    a_out = add64<false>(T1, T2);
}

template<int POS>
void bf_core (
    bool            start,
    bool            active,
    ap_uint<1>      cin,
    ap_uint<1>      &cout,
    ap_uint<7>      &cur_cnt,
    ap_uint<1024>   cand_in,
    ap_uint<1024>   &cand_out
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
        cand_out.range(1023 - 8*(POS + 1), 1023 - 8*(POS + 1) - 7) = CHARSET[0];
    }
    else {
    nxt_cnt  = cur_cnt + cin;
        cout     = (nxt_cnt == CHARSET_LEN);
        nxt_cnt  = (cout) ? ap_uint<7>(0) : nxt_cnt;
        cand_out = cand_in;
        cand_out.range(1023 - 8*(POS + 1), 1023 - 8*(POS + 1) - 7) = CHARSET[nxt_cnt];
        cur_cnt  = nxt_cnt;
    }
}


template<int ROUND>
void sha384_core(
    ap_uint<64> a, ap_uint<64> b, ap_uint<64> c, ap_uint<64> d,
    ap_uint<64> e, ap_uint<64> f, ap_uint<64> g, ap_uint<64> h,
    ap_uint<64> cur_w[16],
    ap_uint<64> &a_out, ap_uint<64> &b_out, ap_uint<64> &c_out, ap_uint<64> &d_out,
    ap_uint<64> &e_out, ap_uint<64> &f_out, ap_uint<64> &g_out, ap_uint<64> &h_out,
    ap_uint<64> nxt_w[16]
) {
    #pragma HLS INLINE off
    #pragma HLS LATENCY min=1 max=1
    #pragma HLS ARRAY_PARTITION variable=cur_w complete
    #pragma HLS ARRAY_PARTITION variable=nxt_w complete

    wgen<ROUND>(cur_w, nxt_w);
    dgen<ROUND>(a, b, c, d, e, f, g, h, cur_w[0], a_out, b_out, c_out, d_out, e_out, f_out, g_out, h_out);
}

// SHA384 pipeline with independent counter per core
void sha384_pipe(int core_id, ap_uint<48> size, hls::stream<ap_uint<1024>> &i_strm, hls::stream<ap_uint<512>> &o_strm) 
{
    #pragma HLS INLINE off

PIPE_SHA384:
    for (ap_uint<48> sk = 0; sk < size; sk++) {
        #pragma HLS PIPELINE II=1
        
        ap_uint<1024> cand = i_strm.read();
        
        
        ap_uint<64> a[81], b[81], c[81], d[81], e[81], f[81], g[81], h[81];
        ap_uint<64> w[81][16];
        
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

        // Initial W[0]
        for (int kk = 0; kk < 16; kk++) {
            #pragma HLS UNROLL
            w[0][kk] = cand.range(1023 - 64*kk, 960 - 64*kk);
        }

        // Initial digest - SHA-384 uses different initial values (from 9th-16th primes)
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
        
        // Use DSP-based 64-bit additions for final hash computation (SHA-384 initial values)
        ap_uint<64> h0 = add64<false>(a[80], ap_uint<64>(0xcbbb9d5dc1059ed8UL));
        ap_uint<64> h1 = add64<false>(b[80], ap_uint<64>(0x629a292a367cd507UL));
        ap_uint<64> h2 = add64<false>(c[80], ap_uint<64>(0x9159015a3070dd17UL));
        ap_uint<64> h3 = add64<false>(d[80], ap_uint<64>(0x152fecd8f70e5939UL));
        ap_uint<64> h4 = add64<false>(e[80], ap_uint<64>(0x67332667ffc00b31UL));
        ap_uint<64> h5 = add64<false>(f[80], ap_uint<64>(0x8eb44a8768581511UL));
        // SHA-384 truncates to 384 bits (6 words), so h6 and h7 are not used in output

        ap_uint<384> digest;
        digest.range(383-64*0, 384-64*1) = h0;
        digest.range(383-64*1, 384-64*2) = h1;
        digest.range(383-64*2, 384-64*3) = h2;
        digest.range(383-64*3, 384-64*4) = h3;
        digest.range(383-64*4, 384-64*5) = h4;
        digest.range(383-64*5, 384-64*6) = h5;

        ap_uint<512> padded = 0;
        padded.range(383, 0) = digest;

    #if (0)
        printf("DBG: core %d produced digest:\n", core_idx);
        print_ap_uint_hex(padded);
    #endif

        o_strm.write(padded);
    }
}

void in_ctrl(ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, ap_uint<48> size, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt, hls::stream<ap_uint<1024>> i_strm[NUM_CORE]) {
    #pragma HLS INLINE off

    ap_uint<7>      cur_cnt[MAX_LEN];
    #pragma HLS ARRAY_PARTITION variable=cur_cnt complete


    for (ap_uint<48> sk = 0; sk < size; sk++) {
        #pragma HLS PIPELINE II=1
        // Generate candidate string from counter
        ap_uint<1>      cout[MAX_LEN];
        ap_uint<1024>   block[MAX_LEN]; 
        ap_uint<1024>   root;

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
        
        ap_uint<1024>  cand[NUM_CORE];
        #pragma HLS ARRAY_PARTITION variable=cand  complete

        for (int pk = 0; pk < NUM_CORE; ++pk) {
            #pragma HLS UNROLL
            cand[pk] = root;
            // MSB character position depends on salt_position:
            // - salt_position == 0: MSB char goes after salt (959:952)
            // - salt_position == 1 or no salt: MSB char goes at front (1023:1016)
            if (use_salt == 1 && salt_position == 0) {
                cand[pk].range(959, 952) = msb_chars.range(8*(pk + 1) - 1, 8*pk);
            } else {
                cand[pk].range(1023, 1023 - 7) = msb_chars.range(8*(pk + 1) - 1, 8*pk);
            }
            i_strm[pk].write(cand[pk]);
        #if (0)
            char dbg_str[MAX_LEN + 2];
            for (int i = 0; i < static_cast<int>(len+1); ++i) {
                int hi = 1023 - (i*8);
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

ap_uint<1024> pad_variable_message(ap_uint<1024> msg_in, int len_bytes, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
    #pragma HLS INLINE

    ap_uint<1024> ret = msg_in;

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
            ret.range(1023, 960) = salt;
            // Place message after salt
            if (len_bytes > 0) {
                int msg_bits = len_bytes * 8;
                ret.range(959, 960 - msg_bits) = msg_in.range(1023, 1024 - msg_bits);
            }
        } else {
            // Salt at back: [message][salt 8 bytes]
            // Place message at MSB first
            if (len_bytes > 0) {
                int msg_bits = len_bytes * 8;
                ret.range(1023, 1024 - msg_bits) = msg_in.range(1023, 1024 - msg_bits);
                // Place salt after message
                ret.range(1023 - msg_bits, 960 - msg_bits) = salt;
            } else {
                // Only salt if message is empty
                ret.range(1023, 960) = salt;
            }
        }
    }

    // Insert 0x80 after message (or salted message)
    ret[1024 - total_len_bits - 1] = 1;

    // Insert message length in bits at the end
    ret.range(127, 0) = total_len_bits;
    
    return ret;
}


extern "C" void m10800_a3(ap_uint<512> *output, ap_uint<48> *match_index, ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt) {
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

    hls::stream<ap_uint<1024>>  i_strm[NUM_CORE];
    hls::stream<ap_uint<512>>   o_strm[NUM_CORE];
    #pragma HLS STREAM variable=i_strm depth=128
    #pragma HLS STREAM variable=o_strm depth=128
    #pragma HLS ARRAY_PARTITION variable=i_strm complete dim=1
    #pragma HLS ARRAY_PARTITION variable=o_strm complete dim=1
    
    in_ctrl(len, msb_chars, size, salt, salt_position, use_salt, i_strm);

    sha384_pipe(0, size, i_strm[0], o_strm[0]);
    sha384_pipe(1, size, i_strm[1], o_strm[1]);
#if NUM_CORE > 2
    sha384_pipe(2, size, i_strm[2], o_strm[2]);
#endif
#if NUM_CORE > 3
    sha384_pipe(3, size, i_strm[3], o_strm[3]);
#endif

    out_ctrl(output, match_index, o_strm, size, mode, golden);
}
