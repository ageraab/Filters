#pragma once

template <class T>
class Filter {
    public:

    virtual ~Filter() {
    }

    virtual void Build(const std::vector<T>& values) = 0;
    virtual bool Find(const T& value) const = 0;

    // If implemented, puts number of bits reserved by hash tables to size
    virtual bool GetHashTableSizeBits(size_t& size) const {
        return false;
    }

    // If implemented, puts number of really used bits by hash tables to size
    virtual bool GetUsedSpaceBits(size_t& size) const {
        return false;
    }
};
