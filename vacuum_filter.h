#pragma once

#include <cmath>

#include "compressed_vector.h"
#include "cuckoo_filter.h"
#include "filter.h"
#include "hash.h"

using HashTableInt = uint32_t;
const size_t kVacuumFilterThreshold = 1 << 18;
const size_t kVacuumFilterBucketSize = 4;

template <class T, class HashFunctionBuilder = LinearHashFunctionBuilder, class FingerprintFunction = std::hash<T>>
class VacuumFilter : public CuckooFilter<T, HashFunctionBuilder, FingerprintFunction> {
    using CF = CuckooFilter<T, HashFunctionBuilder, FingerprintFunction>;
public:
    void Init(size_t expected_size, size_t fingerprint_size_bits, size_t max_num_kicks) {
        CF::bucket_size_ = 4;
        alternate_ranges_ = AlternateRangesSelection(expected_size, CF::bucket_size_);
        std::cerr << "Alternate ranges for vacuum filter: ";
        for (const auto x : alternate_ranges_) {
            std::cerr << x << " ";
        }
        std::cerr << "\n";

        CF::buckets_count_ = GetRealBucketsCount(std::ceil(expected_size / (CF::bucket_size_ * 0.95)));
        CF::CommonInit(fingerprint_size_bits, max_num_kicks);
    }

private:
    size_t GetRealBucketsCount(size_t max_count) const {
        if (max_count <= kVacuumFilterThreshold) {
            return CF::GetRealBucketsCount(max_count) * 2;
        }
        auto result = alternate_ranges_[0] * (max_count / alternate_ranges_[0]);
        if (result == 0) {
            result = alternate_ranges_[0];
        }
        std::cerr << "Buckets count must be divisible by max alternate range length. Reset buckets count to "
                  << result << "\n";
        return result;
    }

    double EstimatedMaxLoad(size_t items_cnt, size_t chunks_number) const {
        double n = static_cast<double>(items_cnt);
        double c = static_cast<double>(chunks_number);
        return n / c + 1.5 * std::sqrt(2 * n * std::log(c) / c);
    }

    bool LoadFactorTest(size_t items_cnt, size_t bucket_size, double target_load_factor, double coefficient, size_t range) const {
        size_t chunks_number = ceil(items_cnt / (bucket_size * target_load_factor * range));
        size_t buckets_count = range * chunks_number;
        size_t inserted_items = bucket_size * coefficient * buckets_count * target_load_factor;
        return EstimatedMaxLoad(inserted_items, chunks_number) < 0.97 * bucket_size * range;
    }

    size_t RangeSelection(size_t items_cnt, size_t bucket_size, double target_load_factor, double coefficient) const {
        size_t range = 1;
        while (!LoadFactorTest(items_cnt, bucket_size, target_load_factor, coefficient, range)) {
            range *= 2;
        }
        return range;
    }

    std::vector<size_t> AlternateRangesSelection(size_t items_cnt, size_t bucket_size) const {
        std::vector<size_t> result(bucket_size);
        for (size_t i = 0; i < bucket_size; ++i) {
            result[i] = RangeSelection(items_cnt, bucket_size, 0.95, 1 - static_cast<double>(i) / bucket_size);
        }
        result[bucket_size - 1] *= 2;
        return result;
    }

    size_t AlternateBucket(size_t bucket, HashTableInt fingerprint) const override {
        if (CF::buckets_count_ <= kVacuumFilterThreshold) {
            auto alt = CF::hash_functions_[1](fingerprint) % CF::buckets_count_;
            return (CF::buckets_count_ - 1 - ((bucket - alt) % CF::buckets_count_) + alt) % CF::buckets_count_;
        }
        size_t alternate_range = alternate_ranges_[fingerprint % alternate_ranges_.size()];
        return (bucket ^ (CF::hash_functions_[1](fingerprint) % alternate_range)) % CF::buckets_count_;
    }

    std::vector<size_t> alternate_ranges_;
};
