#pragma once

#include "consts.h"
#include "discrete_distribution.h"
#include "zipf_mandelbrot.h"

#include <fstream>
#include <random>
#include <string>

template <class T, class Generator>
class TestData {
public:
    TestData(Generator& generator) : generator_(generator), items_() {
    }

    T GenerateQuery() const {
        return generator_.SearchQuery();
    }

    T NewItem() {
        T item = generator_.AddQuery();
        items_.insert(item);
        return item;
    }

    auto Begin() const {
        return items_.begin();
    }

    auto End() const {
        return items_.end();
    }

    bool Contains(const T& item) const {
        return items_.find(item) != items_.end();
    }

private:
    Generator& generator_;
    std::unordered_set<T> items_;
};

template <class Rng>
class UniformIntTestData {
public:
    UniformIntTestData(Rng& rng, int min, int max) : rng_(rng), distribution_(min, max) {
    }

    int AddQuery () {
        return distribution_(rng_);
    }

    int SearchQuery () {
        return distribution_(rng_);
    }

private:
    Rng& rng_;
    std::uniform_int_distribution<int> distribution_;
};

template <class Rng>
class ZipfMandelbrotIntTestData {
public:
    ZipfMandelbrotIntTestData(Rng& rng, double s, double q, int max)
        : rng_(rng), uniform_(0, std::min(max * 10, kMaxNumber)), zipf_(s, q, max) {
    }

    int AddQuery () {
        return uniform_(rng_);
    }

    int SearchQuery () {
        return zipf_(rng_);
    }

private:
    Rng& rng_;
    std::uniform_int_distribution<int> uniform_;
    rng::zipf_mandelbrot_distribution<rng::discrete_distribution_30bit,int> zipf_;
};

template <class Rng>
class RandomTextTestData {
public:
    RandomTextTestData(Rng& rng, int min, int max)
        : rng_(rng), length_distribution_(min, max), char_distribution_(0, 'z' - 'a') {
    }

    std::string AddQuery () {
        std::string s;
        s.resize(length_distribution_(rng_));
        for (size_t i = 0; i < s.length(); ++i) {
            s[i] = 'a' + char_distribution_(rng_);
        }
        return s;
    }

    std::string SearchQuery () {
        return AddQuery();
    }

private:
    Rng& rng_;
    std::uniform_int_distribution<int> length_distribution_;
    std::uniform_int_distribution<int> char_distribution_;
};

class PaymentsCsvParser {
public:
    void operator()(const std::string& s, std::string& key, bool& value) const {
        std::stringstream ss;
        ss << s;
        std::string part;
        int counter = 0;
        while (std::getline(ss, part, ',')) {
            ++counter;
            if (counter == 4) {
                key = part;
            }
            if (counter == 3) {
                value = stof(part) < 10000.0;
            }
        }
    }
};

template <class Rng, class Parser>
class CsvTestData {
public:
    CsvTestData(Rng& rng, const std::string& filename, const Parser& parser)
        : rng_(rng), parser_(parser), keys_to_add_(), keys_to_skip_() , add_counter_(0) {
        std::fstream in;
        in.open(filename);
        std::string s;
        std::string key;
        bool value;
        std::getline(in, s);
        while (std::getline(in, s)) {
            parser(s, key, value);
            if (value) {
                keys_to_add_.push_back(key);
            } else {
                keys_to_skip_.push_back(key);
            }
        }
        std::cout << keys_to_add_.size() << " " << keys_to_skip_.size() << "\n";
        in.close();
    }

    std::string AddQuery() {
        add_counter_ = (add_counter_ + 1) % keys_to_add_.size();
        return keys_to_add_[add_counter_];
    }

    std::string SearchQuery() {
        std::uniform_int_distribution<int> uniform(0, keys_to_skip_.size() - 1);
        return keys_to_skip_[static_cast<size_t>(uniform(rng_))];
    }

private:
    Rng& rng_;
    const Parser& parser_;
    std::vector<std::string> keys_to_add_;
    std::vector<std::string> keys_to_skip_;
    size_t add_counter_;
};