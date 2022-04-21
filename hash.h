#pragma once

#include <climits>
#include <random>

template <class Generator>
int RandomInt(Generator& generator, int lower, int upper) {
    std::uniform_int_distribution<int> distribution(lower, upper);
    return distribution(generator);
}

class LinearHashFunction {
public:
    LinearHashFunction() = default;

    LinearHashFunction(int alpha, int beta, int64_t prime = kLargePrimeNumber)
            : alpha_(alpha % prime), beta_(beta % prime), prime_(prime) {
    }

    uint64_t operator()(int number) const {
        return (((static_cast<uint64_t>(number) + prime_) % prime_) * alpha_ + beta_) % prime_;
    }

    uint64_t operator()(const std::string& string) const {
        uint64_t hash = 0;
        uint64_t pow = 1;
        for (const auto c : string) {
            hash = (hash + static_cast<uint64_t>(c) * pow) % prime_;
            pow *= alpha_;
        }
        return hash;
    }

private:
    int alpha_;
    int beta_;
    int64_t prime_;
    static const int64_t kLargePrimeNumber = 2932031007403;
};

class LinearHashFunctionBuilder {
public:
    LinearHashFunctionBuilder() = default;

    LinearHashFunction operator()(std::mt19937& generator, int64_t prime = 2932031007403) {
        int64_t first = RandomInt(generator, 1, std::numeric_limits<int>::max());
        int64_t second = RandomInt(generator, 0, std::numeric_limits<int>::max());
        return LinearHashFunction(first, second, prime);
    }
};