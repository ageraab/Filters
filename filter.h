#pragma once

template <class T>
struct SearchRange {
    T left;
    T right;

    SearchRange(const T& l, const T& r) : left(l), right(r) {
    }

    friend std::ostream& operator <<(std::ostream& out, const SearchRange<T>& range) {
        out << "[" << range.left << ", " << range.right << "]";
        return out;
    }
};

template <class T>
class Filter {
    public:

    virtual ~Filter() {
    }

    virtual void Build(const std::vector<T>& values) = 0;
    virtual bool Find(const T& value) const = 0;

    virtual bool FindRange(const SearchRange<T>& range) const {
        return true;
    }

    // If implemented, puts number of bits reserved by hash tables to size
    virtual bool GetHashTableSizeBits(size_t& size) const {
        return false;
    }

    // If implemented, puts number of really used bits by hash tables to size
    virtual bool GetUsedSpaceBits(size_t& size) const {
        return false;
    }
};
