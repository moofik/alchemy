#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// подстрой путь инклуда под проект (например, "utils/SpscRing.h")
#include "utils/SpscRing.h"

// Вспомогательная структура для проверки move/копий
struct Payload {
    int v = 0;
    Payload() = default;
    explicit Payload(int x) : v(x) {}
    bool operator==(const Payload& other) const { return v == other.v; }
};

TEST_CASE("SpscRing: basic push/pop FIFO order") {
constexpr size_t N = 8;
SpscRing<Payload, N> q;

// очередь должна быть пустой — pop вернёт false
Payload out;
REQUIRE_FALSE(q.pop(out));

// заполняем неполностью
REQUIRE(q.push(Payload{1}));
REQUIRE(q.push(Payload{2}));
REQUIRE(q.push(Payload{3}));

// читаем FIFO
REQUIRE(q.pop(out));
CHECK(out.v == 1);
REQUIRE(q.pop(out));
CHECK(out.v == 2);
REQUIRE(q.pop(out));
CHECK(out.v == 3);

// снова должна быть пустой
REQUIRE_FALSE(q.pop(out));
}

TEST_CASE("SpscRing: capacity and wrap-around") {
constexpr size_t N = 4;
SpscRing<int, N> q;

// заполнить до упора
REQUIRE(q.push(10));
REQUIRE(q.push(20));
REQUIRE(q.push(30));
REQUIRE_FALSE(q.push(40));

int x;
REQUIRE(q.pop(x)); CHECK(x == 10);
REQUIRE(q.pop(x)); CHECK(x == 20);

// теперь есть место — wrap-around проверяется следующими push
REQUIRE(q.push(50));
REQUIRE(q.push(60));
// буфер снова полон
REQUIRE_FALSE(q.push(70));

REQUIRE(q.pop(x)); CHECK(x == 30);
REQUIRE(q.pop(x)); CHECK(x == 50);
REQUIRE(q.pop(x)); CHECK(x == 60);
REQUIRE_FALSE(q.pop(x));
}

TEST_CASE("SpscRing: single-producer single-consumer throughput (multithread)") {
constexpr size_t N = 1024;
constexpr int TOTAL = 100000;
SpscRing<int, N> q;

std::atomic<bool> start{false};
std::atomic<int> produced{0};
std::atomic<int> consumed{0};

std::thread producer([&]{
    // ждём общего старта, чтобы синхронизироваться
    while (!start.load(std::memory_order_acquire)) {}
    for (int i = 1; i <= TOTAL; ) {
        if (q.push(i)) {
            ++i;
            produced.fetch_add(1, std::memory_order_relaxed);
        } else {
            // буфер полон — краткая уступка
            std::this_thread::yield();
        }
    }
});

std::thread consumer([&]{
    int last = 0;
    // жуём пока не заберём все TOTAL
    while (consumed.load(std::memory_order_relaxed) < TOTAL) {
        int x;
        if (q.pop(x)) {
            // проверяем монотонность (FIFO)
            REQUIRE(x == last + 1);
            last = x;
            consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
            // буфер пуст — уступим квантик
            std::this_thread::yield();
        }
    }
});

start.store(true, std::memory_order_release);

producer.join();
consumer.join();

CHECK(produced.load() == TOTAL);
CHECK(consumed.load() == TOTAL);
}