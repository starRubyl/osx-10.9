/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "ossl-hmac.h"
#include "ossl-evp.h"
#include "ossl-evp-cc.h"

#include "test_common.h"

static int verbose = 1;

static int total = 0;
static int pass = 0;
static int fail = 0;

/*
 *  HMAC Test Engine.
 */

typedef const EVP_MD * (*evp_alg_t)(void);

static int
_hmac_compare(evp_alg_t alg, int i, const void *key, size_t keylen, const void *msg, size_t msglen, const void *md)
{
	const EVP_MD *evp_md = (*alg)();
	unsigned mdlen = EVP_MD_size(evp_md);
	HMAC_CTX ctx;
	uint8_t out[mdlen];

	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, key, keylen, evp_md, NULL);
	HMAC_Update(&ctx, msg, msglen);
	HMAC_Final(&ctx, out, &mdlen);
	HMAC_CTX_cleanup(&ctx);
	if ((mdlen != EVP_MD_size(evp_md)) || (memcmp(out, md, mdlen) != 0)) {
		if (verbose)  printf("Result doesn't match vector (%d)\n", i);
		return (1);
	}

	return (0);
}

static int
_test_hmac(evp_alg_t alg, byteVector_t *key[], stringVector_t *msg[], byteVector_t *md[], const size_t num_tests)
{
	size_t i;
	int err = 0;

	for (i = 0; i < num_tests; i++) {
		err = _hmac_compare(alg, i, key[i]->value, key[i]->len, msg[i]->value,
		    msg[i]->len, md[i]->value);
		if (err) return (err);
	}
	return (0);
}

/*
 * Vector Macros:
 *
 * HMAC_KEY() is the HMAC key (in hex).
 * Parameters: (<Digest Name>, <vector #>, <key length>, <vector data in hex>)
 * HMAC_MSG() is the message (string) to digest.
 * HMAC_MD() is the message digest (in hex).
 * Parameters: (<Digest Name>, <vector #>, <vector length> <vector data: string or hex numbers>)
 */
#define	HMAC_KEY(NAME, N, LEN, VALUE...) static byteVector_t	NAME ## _hmac_key_ ## N = \
						{ LEN, { VALUE } };
#define	HMAC_MSG(NAME, N, VALUE)	static stringVector_t	NAME ## _hmac_msg_ ## N =  \
						{ sizeof(VALUE)-1, VALUE };
#define	HMAC_MD(NAME, N, LEN, VALUE...)	static byteVector_t	NAME ## _hmac_md_ ## N = \
						{ LEN, { VALUE } };

/*
 * HMAC_TEST() generates the testing code stub.
 * Parameters: (<HMAC Name>, <Number of test vectors>)
 */
#define	HMAC_TEST(ALG_NAME, NUM_TESTS) 							\
static byteVector_t * ALG_NAME ## _hmac_key_vects[] =					\
	{ ARRAY_FOR_ ## NUM_TESTS (& ALG_NAME ## _hmac_key_ ) NULL };			\
											\
static stringVector_t * ALG_NAME ## _hmac_msg_vects[] =					\
	{ ARRAY_FOR_ ## NUM_TESTS (& ALG_NAME ## _hmac_msg_ ) NULL };			\
											\
static byteVector_t * ALG_NAME ## _hmac_md_vects[] =					\
	{ ARRAY_FOR_ ## NUM_TESTS (& ALG_NAME ## _hmac_md_ ) NULL };			\
											\
static int testHmac_ ## ALG_NAME(void) {						\
	return (_test_hmac(EVP_ ## ALG_NAME, 						\
		ALG_NAME ## _hmac_key_vects, ALG_NAME ## _hmac_msg_vects,		\
		ALG_NAME ## _hmac_md_vects, NUM_TESTS));				\
}

/*
 * HMAC SHA-1 Test Vectors.
 */
/* from http://csrc.nist.gov/publications/fips/fips198/fips-198a.pdf */
HMAC_KEY(sha1, 0, 20,	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
			0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
			0x40,0x41,0x42,0x43)
HMAC_MSG(sha1, 0,	"Sample #2")
HMAC_MD(sha1,  0, 20,	0x09,0x22,0xd3,0x40,0x5f,0xaa,0x3d,0x19,
			0x4f,0x82,0xa4,0x58,0x30,0x73,0x7d,0x5c,
			0xc6,0xc7,0x5d,0x24)

HMAC_TEST(sha1, 1)

/*
 * HMAC SHA224 Test Vectors.
 */
HMAC_KEY(sha224, 0, 20,	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
			0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
			0x40,0x41,0x42,0x43)
HMAC_MSG(sha224, 0,	"Sample #2")
HMAC_MD(sha224,  0, 28,	0xdd,0xef,0x0a,0x40,0xcb,0x7d,0x50,0xfb,
			0x6e,0xe6,0xce,0xa1,0x20,0xba,0x26,0xaa,
			0x08,0xf3,0x07,0x75,0x87,0xb8,0xad,0x1b,
			0x8c,0x8d,0x12,0xc7)

HMAC_TEST(sha224, 1)

/*
 * HMAC SHA256 Test Vectors.
 */
HMAC_KEY(sha256, 0, 20,	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
			0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
			0x40,0x41,0x42,0x43)
HMAC_MSG(sha256, 0,	"Sample #2")
HMAC_MD(sha256,  0, 32,	0xb8,0xf2,0x0d,0xb5,0x41,0xea,0x43,0x09,
			0xca,0x4e,0xa9,0x38,0x0c,0xd0,0xe8,0x34,
			0xf7,0x1f,0xbe,0x91,0x74,0xa2,0x61,0x38,
			0x0d,0xc1,0x7e,0xae,0x6a,0x34,0x51,0xd9)

HMAC_TEST(sha256, 1)

/*
 * HMAC SHA384 Test Vectors.
 */
HMAC_KEY(sha384, 0, 20,	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
			0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
			0x40,0x41,0x42,0x43)
HMAC_MSG(sha384, 0,	"Sample #2")
HMAC_MD(sha384,  0, 48,	0x08,0xbc,0xb0,0xda,0x49,0x1e,0x87,0xad,
			0x9a,0x1d,0x6a,0xce,0x23,0xc5,0x0b,0xf6,
			0xb7,0x18,0x06,0xa5,0x77,0xcd,0x49,0x04,
			0x89,0xf1,0xe6,0x23,0x44,0x51,0x51,0x9f,
			0x85,0x56,0x80,0x79,0x0c,0xbd,0x4d,0x50,
			0xa4,0x5f,0x29,0xe3,0x93,0xf0,0xe8,0x7f)

HMAC_TEST(sha384, 1)

/*
 * HMAC SHA512 Test Vectors.
 */
HMAC_KEY(sha512, 0, 20,	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
			0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
			0x40,0x41,0x42,0x43)
HMAC_MSG(sha512, 0,	"Sample #2")
HMAC_MD(sha512,  0, 64,	0x80,0x9d,0x44,0x05,0x7c,0x5b,0x95,0x41,
			0x05,0xbd,0x04,0x13,0x16,0xdb,0x0f,0xac,
			0x44,0xd5,0xa4,0xd5,0xd0,0x89,0x2b,0xd0,
			0x4e,0x86,0x64,0x12,0xc0,0x90,0x77,0x68,
			0xf1,0x87,0xb7,0x7c,0x4f,0xae,0x2c,0x2f,
			0x21,0xa5,0xb5,0x65,0x9a,0x4f,0x4b,0xa7,
			0x47,0x02,0xa3,0xde,0x9b,0x51,0xf1,0x45,
			0xbd,0x4f,0x25,0x27,0x42,0x98,0x99,0x05)

HMAC_KEY(sha512, 1, 20, 0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
    			0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
			0x0b,0x0b,0x0b,0x0b)
HMAC_MSG(sha512, 1,	"Hi There")
HMAC_MD(sha512,  1, 64,	0x87,0xaa,0x7c,0xde,0xa5,0xef,0x61,0x9d,
    			0x4f,0xf0,0xb4,0x24,0x1a,0x1d,0x6c,0xb0,
			0x23,0x79,0xf4,0xe2,0xce,0x4e,0xc2,0x78,
			0x7a,0xd0,0xb3,0x05,0x45,0xe1,0x7c,0xde,
			0xda,0xa8,0x33,0xb7,0xd6,0xb8,0xa7,0x02,
			0x03,0x8b,0x27,0x4e,0xae,0xa3,0xf4,0xe4,
			0xbe,0x9d,0x91,0x4e,0xeb,0x61,0xf1,0x70,
			0x2e,0x69,0x6c,0x20,0x3a,0x12,0x68,0x54)
HMAC_TEST(sha512, 2)

/*
 * HMAC MD5 Test Vectors.
 * Test Vectors from RFC 2104.
 */
HMAC_KEY(md5,  0, 16,	0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
			0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b)
HMAC_MSG(md5,  0,	"Hi There")
HMAC_MD(md5,   0, 16,	0x92,0x94,0x72,0x7a,0x36,0x38,0xbb,0x1c,
			0x13,0xf4,0x8e,0xf8,0x15,0x8b,0xfc,0x9d)

HMAC_KEY(md5,  1,  4,	0x4a,0x65,0x66,0x65)
HMAC_MSG(md5,  1,	"what do ya want for nothing?")
HMAC_MD(md5,   1, 16,	0x75,0x0c,0x78,0x3e,0x6a,0xb0,0xb5,0x03,
			0xea,0xa8,0x6e,0x31,0x0a,0x5d,0xb7,0x38)

HMAC_KEY(md5,  2, 16,	0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
			0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa)
HMAC_MSG(md5,  2,	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd")
HMAC_MD(md5,   2, 16,	0x56,0xbe,0x34,0x52,0x1d,0x14,0x4c,0x88,
			0xdb,0xb8,0xc7,0x33,0xf0,0xe8,0xb3,0xf6)

HMAC_TEST(md5, 3)

/*
 * HMAC RMD128 Test Vectors.
 * Test Vectors from RFC 2286.
 */
HMAC_KEY(rmd128, 0, 16,	0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
			0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b)
HMAC_MSG(rmd128, 0,	"Hi There")
HMAC_MD(rmd128,  0, 16, 0xfb,0xf6,0x1f,0x94,0x92,0xaa,0x4b,0xbf,
			0x81,0xc1,0x72,0xe8,0x4e,0x07,0x34,0xdb)

HMAC_KEY(rmd128, 1,  4,	0x4a,0x65,0x66,0x65)
HMAC_MSG(rmd128, 1,	"what do ya want for nothing?")
HMAC_MD(rmd128,  1, 16, 0x87,0x5f,0x82,0x88,0x62,0xb6,0xb3,0x34,
			0xb4,0x27,0xc5,0x5f,0x9f,0x7f,0xf0,0x9b)

HMAC_KEY(rmd128, 2, 16,	0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
			0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa)
HMAC_MSG(rmd128, 2,	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd")
HMAC_MD(rmd128, 2, 16,	0x09,0xf0,0xb2,0x84,0x6d,0x2f,0x54,0x3d,
			0xa3,0x63,0xcb,0xec,0x8d,0x62,0xa3,0x8d)

HMAC_TEST(rmd128, 3)

/*
 * HMAC RMD160 Test Vectors.
 * Test Vectors from RFC 2286.
 */
HMAC_KEY(rmd160, 0, 20,	0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
			0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,
			0x0b,0x0b,0x0b,0x0b)
HMAC_MSG(rmd160, 0,	"Hi There")
HMAC_MD(rmd160,  0, 20, 0x24,0xcb,0x4b,0xd6,0x7d,0x20,0xfc,0x1a,
			0x5d,0x2e,0xd7,0x73,0x2d,0xcc,0x39,0x37,
			0x7f,0x0a,0x56,0x68)

HMAC_KEY(rmd160, 1,  4,	0x4a,0x65,0x66,0x65)
HMAC_MSG(rmd160, 1,	"what do ya want for nothing?")
HMAC_MD(rmd160,  1, 20, 0xdd,0xa6,0xc0,0x21,0x3a,0x48,0x5a,0x9e,
			0x24,0xf4,0x74,0x20,0x64,0xa7,0xf0,0x33,
			0xb4,0x3c,0x40,0x69)

HMAC_KEY(rmd160, 2, 20,	0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
			0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
			0xaa,0xaa,0xaa,0xaa)
HMAC_MSG(rmd160, 2,	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
			"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd")
HMAC_MD(rmd160, 2, 20,	0xb0,0xb1,0x05,0x36,0x0d,0xe7,0x59,0x96,
			0x0a,0xb4,0xf3,0x52,0x98,0xe1,0x16,0xe2,
			0x95,0xd8,0xe7,0xc1)

HMAC_TEST(rmd160, 3)

/*
 * HMAC test functions array.
 */
static testFunction_t hmacTestFunctions[] = {
    { testHmac_md5,	"MD5" },
    { testHmac_sha1,	"SHA1" },
    { testHmac_sha224,	"SHA224" },
    { testHmac_sha256,	"SHA256" },
    { testHmac_sha384,	"SHA384" },
    { testHmac_sha512,	"SHA512" },
    { testHmac_rmd128,  "RMD128" },
    { testHmac_rmd160,  "RMD160" },
};
#define	numHmacTestFunctions	(sizeof(hmacTestFunctions) / sizeof(hmacTestFunctions[0]))

static int
testAllHmacs(void)
{
	unsigned i;
	int err = 0;

	for (i = 0; i < numHmacTestFunctions; i++) {
		if (NULL == hmacTestFunctions[i].description) continue;
		if (verbose) printf("[BEGIN] %s HMAC\n", hmacTestFunctions[i].description);
		err = hmacTestFunctions[i].funcptr();
		total++;
		if (err) {
			fail++;
			if (verbose) printf("[FAIL] %s HMAC\n", hmacTestFunctions[i].description);
		} else {
			pass++;
			if (verbose) printf("[PASS] %s HMAC\n", hmacTestFunctions[i].description);
		}
	}
	return (fail);
}

static struct option args[] = {
	{ "help",       no_argument,    NULL,   'h' },
	{ "quite",      no_argument,    NULL,   'q' },
	{ 0, 0, 0, 0 }
};

static void
usage (int ret)
{
    fprintf(stderr, "usage: %s [--quite/-q] [--help/-h]\n", getprogname());
    exit (ret);
}

int
main(int argc, char **argv)
{
    int i, idx = 0, rv = 0;

    setprogname(argv[0]);

    while(1) {
	    int c = getopt_long(argc, argv, "hq", args, &idx);

	    if (c == -1)
		break;

	    switch (c) {
	    case 'q':
		    verbose = 0;
		    break;
	    case 'h':
		    usage(0);
		    break;
	    case '?':
	    default:
		    usage(-1);
		    break;
	    }
    }

    /*
    argc -= idx;
    argv += idx;
    */

    if (verbose) printf("[TEST] HMAC KATs\n");
    rv = testAllHmacs();
    if (verbose) {
	    printf("[SUMMARY]\n");
	    printf("Total: %d\n", total);
	    printf("passed: %d\n", pass);
	    printf("failed: %d\n", fail);
    }

    return (rv); 
}