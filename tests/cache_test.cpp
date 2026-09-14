#include "lru/cache.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

#define CHECK(expr) do { if (!(expr)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " #expr); } while (false)

void basics() {
    lru::Cache<int, std::string> c(2);
    CHECK(c.empty()); CHECK(c.get(9) == nullptr);
    CHECK(c.put(1, "one")); CHECK(c.put(2, "two"));
    CHECK(*c.get(1) == "one"); c.put(3, "three");
    CHECK(!c.contains(2)); CHECK(c.contains(1));
    c.put(1, "ONE"); CHECK(c.size() == 2); CHECK(*c.peek(1) == "ONE");
    CHECK(c.snapshot().front().first == 1);
    CHECK(*c.peek(3) == "three"); CHECK(c.snapshot().front().first == 1);
    c.resize(1); CHECK(!c.contains(3));
    CHECK(c.statistics().evictions == 2); CHECK(c.statistics().hits == 1);
    CHECK(c.statistics().misses == 1); CHECK(c.statistics().hit_rate() == 0.5);
    CHECK(c.erase(1)); CHECK(!c.erase(1)); CHECK(c.empty());
    c.resize(0); CHECK(!c.put(4, "four")); CHECK(c.empty());
    c.resize(2); c.put(5, "five"); c.clear(); CHECK(c.empty());
    CHECK(c.statistics().evictions == 2); c.reset_statistics();
    CHECK(c.statistics().hit_rate() == 0.0);
    lru::Cache<int, int> single(1);
    single.put(1, 1); single.put(1, 2); CHECK(single.statistics().evictions == 0);
    single.put(2, 2); CHECK(!single.contains(1)); single.resize(0); CHECK(single.empty());
}
struct CollisionHash { std::size_t operator()(int) const noexcept { return 0; } };
void collisions_and_ownership() {
    lru::Cache<int, int, CollisionHash> c(128);
    for (int i = 0; i < 256; ++i) c.put(i, i * i);
    for (int i = 128; i < 256; ++i) CHECK(*c.get(i) == i * i);
    CHECK(c.size() == 128); CHECK(c.statistics().evictions == 128);
    lru::Cache<int, std::unique_ptr<int>> owned(1);
    owned.put(1, std::make_unique<int>(42)); CHECK(**owned.get(1) == 42);
    owned.put(2, std::make_unique<int>(7)); CHECK(!owned.contains(1));
}
struct ThrowingKey {
    int value;
    static inline bool fail = false;
    explicit ThrowingKey(int v) : value(v) {}
    ThrowingKey(const ThrowingKey& other) : value(other.value) {
        if (fail) throw std::runtime_error("injected key copy failure");
    }
    ThrowingKey(ThrowingKey&&) = default;
    bool operator==(const ThrowingKey&) const noexcept = default;
};
struct KeyHash { std::size_t operator()(const ThrowingKey& k) const noexcept { return static_cast<std::size_t>(k.value); } };
void rollback() {
    lru::Cache<ThrowingKey, int, KeyHash> c(1);
    c.put(ThrowingKey(1), 10);
    ThrowingKey::fail = true;
    bool threw = false;
    try { c.put(ThrowingKey(2), 20); } catch (const std::runtime_error&) { threw = true; }
    ThrowingKey::fail = false;
    CHECK(threw); CHECK(c.size() == 1); CHECK(*c.peek(ThrowingKey(1)) == 10);
    CHECK(!c.contains(ThrowingKey(2))); CHECK(c.statistics().evictions == 0);
    c.put(ThrowingKey(2), 20); CHECK(*c.get(ThrowingKey(2)) == 20);
}
void randomized() {
    // Independent vector model: compare all values, full recency, and counters.
    for (unsigned seed : {1U, 42U, 2026U, 99173U}) {
        std::mt19937 rng(seed);
        std::size_t cap = 7;
        lru::Cache<int, int> c(cap);
        std::vector<std::pair<int, int>> model;
        lru::Statistics stats;
        for (int step = 0; step < 25000; ++step) {
            const int key = static_cast<int>(rng() % 40);
            const int value = static_cast<int>(rng() % 10000);
            auto it = std::find_if(model.begin(), model.end(), [key](auto p) { return p.first == key; });
            switch (rng() % 7) {
            case 0:
                CHECK(c.put(key, value) == (cap != 0));
                if (cap) {
                    if (it != model.end()) model.erase(it);
                    model.insert(model.begin(), {key, value});
                    if (model.size() > cap) { model.pop_back(); ++stats.evictions; }
                }
                break;
            case 1: {
                const auto* actual = c.get(key);
                if (it == model.end()) { CHECK(actual == nullptr); ++stats.misses; }
                else {
                    CHECK(actual && *actual == it->second); ++stats.hits;
                    const auto pair = *it; model.erase(it); model.insert(model.begin(), pair);
                }
                break;
            }
            case 2:
                CHECK(c.erase(key) == (it != model.end()));
                if (it != model.end()) model.erase(it);
                break;
            case 3:
                cap = rng() % 20; c.resize(cap);
                while (model.size() > cap) { model.pop_back(); ++stats.evictions; }
                break;
            case 4: {
                const auto* actual = c.peek(key);
                CHECK((actual != nullptr) == (it != model.end()));
                if (actual) CHECK(*actual == it->second);
                break;
            }
            case 5: c.clear(); model.clear(); break;
            case 6: c.reset_statistics(); stats = {}; break;
            }
            CHECK(c.snapshot() == model); CHECK(c.size() <= c.capacity());
            CHECK(c.statistics().hits == stats.hits);
            CHECK(c.statistics().misses == stats.misses);
            CHECK(c.statistics().evictions == stats.evictions);
        }
    }
}
void concurrency() {
    lru::SynchronizedCache<int, int> c(64);
    std::atomic<bool> valid{true};
    std::vector<std::thread> workers;
    for (int t = 0; t < 8; ++t) workers.emplace_back([&, t] {
        for (int i = 0; i < 5000; ++i) {
            const int key = t * 100 + i % 100;
            c.put(key, key * 2);
            if (const auto v = c.get(key); v && *v != key * 2) valid = false;
            if (i % 7 == 0) c.erase(key);
            if (i % 101 == 0) c.resize(32 + static_cast<std::size_t>(i % 33));
        }
    });
    for (auto& thread : workers) thread.join();
    CHECK(valid.load()); CHECK(c.snapshot().size() <= 64);
    const auto s = c.statistics(); CHECK(s.hits + s.misses == 40000);
}
int main() {
    try {
        basics(); std::cout << "PASS edge cases, recency, counters\n";
        collisions_and_ownership(); std::cout << "PASS collisions, rehash, move-only values\n";
        rollback(); std::cout << "PASS insertion exception rollback\n";
        randomized(); std::cout << "PASS 100000 randomized model operations\n";
        concurrency(); std::cout << "PASS 8-thread stress, 40000 reads\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
