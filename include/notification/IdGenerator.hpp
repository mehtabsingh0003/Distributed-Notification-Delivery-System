#pragma once

#include <random>
#include <sstream>
#include <string>

namespace notification {

// Thread-safe UUIDv4 generator. Each thread gets its own Mersenne Twister
// seeded from std::random_device so worker threads don't contend on a
// shared generator (which would otherwise become a lock hotspot under load).
class IdGenerator {
public:
    static std::string uuid4() {
        thread_local std::mt19937_64 rng{std::random_device{}()};
        thread_local std::uniform_int_distribution<int> dist(0, 15);
        thread_local std::uniform_int_distribution<int> distVariant(8, 11);

        static const char* hex = "0123456789abcdef";
        std::string out;
        out.reserve(36);

        auto appendHex = [&](int n) { out.push_back(hex[n]); };

        for (int i = 0; i < 8; ++i) appendHex(dist(rng));
        out.push_back('-');
        for (int i = 0; i < 4; ++i) appendHex(dist(rng));
        out.push_back('-');
        out.push_back('4');  // version 4
        for (int i = 0; i < 3; ++i) appendHex(dist(rng));
        out.push_back('-');
        appendHex(distVariant(rng));  // variant bits
        for (int i = 0; i < 3; ++i) appendHex(dist(rng));
        out.push_back('-');
        for (int i = 0; i < 12; ++i) appendHex(dist(rng));

        return out;
    }
};

}  // namespace notification
