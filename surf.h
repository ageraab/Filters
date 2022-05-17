#pragma once

#include <algorithm>
#include <exception>

#include "bitvector.h"
#include "compressed_vector.h"
#include "consts.h"
#include "filter.h"

bool HaveCommonPrefixes(const std::string& a, const std::string& b, size_t pos) {
    if (a.size() <= pos || b.size() <= pos) {
        return false;
    }
    for (int i = pos; i >= 0; --i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

bool IsSubstr(const std::string& a, const std::string& b) {
    if (a.size() == 0) {
        return true;
    }
    return HaveCommonPrefixes(a, b, a.size() - 1);
}

enum class SuffixType {
    Empty = 0,
    Hash = 1,
    Real = 2
};

class SuffixVector {
public:
    SuffixVector() = default;

    SuffixVector(SuffixType type, size_t capacity, size_t item_size)
        : data_(capacity, item_size), hash_(), item_size_(item_size), size_(0), type_(type) {}

    void AddSuffix(const std::string& s, size_t pos) {
        if (type_ == SuffixType::Empty) {
            return;
        }
        uint32_t value = 0;
        if (type_ == SuffixType::Real) {
            if (pos + 1 < s.size()) {
                value = ToUint32(s[pos + 1]);
            } else {
                value = ToUint32(kTerminator);
            }
        }
        if (type_ == SuffixType::Hash) {
            value = hash_(s) % (1 << item_size_);
        }
        data_.SetValueByIndex(size_, value);
        ++size_;
    };

    bool MatchSuffix(const std::string& s, size_t pos, size_t index) const {
        if (type_ == SuffixType::Empty) {
            return true;
        }
        if (type_ == SuffixType::Real) {
            uint32_t next_symbol = 0;
            if (pos + 1 < s.size()) {
                next_symbol = ToUint32(s[pos + 1]);
            } else {
                next_symbol = ToUint32(kTerminator);
            }
            return data_.GetValueByIndex(index) == next_symbol;
        }
        if (type_ == SuffixType::Hash) {
            return hash_(s) % (1 << item_size_) == data_.GetValueByIndex(index);
        }
        return true;
    }

    char GetSuffix(size_t index) const {
        if (type_ != SuffixType::Real) {
            throw "Call GetSuffix for SuffixVector without real suffix";
        }
        return FromUint32(data_.GetValueByIndex(index));
    }

    size_t DataSizeBits() const {
        return data_.BitsSize();
    }

private:
    uint32_t ToUint32(char c, size_t bits = 8) const {
        uint32_t result = 0;
        char* ptr = (char*)(&result);
        ptr[0] = c;
        result &= (1 << bits) - 1;
        return result;
    }

    char FromUint32(uint32_t x) const {
        return ((char*)(&x))[0];
    }

    CompressedVector<uint32_t> data_;
    std::hash<std::string> hash_;
    size_t item_size_;
    size_t size_;
    SuffixType type_;
};

class FastSuccinctTrie {
public:
    FastSuccinctTrie() {
    }

    void Init(SuffixType suf_type, size_t suffix_size = kDefaultSurfSuffixSize) {
        suffix_type_ = suf_type;
        if (suffix_type_ == SuffixType::Empty) {
            suffix_size_ = 0;
        } else if (suffix_type_ == SuffixType::Real) {
            suffix_size_ = 8;
        } else {
            suffix_size_ = suffix_size;
        }
    }

    void Build(const std::vector<std::string>& values, bool use_terminator = true) {
        use_terminator_ = use_terminator;
        std::vector<bool> done(values.size(), false);
        s_values_ = SuffixVector(suffix_type_, values.size(), suffix_size_);

        std::vector<bool> s_has_child;
        std::vector<bool> s_louds;

        size_t idx = 0;
        bool updated = true;
        while (updated) {
            updated = false;
            for (size_t i = 0; i < values.size(); ++i) {
                if (done[i]) {
                    continue;
                }
                if (idx < values[i].size()) {
                    updated = true;

                    if (i == 0 || !HaveCommonPrefixes(values[i - 1], values[i], idx)) {
                        s_labels_.push_back(values[i][idx]);
                        s_has_child.push_back(false);
                        s_louds.push_back(i == 0 || !(idx == 0 || HaveCommonPrefixes(values[i - 1], values[i], idx - 1)));
                        if (i == values.size() - 1 || !HaveCommonPrefixes(values[i], values[i + 1], idx)) {
                            s_values_.AddSuffix(values[i], idx);
                            done[i] = true;
                        }
                    }
                    if (!done[i]) {
                        if (idx + 1 < values[i].size()) {
                            s_has_child[s_has_child.size() - 1] = true;
                        } else {
                            s_values_.AddSuffix(values[i], idx);
                            done[i] = true;
                        }
                    }
                }
            }
            ++idx;
        }

        s_has_child_.Init(s_has_child);
        s_louds_.Init(s_louds);

        // DebugPrint();
    }

    bool Find(const std::string& key) const {
        int pos = -1;
        int idx = 0;
        for (const auto& c : key) {
            pos = Go(pos, c);
            if (pos == -1) {
                return false;
            }
            if (!s_has_child_[pos]) {
                return s_values_.MatchSuffix(key, idx, pos - s_has_child_.Rank(pos));
            }
            ++idx;
        }
        if (pos != -1 && !s_has_child_[pos]) {
            return true;
        }
        pos = Go(pos, kTerminator);
        return pos != -1;
    }

    bool FindPrefix(const std::string& prefix) const {
        int pos = -1;
        int idx = 0;
        for (const auto& c : prefix) {
            if (pos != -1 && !s_has_child_[pos]) {
                return suffix_type_ != SuffixType::Real || s_values_.MatchSuffix(prefix, idx - 1, pos - s_has_child_.Rank(pos));
            }
            pos = Go(pos, c);
            if (pos == -1) {
                return false;
            }
            ++idx;
        }
        if (pos != -1) {
            return true;
        }
        return false;
    }

    std::string LowerBound(const std::string& key) const {
        int pos = -1;
        for (const auto& c : key) {
            if (pos != -1 && !s_has_child_[pos]) {
                if (suffix_type_ != SuffixType::Real) {
                    break;
                }
                auto suf = s_values_.GetSuffix(pos - s_has_child_.Rank(pos));
                if ((c >= 0 && suf >= 0 && c > suf) || (c < 0 && (suf >= 0 || (suf < 0 && c > suf)))) {
                    pos = MoveToNext(pos);
                }
                break;
            }

            int new_pos = Go(pos, c, true);
            if (new_pos == -1) {
                pos = MoveToNext(pos);
                break;
            } else if (s_labels_[new_pos] != c) {
                pos = MoveToNext(new_pos, true);
                break;
            }
            pos = new_pos;
        }

        return RestoreString(pos);
    }

    size_t CalculateSize() const {
        size_t size = s_labels_.size() * CHAR_BIT;
        size += s_has_child_.Size() + s_louds_.Size();
        size += s_values_.DataSizeBits();
        return size;
    }

    void DebugPrint() const {
        for (size_t i = 0; i < s_has_child_.Size(); ++i) {
            std::cerr << i << " ";
        }
        std::cerr << "\n";
        for (const auto& c: s_labels_) {
            std::cerr << c << " ";
        }
        std::cerr << "\n";
        for (size_t i = 0; i < s_has_child_.Size(); ++i) {
            std::cerr << static_cast<int>(s_has_child_[i]) << " ";
        }
        std::cerr << "\n";
        for (size_t i = 0; i < s_louds_.Size(); ++i) {
            std::cerr << static_cast<int>(s_louds_[i]) << " ";
        }
        std::cerr << "\n\n";
    }

private:
    int MoveToChildren(int parent) const {
        if (parent == -1) {
            return 0;
        }
        if (s_has_child_[parent] == 0) {
            return -1;
        }
        return s_louds_.Select(s_has_child_.Rank(parent) + 1);
    }

    int MoveToParent(int child) const {
        int r = s_louds_.Rank(child);
        if (r == 1) {
            return -1;
        }
        return s_has_child_.Select(r - 1);
    }

    int FindChild(int start, char c, bool lower_bound = false) const {
        for (size_t i = start; i < s_labels_.size(); ++i) {
            if (i > start && s_louds_[i]) {
                return -1;
            }
            if (s_labels_[i] == c || (lower_bound && (
                    (c < 0 && s_labels_[i] < 0 && c < s_labels_[i]) || (c >= 0 && (s_labels_[i] < 0 || c < s_labels_[i]))
                ))) {
                return i;
            }
        }
        return -1;
    }

    int Go(int start, char c, bool lower_bound = false) const {
        int children_start = MoveToChildren(start);
        if (children_start == -1) {
            return -1;
        }
        return FindChild(children_start, c, lower_bound);
    }

    int MoveToNext(int pos, bool shift_done = false) const {
        while (pos != -1) {
            if (shift_done || (pos + 1 < s_louds_.Size() && !s_louds_[pos + 1])) {
                if (!shift_done) {
                    pos = pos + 1;
                }
                while (s_has_child_[pos]) {
                    pos = MoveToChildren(pos);
                }
                return pos;
            }
            pos = MoveToParent(pos);
        }
        return -1;
    }

    std::string RestoreString(int pos) const {
        if (pos == -1) {
            return "";
        }

        std::string result;
        if (!s_has_child_[pos] && suffix_type_ == SuffixType::Real) {
            auto suf = s_values_.GetSuffix(pos - s_has_child_.Rank(pos));
            if (suf != kTerminator || !use_terminator_) {
                result += suf;
            }
        }
        while (pos != -1) {
            auto suf = s_labels_[pos];
            if (suf != kTerminator || !use_terminator_) {
                result += suf;
            }
            pos = MoveToParent(pos);
        }

        std::reverse(result.begin(), result.end());
        return result;
    }

    std::vector<char> s_labels_;
    BitVector s_has_child_;
    BitVector s_louds_;
    SuffixVector s_values_;
    SuffixType suffix_type_;
    size_t suffix_size_;
    bool use_terminator_;
};

template <class T>
class DefaultSurfConverter {
public:
    std::string ToString(const T& x) const = 0;
    T ToString(const std::string& s) const = 0;
};

template<>
class DefaultSurfConverter<std::string> {
public:
    std::string ToString(const std::string& s) const {
        return s;
    }

    std::string FromString(const std::string& s) const {
        return s;
    }
};

template<>
class DefaultSurfConverter<int> {
public:
    std::string ToString(int x) const {
        std::string result(4, '\0');
        char* c = (char*)(&x);
        result[0] = c[3] ^ (1 << 7);
        result[1] = c[2];
        result[2] = c[1];
        result[3] = c[0];
        return result;
    }

    int FromString(const std::string& s) const {
        // TODO
        return 0;
    }
};

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

template <class T, class Converter=DefaultSurfConverter<T>>
class SuccinctRangeFilter : public Filter<T> {
public:
    SuccinctRangeFilter() = default;

    void Init(SuffixType suf_type, size_t suffix_size = kDefaultSurfSuffixSize) {
        trie_.Init(suf_type, suffix_size);
    }

    void Build(const std::vector<T>& values) override {
        std::vector<std::string> strings;
        bool used_terminator = false;
        for (const auto& x : values) {
            strings.push_back(converter_.ToString(x));
        }
        std::sort(strings.begin(), strings.end());
        strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i + 1 < strings.size() && IsSubstr(strings[i], strings[i + 1])) {
                used_terminator = true;
                strings[i].push_back(kTerminator);
            }
        }
        trie_.Build(strings, used_terminator);
    }

    bool Find(const T& value) const override {
        return trie_.Find(converter_.ToString(value));
    }

    bool FindPrefix(const std::string& value) const {
        return trie_.FindPrefix(value);
    }

    bool FindRange(const T& left, const T& right) const {
        if (left == right) {
            return Find(left);
        }
        return trie_.LowerBound(converter_.ToString(left)) <= converter_.ToString(right);
    }

    bool FindRange(const SearchRange<T>& range) const {
        return FindRange(range.left, range.right);
    }

    bool GetHashTableSizeBits(size_t& size) const override {
        size = trie_.CalculateSize();
        return true;
    }

    bool GetUsedSpaceBits(size_t& size) const override {
        return GetHashTableSizeBits(size);
    }

private:
    FastSuccinctTrie trie_;
    Converter converter_;
};
