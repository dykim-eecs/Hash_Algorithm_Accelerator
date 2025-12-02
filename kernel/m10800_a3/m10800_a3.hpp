//--------------------------------------------------------------------------------------------------
// Copyright (c) 2025 Kim, Doh Yon (Sungkyunkwan University)
//
// This source code is provided for portfolio review purposes only.
// Unauthorized copying, modification, or distribution is strictly prohibited.
//
// All Rights Reserved.
//--------------------------------------------------------------------------------------------------
#ifndef __M10800_A3_HPP__
#define __M10800_A3_HPP__

#ifndef MAX_LEN
#define MAX_LEN 7
#endif

#ifndef COUNTER_BYTES
#define COUNTER_BYTES 6
#endif

#ifndef NUM_CORE
#define NUM_CORE 3
#endif

// ALNUM_SPECIAL charset definition (94 characters)
// a-z (26) + A-Z (26) + 0-9 (10) + special chars (32) = 94 total
#ifndef CHARSET_CONTENT
#define CHARSET_CONTENT \
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', \
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', \
    'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', \
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', \
    'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', \
    '8', '9', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', \
    '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '@', \
    '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~'
#endif

#ifndef CHARSET_LEN
#define CHARSET_LEN 94
#endif

// Function prototypes
#include <ap_int.h>
#include <hls_stream.h>

// Global constant tables
extern const ap_uint<64>    SHA384_K[80];

// Core SHA384 functions
inline ap_uint<64> rotr(ap_uint<64> in, int n);
inline ap_uint<64> sigma0(ap_uint<64> w);
inline ap_uint<64> sigma1(ap_uint<64> w);
inline ap_uint<64> Sigma0(ap_uint<64> in);
inline ap_uint<64> Sigma1(ap_uint<64> in);
inline ap_uint<64> Ch(ap_uint<64> x, ap_uint<64> y, ap_uint<64> z);
inline ap_uint<64> Maj(ap_uint<64> x, ap_uint<64> y, ap_uint<64> z);

// Configurable 64-bit adder
template<bool use_dsp>
inline ap_uint<64> add64(ap_uint<64> a, ap_uint<64> b);

// SHA384 pipeline functions
template<int ROUND>
void wgen(ap_uint<64> cur_w[16], ap_uint<64> nxt_w[16]);

template<int ROUND>
void dgen(
    ap_uint<64> a, ap_uint<64> b, ap_uint<64> c, ap_uint<64> d,
    ap_uint<64> e, ap_uint<64> f, ap_uint<64> g, ap_uint<64> h,
    ap_uint<64> w_t,
    ap_uint<64> &a_out, ap_uint<64> &b_out, ap_uint<64> &c_out, ap_uint<64> &d_out,
    ap_uint<64> &e_out, ap_uint<64> &f_out, ap_uint<64> &g_out, ap_uint<64> &h_out
);

template<int ROUND>
void sha384_core(
    ap_uint<64> a, ap_uint<64> b, ap_uint<64> c, ap_uint<64> d,
    ap_uint<64> e, ap_uint<64> f, ap_uint<64> g, ap_uint<64> h,
    ap_uint<64> cur_w[16],
    ap_uint<64> &a_out, ap_uint<64> &b_out, ap_uint<64> &c_out, ap_uint<64> &d_out,
    ap_uint<64> &e_out, ap_uint<64> &f_out, ap_uint<64> &g_out, ap_uint<64> &h_out,
    ap_uint<64> nxt_w[16]
);

// Candidate generation and padding
ap_uint<1024> generate_candidate(ap_uint<48> counter, ap_uint<6> len, ap_uint<8> msb_char, int core_id);
ap_uint<1024> pad_variable_message(ap_uint<1024> msg_in, int len_bytes, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt);

// Pipeline and control functions (384-bit output for SHA-384)
void in_ctrl(ap_uint<6> len, ap_uint<8*NUM_CORE> msb_chars, ap_uint<48> size, ap_uint<64> salt, ap_uint<1> salt_position, ap_uint<1> use_salt, hls::stream<ap_uint<1024>> i_strm[NUM_CORE]);
void sha384_pipe(int core_id, ap_uint<48> size, hls::stream<ap_uint<1024>>& i_strm, hls::stream<ap_uint<512>>& o_strm);
void out_ctrl(ap_uint<512> *output, ap_uint<48> *match_index, hls::stream<ap_uint<512>> o_strm[NUM_CORE], ap_uint<48> size, ap_uint<1> mode, ap_uint<512> golden);

#endif // __M10800_A3_HPP__
