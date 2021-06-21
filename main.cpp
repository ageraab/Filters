#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bloom_filter.h"
#include "cuckoo_filter.h"
#include "hash.h"
#include "hash_set_filter.h"
#include "xor_filter.h"

const size_t kDefaultNumbersCount = 1000000; // numbers to put into filter

// Bloom filter consts
const size_t kDefaultBucketsCount = 8000000;
const size_t kDefaultHashFunctionsCount = 6;

// Cuckoo filter consts
const size_t kDefaultMaxBucketsCount = 1 << 18;
const size_t kDefaultBucketSize = 4;
const size_t kDefaultFingerprintSizeBits = 8; // Also for xor filter
const size_t kDefaultMaxNumKicks = 500;

// Xor filter consts
const double kDefaultBucketsCountCoefficient = 1.23;
const size_t kDefaultAdditionalBuckets = 32;

const int kMinNumber = -2000000000;
const int kMaxNumber = 2000000000;

template <class Generator>
void AddNumbers(Filter<int>& filter_to_examine, HashSetFilter<int>& correct_filter,
                Generator& generator, size_t numbers_count) {
    std::vector<int> numbers;
    for (size_t i = 0; i < numbers_count; ++i) {
        numbers.push_back(RandomInt(generator, kMinNumber, kMaxNumber));
    }
    filter_to_examine.Build(numbers);
    correct_filter.Build(numbers);

    std::cerr << "Put " << numbers_count << " numbers\n";
    size_t size = 0;
    if (filter_to_examine.GetHashTableSizeBits(size)) {
        std::cerr << "Hash tables size (in bits):  " << size << "\n";
    }
    if (filter_to_examine.GetUsedSpaceBits(size)) {
        std::cerr << "Really used space (in bits): " << size << "\n";
    }
}

void CheckExistingNumbers(const Filter<int>& filter_to_examine,
                          const HashSetFilter<int>& correct_filter) {
    std::cout << "Checking existing numbers (need 100%): ";
    auto numbers = correct_filter.GetHashSet();
    int found = 0;
    for (const int& x : numbers) {
        if (filter_to_examine.Find(x)) {
            ++found;
        } else {
            std::cerr << "NOT FOUND " << x << "\n";
        }
    }

    double percent_found = 100 * static_cast<double>(found) / numbers.size();
    std::cout << "found " << found << " of " << numbers.size() << " (" << percent_found << "%)\n";
}

template <class Generator>
void CheckMissingNumbers(const Filter<int>& filter_to_examine,
                          const HashSetFilter<int>& correct_filter,
                          Generator& generator,
                          size_t numbers_count) {
    std::cout << "Checking missing numbers (perfect is 0%): ";
    int checked = 0, found = 0;
    for (size_t i = 0; i < numbers_count; ++i) {
        int x = RandomInt(generator, kMinNumber, kMaxNumber);
        if (!correct_filter.Find(x)) {
            ++checked;
            if (filter_to_examine.Find(x)) {
                ++found;
            }
        }
    }

    double percent_found = 100 * static_cast<double>(found) / checked;
    std::cout << "found " << found << " of " << checked << " (" << percent_found << "%)\n";
}

template <class Generator>
std::unique_ptr<Filter<int>> GetFilter(int argc, char** argv, Generator& generator) {
    std::string name = argv[1];
    if (name == "bloom") {
        size_t buckets_count = kDefaultBucketsCount;
        size_t hash_functions_count = kDefaultHashFunctionsCount;

        if (argc > 3) {
            buckets_count = std::stoi(argv[3]);
        }
        if (argc > 4) {
            hash_functions_count = std::stoi(argv[4]);
        }

        auto ptr = std::make_unique<BloomFilter<int>>();
        ptr->Init(generator, buckets_count, hash_functions_count);
        return ptr;
    }
    if (name == "cuckoo") {
        size_t max_buckets_count = kDefaultMaxBucketsCount;
        size_t bucket_size = kDefaultBucketSize;
        size_t fingerprint_size_bits = kDefaultFingerprintSizeBits;
        size_t max_num_kicks = kDefaultMaxNumKicks;

        if (argc > 3) {
            max_buckets_count = std::stoi(argv[3]);
        }
        if (argc > 4) {
            bucket_size = std::stoi(argv[4]);
        }
        if (argc > 5) {
            fingerprint_size_bits = std::stoi(argv[5]);
        }
        if (argc > 6) {
            max_num_kicks = std::stoi(argv[6]);
        }

        auto ptr = std::make_unique<CuckooFilter<int>>();
        ptr->Init(max_buckets_count, bucket_size, fingerprint_size_bits, max_num_kicks);
        return ptr;
    }
    if (name == "xor") {
        size_t fingerprint_size_bits = kDefaultFingerprintSizeBits;
        double buckets_count_coefficient = kDefaultBucketsCountCoefficient;
        size_t additional_buckets = kDefaultAdditionalBuckets;

        if (argc > 3) {
            fingerprint_size_bits = std::stoi(argv[3]);
        }
        if (argc > 4) {
            buckets_count_coefficient = std::stod(argv[4]);
        }
        if (argc > 5) {
            additional_buckets = std::stoi(argv[5]);
        }

        auto ptr = std::make_unique<XorFilter<int>>();
        ptr->Init(fingerprint_size_bits, buckets_count_coefficient, additional_buckets);
        return ptr;
    }
    throw "Unknown filter name. Use one of: bloom, cuckoo, xor";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./main filter_name items_cnt [filter params]\n";
        std::cerr << "Bloom filter params: [buckets_count] [hash_functions_count]\n";
        std::cerr << "Cuckoo filter params: [max_buckets_count] [bucket_size] [fingerprint_size_bits] [max_num_kicks]\n";
        std::cerr << "Xor filter params: [fingerprint_size_bits] [buckets_count_coefficient] [additional_buckets]\n";
        return 1;
    }
    std::string filter_name = argv[1];

    size_t numbers_count = kDefaultNumbersCount;
    if (argc > 2) {
        numbers_count = std::stoi(argv[2]);
    }

    std::mt19937 generator(228);
    std::unique_ptr<Filter<int>> filter_to_test = GetFilter(argc, argv, generator);
    HashSetFilter<int> hash_set_filter;

    std::cout << std::fixed << std::setprecision(2);
    AddNumbers(*filter_to_test, hash_set_filter, generator, numbers_count);
    CheckExistingNumbers(*filter_to_test, hash_set_filter);
    CheckMissingNumbers(*filter_to_test, hash_set_filter, generator, numbers_count);
}
