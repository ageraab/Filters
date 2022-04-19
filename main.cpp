#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bloom_filter.h"
#include "consts.h"
#include "cuckoo_filter.h"
#include "vacuum_filter.h"
#include "hash.h"
#include "hash_set_filter.h"
#include "testdata.h"
#include "xor_filter.h"


template <class Function>
void MeasureTime(std::string label, Function f) {
    auto t1 = std::chrono::high_resolution_clock::now();
    f();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cerr << label << " time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms\n";
}

template <class T, class Generator>
void AddItems(Filter<T>& filter_to_examine, TestData<T, Generator>& test_data, size_t items_count) {
    std::vector<T> items;
    for (size_t i = 0; i < items_count; ++i) {
        items.push_back(test_data.NewItem());
    }
    MeasureTime("Filter build", [&](){filter_to_examine.Build(items);});

    std::cerr << "Put " << items_count << " items\n";
    size_t size = 0;
    if (filter_to_examine.GetHashTableSizeBits(size)) {
        std::cerr << "Hash tables size (in bits):  " << size << "\n";
    }
    if (filter_to_examine.GetUsedSpaceBits(size)) {
        std::cerr << "Really used space (in bits): " << size << "\n";
    }
}

template <class T, class Generator>
void CheckExistingItems(const Filter<T>& filter_to_examine,
                        const TestData<T, Generator>& test_data) {
    int found = 0;
    int items_count = 0;

    MeasureTime("Checking existing items", [&]() {
        for (auto it = test_data.Begin(); it != test_data.End(); ++it) {
            ++items_count;
            if (filter_to_examine.Find(*it)) {
                ++found;
            } else {
                std::cerr << "NOT FOUND " << *it << "\n";
            }
        }
    });

    double percent_found = 100 * static_cast<double>(found) / items_count;
    std::cout << "Existing itemss check (required 100%): ";
    std::cout << "found " << found << " of " << items_count << " (" << percent_found << "%)\n";
}

template <class T, class Generator>
void CheckMissingItems(const Filter<T>& filter_to_examine,
                       const TestData<T, Generator>& test_data,
                       size_t items_count) {
    std::vector<int> items;
    while (items.size() < items_count) {
        auto x = test_data.GenerateItem();
        if (!test_data.Contains(x)) {
            items.push_back(x);
        }
    }

    int found = 0;
    MeasureTime("Checking missing items", [&]() {
        for (const auto& x : items) {
            if (filter_to_examine.Find(x)) {
                ++found;
            }
        }
    });

    double percent_found = 100 * static_cast<double>(found) / items.size();
    std::cout << "Missing items check (perfect is 0%): ";
    std::cout << "found " << found << " of " << items.size() << " (" << percent_found << "%)\n";
}

template <class T, class Generator>
void RunTestCase(Filter<int>& filter, TestData<T, Generator> test_data, size_t numbers_count) {
    AddItems(filter, test_data, numbers_count);
    CheckExistingItems(filter, test_data);
    CheckMissingItems(filter, test_data, numbers_count);
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
    if (name == "vacuum") {
        size_t fingerprint_size_bits = kDefaultFingerprintSizeBits;
        size_t max_num_kicks = kDefaultMaxNumKicks;

        size_t expected_size = std::stoi(argv[2]);
        if (argc > 3) {
            fingerprint_size_bits = std::stoi(argv[3]);
        }
        if (argc > 4) {
            max_num_kicks = std::stoi(argv[4]);
        }

        auto ptr = std::make_unique<VacuumFilter<int>>();
        ptr->Init(expected_size, fingerprint_size_bits, max_num_kicks);
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
        std::cerr << "Vacuum filter params: [fingerprint_size_bits] [max_num_kicks]\n";
        std::cerr << "Xor filter params: [fingerprint_size_bits] [buckets_count_coefficient] [additional_buckets]\n";
        return 1;
    }

    size_t numbers_count = kDefaultNumbersCount;
    if (argc > 2) {
        numbers_count = std::stoi(argv[2]);
    }

    std::mt19937 generator(228);
    std::unique_ptr<Filter<int>> filter_to_test = GetFilter(argc, argv, generator);
    HashSetFilter<int> hash_set_filter;

    std::cout << std::fixed << std::setprecision(2);
    RunTestCase(*filter_to_test, TestData<int, UniformIntTestData<std::mt19937>>(generator), numbers_count);
}
