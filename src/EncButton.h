/*
    Ультра лёгкая и быстрая библиотека для энкодера, энкодера с кнопкой или просто кнопки
    Документация:
    GitHub: https://github.com/GyverLibs/EncButton
    Возможности:
    - Максимально быстрое чтение пинов для AVR (ATmega328/ATmega168, ATtiny85/ATtiny13)
    - Оптимизированный вес
    - Быстрые и лёгкие алгоритмы кнопки и энкодера
    - Энкодер: поворот, нажатый поворот, быстрый поворот, счётчик
    - Кнопка: антидребезг, клик, несколько кликов, счётчик кликов, удержание, режим step
    - Подключение - только HIGH PULL!
    - Опциональный режим callback (+22б SRAM на каждый экземпляр)

    AlexGyver, alex@alexgyver.ru
    https://alexgyver.ru/
    MIT License
    Опционально используется алгоритм из библиотеки // https://github.com/mathertel/RotaryEncoder

    Версии:
    v1.1 - пуллап отдельныи методом
    v1.2 - можно передать конструктору параметр INPUT_PULLUP / INPUT(умолч)
    v1.3 - виртуальное зажатие кнопки энкодера вынесено в отдельную функцию + мелкие улучшения
    v1.4 - обработка нажатия и отпускания кнопки
    v1.5 - добавлен виртуальный режим
    v1.6 - оптимизация работы в прерывании
    v1.6.1 - PULLUP по умолчанию
    v1.7 - большая оптимизация памяти, переделан FastIO
    v1.8 - индивидуальная настройка таймаута удержания кнопки (была общая на всех)
    v1.8.1 - убран FastIO
    v1.9 - добавлена отдельная отработка нажатого поворота и запрос направления
    v1.10 - улучшил обработку released, облегчил вес в режиме callback и исправил баги
    v1.11 - ещё больше всякой оптимизации + настройка уровня кнопки
    v1.11.1 - совместимость Digispark
    v1.12 - добавил более точный алгоритм энкодера EB_BETTER_ENC
    v1.13 - добавлен экспериментальный EncButton2
    v1.14 - добавлена releaseStep(). Отпускание кнопки внесено в дебаунс
    v1.15 - добавлен setPins() для EncButton2
    v1.16 - добавлен режим EB_HALFSTEP_ENC для полушаговых энкодеров
    v1.17 - добавлен step с предварительными кликами
    v1.18 - не считаем клики после активации step. held() и hold() тоже могут принимать предварительные клики. Переделан и улучшен дебаунс
    v1.18.1 - исправлена ошибка в releaseStep() (не возвращала результат)
    v1.18.2 - fix compiler warnings
*/

#ifndef _EncButton_h
#define _EncButton_h

// =========== НЕ ТРОГАЙ ============
#include <Arduino.h>
#include <stdint.h>

// ========= НАСТРОЙКИ (можно передефайнить из скетча) ==========
#ifndef EB_FAST // таймаут быстрого поворота
#define EB_FAST 30
#endif
#ifndef EB_DEB // дебаунс кнопки
#define EB_DEB 50
#endif
#ifndef EB_HOLD // таймаут удержания кнопки
#define EB_HOLD 1000
#endif
#ifndef EB_STEP // период срабатывания степ
#define EB_STEP 500
#endif
#ifndef EB_CLICK // таймаут накликивания
#define EB_CLICK 400
#endif

//#define EB_BETTER_ENC // точный алгоритм отработки энкодера (можно задефайнить в скетче)

#ifndef nullptr
#define nullptr NULL
#endif

enum EbCallback : uint8_t {
    TURN_HANDLER,    // 0
    LEFT_HANDLER,    // 1
    RIGHT_HANDLER,   // 2
    LEFT_H_HANDLER,  // 3
    RIGHT_H_HANDLER, // 4
    CLICK_HANDLER,   // 5
    HOLDED_HANDLER,  // 6
    STEP_HANDLER,    // 7
    PRESS_HANDLER,   // 8
    CLICKS_HANDLER,  // 9
    RELEASE_HANDLER, // 10
    HOLD_HANDLER,    // 11
    TURN_H_HANDLER,  // 12
    // CLICKS_AMOUNT 13
};

namespace Internals {
    template <class T, T v>
    struct integral_constant {
        static constexpr T value = v;
        using value_type = T;
        using type = integral_constant; // using injected-class-name
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    using true_type = integral_constant<bool, true>;
    using false_type = integral_constant<bool, false>;

    template <uint8_t V>
    using u8_constant = integral_constant<uint8_t, V>;

    template <bool B, class T, class F>
    struct conditional { typedef T type; };

    template <class T, class F>
    struct conditional<false, T, F> { typedef F type; };

    template <bool B, class T, class F>
    using conditional_t = typename conditional<B, T, F>::type;

    template <bool B, class T = void>
    struct enable_if { };

    template <class T>
    struct enable_if<true, T> { typedef T type; };

    template <bool B, class T = void>
    using enable_if_t = typename enable_if<B, T>::type;

    template <typename Derived, typename Mode, uint8_t...>
    struct Tick;

    template <typename Derived>
    struct CallbacksImpl;

    /////////////////////////////////////////////////////////////////////////////////////
    template <typename Derived, bool>
    struct EncoderImpl;
    template <typename Derived>
    struct ButtonImpl;

    enum Flags : uint8_t {
        EncTurn = 0,          // 0 - enc turn
        EncFast,              // 1 - enc fast
        EncWasTurn,           // 2 - enc был поворот
        BtnFlag,              // 3 - флаг кнопки
        Hold,                 // 4 - hold
        ClicksFlag,           // 5 - clicks flag
        ClicksGet,            // 6 - clicks get
        ClicksGetNum,         // 7 - clicks get num
        EncButtonHold,        // 8 - enc button hold
        EncTurnHolded,        // 9 - enc turn holded
        Released,             // 10 - btn released
        Btnlevel,             // 11 - btn level
        BtnReleasedAfterStep, // 12 - btn released after step
        StepFlag,             // 13 - step flag
        DebFlag,              // 14 - deb flag
    };

    enum State : uint8_t {
        // EBState
        Idle = 0, // 0 - idle
        Left,     // 1 - left
        Right,    // 2 - right
        LeftH,    // 3 - leftH
        RightH,   // 4 - rightH
        Click,    // 5 - click
        Held,     // 6 - held
        Step,     // 7 - step
        Press,    // 8 - press
    };

#if __cplusplus < 201700
    template <typename... Fs>
    constexpr uint16_t mask(Flags f, Fs... fs) { return (1 << f) | mask(fs...); }
    constexpr uint16_t mask(Flags f) { return 1 << f; }
#define NODISCARD [[nodiscard]]
#else
    template <typename... Fs>
    constexpr uint16_t mask(Flags f, Fs... fs) { return (1 << f) | ((1 << fs) | ...); }

#define NODISCARD [[nodiscard]]
#endif

    enum : uint8_t {
        CompileTime = 0,
        Button = 1,
        Encoder = 2,
        Virtual = 4,
        UseCallback = 128,
    };

    using EB_PERMANENT = u8_constant<CompileTime>;
    using EB_CALLBACK = u8_constant<UseCallback>;

    using EB_BTN = u8_constant<Button>;
    using EB_ENC = u8_constant<Encoder>;
    using EB_ENCBTN = u8_constant<Button + Encoder>;

    using EB_VIRT_BTN = u8_constant<Button + Virtual>;
    using EB_VIRT_ENC = u8_constant<Encoder + Virtual>;
    using EB_VIRT_ENCBTN = u8_constant<Button + Encoder + Virtual>;

    NODISCARD bool fastRead(const uint8_t pin) {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
        if (pin < 8)
            return bitRead(PIND, pin);
        else if (pin < 14)
            return bitRead(PINB, pin - 8);
        else if (pin < 20)
            return bitRead(PINC, pin - 14);
#elif defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny13__)
        return bitRead(PINB, pin);
#else
        return digitalRead(pin);
#endif
    }

}; // namespace Internals

/////////////////////////////////////////////////////////////////////////////////////

// ===================================== CLASS =====================================
template <typename Mode, typename UseCallback = Internals::u8_constant<0>, uint8_t... Pins>
class EncButton : public Internals::CallbacksImpl<Internals::conditional_t<UseCallback::value >= 128, EncButton<Mode, UseCallback, Pins...>, void>>,
                  public Internals::Tick<EncButton<Mode, UseCallback, Pins...>, Mode, Pins...> //
{
    using mode = Mode;

    using Tick = Internals::Tick<EncButton, Mode, Pins...>;
    using Tick::Tick;
    friend Tick;

    template <typename Derived>
    friend struct Internals::CallbacksImpl;

    template <typename Derived, bool>
    friend struct Internals::EncoderImpl;

    template <typename Derived>
    friend struct Internals::ButtonImpl;

public:
    using type = EncButton;

    // установить таймаут удержания кнопки для isHold(), мс (до 30 000)
    void setHoldTimeout(int tout) {
        _holdT = tout >> 7;
    }

    // виртуально зажать кнопку энкодера
    void holdEncButton(bool state) {
        state ? setFlag(Internals::EncButtonHold) : clrFlag(Internals::EncButtonHold);
    }

    // уровень кнопки: LOW - кнопка подключает GND (умолч.), HIGH - кнопка подключает VCC
    void setButtonLevel(bool level) {
        level ? clrFlag(Internals::Btnlevel) : setFlag(Internals::Btnlevel);
    }

    // ===================================== STATUS =====================================
    uint8_t getState() { return EBState; } // получить статус
    void resetState() { EBState = {}; }    // сбросить статус

    // ======================================= ENC =======================================
    NODISCARD bool left() { return checkState(Internals::Left); }     // поворот влево
    NODISCARD bool right() { return checkState(Internals::Right); }   // поворот вправо
    NODISCARD bool leftH() { return checkState(Internals::LeftH); }   // поворот влево нажатый
    NODISCARD bool rightH() { return checkState(Internals::RightH); } // поворот вправо нажатый

    NODISCARD bool fast() { return getFlag(Internals::EncFast); }            // быстрый поворот
    NODISCARD bool turn() { return checkFlag<Internals::EncTurn>(); }        // энкодер повёрнут
    NODISCARD bool turnH() { return checkFlag<Internals::EncTurnHolded>(); } // энкодер повёрнут нажато

    NODISCARD int8_t getDir() { return _dir; } // направление последнего поворота, 1 или -1

    // ======================================= BTN =======================================
    NODISCARD bool state() { return _btnState; }                          // статус кнопки
    NODISCARD bool press() { return checkState(Internals::Press); }       // кнопка нажата
    NODISCARD bool release() { return checkFlag<Internals::Released>(); } // кнопка отпущена
    NODISCARD bool click() { return checkState(Internals::Click); }       // клик по кнопке

    NODISCARD bool held() { return checkState(Internals::Held); }                         // кнопка удержана
    NODISCARD bool hold() { return getFlag(Internals::Hold); }                            // кнопка удерживается
    NODISCARD bool step() { return checkState(Internals::Step); }                         // режим импульсного удержания
    NODISCARD bool releaseStep() { return checkFlag<Internals::BtnReleasedAfterStep>(); } // кнопка отпущена после импульсного удержания

    // =================================== DEPRECATED ====================================
    [[deprecated("Use step()")]] bool isStep() { return step(); }
    [[deprecated("Use hold()")]] bool isHold() { return hold(); }
    [[deprecated("Use held()")]] bool isHolded() { return held(); }
    [[deprecated("Use held()")]] bool isHeld() { return held(); }
    [[deprecated("Use click()")]] bool isClick() { return click(); }
    [[deprecated("Use release()")]] bool isRelease() { return release(); }
    [[deprecated("Use press()")]] bool isPress() { return press(); }
    [[deprecated("Use turnH()")]] bool isTurnH() { return turnH(); }
    [[deprecated("Use turn()")]] bool isTurn() { return turn(); }
    [[deprecated("Use fast()")]] bool isFast() { return fast(); }
    [[deprecated("Use leftH()")]] bool isLeftH() { return leftH(); }
    [[deprecated("Use rightH()")]] bool isRightH() { return rightH(); }
    [[deprecated("Use left()")]] bool isLeft() { return left(); }
    [[deprecated("Use right()")]] bool isRight() { return right(); }

    // ===================================== PRIVATE =====================================
private:
    // ===================================== MISC =====================================

    // флаг макро
    void setFlag(Internals::Flags x) { flags |= +(1 << x); }
    void clrFlag(Internals::Flags x) { flags &= ~(1 << x); }
    NODISCARD bool getFlag(Internals::Flags x) { return flags & (1 << x); }

    template <Internals::Flags val>
    NODISCARD bool checkFlag() { return getFlag(val) ? clrFlag(val), true : false; }

    NODISCARD bool checkState(Internals::State val) { return (EBState == val) ? EBState = 0, true : false; }

    uint8_t EBState : 4;
    uint8_t _prev : 2;
    bool _isrFlag : 1;
    bool _btnState : 1;
    bool _encRST : 1;
    uint16_t flags { mask(Internals::Btnlevel) };

    uint32_t _debTimer {};
    uint8_t _holdT { EB_HOLD >> 7 };
    int8_t _dir {};
};

template <typename Derived>
struct GetDerived {
protected:
    Derived& d() { return *static_cast<Derived*>(this); }
};
namespace Internals {

#define D static_cast<Derived&>(*this)

    /////////////////////////////////////////////////////////////////////////////////////
    /// \brief The EncoderImpl struct
    template <typename Derived, bool UseButton>
    struct EncoderImpl {
        int16_t counter = 0; // счётчик энкодера
    protected:
        // ===================================== POOL ENC =====================================
        void poolEnc(const uint8_t& pinA, const uint8_t& pinB) {
            const uint8_t state = fastRead(pinA) | (fastRead(pinB) << 1);
#ifdef EB_BETTER_ENC
            static constexpr int8_t _EB_DIR[] = {
                0, -1, 1, 0,
                1, 0, 0, -1,
                -1, 0, 0, 1,
                0, 1, -1, 0
            };

            if (D._prev != state) {
                _ecount += _EB_DIR[state | (D._prev << 2)]; // сдвиг внутреннего счётчика
                D._prev = state;
#ifdef EB_HALFSTEP_ENC // полушаговый энкодер
                // спасибо https://github.com/GyverLibs/EncButton/issues/10#issue-1092009489
                if ((state == 0x3 || state == 0x0) && _ecount != 0) {
#else // полношаговый
                if (state == 0x3 && _ecount != 0) { // защёлкнули позицию
#endif
                    D.EBState = (_ecount < 0) ? 1 : 2;
                    _ecount = 0;
                    if (UseButton) { // энкодер с кнопкой
                        if (!D.getFlag(Hold) && (D._btnState || D.getFlag(EncButtonHold)))
                            D.EBState += 2; // если кнопка не "удерживается"
                    }
                    D._dir = (D.EBState & 1) ? -1 : 1; // направление
                    D.counter += D._dir;               // счётчик
                    if (D.EBState <= 2)
                        D.setFlag(EncTurn); // флаг поворота для юзера
                    else if (D.EBState <= 4)
                        D.setFlag(EncTurnHolded); // флаг нажатого поворота для юзера
                    if (millis() - D._debTimer < EB_FAST)
                        D.setFlag(EncFast); // быстрый поворот
                    else
                        D.clrFlag(EncFast); // обычный поворот
                    D._debTimer = millis();
                }
            }
#else
            if (D._encRST && state == 0b11) {                                 // ресет и энк защёлкнул позицию
                if (UseButton) {                                              // энкодер с кнопкой
                    if ((D._prev == 1 || D._prev == 2) && !D.getFlag(Hold)) { // если кнопка не "удерживается" и энкодер в позиции 1 или 2
                        D.EBState = D._prev;
                        if (D._btnState || D.getFlag(EncButtonHold))
                            D.EBState += 2;
                    }
                } else { // просто энкодер
                    if (D._prev == 1 || D._prev == 2)
                        D.EBState = D._prev;
                }

                if (D.EBState > 0) {                   // был поворот
                    D._dir = (D.EBState & 1) ? -1 : 1; // направление
                    D.counter += D._dir;               // счётчик
                    if (D.EBState <= 2)
                        D.setFlag(EncTurn); // флаг поворота для юзера
                    else if (D.EBState <= 4)
                        D.setFlag(EncTurnHolded); // флаг нажатого поворота для юзера
                    if (millis() - D._debTimer < EB_FAST)
                        D.setFlag(EncFast); // быстрый поворот
                    else
                        D.clrFlag(EncFast); // обычный поворот
                }

                D._encRST = 0;
                D._debTimer = millis();
            }
            if (state == 0b00)
                D._encRST = 1;
            D._prev = state;
#endif
        }
#ifdef EB_BETTER_ENC
        int8_t _ecount {};
#endif
    };

    /////////////////////////////////////////////////////////////////////////////////////
    /// \brief The ButtonImpl struct
    template <typename Derived>
    struct ButtonImpl {
        uint8_t clicks = 0;                                                         // счётчик кликов
        bool held(uint8_t clk) { return (clicks == clk) ? D.checkState(Held) : 0; } // кнопка удержана с предварительным накликиванием
        bool hold(uint8_t clk) { return (clicks == clk) ? D.getFlag(Hold) : 0; }    // кнопка удерживается с предварительным накликиванием
        bool step(uint8_t clk) { return (clicks == clk) ? D.checkState(Step) : 0; } // режим импульсного удержания с предварительным накликиванием

        bool releaseStep(uint8_t clk) { return (clicks == clk) ? D.template checkFlag<BtnReleasedAfterStep>() : 0; } // кнопка отпущена после импульсного удержания с предварительным накликиванием

        bool hasClicks(uint8_t num) { return (clicks == num && D.template checkFlag<ClicksGetNum>()) ? 1 : 0; } // имеются клики
        uint8_t hasClicks() { return D.template checkFlag<ClicksGet>() ? clicks : 0; }                          // имеются клики

    protected:
        // ===================================== POOL BTN =====================================
        void poolBtn(const uint8_t& pin) {
            D._btnState = fastRead(pin) ^ D.getFlag(Btnlevel);
            const uint32_t thisMls = millis();
            const uint32_t debounce = thisMls - D._debTimer;
            if (D._btnState) {                     // кнопка нажата
                if (!D.getFlag(BtnFlag)) {         // и не была нажата ранее
                    if (D.getFlag(DebFlag)) {      // ждём дебаунс
                        if (debounce > EB_DEB) {   // прошел дебаунс
                            D.setFlag(BtnFlag);    // флаг кнопка была нажата
                            D.EBState = 8;         // кнопка нажата
                            D._debTimer = thisMls; // сброс таймаутов
                        }
                    } else { // первое нажатие
                        D.EBState = 0;
                        D.setFlag(DebFlag);                                 // запомнили что хотим нажать
                        if (debounce > EB_CLICK || D.getFlag(ClicksFlag)) { // кнопка нажата после EB_CLICK
                            D.clicks = 0;                                   // сбросить счётчик и флаг кликов
                            D.flags &= ~mask(
                                ClicksFlag, ClicksGet, ClicksGetNum, BtnReleasedAfterStep, StepFlag); // clear 5 6 7 12 13 (клики)
                        }
                        D._debTimer = thisMls;
                    }
                } else {                                            // кнопка уже была нажата
                    if (!D.getFlag(Hold)) {                         // и удержание ещё не зафиксировано
                        if (debounce < (uint32_t)(D._holdT << 7)) { // прошло меньше удержания
                            if (D.EBState != 0 && D.EBState != 8)
                                D.setFlag(EncWasTurn);             // но энкодер повёрнут! Запомнили
                        } else {                                   // прошло больше времени удержания
                            if (!D.getFlag(EncWasTurn)) {          // и энкодер не повёрнут
                                D.EBState = 6;                     // значит это удержание (сигнал)
                                D.flags |= mask(Hold, ClicksFlag); // set 4 5 запомнили что удерживается и отключаем сигнал о кликах
                                D._debTimer = thisMls;             // сброс таймаута
                            }
                        }
                    } else {                       // удержание зафиксировано
                        if (debounce > EB_STEP) {  // таймер степа
                            D.EBState = 7;         // сигналим
                            D.setFlag(StepFlag);   // зафиксирован режим step
                            D._debTimer = thisMls; // сброс таймаута
                        }
                    }
                }
            } else {                      // кнопка не нажата
                if (D.getFlag(BtnFlag)) { // но была нажата
                    if (debounce > EB_DEB) {
                        if (!D.getFlag(Hold) && !D.getFlag(EncWasTurn)) { // энкодер не трогали и не удерживали - это клик
                            D.EBState = 5;                                // click
                            D.clicks++;
                        }
                        D.flags &= ~mask(EncWasTurn, BtnFlag, Hold); // clear
                        D._debTimer = thisMls;                       // сброс таймаута
                        D.setFlag(Released);                         // кнопка отпущена
                        if (D.template checkFlag<StepFlag>())
                            D.setFlag(BtnReleasedAfterStep); // кнопка отпущена после step
                    }
                } else if (D.clicks > 0 && debounce > EB_CLICK && !D.getFlag(ClicksFlag))
                    D.flags |= mask(ClicksFlag, ClicksGet, ClicksGetNum); // set (клики)
                D.clrFlag(DebFlag);                                       // сброс ожидания нажатия
            }
        }
    };

    /////////////////////////////////////////////////////////////////////////////////////
    /// \brief Compiletime Pin defined Tick structures
    template <typename Derived, typename Mode, uint8_t PinEncA, uint8_t PinEncB, uint8_t PinBtn>
    struct Tick<Derived, Mode, PinEncA, PinEncB, PinBtn> : ButtonImpl<Derived>, EncoderImpl<Derived, true> {

        // можно указать режим работы пина
        Tick(const uint8_t mode = INPUT_PULLUP) {
            if (mode == INPUT_PULLUP)
                pullUp();
        }

        // подтянуть пины внутренней подтяжкой
        void pullUp() { pinMode(PinEncA, INPUT_PULLUP), pinMode(PinEncB, INPUT_PULLUP), pinMode(PinBtn, INPUT_PULLUP); }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие
        uint8_t tick() {
            tickISR();
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR() {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolEnc(PinEncA, PinEncB);
                D.poolBtn(PinBtn);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
    };

    template <typename Derived, typename Mode, uint8_t PinEncA, uint8_t PinEncB>
    struct Tick<Derived, Mode, PinEncA, PinEncB> : EncoderImpl<Derived, false> {
        // можно указать режим работы пина
        Tick(const uint8_t mode = INPUT_PULLUP) {
            if (mode == INPUT_PULLUP)
                pullUp();
        }

        // подтянуть пины внутренней подтяжкой
        void pullUp() { pinMode(PinEncA, INPUT_PULLUP), pinMode(PinEncB, INPUT_PULLUP); }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие
        uint8_t tick() {

            tickISR();
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR() {

            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolEnc(PinEncA, PinEncB);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
    };

    template <typename Derived, typename Mode, uint8_t PinBtn>
    struct Tick<Derived, Mode, PinBtn> : ButtonImpl<Derived> {
        static constexpr auto pinBtn = PinBtn;

        // можно указать режим работы пина
        Tick(const uint8_t mode = INPUT_PULLUP) {
            if (mode == INPUT_PULLUP)
                pullUp();
        }

        // подтянуть пины внутренней подтяжкой
        void pullUp() { pinMode(PinBtn, INPUT_PULLUP); }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие
        uint8_t tick() {
            tickISR();
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR() {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolBtn(PinBtn);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
    };
    /////////////////////////////////////////////////////////////////////////////////////
    /// \brief Runtime Pin defined Tick structures
    template <typename Derived>
    struct Tick<Derived, EB_ENCBTN> : ButtonImpl<Derived>, EncoderImpl<Derived, true> {

        Tick(uint8_t pinEncA, uint8_t pinEncB, uint8_t pinBtn, uint8_t mode = INPUT) {

            setPins(mode, pinEncA, pinEncB, pinBtn);
        }

        // установить пины
        void setPins(uint8_t pinEncA, uint8_t pinEncB, uint8_t pinBtn, uint8_t mode = INPUT) {
            pinMode(pinEncA, mode);
            pinMode(pinEncB, mode);
            pinMode(pinBtn, mode);
            _pins[0] = pinEncA;
            _pins[1] = pinEncB;
            _pins[2] = pinBtn;
        }

        // подтянуть пины внутренней подтяжкой
        void pullUp() { pinMode(_pins[0], INPUT_PULLUP), pinMode(_pins[1], INPUT_PULLUP), pinMode(_pins[2], INPUT_PULLUP); }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие
        uint8_t tick() {

            tickISR();
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR() {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolEnc(_pins[0], _pins[1]);
                D.poolBtn(_pins[2]);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
        uint8_t _pins[Button + Encoder];
    };

    template <typename Derived>
    struct Tick<Derived, EB_ENC> : EncoderImpl<Derived, false> {
        uint8_t pinEncA {};
        uint8_t pinEncB {};
        uint8_t pinBtn {};

        Tick(uint8_t pinEncA, uint8_t pinEncB, uint8_t mode = INPUT) {

            setPins(mode, pinEncA, pinEncB);
        }

        // установить пины
        void setPins(uint8_t pinEncA, uint8_t pinEncB, uint8_t mode = INPUT) {
            pinMode(pinEncA, mode);
            pinMode(pinEncB, mode);
            _pins[0] = pinEncA;
            _pins[1] = pinEncB;
        }

        // подтянуть пины внутренней подтяжкой
        void pullUp() { pinMode(_pins[0], INPUT_PULLUP), pinMode(_pins[1], INPUT_PULLUP); }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие

        uint8_t tick() {

            tickISR();
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR() {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolEnc(_pins[0], _pins[1]);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
        uint8_t _pins[Encoder];
    };

    template <typename Derived>
    struct Tick<Derived, EB_BTN> : ButtonImpl<Derived> {
        uint8_t pinEncA {};
        uint8_t pinEncB {};
        uint8_t pinBtn {};

        Tick(uint8_t pinBtn, uint8_t mode = INPUT) {

            setPins(mode, pinBtn);
        }

        // установить пины
        void setPins(uint8_t pinBtn, uint8_t mode = INPUT) {
            pinMode(pinBtn, mode);
            _pin = pinBtn;
        }

        // подтянуть пины внутренней подтяжкой
        void pullUp() { pinMode(_pin, INPUT_PULLUP); }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие

        uint8_t tick() {

            tickISR();
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR() {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolBtn(_pin);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
        uint8_t _pin;
    };

    /////////////////////////////////////////////////////////////////////////////////////
    /// \brief Virtual Pin defined Tick structures
    template <typename Derived>
    struct Tick<Derived, EB_VIRT_ENCBTN> : ButtonImpl<Derived>, EncoderImpl<Derived, true> {

        Tick() { }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие
        uint8_t tick(uint8_t pinEncA, uint8_t pinEncB, uint8_t pinBtn) {
            tickISR(pinEncA, pinEncB, pinBtn);
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR(uint8_t pinEncA, uint8_t pinEncB, uint8_t pinBtn) {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolEnc(pinEncA, pinEncB);
                D.poolBtn(pinBtn);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
    };

    template <typename Derived>
    struct Tick<Derived, EB_VIRT_ENC> : EncoderImpl<Derived, false> {
        Tick() { }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие

        uint8_t tick(uint8_t pinEncA, uint8_t pinEncB) {
            tickISR(pinEncA, pinEncB);
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR(uint8_t pinEncA, uint8_t pinEncB) {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolEnc(pinEncA, pinEncB);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
    };

    template <typename Derived>
    struct Tick<Derived, EB_VIRT_BTN> : ButtonImpl<Derived> {
        Tick() { }

        // ===================================== TICK =====================================
        // тикер, вызывать как можно чаще
        // вернёт отличное от нуля значение, если произошло какое то событие

        uint8_t tick(uint8_t pinBtn) {
            tickISR(pinBtn);
            D.checkCallback();
            return D.EBState;
        }

        // тикер специально для прерывания, не проверяет коллбэки
        uint8_t tickISR(uint8_t pinBtn) {
            if (!D._isrFlag) {
                D._isrFlag = 1;
                D.poolBtn(pinBtn);
            }
            D._isrFlag = 0;
            return D.EBState;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }
    };

    /////////////////////////////////////////////////////////////////////////////////////
    /// \brief The Callbacks struct
    ///
    template <class Derived>
    struct CallbacksImpl {
        using Callback = void (*)();
        // подключить обработчик
        void attach(EbCallback type, Callback handler) { _callbacks[type] = handler; }

        // отключить обработчик
        void detach(EbCallback type) { _callbacks[type] = nullptr; }

        // подключить обработчик на количество кликов (может быть только один!)
        void attachClicks(uint8_t amount, Callback handler) { _amount = amount, _callbacks[13] = handler; }

        // отключить обработчик на количество кликов
        void detachClicks() { _callbacks[13] = nullptr; }

        // ===================================== CALLBACK =====================================
        // проверить callback, чтобы не дёргать в прерывании
        inline void checkCallback() {
            if (D.turn())
                D.exec(TURN_HANDLER);
            if (D.turnH())
                D.exec(TURN_H_HANDLER);
            if (D.EBState > 0 && D.EBState <= 8)
                D.exec(EbCallback(D.EBState));
            if (D.release())
                D.exec(RELEASE_HANDLER);
            if (D.hold())
                D.exec(HOLD_HANDLER);
            if (D.template checkFlag<ClicksGet>()) {
                D.exec(CLICKS_HANDLER);
                if (D.clicks == D._amount)
                    D.exec(EbCallback(13)); // CLICKS_AMOUNT
            }
            D.EBState = 0;
        }

    private:
        Derived& d() { return *static_cast<Derived*>(this); }

        void exec(EbCallback num) {
            if (_callbacks[num])
                _callbacks[num]();
        }

        uint8_t _amount = 0;
        Callback _callbacks[14] {};
    };

    template <>
    struct CallbacksImpl<void> {
        inline void checkCallback() { }
    };

#undef D

} // namespace Internals

using Internals::EB_CALLBACK;
using Internals::EB_PERMANENT;

using Internals::EB_BTN;
using Internals::EB_ENC;
using Internals::EB_ENCBTN;

using Internals::EB_VIRT_BTN;
using Internals::EB_VIRT_ENC;
using Internals::EB_VIRT_ENCBTN;

//template <typename... Ts>
//auto makeArray(Ts&&... ts) -> void {

//}

#endif
