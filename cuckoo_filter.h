#pragma once

#include "compressed_vector.h"
#include "filter.h"
#include "hash.h"

using HashTableInt = uint32_t;

template <class T, class HashFunctionBuilder = LinearHashFunctionBuilder, class FingerprintFunction = std::hash<T>>
class CuckooFilter : public Filter<T> {
public:
    CuckooFilter() : generator_(1111) {
    }

    virtual ~CuckooFilter() {
    }

    void Init(size_t max_buckets_count, size_t bucket_size,
              size_t fingerprint_size_bits, size_t max_num_kicks) {
        buckets_count_ = GetRealBucketsCount(max_buckets_count); // Since buckets count should be a power of 2
        bucket_size_ = bucket_size;
        CommonInit(fingerprint_size_bits, max_num_kicks);
    }

    void Add(const T& value) {
        auto fingerprint = GetFingerPrint(value);
        auto first_hash = hash_functions_[0](value) % buckets_count_;
        auto second_hash = AlternateBucket(first_hash, fingerprint);

        if (TryAddItem(fingerprint, first_hash) || TryAddItem(fingerprint, second_hash)) {
            return;
        }

        auto tmp_fingerprint = fingerprint;
        auto hash_to_replace = first_hash;
        int j = RandomInt(generator_, 0, 1); // Choose one of two buckets for fingerprint
        if (j == 1) {
            hash_to_replace = second_hash;
        }
        for (size_t i = 0; i < max_num_kicks_; ++i) {
            int bucket_to_replace = RandomInt(generator_, 0, bucket_size_ - 1);
            GetHashTableValue(hash_to_replace, bucket_to_replace, tmp_fingerprint);
            SetHashTableValue(hash_to_replace, bucket_to_replace, fingerprint);

            fingerprint = tmp_fingerprint;
            hash_to_replace = AlternateBucket(hash_to_replace, fingerprint);

            if (TryAddItem(fingerprint, hash_to_replace)) {
                return;
            }
        }

        std::cerr << "Add failed with table size = " << size_ << "\n";
        throw size_;
    }

    void Build(const std::vector<T>& values) override {
        for (const auto& x : values) {
            Add(x);
        }
    }

    bool Find(const T& value) const override {
        auto fingerprint = GetFingerPrint(value);
        auto first_hash = hash_functions_[0](value) % buckets_count_;
        auto second_hash = AlternateBucket(first_hash, fingerprint);
        return FindInHashTable(fingerprint, first_hash) != -1 || FindInHashTable(fingerprint, second_hash) != -1;
    }

    bool GetHashTableSizeBits(size_t& size) const override {
        size = hash_table_.BitsSize();
        return true;
    }

    bool GetUsedSpaceBits(size_t& size) const override {
        size = used_space_;
        return true;
    }

protected:
    void CommonInit(size_t fingerprint_size_bits, size_t max_num_kicks) {
        hash_functions_.clear();
        fingerprint_size_bits_ = fingerprint_size_bits;
        max_fingerprint_ = (1ul << fingerprint_size_bits_) - 1;
        max_num_kicks_ = max_num_kicks;

        size_ = 0;
        used_space_ = 0;

        // Allocate (fingerprint_size_bits * buckets_count) bits for hash table
        hash_table_ = CompressedVector<HashTableInt>(buckets_count_ * bucket_size_, fingerprint_size_bits_);
        // use value (1 << fingerprint_size_bits_) - 1 as empty indicator
        for (size_t i = 0; i < hash_table_.Size(); ++i) {
            hash_table_.SetValueByIndex(i, max_fingerprint_);
        }

        for (size_t i = 0; i < hash_functions_count_; ++i) {
            hash_functions_.emplace_back(hash_function_builder_(generator_));
        }
    }

    HashTableInt GetFingerPrint(const T& x) const {
        return fingerprint_function_(x) % (max_fingerprint_);
    }

    virtual size_t AlternateBucket(size_t bucket, HashTableInt fingerprint) const {
        return (bucket ^ hash_functions_[1](fingerprint)) % buckets_count_;
    }

    void SetHashTableValue(size_t hash, size_t bucket, HashTableInt value) {
        hash_table_.SetValueByIndex(hash * bucket_size_ + bucket, value);
    }

    bool GetHashTableValue(size_t hash, size_t bucket, HashTableInt& value) const {
        value = hash_table_.GetValueByIndex(hash * bucket_size_ + bucket);
        return value != max_fingerprint_;
    }

    int FindInHashTable(HashTableInt fingerprint, size_t hash) const {
        HashTableInt found_value = 0;
        for (size_t i = 0; i < bucket_size_; ++i) {
            if (GetHashTableValue(hash, i, found_value) && found_value == fingerprint) {
                return i;
            }
        }
        return -1;
    }

    int FindBucketForItem(HashTableInt fingerprint, size_t hash, bool& already_in_table) const {
        HashTableInt found_value = 0;
        for (size_t i = 0; i < bucket_size_; ++i) {
            bool empty = !GetHashTableValue(hash, i, found_value);
            if (empty) {
                return i;
            }
            if (found_value == fingerprint) {
                already_in_table = true;
                return i;
            }
        }
        return -1;
    }

    bool TryAddItem(HashTableInt fingerprint, size_t hash) {
        bool already_in_table = false;
        int bucket = FindBucketForItem(fingerprint, hash, already_in_table);
        if (bucket != -1) {
            SetHashTableValue(hash, bucket, fingerprint);
            ++size_;
            if (!already_in_table) {
                used_space_ += fingerprint_size_bits_;
            }
            return true;
        }
        return false;
    }

    size_t GetRealBucketsCount(size_t max_count) const {
        size_t count = 1;
        while (count <= max_count) {
            count <<= 1;
        }
        std::cerr << "Buckets count must be a power of 2. Reset buckets count to "
                  << count / 2 << "\n";
        return count / 2;
    }

    CompressedVector<HashTableInt> hash_table_;
    std::vector<LinearHashFunction> hash_functions_;
    FingerprintFunction fingerprint_function_;
    size_t size_;
    size_t used_space_;
    std::mt19937 generator_;
    HashFunctionBuilder hash_function_builder_;
    size_t fingerprint_size_bits_;
    size_t max_fingerprint_;
    size_t buckets_count_;
    size_t bucket_size_;
    size_t max_num_kicks_ = 500;
    static const size_t hash_functions_count_ = 2;
};
