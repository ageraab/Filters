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

size_t CommonPrefixLength(const std::string& a, const std::string& b) {
    size_t count = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (a[i] == b[i]) {
            ++count;
        } else {
            break;
        }
    }
    return count;
}

enum class SuffixType {
    Empty = 0,
    Hash = 1,
    Real = 2
};

class SuffixVector {
public:
    SuffixVector() = default;

    SuffixVector(SuffixType type, size_t capacity, size_t item_size, bool use_any)
        : data_(capacity, item_size), hash_(), item_size_(item_size), size_(0), type_(type), use_any_(use_any) {}

    void AddSuffix(const std::string& s, size_t pos) {
        if (type_ == SuffixType::Empty) {
            return;
        }
        uint32_t value = 0;
        if (type_ == SuffixType::Real) {
            if (pos + 1 < s.size()) {
                value = ToUint32(s[pos + 1], item_size_);
            } else {
                value = ToUint32(kTerminator, item_size_);
            }
        }
        if (type_ == SuffixType::Hash) {
            value = hash_(s) % (1 << item_size_);
        }
        data_.SetValueByIndex(size_, value);
        ++size_;
    };

    void AddAnySuffix() {
        if (!use_any_) {
            throw "Add any suffix without use_any";
        }
        if (type_ == SuffixType::Empty) {
            return;
        }
        uint32_t value = ToUint32(kAnyChar, item_size_);
        data_.SetValueByIndex(size_, value);
        ++size_;
    }

    bool MatchSuffix(const std::string& s, size_t pos, size_t index) const {
        if (type_ == SuffixType::Empty) {
            return true;
        }
        if (type_ == SuffixType::Real) {
            uint32_t next_symbol = 0;
            uint32_t found_symbol = data_.GetValueByIndex(index);
            if (use_any_ && found_symbol == ToUint32(kAnyChar, item_size_)) {
                return true;
            }

            if (pos + 1 < s.size()) {
                next_symbol = ToUint32(s[pos + 1], item_size_);
            } else {
                next_symbol = ToUint32(kTerminator, item_size_);
            }
            return found_symbol == next_symbol;
        }
        if (type_ == SuffixType::Hash) {
            if (use_any_ && ToUint32(kAnyChar, item_size_) == data_.GetValueByIndex(index)) {
                return true;
            }
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

    char GetAny() const {
        return FromUint32(ToUint32(kAnyChar, item_size_));
    }

    size_t DataSizeBits() const {
        return data_.BitsSize();
    }

private:

    uint32_t ToUint32(char c, size_t bits = 8) const {
        uint32_t result = 0;
        char* ptr = (char*)(&result);
        ptr[0] = c;
        result >>= 8 - bits;
        return result;
    }

    char FromUint32(uint32_t x) const {
        // std::cerr << x ;
        x <<= 8 - item_size_;

        // std::cerr << "->" << x << "->" << (((char*)(&x))[0] | ((1 << (8 - item_size_)) - 1)) << "\n";
        return ((char*)(&x))[0];
    }

    CompressedVector<uint32_t> data_;
    std::hash<std::string> hash_;
    size_t item_size_;
    size_t size_;
    SuffixType type_;
    bool use_any_;
};

class FastSuccinctTrie {
public:
    FastSuccinctTrie() {
    }

    void Init(SuffixType suf_type, size_t suffix_size = kDefaultSurfSuffixSize) {
        suffix_type_ = suf_type;
        if (suffix_type_ == SuffixType::Empty) {
            suffix_size_ = 0;
        } else {
            suffix_size_ = suffix_size;
        }
    }

    void Build(const std::vector<std::string>& values, bool use_terminator = true, int fixed_length = -1, bool use_any = false) {
        use_terminator_ = use_terminator;
        fixed_length_ = fixed_length;
        use_any_ = use_any;

        std::vector<bool> done(values.size(), false);
        s_values_ = SuffixVector(suffix_type_, values.size(), suffix_size_, use_any_);

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
                            if (use_any && idx == fixed_length_) {
                                if (!HaveCommonPrefixes(values[i], values[i + 1], idx)) {
                                    s_values_.AddAnySuffix();
                                }
                                done[i] = true;
                                continue;
                            }
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
                if (use_any_ && suf == s_values_.GetAny()) {
                    break;
                }
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
            if ((suf != kTerminator || !use_terminator_) && (suf != s_values_.GetAny() || !use_any_)) {
                result += suf;
            }
        }
        while (pos != -1) {
            auto suf = s_labels_[pos];
            if ((suf != kTerminator || !use_terminator_) && (suf != s_values_.GetAny() || !use_any_)) {
                result += suf;
            }
            pos = MoveToParent(pos);
        }

        std::reverse(result.begin(), result.end());
        if (fixed_length_ != -1) {
            if (!use_any_ || (result.size() > fixed_length_)) {
                result.resize(fixed_length_);
            }
        }
        return result;
    }

    std::vector<char> s_labels_;
    BitVector s_has_child_;
    BitVector s_louds_;
    SuffixVector s_values_;
    SuffixType suffix_type_;
    size_t suffix_size_;
    bool use_terminator_;
    int fixed_length_;
    bool use_any_;
};

template <class T>
class DefaultSurfConverter {
public:
    std::string ToString(const T& x) const = 0;
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

    void Init(SuffixType suf_type,
              size_t suffix_size = kDefaultSurfSuffixSize,
              int fix_length = -1,
              double cut_gain_threshold = 0.0) {
        // fix_length = - 1 to always use kTerminator (must not be set if numeric values are stored)
        // fix_length = 0 to skip kTerminator when all items have the same length
        // fix_length > 0 to cut all strings to fix_length
        trie_.Init(suf_type, suffix_size);
        suffix_type_ = suf_type;
        fix_length_ = fix_length;
        cut_gain_threshold_ = cut_gain_threshold;
    }

    void Build(const std::vector<T>& values) override {
        std::vector<std::string> strings;
        bool used_terminator = false;
        bool use_any = false;
        size_t min_length = 0, max_length = 0;
        for (const auto& x : values) {
            auto s = converter_.ToString(x);
            strings.push_back(s);
            if (strings.back().size() > max_length) {
                max_length = strings.back().size();
            }
            if (min_length == 0 || strings.back().size() < min_length) {
                min_length = strings.back().size();
            }
        }
        int fixed_length = -1;
        if ((min_length == max_length) && fix_length_ >= 0) {
            fixed_length = min_length;
            if (fix_length_ > 0) {
                fixed_length == std::min(fix_length_, fixed_length);
            }
        }
        if (fix_length_ > 0 && max_length > fix_length_) {
            fixed_length = fix_length_;
            use_any = true;
        }

        std::sort(strings.begin(), strings.end());
        strings.erase(std::unique(strings.begin(), strings.end()), strings.end());

        for (size_t i = 0; i < strings.size(); ++i) {
            if (i + 1 < strings.size() && IsSubstr(strings[i], strings[i + 1])) {
                used_terminator = true;
                strings[i].push_back(kTerminator);
            }
        }

        if (cut_gain_threshold_ > 0.0) {
            if (suffix_type_ == SuffixType::Hash) {
                std::cerr << "Warning! For suffix_type = 'hash' cut_gain_threshold must be equal to zero. Reset cut_gain_threshold.";
            } else {
                PreBuildFilter(strings, cut_gain_threshold_);
            }
        }

        trie_.Build(strings, used_terminator, fixed_length, use_any);
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

    void PrintLB(const T& x) const {
        auto k = converter_.ToString(x);
        for (const auto& c : k) {
            std::cerr << static_cast<int>(c) << " ";
        }
        std::cerr << "-> ";
        auto lb = trie_.LowerBound(k);
        for (const auto& c : lb) {
            std::cerr << static_cast<int>(c) << " ";
        }
        std::cerr << "\n";
    }

private:
    void PreBuildFilter(std::vector<std::string>& strings, double threshold) const {
        if (strings.empty()) {
            return;
        }
        std::vector<int> common_prefixes(strings.size());
        std::vector<int> left_subtrees(strings.size());
        std::vector<int> right_subtrees(strings.size());
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i + 1 != strings.size()) {
                common_prefixes[i] = CommonPrefixLength(strings[i], strings[i + 1]);
            } else {
                common_prefixes[i] = 0;
            }
            if (i == 0) {
                left_subtrees[i] = common_prefixes[i] + 1;
            } else {
                left_subtrees[i] = left_subtrees[i - 1] + 1 + std::max(0, common_prefixes[i] - common_prefixes[i - 1]);
            }
        }
        for (int i = strings.size() - 1; i >= 0; --i) {
            if (i == strings.size() - 1) {
                right_subtrees[i] = common_prefixes[i - 1] + 1;
            } else if (i == 0) {
                right_subtrees[i] = right_subtrees[i + 1] + 1;
            } else {
                right_subtrees[i] = right_subtrees[i + 1] + 1 + std::max(0, common_prefixes[i - 1] - common_prefixes[i]);
            }
        }
        int tree_size = right_subtrees[0];

        for (size_t i = 0; i < strings.size(); ++i) {
            size_t j = i;
            while (j < strings.size() - 1 && common_prefixes[j] >= common_prefixes[i] && common_prefixes[j] != 0 && j - i < 20) {
                ++j;
                int cp = i == 0 ? 0 : common_prefixes[i - 1];
                int64_t len = j - i + 1;
                int cut_gain = left_subtrees[j] + right_subtrees[i] - tree_size - std::max(cp, common_prefixes[j]) - 1;
                double cf = static_cast<double>(cut_gain) / (len * len);
                size_t size_after_resize = std::max(cp, common_prefixes[j]) + 2;
                if (cf > threshold) {
                    if (strings[i].size() >= size_after_resize && strings[j].size() >= size_after_resize &&
                        strings[i].substr(0, size_after_resize) == strings[j].substr(0, size_after_resize)) {
                        for (size_t k = i; k <= j; ++k) {
                            strings[k].resize(std::max(cp, common_prefixes[j]) + 2);
                        }
                    }
                }
            }
        }
        strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
    }

    FastSuccinctTrie trie_;
    Converter converter_;
    SuffixType suffix_type_;
    int fix_length_;
    double cut_gain_threshold_;
};
