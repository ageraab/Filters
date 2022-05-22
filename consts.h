#pragma once

const size_t kDefaultNumbersCount = 1000000; // numbers to put into filter

// Bloom filter consts
const size_t kDefaultBucketsCount = 8000000;
const size_t kDefaultHashFunctionsCount = 6;

// Cuckoo filter consts
const size_t kDefaultMaxBucketsCount = 1 << 18;
const size_t kDefaultBucketSize = 4;
const size_t kDefaultFingerprintSizeBits = 8; // Also for xor filter
const size_t kDefaultMaxNumKicks = 500;

// Vacuum filter consts
const size_t kDefaultAlternateRangeLength = 128;

// Xor filter consts
const double kDefaultBucketsCountCoefficient = 1.23;
const size_t kDefaultAdditionalBuckets = 32;

// SuRF consts
const size_t kDefaultSurfSuffixSize = 8;
const char kTerminator = '\0';
const char kAnyChar = -128;
const size_t kMaxRealSuffixSize = CHAR_BIT;

const int kDefaultFixedLengthValue = 0;
const double kDefaultCutGainThreshold = 0.0;

const int kMinNumber = -2000000000;
const int kMaxNumber = 2000000000;

const int kRangeInsertRate = 5;