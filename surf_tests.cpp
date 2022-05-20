#include <climits>
#include <iostream>
#include <memory>
#include <random>

#include "consts.h"
#include "surf.h"
#include "testdata.h"

SuffixType s_type = SuffixType::Hash;
size_t suffix_size = kDefaultSurfSuffixSize;
int fixed_length = 0;
double cut_gain_threshold = 0.0;

template <class T, class Function>
void TestQueries(const std::vector<T>& queries, std::vector<T>& found, std::vector<T>& not_found, Function f) {
    found.clear();
    not_found.clear();
    for (const auto& x : queries) {
        if (f(x)) {
            found.push_back(x);
        } else {
            not_found.push_back(x);
        }
    }
}

template <class T>
void PrintTestResult(const std::string& label, const std::vector<T>& found, const std::vector<T>& not_found, bool check_false_negative = false) {
    std::cerr << label << ": \n";
    std::cerr << "Found: ";
    for (const auto& x : found) {
        std::cerr << x << " ";
    }
    std::cerr << "\n";
    std::cerr << "Not found: ";
    for (const auto& x : not_found) {
        std::cerr << x << " ";
    }
    std::cerr << "\n\n";
    if (!not_found.empty() && check_false_negative) {
        std::cerr << "Has false negative\n";
        throw;
    }
}

template <class T>
void RunExactQueriesTest(const std::vector<T>& a, const std::vector<T>& b) {
    auto ptr = std::make_unique<SuccinctRangeFilter<T>>();
    ptr->Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
    ptr->Build(a);

    std::vector<T> found;
    std::vector<T> not_found;
    TestQueries(a, found, not_found, [&](const T& x) {return ptr->Find(x);});
    PrintTestResult("Checking existing items", found, not_found, true);

    TestQueries(b, found, not_found, [&](const T& x) {return ptr->Find(x);});
    PrintTestResult("Checking missing items", found, not_found);
}

void RunRangeQueriesTest(const std::vector<std::string>& a,
                          const std::vector<SearchRange<std::string>>& true_range,
                          const std::vector<SearchRange<std::string>>& false_range) {
    auto ptr = std::make_unique<SuccinctRangeFilter<std::string>>();
    ptr->Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
    ptr->Build(a);

    std::vector<SearchRange<std::string>> found;
    std::vector<SearchRange<std::string>> not_found;
    TestQueries(true_range, found, not_found, [&](const SearchRange<std::string>& x) {return ptr->FindRange(x);});
    PrintTestResult("Checking existing prefixes", found, not_found, true);

    TestQueries(false_range, found, not_found, [&](const SearchRange<std::string>& x) {return ptr->FindRange(x);});
    PrintTestResult("Checking missing prefixes", found, not_found);
}

void RunPrefixQueriesTest(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    auto ptr = std::make_unique<SuccinctRangeFilter<std::string>>();
    ptr->Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
    ptr->Build(a);

    std::vector<std::string> prefix;
    for (const auto& s : a) {
        for (size_t i = 0; i < s.size(); ++ i) {
            prefix.push_back(s.substr(0, i + 1));
        }
    }
    std::sort(prefix.begin(), prefix.end());
    prefix.erase(std::unique(prefix.begin(), prefix.end()), prefix.end());

    std::vector<std::string> found;
    std::vector<std::string> not_found;
    TestQueries(prefix, found, not_found, [&](const std::string& x) {return ptr->FindPrefix(x);});
    PrintTestResult("Checking existing prefixes", found, not_found, true);

    TestQueries(b, found, not_found, [&](const std::string& x) {return ptr->FindPrefix(x);});
    PrintTestResult("Checking missing prefixes", found, not_found);
}

void RunSmallTests() {
    std::vector<std::string> first({
        "a",
        "aaaafoo",
        "aaabaa",
        "aaababfoo",
        "aaac",
        "babcdefga",
        "babcdefgbfoo",
        "bacfoo",
        "ca",
        "cbfoo",
        "cca",
        "ccaa",
    });
    SuccinctRangeFilter<std::string> first_filter;
    first_filter.Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
    first_filter.Build(first);
    RunExactQueriesTest(first, std::vector<std::string>());
    /*
    std::vector<std::string> a({"f", "far", "fas", "fas", "fast", "fat", "s", "top", "toy", "trie", "trip", "try", "fastbiba"});
    std::vector<std::string> b({"", "t", "g", "abs", "sfar", "tfast", "farm", "tooy", "tree", "rip", "tryf", "true", "fastb", "fastboba", "fastbot", "fastbit"});
    std::vector<std::string> b_prefix({"", "m", "fu", "ti", "trick", "true", "fastmm", "fastbmm", "fastbimmm", "fastbibmmm"});
    std::vector<SearchRange<std::string>> b_true_range({{"aba", "zeta"}, {"far", "faro"}, {"tridvaodin", "trifon"}});
    std::vector<SearchRange<std::string>> b_false_range({{"uhhhh", "vertex"}, {"trifon", "trim"}});
    RunExactQueriesTest(a, b);
    RunPrefixQueriesTest(a, b_prefix);
    RunRangeQueriesTest(a, b_true_range, b_false_range);

    std::vector<std::string> c({"sigma", "sigint", "sigkek", "sigbiba", "sigboba", "sigu"});
    std::vector<std::string> d({"sigbib", "sigboban", "siga", "biba", "sig"});
    std::vector<std::string> d_prefix({"g", "sigkart", "sigkeras", "signature", "sigusigu"});
    RunExactQueriesTest(c, d);
    RunPrefixQueriesTest(c, d_prefix);

    std::vector<int> e({kMinNumber, -4444, -1, -1, 0, 21, 3352, 5555555, kMaxNumber - 2, kMaxNumber, kMaxNumber});
    std::vector<int> f({kMinNumber + 1, -3333, -2, 1, 20, 229, 3096, kMaxNumber - 256 * 256, kMaxNumber - 1});
    RunExactQueriesTest(e, f);
     */
}

void RunLargeTextTest() {
    std::cerr << "Large text test\n";
    std::mt19937 generator(322);
    RandomTextTestData<std::mt19937> g(generator, 1, 15);
    std::uniform_int_distribution<int> distribution(0, 3);

    size_t n = 30000;
    std::vector<std::string> strings(n);
    for (size_t i = 0; i < n; ++i) {
        strings[i] = g.AddQuery();
    }
    std::sort(strings.begin(), strings.end());
    strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
    n = strings.size();

    std::vector<bool> in(n, false);
    std::vector<std::string> strings_to_add;
    std::vector<std::string> missing_strings;
    std::vector<std::string> prefixes;

    for (size_t i = 0; i < n; ++i) {
        in[i] = distribution(generator) == 0;
        if (in[i]) {
            strings_to_add.push_back(strings[i]);
            for (size_t j = 0; j < strings[i].size(); ++j) {
                if (distribution(generator) == 0) {
                    prefixes.push_back(strings[i].substr(0, j + 1));
                }
            }
        } else {
            missing_strings.push_back(strings[i]);
        }
    }
    std::sort(prefixes.begin(), prefixes.end());
    prefixes.erase(std::unique(prefixes.begin(), prefixes.end()), prefixes.end());

    std::cerr << "Build filter for " << strings_to_add.size() << " words of " << n << "\n\n";
    auto ptr = std::make_unique<SuccinctRangeFilter<std::string>>();
    ptr->Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
    ptr->Build(strings_to_add);

    std::cerr << "Checking existing values\n";
    int found = 0;
    for (const auto& s : strings_to_add) {
        if (ptr->Find(s)) {
            ++found;
        }
    }
    double percent_found = 100 * static_cast<double>(found) / strings_to_add.size();
    std::cerr << "Found " << found << " of " << strings_to_add.size() << " (" << percent_found << "%)\n\n";

    std::cerr << "Checking missing values\n";
    found = 0;
    for (const auto& s : missing_strings) {
        if (ptr->Find(s)) {
            ++found;
        }
    }
    percent_found = 100 * static_cast<double>(found) / missing_strings.size();
    std::cerr << "Found " << found << " of " << missing_strings.size() << " (" << percent_found << "%)\n\n";

    std::cerr << "Checking prefixes\n";
    found = 0;
    for (const auto& s : prefixes) {
        if (ptr->FindPrefix(s)) {
            ++found;
        }
    }
    percent_found = 100 * static_cast<double>(found) / prefixes.size();
    std::cerr << "Found " << found << " of " << prefixes.size() << " (" << percent_found << "%)\n\n";

    std::cerr << "Checking ranges\n";
    found = 0;
    int found_fp = 0;
    int mbt = 0;
    int mbf = 0;
    for (size_t i = 0; i < n - 3; ++i) {
        bool must_be_true = false;
        for (size_t j = i; j <= i + 3; ++j) {
            if (in[j]) {
                must_be_true = true;
            }
        }

        if (must_be_true) {
            ++mbt;
        } else {
            ++mbf;
        }

        if (ptr->FindRange(strings[i], strings[i + 3])) {
            if (must_be_true) {
                ++found;
            } else {
                ++found_fp;
            }
        } else if (must_be_true) {
            std::cerr << "BAD: " << strings[i] << " -- " << strings[i + 3] << "\n";
            ptr->PrintLB(strings[i]);
        }
    }
    percent_found = 100 * static_cast<double>(found) / (mbt);
    std::cerr << "Found (true positive) " << found << " of " << mbt << " (" << percent_found << "%)\n";
    percent_found = 100 * static_cast<double>(found_fp) / (mbf);
    std::cerr << "Found (false positive) " << found_fp << " of " << mbf << " (" << percent_found << "%)\n\n\n";

}

void RunLargeIntTest() {
    std::cerr << "Large int test\n";
    std::mt19937 generator(44);
    UniformIntTestData<std::mt19937> g(generator, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    std::uniform_int_distribution<int> distribution(0, 3);

    size_t n = 60000;
    std::vector<int> values(n);
    for (size_t i = 0; i < n; ++i) {
        values[i] = g.AddQuery();
    }
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    n = values.size();

    std::vector<bool> in(n, false);
    std::vector<int> values_to_add;
    std::vector<int> missing_values;

    for (size_t i = 0; i < n; ++i) {
        in[i] = distribution(generator) == 0;
        if (in[i]) {
            values_to_add.push_back(values[i]);
        } else {
            missing_values.push_back(values[i]);
        }
    }

    std::cerr << "Build filter for " << values_to_add.size() << " numbers of " << n << "\n\n";
    auto ptr = std::make_unique<SuccinctRangeFilter<int>>();
    ptr->Init(s_type, suffix_size, fixed_length, cut_gain_threshold);
    ptr->Build(values_to_add);

    std::cerr << "Checking existing values\n";
    int found = 0;
    for (const auto& s : values_to_add) {
        if (ptr->Find(s)) {
            ++found;
        }
    }
    double percent_found = 100 * static_cast<double>(found) / values_to_add.size();
    std::cerr << "Found " << found << " of " << values_to_add.size() << " (" << percent_found << "%)\n\n";

    std::cerr << "Checking missing values\n";
    found = 0;
    for (const auto& s : missing_values) {
        if (ptr->Find(s)) {
            ++found;
        }
    }
    percent_found = 100 * static_cast<double>(found) / missing_values.size();
    std::cerr << "Found " << found << " of " << missing_values.size() << " (" << percent_found << "%)\n\n";

    std::cerr << "Checking ranges\n";
    found = 0;
    int found_fp = 0;
    int mbt = 0;
    int mbf = 0;
    for (size_t i = 0; i < n - 3; ++i) {
        bool must_be_true = false;
        for (size_t j = i; j <= i + 3; ++j) {
            if (in[j]) {
                must_be_true = true;
            }
        }

        if (must_be_true) {
            ++mbt;
        } else {
            ++mbf;
        }

        if (ptr->FindRange(values[i], values[i + 3])) {
            if (must_be_true) {
                ++found;
            } else {
                ++found_fp;
            }
        } else if (must_be_true) {
            std::cerr << "BAD: " << values[i] << " -- " << values[i + 3] << "\n";
            ptr->PrintLB(values[i]);
        }
    }
    percent_found = 100 * static_cast<double>(found) / (mbt);
    std::cerr << "Found (true positive) " << found << " of " << mbt << " (" << percent_found << "%)\n";
    percent_found = 100 * static_cast<double>(found_fp) / (mbf);
    std::cerr << "Found (false positive) " << found_fp << " of " << mbf << " (" << percent_found << "%)\n\n\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./surf type [suffix_size]\n";
        return 0;
    }

    std::string type = argv[1];
    if (type == "empty" || type == "base") {
        s_type = SuffixType::Empty;
    } else if (type == "real") {
        s_type = SuffixType::Real;
    }

    if (argc > 2) {
        suffix_size = std::stoi(argv[2]);
    }
    if (argc > 3) {
        fixed_length = std::stoi(argv[3]);
    }
    if (argc > 4) {
        cut_gain_threshold = std::stod(argv[4]);
    }

    RunSmallTests();
    RunLargeTextTest();
    RunLargeIntTest();
}