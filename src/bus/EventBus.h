#pragma once
#include <functional>
#include <string>
#include <variant>
#include <vector>

/**
 * EvPadPressed
 * ----------------------------------------------------------------
 * Сигнал от UX-слоя о нажатии/отпускании пэда.
 *
 * ► Источник: GUI/консоль/MIDI-адаптер → UI-тред.
 * ► Куда: EventBus.publishFromUI(...) → читается в аудио-цикле через drainUIEvents().
 * ► Назначение: запустить/остановить воспроизведение пэда, либо
 *   транслировать дальше в Sequencer (если включена live-запись).
 *
 * RT: безопасно — маленькое сообщение, без I/O и аллокаций в обработчике.
 * Типичный обработчик: SamplerPadManager / SequencerLiveRec.
 */
struct EvPadPressed { int pad; bool on; };

/**
 * EvNoteOn
 * ----------------------------------------------------------------
 * Нота «включить» с нормализованной скоростью (velocity в 0..1).
 *
 * ► Источник: GUI-клава, внешняя MIDI-клава (нормализовано), тесты.
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: триггер ноты в активном инструменте/треке (минуя пэды).
 *
 * RT: ок, без тяжёлых операций.
 * Типичный обработчик: текущий активный инструмент (Sampler/Synth).
 */
struct EvNoteOn { int note; float vel; };

/**
 * EvNoteOff
 * ----------------------------------------------------------------
 * Нота «выключить».
 *
 * ► Источник: GUI/MIDI.
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: завершить ноту/перевести голос в Release.
 *
 * RT: ок.
 * Типичный обработчик: тот же инструмент, что включал EvNoteOn.
 */
struct EvNoteOff { int note; };

/**
 * EvLoadSample
 * ----------------------------------------------------------------
 * Просьба назначить/загрузить WAV/AIFF на пэд.
 *
 * ► Источник: GUI (кнопка/drag&drop) или консоль.
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: инициировать асинхронную загрузку файла и
 *   назначить полученный буфер на pad.
 *
 * ВАЖНО (RT):
 *   - Обработчик НЕ должен читать файл в аудио-треде.
 *   - Нужно перекинуть задачу в фонового воркера (thread-pool),
 *     декодировать → поместить буфер в кеш → затем атомарно
 *     привязать к пэду (или отправить вторичное EvAssignSampleToPad).
 *
 * Типичный обработчик: SampleLibrary/Loader + PadAssignment.
 */
struct EvLoadSample { int pad; std::string path; };

/**
 * EvTransport
 * ----------------------------------------------------------------
 * Управление транспортом: play=true — старт, play=false — стоп.
 *
 * ► Источник: GUI/консоль/горячие клавиши.
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: сброс фаз секвенсора/таймера, запуск/останов.
 *
 * RT: ок — это «флажок»; обработчик должен делать быстрые переключения,
 *     без блокировок/аллокаций в аудио-колбэке.
 * Типичный обработчик: AudioEngine/Sequencer.
 */
struct EvTransport { bool play; };

/**
 * EvSeqSetBpm
 * ----------------------------------------------------------------
 * Установка темпа секвенсора, bpm > 0.
 *
 * ► Источник: GUI (крутилка BPM), консольная команда, MIDI (tap tempo конверсия).
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: обновить tempo в тикере секвенсора (samplesPerStep и т.п.).
 *
 * RT: ок — обновление атомика/структуры тайминга; пересчёт производить
 *     в безопасной секции (на границе аудио-тиков).
 * Типичный обработчик: Sequencer/Clock.
 */
struct EvSeqSetBpm { float bpm; };

/**
 * EvSeqSetSwing
 * ----------------------------------------------------------------
 * Свинг секвенсора (0..1) — смещение чётных долей.
 *
 * ► Источник: GUI/крутилка.
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: изменить микротайминг на уровне генерации событий Step.
 *
 * RT: ок — хранить параметр в атомике, применять в расчёте позиций шагов.
 * Типичный обработчик: Sequencer (scheduler шага).
 */
struct EvSeqSetSwing { float k01; }; // 0..1

/**
 * EvSeqSetPattern
 * ----------------------------------------------------------------
 * Переключение активного паттерна by index (0..N-1).
 *
 * ► Источник: GUI/кнопки банка, MIDI-программа.
 * ► Куда: EventBus UI→Engine.
 * ► Назначение: сменить текущий паттерн; по желанию — квантовать к границе такта.
 *
 * RT: ок — переключение указателей/индексов; при необходимости — квантовать
 *     в секвенсоре на ближайшую «музыкальную» границу.
 * Типичный обработчик: Sequencer/PatternBank.
 */
struct EvSeqSetPattern { int index; };

/**
 * Сводный тип события для шины.
 * Порядок значений не критичен, но старайтесь группировать по доменам (UI pads / MIDI notes / File I/O / Transport / Sequencer).
 *
 * ▼ Генеральное правило использования:
 *   - Любое РАЗОВОЕ действие, богатое данными → событие (EventBus).
 *   - Любой НЕПРЕРЫВНЫЙ параметр (ADSR, DRAG, SEND, макросы) → ParamStore (атомики),
 *     читается DSP-тредом и сглаживается на стороне DSP.
 */
using Event = std::variant<
        EvPadPressed,
        EvNoteOn,
        EvNoteOff,
        EvLoadSample,
        EvTransport,
        EvSeqSetBpm,
        EvSeqSetSwing,
        EvSeqSetPattern
>;

struct IEventBus {
    virtual ~IEventBus() = default;
    using Handler = std::function<void(const Event&)>;
    virtual void publish(const Event& e) = 0;          // UI -> Engine
    virtual void subscribe(Handler h) = 0;             // Engine -> UI updates
    virtual void commit() = 0;
    virtual void publishFromUI(const Event& e) = 0;
    virtual void drainUIEvents() = 0;
};