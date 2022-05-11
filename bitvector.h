#pragma once

#include <cmath>
#include <vector>

#include "compressed_vector.h"

class DummyBitVector {
public:
    DummyBitVector() = default;

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
        return data_.size();
    }

    int Rank(int pos) const {
        int rank = 0;
        for (size_t i = 0; i <= pos && i < data_.size(); ++i) {
            if (data_[i]) {
                rank += 1;
            }
        }
        return rank;
    }

    int Select(int i) const {
        int cnt = 0;
        int pos = -1;
        while (cnt < i) {
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
    std::vector<bool> data_;
};


class BitVector {
public:
    BitVector() = default;

    void Init(const std::vector<bool>& data) {
        data_ = data;
        size_t blocks_count = std::ceil(static_cast<double>(data.size()) / block_size_);
        rank_blocks_ = CompressedVector<uint32_t>(blocks_count, GetBlockBitsCount(data.size()));

        size_t bit_count = 0;
        for (size_t i = 0; i < blocks_count; ++i) {
            for (size_t j = i * block_size_; j < (i + 1) * block_size_ && j < data_.size(); ++j) {
                if (data_[j]) {
                    ++bit_count;
                }
            }
            rank_blocks_.SetValueByIndex(i, bit_count);
        }

        size_t select_blocks_count = std::floor(static_cast<double>(bit_count) / sample_step_);
        select_blocks_ = CompressedVector<uint32_t>(select_blocks_count, GetBlockBitsCount(data.size()));

        int j = -1;
        bit_count = 0;
        for (size_t i = 0; i < select_blocks_count; ++i) {
            while (bit_count < sample_step_ * (i + 1)) {
                ++j;
                if (data_[j]) {
                    ++bit_count;
                }
            }
            select_blocks_.SetValueByIndex(i, j);
        }
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
        return data_.size() + rank_blocks_.BitsSize() + select_blocks_.BitsSize();
    }

    int Rank(int pos) const {
        size_t block_number = pos / block_size_;
        int rank = 0;
        if (block_number > 0) {
            rank = rank_blocks_.GetValueByIndex(block_number - 1);
        }
        for (size_t i = block_number * block_size_; i <= pos && i < data_.size(); ++i) {
            if (data_[i]) {
                rank += 1;
            }
        }

        return rank;
    }

    int Select(int i) const {
        size_t block_idx = i / sample_step_;
        int cnt = block_idx * sample_step_;
        int pos = -1;
        if (block_idx > 0) {
            pos = select_blocks_.GetValueByIndex(block_idx - 1);
        }
        while (cnt < i) {
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
        return std::ceil(std::log2(size));
    }

    std::vector<bool> data_;
    CompressedVector<uint32_t> rank_blocks_;
    CompressedVector<uint32_t> select_blocks_;
    const size_t block_size_ = 1024;
    const size_t sample_step_ = 512;
};