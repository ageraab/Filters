#include "compressed_vector.h"
#include "filter.h"
#include "hash.h"

#include <cmath>
#include <queue>
#include <stack>
#include <unordered_set>

using HashTableInt = uint32_t;

template <class T, class HashFunctionBuilder = LinearHashFunctionBuilder, class FingerprintFunction = std::hash<T>>
class XorFilter : public Filter<T> {
public:
    XorFilter() : generator_(2941) {
    }

    void Init(size_t fingerprint_size_bits, double buckets_count_coefficient, size_t additional_buckets) {
        fingerprint_size_bits_ = fingerprint_size_bits;
        buckets_count_coefficient_ = buckets_count_coefficient;
        additional_buckets_ = additional_buckets;
    }

    void Build(const std::vector<T>& values) override {
        size_t table_size = std::ceil(buckets_count_coefficient_ * values.size()) + additional_buckets_;
        hash_table_ = CompressedVector<HashTableInt>(table_size, fingerprint_size_bits_);

        std::stack<std::pair<T, size_t>> building_stack;
        do {
            hash_functions_.clear();
            while (!building_stack.empty()) {
                building_stack.pop();
            }
            used_buckets_ = 0;

            for (size_t i = 0; i < hash_functions_count_; ++i) {
                hash_functions_.emplace_back(hash_function_builder_(generator_));
            }
        } while (!DoMappingStep(values, building_stack));

        while (!building_stack.empty()) {
            auto pair = building_stack.top();
            building_stack.pop();

            hash_table_.SetValueByIndex(pair.second, 0);
            HashTableInt number_to_store = GetFingerPrint(pair.first);
            for (size_t i = 0; i < hash_functions_count_; ++i) {
                number_to_store ^= hash_table_.GetValueByIndex(CountHash(pair.first, i));
            }
            hash_table_.SetValueByIndex(pair.second, number_to_store);
        }
    }

    bool Find(const T& value) const override {
        HashTableInt result = 0;
        for (size_t i = 0; i < hash_functions_count_; ++i) {
            result ^= hash_table_.GetValueByIndex(CountHash(value, i));
        }
        return result == GetFingerPrint(value);
    }

    bool GetHashTableSizeBits(size_t& size) const override {
        size = hash_table_.BitsSize();
        return true;
    }

    bool GetUsedSpaceBits(size_t& size) const override {
        size = used_buckets_ * fingerprint_size_bits_;
        return true;
    }

private:
    HashTableInt GetFingerPrint(const T& x) const {
        return fingerprint_function_(x) % (1 << fingerprint_size_bits_);
    }

    size_t CountHash(const T& value, size_t function_num) const {
        size_t range = hash_table_.Size() / hash_functions_count_;
        return range * function_num + hash_functions_[function_num](value) % range;
    }

    bool DoMappingStep(const std::vector<T>& values, std::stack<std::pair<T, size_t>>& output_stack) {
        std::vector<std::unordered_set<T>> distribution(hash_table_.Size());
        size_t unique_count = 0;
        for (const auto& x : values) {
            for (size_t i = 0; i < hash_functions_count_; ++i) {
                auto hash = CountHash(x, i);
                if (distribution[hash].find(x) == distribution[hash].end()) {
                    distribution[hash].insert(x);
                    if (i == 0) {
                        ++unique_count;
                    }
                }
            }
        }

        std::queue<size_t> queue;
        for (size_t i = 0; i < distribution.size(); ++i) {
            if (!distribution[i].empty()) {
                ++used_buckets_;
                if (distribution[i].size() == 1) {
                    queue.push(i);
                }
            }
        }

        while (!queue.empty()) {
            size_t index = queue.front();
            queue.pop();

            if (distribution[index].size() == 1) {
                auto value = *distribution[index].begin();
                output_stack.push(std::make_pair(value, index));

                for (size_t i = 0; i < hash_functions_count_; ++i) {
                    auto set = distribution[CountHash(value, i)];
                    distribution[CountHash(value, i)].erase(value);
                    if (distribution[CountHash(value, i)].size() == 1) {
                        queue.push(CountHash(value, i));
                    }
                }
            }
        }

        return output_stack.size() == unique_count;
    }

    CompressedVector<HashTableInt> hash_table_;
    std::vector<LinearHashFunction> hash_functions_;
    HashFunctionBuilder hash_function_builder_;
    FingerprintFunction fingerprint_function_;
    std::mt19937 generator_;
    size_t fingerprint_size_bits_;
    double buckets_count_coefficient_;
    size_t additional_buckets_;
    size_t used_buckets_;
    static const size_t hash_functions_count_ = 3;
};
