#pragma once
#include <memory>
#include <vector>
#include <string_view>
#include <span>
#include "devices/IFX.h"
#include "devices/IFXFactory.h"


using TrackId = int;
/**
 * \brief Пер-трековый аудиобуфер на текущий аудиоблок.
 *
 * Это не владеющий дескриптор на стерео-память, куда:
 *   1) накапливается DRY сигнал источников (через ITrackSink::addDry),
 *   2) затем in-place применяется цепочка insert-FX трека (Track::processChain).
 *
 * Указатели валидны только в рамках одного аудиоблока — их на каждый блок
 * привязывает TrackManager::bindBuses(...).
 */
struct TrackBus {
    float* L = nullptr;   ///< указатель на левый канал (frames float'ов)
    float* R = nullptr;   ///< указатель на правый канал (frames float'ов)
    int    frames = 0;    ///< размер текущего аудиоблока в сэмплах
};

/**
 * \brief Трек: контейнер цепочки insert-FX и "ручек" над пер-трековым буфером.
 *
 * Track сам не владеет аудиопамятью. На каждый блок ему привязывают внешние
 * буферы (TrackBus), и он прогоняет по ним свою FX-цепочку in-place.
 */
class Track {
public:
    explicit Track(int id);

    /**
     * \brief Готовит трек к работе при смене SR/размера блока.
     *
     * Вызывается из не-RT кода (при инициализации и при изменении параметров
     * аудиодрайвера). Передаёт новые значения всем FX в цепочке.
     */
    void prepare(int sampleRate, int blockSize);

    /**
     * \brief Сбрасывает внутреннее состояние FX (не-RT).
     *
     * Полезно при стопе/транспорте. Аудиопамяти не касается.
     */
    void reset();

    /**
     * \brief Привязывает внешние буферы (L/R) на текущий аудиоблок.
     *
     * Буферы — это те самые per-track «накопители», в которые источники
     * уже сложили DRY (через ITrackSink). Track будет применять свою цепочку FX
     * по этим указателям in-place.
     *
     * \note Валидность указателей ограничена одним блоком. На следующий блок
     *       TrackManager снова вызовет bindBus с новыми/теми же адресами.
     */
    void bindBus(TrackBus bus);

    /**
     * \brief RT-метод: прогоняет цепочку insert-FX по привязанным буферам.
     *
     * Никаких аллокаций/локов/исключений; эффекты должны работать in-place.
     * Если буферы не привязаны (nullptr/0 frames), метод ничего не делает.
     */
    void processChain();

    // ====== редактирование цепочки (НЕ RT) ======

    /// Добавить FX в конец цепочки. Вызовет fx->prepare(sr_, bs_).
    bool addEffect(std::unique_ptr<IFx> fx);

    /// Вставить FX по индексу [0..size]. Вызовет fx->prepare(sr_, bs_).
    bool insertEffect(size_t index, std::unique_ptr<IFx> fx);

    /// Удалить FX по индексу. Возвращает false если индекс невалиден.
    bool removeEffect(size_t index);

    /// Переместить FX внутри цепочки. Возвращает false если индексы невалидны.
    bool moveEffect(size_t from, size_t to);

    // ====== параметры FX (НЕ RT) ======

    /// Установить параметр FX по индексу/идентификатору.
    bool  setFxParam(size_t fxIndex, std::string_view paramId, float value);

    /// Получить значение параметра FX по индексу/идентификатору.
    float getFxParam(size_t fxIndex, std::string_view paramId) const;

    /// Вернуть описания параметров FX (для UI). Пустой span при ошибке.
    std::span<const FxParamDesc> fxParams(size_t fxIndex) const;

    // ====== introspection ======

    size_t chainSize() const;
    int    id()        const;

private:
    int id_ = -1;
    int sr_ = 48000;
    int bs_ = 64;

    TrackBus bus_{};  // актуальные буферы на текущий блок (не владеем)
    std::vector<std::unique_ptr<IFx>> chain_;
};
