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
#include "surf.h"
#include "testdata.h"
#include "xor_filter.h"


template <class Function>
void MeasureTime(std::string label, Function f) {
    auto t1 = std::chrono::high_resolution_clock::now();
    f();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << label << " time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms\n";
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
        std::cout << "Hash tables size (in bits):  " << size << "\n";
    }
    if (filter_to_examine.GetUsedSpaceBits(size)) {
        std::cout << "Really used space (in bits): " << size << "\n";
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
    std::cout << "Existing items check (required 100%): ";
    std::cout << "found " << found << " of " << items_count << " (" << percent_found << "%)\n";
}

template <class T, class Generator>
void CheckMissingItems(const Filter<T>& filter_to_examine,
                       const TestData<T, Generator>& test_data,
                       size_t items_count) {
    std::vector<T> items;
    while (items.size() < items_count) {
        auto x = test_data.GenerateQuery();
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
void RunTestCase(Filter<T>& filter, TestData<T, Generator> test_data, size_t items_count, const std::string& label) {
    std::cout << "TEST CASE: " << label << "\n\n";
    AddItems(filter, test_data, items_count);
    CheckExistingItems(filter, test_data);
    CheckMissingItems(filter, test_data, items_count);
    std::cout << "_______________________________________\n\n";
}

template <class T, class Generator = std::mt19937>
std::unique_ptr<Filter<T>> GetFilter(int argc, char** argv, Generator& generator) {
    std::string name = argv[1];
    if (name == "bloom") {
        size_t buckets_count = kDefaultBucketsCount;
        size_t hash_functions_count = kDefaultHashFunctionsCount;

        if (argc > 4) {
            buckets_count = std::stoi(argv[4]);
        }
        if (argc > 5) {
            hash_functions_count = std::stoi(argv[5]);
        }

        auto ptr = std::make_unique<BloomFilter<T>>();
        ptr->Init(generator, buckets_count, hash_functions_count);
        return ptr;
    }
    if (name == "cuckoo") {
        size_t max_buckets_count = kDefaultMaxBucketsCount;
        size_t bucket_size = kDefaultBucketSize;
        size_t fingerprint_size_bits = kDefaultFingerprintSizeBits;
        size_t max_num_kicks = kDefaultMaxNumKicks;

        if (argc > 4) {
            max_buckets_count = std::stoi(argv[4]);
        }
        if (argc > 5) {
            bucket_size = std::stoi(argv[5]);
        }
        if (argc > 6) {
            fingerprint_size_bits = std::stoi(argv[6]);
        }
        if (argc > 7) {
            max_num_kicks = std::stoi(argv[7]);
        }

        auto ptr = std::make_unique<CuckooFilter<T>>();
        ptr->Init(max_buckets_count, bucket_size, fingerprint_size_bits, max_num_kicks);
        return ptr;
    }
    if (name == "vacuum") {
        size_t fingerprint_size_bits = kDefaultFingerprintSizeBits;
        size_t max_num_kicks = kDefaultMaxNumKicks;

        size_t expected_size = std::stoi(argv[3]);
        if (argc > 4) {
            fingerprint_size_bits = std::stoi(argv[4]);
        }
        if (argc > 5) {
            max_num_kicks = std::stoi(argv[5]);
        }

        auto ptr = std::make_unique<VacuumFilter<T>>();
        ptr->Init(expected_size, fingerprint_size_bits, max_num_kicks);
        return ptr;
    }
    if (name == "xor") {
        size_t fingerprint_size_bits = kDefaultFingerprintSizeBits;
        double buckets_count_coefficient = kDefaultBucketsCountCoefficient;
        size_t additional_buckets = kDefaultAdditionalBuckets;

        if (argc > 4) {
            fingerprint_size_bits = std::stoi(argv[4]);
        }
        if (argc > 5) {
            buckets_count_coefficient = std::stod(argv[5]);
        }
        if (argc > 6) {
            additional_buckets = std::stoi(argv[6]);
        }

        auto ptr = std::make_unique<XorFilter<T>>();
        ptr->Init(fingerprint_size_bits, buckets_count_coefficient, additional_buckets);
        return ptr;
    }
    if (name == "surf") {
        SuffixType s_type = SuffixType::Hash;
        size_t suffix_size = kDefaultSurfSuffixSize;
        int fixed_length = kDefaultFixedLengthValue;
        double cut_gain_threshold = kDefaultCutGainThreshold;

        if (argc > 4) {
            std::string type = argv[4];
            if (type == "empty" || type == "base") {
                s_type = SuffixType::Empty;
            } else if (type == "real") {
                s_type = SuffixType::Real;
            }
        }
        if (argc > 5) {
            suffix_size = std::stoi(argv[5]);
        }
        if (argc > 6) {
            fixed_length = std::stoi(argv[6]);
        }
        if (argc > 7) {
            cut_gain_threshold = std::stod(argv[7]);
        }

        auto ptr = std::make_unique<SuccinctRangeFilter<T>>();
        ptr->Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
        return ptr;
    }
    throw "Unknown filter name. Use one of: bloom, cuckoo, xor, vacuum, surf";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./main filter_name test_data items_cnt [filter params]\n";
        std::cerr << "Bloom filter params: [buckets_count] [hash_functions_count]\n";
        std::cerr << "Cuckoo filter params: [max_buckets_count] [bucket_size] [fingerprint_size_bits] [max_num_kicks]\n";
        std::cerr << "Vacuum filter params: [fingerprint_size_bits] [max_num_kicks]\n";
        std::cerr << "Xor filter params: [fingerprint_size_bits] [buckets_count_coefficient] [additional_buckets]\n";
        std::cerr << "SuRF params: [suffix_type] [suffix_size] [fix_length] [cut_gain_threshold]\n";
        return 1;
    }

    std::string test_data = "all";
    if (argc > 2) {
        test_data = argv[2];
    }

    size_t items_count = kDefaultNumbersCount;
    if (argc > 3) {
        items_count = std::stoi(argv[3]);
    }

    std::mt19937 generator(228);

    std::cout << std::fixed << std::setprecision(2);

    if (test_data == "uniform" || test_data == "all") {
        std::unique_ptr<Filter<int>> filter_to_test = GetFilter<int>(argc, argv, generator);
        UniformIntTestData<std::mt19937> g(generator, kMinNumber, kMaxNumber);
        RunTestCase(*filter_to_test, TestData<int, UniformIntTestData<std::mt19937>>(g), items_count, "Uniform distribution for integers");
    }

    if (test_data == "zipf" || test_data == "all") {
        std::unique_ptr<Filter<int>> filter_to_test = GetFilter<int>(argc, argv, generator);
        ZipfMandelbrotIntTestData<std::mt19937> zm(generator, 1.13, 2.73, 1000000);
        TestData<int, ZipfMandelbrotIntTestData<std::mt19937>> td(zm);
        RunTestCase(*filter_to_test, td, items_count, "Zipf-mandelbrot distribution for integers");
    }

    if (test_data == "text" || test_data == "all") {
        RandomTextTestData<std::mt19937> g(generator, 5, 100);
        std::unique_ptr <Filter<std::string>> string_filter = GetFilter<std::string>(argc, argv, generator);
        RunTestCase(*string_filter, TestData<std::string, RandomTextTestData<std::mt19937>>(g), items_count, "Random strings");
    }

    if (test_data == "real" || test_data == "all") {
        PaymentsCsvParser parser;
        CsvTestData<std::mt19937, PaymentsCsvParser> g(generator, "data/payments.csv", parser);
        std::unique_ptr <Filter<std::string>> string_filter = GetFilter<std::string>(argc, argv, generator);
        RunTestCase(*string_filter, TestData<std::string, CsvTestData<std::mt19937, PaymentsCsvParser>>(g), items_count, "Csv data");
    }
}
