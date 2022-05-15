#pragma once

#include <cmath>
#include <vector>

#include "compressed_vector.h"

class BitVector {
public:
    BitVector() = default;

    void Init(const std::vector<bool>& data) {
        data_ = data;
        InitBlocks();
        InitSelectStats();
    }

    void PushBack(bool x) {
        data_.push_back(x);
    }

    bool operator[](size_t i) const {
        return data_[i];
    }

    std::vector<bool>::reference operator[](size_t i) {
        return data_[i];
    }

    size_t Size() const {
        return data_.size() + aggregates_.BitsSize() + blocks_.BitsSize() + select_stats_.BitsSize();
    }

    int Rank(int pos) const {
        size_t large_block_number = pos / aggregate_step_;
        size_t small_block_number = pos / basic_block_size_;

        int rank = 0;
        if (large_block_number > 0) {
            rank = aggregates_.GetValueByIndex(large_block_number - 1);
        }

        for (size_t i = large_block_number * aggregate_step_ / basic_block_size_; i < small_block_number; ++i) {
            rank += blocks_.GetValueByIndex(i);
        }

        for (size_t i = small_block_number * basic_block_size_; i <= pos && i < data_.size(); ++i) {
            if (data_[i]) {
                ++rank;
            }
        }

        return rank;
    }

    int Select(int i) const {
        size_t select_bucket = i / select_step_;
        int cnt = select_bucket * select_step_;
        int pos = -1;
        if (select_bucket > 0) {
            pos = select_stats_.GetValueByIndex(select_bucket - 1);
        }

        size_t large_block_number = std::max(pos, 0) / aggregate_step_;
        while (large_block_number < aggregates_.Size()) {
            int new_cnt = aggregates_.GetValueByIndex(large_block_number);
            if (new_cnt < i) {
                cnt = new_cnt;
                pos = (large_block_number + 1) * aggregate_step_ - 1;
                ++large_block_number;
            } else {
                break;
            }
        }

        while (cnt < i) {
            // This slows down select
            /*
            if (pos + 1 % basic_block_size_ == 0) {
                int block_ones = blocks_.GetValueByIndex((pos + 1) / basic_block_size_);
                if (cnt + block_ones < i) {
                    pos += basic_block_size_;
                    cnt += block_ones;
                }
            }
            */
            pos += 1;
            if (pos >= data_.size()) {
                return -1;
            }
            if (data_[pos]) {
                cnt += 1;
            }
        }

        return pos;
    }

private:
    size_t GetBlockBitsCount(size_t size) const {
        size_t x = std::ceil(std::log2(size));
        while (x % 4 != 0) {
            ++x;
        }
        return x;
    }

    void InitBlocks() {
        size_t large_blocks_count = std::ceil(static_cast<double>(data_.size()) / aggregate_step_);
        size_t small_blocks_count = std::ceil(static_cast<double>(data_.size()) / basic_block_size_);
        aggregates_ = CompressedVector<uint32_t>(large_blocks_count, GetBlockBitsCount(data_.size()));
        blocks_ = CompressedVector<uint32_t>(small_blocks_count, GetBlockBitsCount(basic_block_size_ + 1));

        size_t ones_count = 0;
        size_t basic_block_ones_count = 0;
        for (size_t i = 0; i < data_.size(); ++i) {
            if (i > 0 && i % aggregate_step_ == 0) {
                aggregates_.SetValueByIndex(i / aggregate_step_ - 1, ones_count);
            }
            if (i > 0 && i % basic_block_size_ == 0) {
                blocks_.SetValueByIndex(i / basic_block_size_ - 1, basic_block_ones_count);
                basic_block_ones_count = 0;
            }
            if (data_[i]) {
                ++ones_count;
                ++basic_block_ones_count;
            }
        }
        aggregates_.SetValueByIndex(large_blocks_count - 1, ones_count);
        blocks_.SetValueByIndex(small_blocks_count - 1, basic_block_ones_count);

        ones_count_ = ones_count;
    }

    void InitSelectStats() {
        size_t select_blocks_count = std::floor(static_cast<double>(ones_count_) / select_step_);
        select_stats_ = CompressedVector<uint32_t>(select_blocks_count, GetBlockBitsCount(data_.size()));

        int j = -1;
        size_t bit_count = 0;
        for (size_t i = 0; i < select_blocks_count; ++i) {
            while (bit_count < select_step_ * (i + 1)) {
                ++j;
                if (data_[j]) {
                    ++bit_count;
                }
            }
            select_stats_.SetValueByIndex(i, j);
        }
    }

    std::vector<bool> data_;
    CompressedVector<uint32_t> aggregates_;
    CompressedVector<uint32_t> blocks_;
    CompressedVector<uint32_t> select_stats_;
    const size_t aggregate_step_ = 256;
    const size_t basic_block_size_ = 32;
    const size_t select_step_ = 256;
    size_t ones_count_;
};