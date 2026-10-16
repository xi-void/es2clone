#pragma once
#include "audio_assets.hpp"
#include <atomic>
#include <thread>
#include <random>

namespace siege {
// The decoder worker owns all file I/O. SDL only consumes a bounded SPSC ring.
class MusicStream {
public:
    ~MusicStream(){stop();}
    void start(const std::vector<std::filesystem::path>& paths,bool random,float crossfade);
    void stop();
    bool pop(float& left,float& right) noexcept;
    size_t buffered() const noexcept {return size_t(written.load(std::memory_order_acquire)-read.load(std::memory_order_acquire));}
private:
    static constexpr size_t capacity=audioRate*2;
    std::array<float,capacity*2> ring{};
    std::atomic<uint64_t> written{0},read{0};
    std::atomic<bool> running{false};
    std::thread worker;
    std::vector<std::filesystem::path> playlist;
    bool shuffle=false;
    float fadeSeconds=0;
    void produce();
};
}
