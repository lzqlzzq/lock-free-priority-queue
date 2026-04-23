#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include "rigtorp/MPMCQueue.h"

#if defined(_MSC_VER)
  #include <intrin.h>
#endif


namespace lock_free {

static inline int countr_zero64(std::uint64_t x) noexcept {
    if (x == 0) return -1;

#if defined(__clang__) || defined(__GNUC__)
    return __builtin_ctzll(x);

#elif defined(_MSC_VER)
#if defined(_M_X64) || defined(_M_ARM64)
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<int>(idx);
#else
    unsigned long idx;
    std::uint32_t lo = static_cast<std::uint32_t>(x);
    if (lo) {
        _BitScanForward(&idx, lo);
        return static_cast<int>(idx);
    } else {
        _BitScanForward(&idx, static_cast<std::uint32_t>(x >> 32));
        return static_cast<int>(idx + 32);
    }
#endif

#else
    // de Bruijn fallback
    static constexpr int table[64] = {
        0,  1, 48,  2, 57, 49, 28,  3,
       61, 58, 50, 42, 38, 29, 17,  4,
       62, 55, 59, 36, 53, 51, 43, 22,
       45, 39, 33, 30, 24, 18, 12,  5,
       63, 47, 56, 27, 60, 41, 37, 16,
       54, 35, 52, 21, 44, 32, 23, 11,
       46, 26, 40, 15, 34, 20, 31, 10,
       25, 14, 19,  9, 13,  8,  7,  6
   };

    std::uint64_t isolated = x & (~x + 1);
    return table[(isolated * 0x03f79d71b4cb0a89ULL) >> 58];
#endif
}

using PriorityMask = std::uint64_t;

template<typename T>
class PriorityQueue {
    static_assert(std::is_nothrow_default_constructible_v<T>,
                  "T must be nothrow default constructible");

public:
    explicit PriorityQueue(std::size_t capacity) :
        bitmask(0), buckets(make_buckets(capacity)) {}
    ~PriorityQueue() = default;

    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;
    PriorityQueue(PriorityQueue&&) = delete;
    PriorityQueue& operator=(PriorityQueue&&) = delete;

    template<typename ...Args>
    bool try_emplace(std::uint32_t priority, Args &&... args) noexcept {
        const auto bucketIdx = normalize_priority(priority);
        auto& bucket = this->buckets[bucketIdx];

        if (bucket.queue.try_emplace(std::forward<Args>(args)...)) {
            bucket.version.fetch_add(1, std::memory_order_release);
            const auto mask = get_priority_mask(bucketIdx);
            if ((this->bitmask.load(std::memory_order_relaxed) & mask) == 0) {
                this->bitmask.fetch_or(mask, std::memory_order_release);
            }
            return true;
        }

        return false;
    }

    bool try_push(std::uint32_t priority, T&& item) noexcept {
        return try_emplace(priority, std::move(item));
    }

    bool try_push(std::uint32_t priority, const T& item) noexcept {
        return try_emplace(priority, item);
    }

    std::optional<T> try_pop() noexcept {
        T item;

        for (;;) {
            PriorityMask candidates = this->bitmask.load(std::memory_order_acquire);
            if (candidates == 0) {
                return std::nullopt;
            }

            const auto bucketIdx = static_cast<std::uint32_t>(countr_zero64(candidates));
            auto& bucket = this->buckets[bucketIdx];
            const auto version = bucket.version.load(std::memory_order_acquire);

            if (bucket.queue.try_pop(item)) {
                const auto currentVersion = bucket.version.load(std::memory_order_acquire);
                this->clear_if_empty(bucketIdx, bucket, currentVersion);
                return std::move(item);
            }

            if (bucket.version.load(std::memory_order_acquire) == version) {
                this->clear_if_empty(bucketIdx, bucket, version);
            }
        }
    }

    bool empty() const noexcept {
        return !this->bitmask.load(std::memory_order_acquire);
    }

    static constexpr std::uint32_t maxPriority = std::numeric_limits<PriorityMask>::digits;

private:
    struct Bucket {
        explicit Bucket(std::size_t capacity) : queue(capacity) {}

        rigtorp::MPMCQueue<T> queue;
        std::atomic<std::uint64_t> version = 0;
    };

    static constexpr std::uint32_t normalize_priority(std::uint32_t priority) noexcept {
        return std::clamp(priority, static_cast<std::uint32_t>(0), maxPriority - 1);
    }

    static constexpr PriorityMask get_priority_mask(std::uint32_t priority) noexcept {
        return static_cast<PriorityMask>(1) << priority;
    }

    void clear_if_empty(std::uint32_t priority, Bucket& bucket, std::uint64_t observedVersion) noexcept {
        if (!bucket.queue.empty()) {
            return;
        }

        if (bucket.version.load(std::memory_order_acquire) != observedVersion) {
            return;
        }

        const auto priorityMask = get_priority_mask(priority);
        this->bitmask.fetch_and(~priorityMask, std::memory_order_acq_rel);

        // A producer can publish between empty() and fetch_and(); restore the
        // bit if the bucket became non-empty during that window.
        if (bucket.version.load(std::memory_order_acquire) != observedVersion ||
            !bucket.queue.empty()) {
            this->bitmask.fetch_or(priorityMask, std::memory_order_release);
        }
    }

    static std::array<Bucket, maxPriority> make_buckets(std::size_t capacity) {
        return make_buckets(capacity, std::make_index_sequence<maxPriority>{});
    }

    template<std::size_t... Indices>
    static std::array<Bucket, maxPriority> make_buckets(
        std::size_t capacity, std::index_sequence<Indices...>) {
        return {((void)Indices, Bucket(capacity))...};
    }

    std::atomic<PriorityMask> bitmask;
    std::array<Bucket, maxPriority> buckets;
};

}
