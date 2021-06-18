#pragma once

#include <cassert>
#include <vector>

// index = bucket_size_ * hash + bucket

template <class Int = uint32_t>
class CompressedVector {
public:
    CompressedVector() = default;

    CompressedVector(size_t vector_size, size_t item_size)
        : data_(), vector_size_(vector_size), item_size_(item_size), int_size_(sizeof(Int) * CHAR_BIT) {
        assert(item_size <= int_size_);
        data_.resize((item_size_ * vector_size / int_size_) + 1);
    };

    Int GetValueByIndex(size_t index) const {
        size_t start_pos = (item_size_ * index) / int_size_;
        size_t start_offset = (item_size_ * index) % int_size_;
        Int value = 0;
        if (start_offset + item_size_ <= int_size_) {
            value = GetBitsFromInt(data_[start_pos], start_offset, start_offset + item_size_);
        } else {
            size_t end_offset = start_offset + item_size_ - int_size_;
            value = GetBitsFromInt(data_[start_pos], start_offset, int_size_) << end_offset;
            value |= GetBitsFromInt(data_[start_pos + 1], 0, end_offset);
        }
        return value;
    }

    void SetValueByIndex(size_t index, Int value) {
        size_t start_pos = (item_size_ * index) / int_size_;
        size_t start_offset = (item_size_ * index) % int_size_;
        if (start_offset + item_size_ <= int_size_) {
            SetBitsToInt(data_[start_pos], value, start_offset, start_offset + item_size_);
        } else {
            size_t end_offset = start_offset + item_size_ - int_size_;
            SetBitsToInt(data_[start_pos], value >> end_offset, start_offset, int_size_);
            SetBitsToInt(data_[start_pos + 1], value & ((1ul << end_offset) - 1), 0, end_offset);
        }
    }

    size_t Size() const {
        return vector_size_;
    }

    size_t BitsSize() const {
        return data_.size() * int_size_;
    }

private:
    Int GetBitsFromInt(Int x, size_t start, size_t end) const {
        return (x >> (int_size_ - end)) & ((1ul << (end - start)) - 1);
    }

    void SetBitsToInt(Int& x, Int value, size_t start, size_t end) {
        Int mask = ((1ul << int_size_) - 1) ^ (((1ul << (end - start)) - 1) << (int_size_ - end));
        x &= mask;
        x |= value << (int_size_ - end);
    }

    std::vector <Int> data_;
    size_t vector_size_;
    size_t item_size_;
    size_t int_size_;
};