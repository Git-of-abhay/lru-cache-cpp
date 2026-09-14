#include "lru/cache.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

class LinearCache {
    std::size_t capacity_;
    std::vector<std::pair<int, int>> data_;
public:
    explicit LinearCache(std::size_t n) : capacity_(n) { data_.reserve(n + 1); }
    const int* get(int key) {
        const auto it = std::find_if(data_.begin(), data_.end(), [key](auto p) { return p.first == key; });
        if (it == data_.end()) return nullptr;
        const auto p = *it; data_.erase(it); data_.insert(data_.begin(), p);
        return &data_.front().second;
    }
    void put(int key, int value) {
        const auto it = std::find_if(data_.begin(), data_.end(), [key](auto p) { return p.first == key; });
        if (it != data_.end()) data_.erase(it);
        data_.insert(data_.begin(), {key, value});
        if (data_.size() > capacity_) data_.pop_back();
    }
};
struct Result { double ns; std::uint64_t checksum; std::size_t hits; };
template<class C> Result measure(std::size_t capacity, const std::vector<int>& keys) {
    C cache(capacity);
    std::uint64_t sum = 0;
    std::size_t hits = 0;
    const auto start = std::chrono::steady_clock::now();
    for (const int key : keys) {
        if (const auto* v = cache.get(key)) { sum += static_cast<unsigned>(*v); ++hits; }
        else cache.put(key, key);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return {std::chrono::duration<double, std::nano>(elapsed).count() / keys.size(), sum, hits};
}
int main(int argc, char** argv) {
    std::size_t count = 200000;
    if (argc > 2) return 1;
    if (argc == 2) {
        std::string_view arg(argv[1]);
        const auto [end, error] = std::from_chars(arg.data(), arg.data() + arg.size(), count);
        if (error != std::errc{} || end != arg.data() + arg.size() || count == 0 || count > 10000000) {
            std::cerr << "operations must be 1..10000000\n"; return 1;
        }
    }
    std::cout << "workload,capacity,operations,hash_list_ns/op,linear_ns/op,speedup,hits,checksum\n";
    for (const auto capacity : {64U, 512U, 4096U}) {
        for (const bool hot : {false, true}) {
            std::mt19937 rng(42);
            std::vector<int> keys(count);
            for (auto& key : keys) {
                const unsigned range = hot && rng() % 10 < 9 ? capacity / 4 : capacity * 4;
                key = static_cast<int>(rng() % range);
            }
            const auto a = measure<lru::Cache<int, int>>(capacity, keys);
            const auto b = measure<LinearCache>(capacity, keys);
            if (a.checksum != b.checksum || a.hits != b.hits) return 2;
            std::cout << (hot ? "hot90" : "uniform") << ',' << capacity << ',' << count << ','
                      << a.ns << ',' << b.ns << ',' << b.ns / a.ns << ',' << a.hits << ',' << a.checksum << '\n';
        }
    }
}
