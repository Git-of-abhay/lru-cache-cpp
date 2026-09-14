#include "lru/cache.hpp"
#include <charconv>
#include <iostream>
#include <sstream>
#include <string>

bool run(lru::Cache<std::string, std::string>& cache, const std::string& line) {
    std::istringstream input(line);
    std::string command, key, value, extra;
    input >> command;
    if (command.empty()) return true;
    if (command == "put" && input >> key >> value && !(input >> extra)) {
        std::cout << (cache.put(key, value) ? "stored" : "disabled: capacity is zero") << '\n';
    } else if ((command == "get" || command == "peek" || command == "erase") &&
               input >> key && !(input >> extra)) {
        if (command == "erase") std::cout << (cache.erase(key) ? "erased" : "missing") << '\n';
        else {
            const auto* result = command == "get" ? cache.get(key) : cache.peek(key);
            std::cout << (result ? *result : "MISS") << '\n';
        }
    } else if (command == "resize" && input >> value && !(input >> extra)) {
        std::size_t capacity{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), capacity);
        if (error != std::errc{} || end != value.data() + value.size())
            std::cout << "error: capacity must be a nonnegative integer\n";
        else { cache.resize(capacity); std::cout << "capacity=" << capacity << '\n'; }
    } else if (command == "show" && !(input >> extra)) {
        std::cout << "MRU [ ";
        for (const auto& [k, v] : cache.snapshot()) std::cout << k << ':' << v << ' ';
        std::cout << "] LRU\n";
    } else if (command == "stats" && !(input >> extra)) {
        const auto s = cache.statistics();
        std::cout << "hits=" << s.hits << " misses=" << s.misses << " evictions=" << s.evictions
                  << " hit_rate=" << s.hit_rate() << '\n';
    } else if (command == "clear" && !(input >> extra)) { cache.clear(); std::cout << "cleared\n"; }
    else if (command == "quit" && !(input >> extra)) return false;
    else std::cout << "error: use put KEY VALUE | get KEY | peek KEY | erase KEY | resize N | show | stats | clear | quit\n";
    return true;
}
int main(int argc, char** argv) {
    lru::Cache<std::string, std::string> cache(3);
    if (argc == 2 && std::string(argv[1]) == "--demo") {
        for (const auto* line : {"put A apple", "put B banana", "put C cherry", "show", "get A",
                                 "put D date", "show", "get B", "stats"}) {
            std::cout << "> " << line << '\n'; run(cache, line);
        }
        return 0;
    }
    if (argc != 1) { std::cerr << "Usage: lru_cli [--demo]\n"; return 1; }
    std::cout << "LRU cache | capacity=3 | type show or put KEY VALUE; quit to exit\n";
    for (std::string line; std::getline(std::cin, line);) if (!run(cache, line)) break;
}
