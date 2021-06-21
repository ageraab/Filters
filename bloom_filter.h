#include "filter.h"
#include "hash.h"

template <class T, class HashFunctionBuilder = LinearHashFunctionBuilder>
class BloomFilter : public Filter<T> {
public:
    BloomFilter() = default;

    template <class Generator>
    void Init(Generator& generator, size_t buckets_count, size_t functions_count) {
        filter_.clear();
        hash_functions_.clear();
        functions_count_ = functions_count;
        buckets_count_ = buckets_count;
        used_space_ = 0;

        filter_.resize(buckets_count_);

        for (size_t i = 0; i < functions_count_; ++i) {
            hash_functions_.emplace_back(hash_function_builder_(generator));
        }
    }

    void Add(const T& value) {
        for (size_t i = 0; i < hash_functions_.size(); ++i) {
            int hash = hash_functions_[i](value);
            if (!filter_[hash % buckets_count_]) {
                ++used_space_;
            }
            filter_[hash % buckets_count_] = true;
        }
    }

    void Build(const std::vector<T>& values) override {
        for (const auto& x : values) {
            Add(x);
        }
    }

    bool Find(const T& value) const override {
        for (size_t i = 0; i < hash_functions_.size(); ++i) {
            int hash = hash_functions_[i](value);
            if (!filter_[hash % buckets_count_]) {
                return false;
            }
        }
        return true;
    }

    bool GetHashTableSizeBits(size_t& size) const override {
        size = filter_.size();
        return true;
    }

    bool GetUsedSpaceBits(size_t& size) const override {
        size = used_space_;
        return true;
    }

private:
    std::vector<bool> filter_;
    std::vector<LinearHashFunction> hash_functions_;
    HashFunctionBuilder hash_function_builder_;
    size_t functions_count_;
    size_t buckets_count_;
    size_t used_space_;
};
