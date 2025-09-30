#pragma once
#include <cstddef>
#include <atomic>

// ну это типа у нас очередь lock-free )))

template<class T, size_t N>                // шаблон: T — тип элементов, N — размер очереди (кол-во слотов)
struct SpscRing {
    static_assert((N & (N-1)) == 0,          // компиляционная проверка: N — степень двойки (1<<k)
                  "N must be power of two"); // нужно, чтобы быстрый модуль по маске работал корректно

    alignas(64) std::atomic<size_t> w{0};    // индекс записи (write), атомик; alignas(64) — чтобы лежал на своей кэш-линии
    alignas(64) std::atomic<size_t> r{0};    // индекс чтения (read), атомик; отделён от w, чтобы избежать false sharing
    alignas(64) T q[N];                      // сам буфер фиксированного размера на N элементов; тоже выровняли «на всякий»

    bool push(const T& v) noexcept {         // метод продюсера (UI-поток): попытаться записать элемент v
        size_t w0 = w.load(std::memory_order_relaxed);      // локально читаем текущий индекс записи (без барьеров)
        size_t w1 = (w0 + 1) & (N - 1);                     // считаем следующий индекс по кольцу: (w0+1) mod N через маску
        if (w1 == r.load(std::memory_order_acquire))        // если следующий write встретился с read → буфер полон
            return false;                                     // нет места — сообщаем об этом (не блокируемся)

        q[w0] = v;                                          // кладём элемент в текущую ячейку (w0)
        // важно: T желательно trivially copyable для RT (никаких аллокаций)

        w.store(w1, std::memory_order_release);             // публикуем новую позицию записи (release, чтобы запись в q увидел потребитель)
        return true;                                        // удачно записали
    }

    bool pop(T& v) noexcept {               // метод консюмера (аудио-поток): попытаться прочитать элемент в v
        size_t r0 = r.load(std::memory_order_relaxed);      // локально читаем текущий индекс чтения
        if (r0 == w.load(std::memory_order_acquire))        // если read догнал write → пусто
            return false;                                     // нечего читать — сразу выходим

        v = q[r0];                                          // читаем элемент из текущей ячейки (r0)
        r.store((r0 + 1) & (N - 1), std::memory_order_release); // сдвигаем read вперёд и публикуем (release)
        return true;                                        // успешно прочитали
    }
};