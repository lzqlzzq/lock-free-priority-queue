#include "pq/PriorityQueue.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

namespace {

void print_basic_usage() {
    lock_free::PriorityQueue<int> queue(8);

    std::cout << "Basic priority queue example\n";
    std::cout << "Push: value=30 priority=3\n";
    queue.try_push(3, 30);
    std::cout << "Push: value=10 priority=1\n";
    queue.try_push(1, 10);
    std::cout << "Push: value=20 priority=2\n";
    queue.try_push(2, 20);
    std::cout << "Push: value=0 priority=0\n";
    queue.try_push(0, 0);

    std::cout << "Pop order, lower priority number first:";
    while (auto value = queue.try_pop()) {
        std::cout << ' ' << *value;
    }
    std::cout << "\n\n";
}

void print_concurrent_usage() {
    constexpr int producerCount = 4;
    constexpr int consumerCount = 4;
    constexpr int itemsPerProducer = 1024000;
    constexpr int totalItems = producerCount * itemsPerProducer;

    lock_free::PriorityQueue<int> queue(1024);
    std::atomic<int> poppedCount{0};
    std::atomic<long long> checksum{0};

    std::cout << "Concurrent example\n";
    std::cout << "Start " << producerCount << " producers and "
              << consumerCount << " consumers\n";

    const auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> consumers;
    consumers.reserve(consumerCount);
    for (int i = 0; i < consumerCount; ++i) {
        consumers.emplace_back([&] {
            while (poppedCount.load(std::memory_order_acquire) < totalItems) {
                if (std::optional<int> value = queue.try_pop()) {
                    checksum.fetch_add(*value, std::memory_order_relaxed);
                    poppedCount.fetch_add(1, std::memory_order_release);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::vector<std::thread> producers;
    producers.reserve(producerCount);
    for (int producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&, producer] {
            for (int item = 0; item < itemsPerProducer; ++item) {
                const int value = producer * itemsPerProducer + item;
                const auto priority = static_cast<std::uint32_t>(
                    value % lock_free::PriorityQueue<int>::maxPriority);

                while (!queue.try_push(priority, value)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    for (auto& consumer : consumers) {
        consumer.join();
    }

    const auto elapsed = std::chrono::high_resolution_clock::now() - start;
    const auto elapsedUs =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::cout << "Popped items: " << poppedCount.load() << '\n';
    std::cout << "Checksum: " << checksum.load() << "\n\n";
    std::cout << "Elapsed time: " << elapsedUs << " us\n\n";
}

}  // namespace

int main() {
    print_basic_usage();
    print_concurrent_usage();
    return 0;
}
