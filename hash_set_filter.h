#include "filter.h"

#include <unordered_set>

template <class T, class Hash = std::hash<T>>
class HashSetFilter : public Filter<T> {
public:

    HashSetFilter() {
    }

    void Add(const T& value) {
        values.insert(value);
    }

    void Build(const std::vector<T>& values) override {
        for (const auto& x : values) {
            Add(x);
        }
    }

    bool Find(const T& value) const override {
        return values.find(value) != values.end();
    }

    std::unordered_set<T, Hash> GetHashSet() const {
        return values;
    }

private:
    std::unordered_set<T, Hash> values;
};
