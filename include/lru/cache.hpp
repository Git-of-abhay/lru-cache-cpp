#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lru {
struct Statistics {
    std::size_t hits{}, misses{}, evictions{};
    [[nodiscard]] double hit_rate() const noexcept {
        return hits + misses == 0 ? 0.0 : static_cast<double>(hits) / (hits + misses);
    }
};

// Most recently used at the front. List iterators survive splice and rehash.
// Keys must satisfy unordered_map requirements; Hash/Equal must not throw.
template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
class Cache {
    using List = std::list<std::pair<Key, Value>>;
    using Iterator = typename List::iterator;
    std::size_t capacity_;
    List entries_;
    std::unordered_map<Key, Iterator, Hash, Equal> index_;
    Statistics stats_;

    void evict_one() {
        const auto victim = std::prev(entries_.end());
        index_.erase(victim->first);
        entries_.erase(victim);
        ++stats_.evictions;
    }
public:
    explicit Cache(std::size_t capacity) : capacity_(capacity) {}
    // Default copy would leave the index pointing into the original list.
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    Cache(Cache&&) = delete;
    Cache& operator=(Cache&&) = delete;

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] Statistics statistics() const noexcept { return stats_; }
    void reset_statistics() noexcept { stats_ = {}; }

    // Pointer is valid until this key is replaced, erased, evicted, or cleared.
    [[nodiscard]] const Value* get(const Key& key) {
        const auto found = index_.find(key);
        if (found == index_.end()) { ++stats_.misses; return nullptr; }
        ++stats_.hits;
        entries_.splice(entries_.begin(), entries_, found->second);
        return &found->second->second;
    }
    [[nodiscard]] const Value* peek(const Key& key) const {
        const auto found = index_.find(key);
        return found == index_.end() ? nullptr : &found->second->second;
    }
    [[nodiscard]] bool contains(const Key& key) const { return index_.contains(key); }

    // Returns false only when capacity is zero; updates also become MRU.
    bool put(Key key, Value value) {
        if (capacity_ == 0) return false;
        const auto found = index_.find(key);
        // Construct first so a throwing value construction leaves old data intact.
        entries_.emplace_front(std::move(key), std::move(value));
        if (found != index_.end()) {
            const auto old = found->second;
            found->second = entries_.begin();
            entries_.erase(old);
        } else {
            try {
                index_.emplace(entries_.front().first, entries_.begin());
            } catch (...) {
                entries_.pop_front();
                throw;
            }
            if (size() > capacity_) evict_one();
        }
        return true;
    }
    bool erase(const Key& key) {
        const auto found = index_.find(key);
        if (found == index_.end()) return false;
        entries_.erase(found->second);
        index_.erase(found);
        return true;
    }
    void resize(std::size_t capacity) {
        while (size() > capacity) evict_one();
        capacity_ = capacity;
    }
    void clear() noexcept { index_.clear(); entries_.clear(); }
    // Requires copyable keys and values. Ordered MRU -> LRU.
    [[nodiscard]] std::vector<std::pair<Key, Value>> snapshot() const {
        return {entries_.begin(), entries_.end()};
    }
};

// Each operation holds one mutex. Return copies, never pointers escaping a lock.
template<class Key, class Value>
class SynchronizedCache {
    mutable std::mutex mutex_;
    Cache<Key, Value> cache_;
public:
    explicit SynchronizedCache(std::size_t capacity) : cache_(capacity) {}
    bool put(Key key, Value value) {
        std::lock_guard lock(mutex_);
        return cache_.put(std::move(key), std::move(value));
    }
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        std::lock_guard lock(mutex_);
        const auto* value = cache_.get(key);
        return value ? std::optional<Value>(*value) : std::nullopt;
    }
    bool erase(const Key& key) { std::lock_guard lock(mutex_); return cache_.erase(key); }
    void resize(std::size_t capacity) { std::lock_guard lock(mutex_); cache_.resize(capacity); }
    [[nodiscard]] auto snapshot() const { std::lock_guard lock(mutex_); return cache_.snapshot(); }
    [[nodiscard]] Statistics statistics() const {
        std::lock_guard lock(mutex_); return cache_.statistics();
    }
};
} // namespace lru
