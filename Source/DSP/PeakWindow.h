#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace skelly {
// Fixed monotonic queue for an exact rolling maximum of finite nonnegative
// samples. No allocation. Unsigned age subtraction remains valid on wrap.
template<std::size_t Window> class PeakWindow {
    static_assert(Window>0);
    struct Entry { float value=0.f; std::uint64_t time=0; };
    std::array<Entry,Window> entries{};
    std::size_t head=0, count=0;
    std::uint64_t time=0;
    static std::size_t index(std::size_t i) noexcept { return i>=Window ? i-Window : i; }
public:
    void reset() noexcept { head=count=0;time=0; }
    void push(float value) noexcept {
        if(count && time-entries[head].time>=Window) {head=index(head+1);--count;}
        while(count && entries[index(head+count-1)].value<=value) --count;
        entries[index(head+count)]={value,time++};
        ++count;
    }
    float maximum() const noexcept {return count ? entries[head].value : 0.f;}
};
}
