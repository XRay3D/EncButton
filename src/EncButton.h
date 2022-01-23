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

namespace xr {
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

    template <bool PinEncB, class T, class F>
    struct conditional {
        typedef T type;
    };

    template <class T, class F>
    struct conditional<false, T, F> {
        typedef F type;
    };

    template <bool PinEncB, class T, class F>
    using conditional_t = typename conditional<PinEncB, T, F>::type;

    template <bool PinEncB, class T = void>
    struct enable_if {
    };

    template <class T>
    struct enable_if<true, T> {
        typedef T type;
    };

    template <bool PinEncB, class T = void>
    using enable_if_t = typename enable_if<PinEncB, T>::type;

    bool fastRead(const uint8_t pin) {
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

}; // namespace xr

enum : uint8_t {
    CompileTime = 0,
    Button = 1,
    Encoder = 2,
    Virtual = 4,
    UseCallback = 128,
};

using EB_TICK = xr::u8_constant<CompileTime>;
using EB_CALLBACK = xr::u8_constant<UseCallback>;

using EB_BTN = xr::u8_constant<Button>;
using EB_ENC = xr::u8_constant<Encoder>;
using EB_ENCBTN = xr::u8_constant<Button + Encoder>;

using EB_VIRT_BTN = xr::u8_constant<Button + Virtual>;
using EB_VIRT_ENC = xr::u8_constant<Encoder + Virtual>;
using EB_VIRT_ENCBTN = xr::u8_constant<Button + Encoder + Virtual>;

/////////////////////////////////////////////////////////////////////////////////////
template <typename Derived, typename Mode, uint8_t...>
struct Tick;

template <typename Derived>
struct Callbacks;

// ===================================== CLASS =====================================
template <typename Mode, typename UseCallback = xr::u8_constant<0>, uint8_t... Pins>
class EncButton : public xr::conditional_t<UseCallback::value >= 128, Callbacks<EncButton<Mode, UseCallback, Pins...>>, Callbacks<void>>,
                  public Tick<EncButton<Mode, UseCallback, Pins...>, Mode, Pins...>,
                  Mode //
{
    using mode = Mode;
    using CB = xr::conditional_t<UseCallback::value >= 128, Callbacks<EncButton>, Callbacks<void>>;
    friend CB;

    using TT = Tick<EncButton, Mode, Pins...>;
    friend TT;
    using TT::TT;

    enum Flags : uint8_t {
        EncTurn,              // 0 - enc turn
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
        Idle,   // 0 - idle
        Left,   // 1 - left
        Right,  // 2 - right
        LeftH,  // 3 - leftH
        RightH, // 4 - rightH
        Click,  // 5 - click
        Held,   // 6 - held
        Step,   // 7 - step
        Press,  // 8 - press
    };

public:
    using type = EncButton;

    // установить таймаут удержания кнопки для isHold(), мс (до 30 000)
    void setHoldTimeout(int tout) {
        _holdT = tout >> 7;
    }

    // виртуально зажать кнопку энкодера
    void holdEncButton(bool state) {
        state ? setFlag(EncButtonHold) : clrFlag(EncButtonHold);
    }

    // уровень кнопки: LOW - кнопка подключает GND (умолч.), HIGH - кнопка подключает VCC
    void setButtonLevel(bool level) {
        level ? clrFlag(Btnlevel) : setFlag(Btnlevel);
    }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие
    //    uint8_t tick();

    // тикер специально для прерывания, не проверяет коллбэки
    //    uint8_t tickISR(...); {

    // ===================================== CALLBACK =====================================
    // проверить callback, чтобы не дёргать в прерывании
    // void checkCallback() { CbMode::checkCallback(this); }

    // ===================================== STATUS =====================================
    uint8_t getState() { return EBState; } // получить статус
    void resetState() { EBState = {}; }    // сбросить статус

    // ======================================= ENC =======================================
    bool left() { return checkState(Left); }     // поворот влево
    bool right() { return checkState(Right); }   // поворот вправо
    bool leftH() { return checkState(LeftH); }   // поворот влево нажатый
    bool rightH() { return checkState(RightH); } // поворот вправо нажатый

    bool fast() { return readFlag(EncFast); }           // быстрый поворот
    bool turn() { return checkFlag<EncTurn>(); }        // энкодер повёрнут
    bool turnH() { return checkFlag<EncTurnHolded>(); } // энкодер повёрнут нажато

    int8_t getDir() { return _dir; } // направление последнего поворота, 1 или -1
    int16_t counter = 0;             // счётчик энкодера

    // ======================================= BTN =======================================
    bool state() { return _btnState; }               // статус кнопки
    bool press() { return checkState(Press); }       // кнопка нажата
    bool release() { return checkFlag<Released>(); } // кнопка отпущена
    bool click() { return checkState(Click); }       // клик по кнопке

    bool held() { return checkState(Held); }                         // кнопка удержана
    bool hold() { return readFlag(Hold); }                           // кнопка удерживается
    bool step() { return checkState(Step); }                         // режим импульсного удержания
    bool releaseStep() { return checkFlag<BtnReleasedAfterStep>(); } // кнопка отпущена после импульсного удержания

    bool held(uint8_t clk) { return (clicks == clk) ? checkState(Held) : 0; }                         // кнопка удержана с предварительным накликиванием
    bool hold(uint8_t clk) { return (clicks == clk) ? readFlag(Hold) : 0; }                           // кнопка удерживается с предварительным накликиванием
    bool step(uint8_t clk) { return (clicks == clk) ? checkState(Step) : 0; }                         // режим импульсного удержания с предварительным накликиванием
    bool releaseStep(uint8_t clk) { return (clicks == clk) ? checkFlag<BtnReleasedAfterStep>() : 0; } // кнопка отпущена после импульсного удержания с предварительным накликиванием

    uint8_t clicks = 0;                                                                          // счётчик кликов
    bool hasClicks(uint8_t num) { return (clicks == num && checkFlag<ClicksGetNum>()) ? 1 : 0; } // имеются клики
    uint8_t hasClicks() { return checkFlag<ClicksGet>() ? clicks : 0; }                          // имеются клики

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
    // ===================================== POOL ENC =====================================
    void poolEnc(uint8_t state) {
#ifdef EB_BETTER_ENC
        static constexpr int8_t _EB_DIR[] = {
            0, -1, 1, 0,
            1, 0, 0, -1,
            -1, 0, 0, 1,
            0, 1, -1, 0
        };

        if (_prev != state) {
            _ecount += _EB_DIR[state | (_prev << 2)]; // сдвиг внутреннего счётчика
            _prev = state;
#ifdef EB_HALFSTEP_ENC // полушаговый энкодер
            // спасибо https://github.com/GyverLibs/EncButton/issues/10#issue-1092009489
            if ((state == 0x3 || state == 0x0) && _ecount != 0) {
#else // полношаговый
            if (state == 0x3 && _ecount != 0) { // защёлкнули позицию
#endif
                EBState = (_ecount < 0) ? 1 : 2;
                _ecount = 0;
                if (sizeof...(Pins) == 3 || (Mode::value & 3) == 3) { // энкодер с кнопкой
                    if (!readFlag(Hold) && (_btnState || readFlag(EncButtonHold)))
                        EBState += 2; // если кнопка не "удерживается"
                }
                _dir = (EBState & 1) ? -1 : 1; // направление
                counter += _dir;               // счётчик
                if (EBState <= 2)
                    setFlag(EncTurn); // флаг поворота для юзера
                else if (EBState <= 4)
                    setFlag(EncTurnHolded); // флаг нажатого поворота для юзера
                if (millis() - _debTimer < EB_FAST)
                    setFlag(EncFast); // быстрый поворот
                else
                    clrFlag(EncFast); // обычный поворот
                _debTimer = millis();
            }
        }
#else
        if (_encRST && state == 0b11) {                              // ресет и энк защёлкнул позицию
            if (sizeof...(Pins) == 3 || (Mode::value & 3) == 3) {    // энкодер с кнопкой
                if ((_prev == 1 || _prev == 2) && !readFlag(Hold)) { // если кнопка не "удерживается" и энкодер в позиции 1 или 2
                    EBState = _prev;
                    if (_btnState || readFlag(EncButtonHold))
                        EBState += 2;
                }
            } else { // просто энкодер
                if (_prev == 1 || _prev == 2)
                    EBState = _prev;
            }

            if (EBState > 0) {                 // был поворот
                _dir = (EBState & 1) ? -1 : 1; // направление
                counter += _dir;               // счётчик
                if (EBState <= 2)
                    setFlag(EncTurn); // флаг поворота для юзера
                else if (EBState <= 4)
                    setFlag(EncTurnHolded); // флаг нажатого поворота для юзера
                if (millis() - _debTimer < EB_FAST)
                    setFlag(EncFast); // быстрый поворот
                else
                    clrFlag(EncFast); // обычный поворот
            }

            _encRST = 0;
            _debTimer = millis();
        }
        if (state == 0b00)
            _encRST = 1;
        _prev = state;
#endif
    }

    // ===================================== POOL BTN =====================================
    void poolBtn() {
        _btnState ^= readFlag(Btnlevel);
        const uint32_t thisMls = millis();
        const uint32_t debounce = thisMls - _debTimer;
        if (_btnState) {                     // кнопка нажата
            if (!readFlag(BtnFlag)) {        // и не была нажата ранее
                if (readFlag(DebFlag)) {     // ждём дебаунс
                    if (debounce > EB_DEB) { // прошел дебаунс
                        setFlag(BtnFlag);    // флаг кнопка была нажата
                        EBState = 8;         // кнопка нажата
                        _debTimer = thisMls; // сброс таймаутов
                    }
                } else { // первое нажатие
                    EBState = 0;
                    setFlag(DebFlag);                                  // запомнили что хотим нажать
                    if (debounce > EB_CLICK || readFlag(ClicksFlag)) { // кнопка нажата после EB_CLICK
                        clicks = 0;                                    // сбросить счётчик и флаг кликов
                        flags &= ~0b0011000011100000;                  // clear 5 6 7 12 13 (клики)
                    }
                    _debTimer = thisMls;
                }
            } else {                                          // кнопка уже была нажата
                if (!readFlag(Hold)) {                        // и удержание ещё не зафиксировано
                    if (debounce < (uint32_t)(_holdT << 7)) { // прошло меньше удержания
                        if (EBState != 0 && EBState != 8)
                            setFlag(EncWasTurn);     // но энкодер повёрнут! Запомнили
                    } else {                         // прошло больше времени удержания
                        if (!readFlag(EncWasTurn)) { // и энкодер не повёрнут
                            EBState = 6;             // значит это удержание (сигнал)
                            flags |= 0b00110000;     // set 4 5 запомнили что удерживается и отключаем сигнал о кликах
                            _debTimer = thisMls;     // сброс таймаута
                        }
                    }
                } else {                      // удержание зафиксировано
                    if (debounce > EB_STEP) { // таймер степа
                        EBState = 7;          // сигналим
                        setFlag(StepFlag);    // зафиксирован режим step
                        _debTimer = thisMls;  // сброс таймаута
                    }
                }
            }
        } else {                     // кнопка не нажата
            if (readFlag(BtnFlag)) { // но была нажата
                if (debounce > EB_DEB) {
                    if (!readFlag(Hold) && !readFlag(EncWasTurn)) { // энкодер не трогали и не удерживали - это клик
                        EBState = 5;                                // click
                        clicks++;
                    }
                    flags &= ~0b00011100; // clear 2 3 4
                    _debTimer = thisMls;  // сброс таймаута
                    setFlag(Released);    // кнопка отпущена
                    if (checkFlag<StepFlag>())
                        setFlag(BtnReleasedAfterStep); // кнопка отпущена после step
                }
            } else if (clicks > 0 && debounce > EB_CLICK && !readFlag(ClicksFlag))
                flags |= 0b11100000; // set 5 6 7 (клики)
            checkFlag<DebFlag>();    // сброс ожидания нажатия
        }
    }

    // ===================================== MISC =====================================

    // флаг макро
    void setFlag(Flags x) { flags |= 1 << x; }
    void clrFlag(Flags x) { flags &= ~(1 << x); }
    bool readFlag(Flags x) { return flags & (1 << x); }

    template <Flags val>
    bool checkFlag() {
        return readFlag(val) ? clrFlag(val), true : false;
    }

    bool checkState(State val) {
        return (EBState == val) ? EBState = 0, true : false;
    }

    uint8_t EBState : 4;
    uint8_t _prev : 2;
    bool _isrFlag : 1;
    bool _btnState : 1;
    bool _encRST : 1;
    uint16_t flags = 0;

#ifdef EB_BETTER_ENC
    int8_t _ecount = 0;
#endif

    uint32_t _debTimer = 0;
    uint8_t _holdT = (EB_HOLD >> 7);
    int8_t _dir = 0;
};

template <typename Derived>
struct GetDerived {
protected:
    Derived* d() { return static_cast<Derived*>(this); }
};

/////////////////////////////////////////////////////////////////////////////////////
/// \brief Compiletime Pin defined Tick structures
///
template <typename Derived, typename Mode, uint8_t PinEncA, uint8_t PinEncB, uint8_t PinBtn>
struct Tick<Derived, Mode, PinEncA, PinEncB, PinBtn> {

    // можно указать режим работы пина
    Tick(const uint8_t mode = INPUT_PULLUP) {
        if (mode == INPUT_PULLUP)
            pullUp();
        d()->setButtonLevel(LOW);
    }

    // подтянуть пины внутренней подтяжкой
    void pullUp() { pinMode(PinEncA, INPUT_PULLUP), pinMode(PinEncB, INPUT_PULLUP), pinMode(PinBtn, INPUT_PULLUP); }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие

    uint8_t tick() {

        tickISR();
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR() {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->poolEnc(xr::fastRead(PinEncA) | (xr::fastRead(PinEncB) << 1));
            d()->_btnState = xr::fastRead(PinBtn);
            d()->poolBtn();
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
};

template <typename Derived, typename Mode, uint8_t PinEncA, uint8_t PinEncB>
struct Tick<Derived, Mode, PinEncA, PinEncB> {
    // можно указать режим работы пина
    Tick(const uint8_t mode = INPUT_PULLUP) {
        if (mode == INPUT_PULLUP)
            pullUp();
        d()->setButtonLevel(LOW);
    }

    // подтянуть пины внутренней подтяжкой
    void pullUp() { pinMode(PinEncA, INPUT_PULLUP), pinMode(PinEncB, INPUT_PULLUP); }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие
    uint8_t tick() {

        tickISR();
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR() {

        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->poolEnc(xr::fastRead(PinEncA) | (xr::fastRead(PinEncB) << 1));
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
};

template <typename Derived, typename Mode, uint8_t PinBtn>
struct Tick<Derived, Mode, PinBtn> {
    // можно указать режим работы пина
    Tick(const uint8_t mode = INPUT_PULLUP) {
        if (mode == INPUT_PULLUP)
            pullUp();
        d()->setButtonLevel(LOW);
    }

    // подтянуть пины внутренней подтяжкой
    void pullUp() { pinMode(PinBtn, INPUT_PULLUP); }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие
    uint8_t tick() {

        tickISR();
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR() {

        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->_btnState = xr::fastRead(PinBtn);
            d()->poolBtn();
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
};
/////////////////////////////////////////////////////////////////////////////////////
/// \brief Runtime Pin defined Tick structures
template <typename Derived>
struct Tick<Derived, EB_ENCBTN> {

    Tick(uint8_t PinEncA, uint8_t PinEncB, uint8_t PinBtn, uint8_t mode = INPUT) {
        d()->setButtonLevel(LOW);
        setPins(mode, PinEncA, PinEncB, PinBtn);
    }

    // установить пины
    void setPins(uint8_t PinEncA, uint8_t PinEncB, uint8_t PinBtn, uint8_t mode = INPUT) {
        pinMode(PinEncA, mode);
        pinMode(PinEncB, mode);
        pinMode(PinBtn, mode);
        _pins[0] = PinEncA;
        _pins[1] = PinEncB;
        _pins[2] = PinBtn;
    }

    // подтянуть пины внутренней подтяжкой
    void pullUp() { pinMode(_pins[0], INPUT_PULLUP), pinMode(_pins[1], INPUT_PULLUP), pinMode(_pins[2], INPUT_PULLUP); }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие
    uint8_t tick() {

        tickISR();
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR() {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->poolEnc(xr::fastRead(_pins[0]) | (xr::fastRead(_pins[1]) << 1));
            d()->_btnState = xr::fastRead(_pins[2]);
            d()->poolBtn();
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
    uint8_t _pins[Button + Encoder];
};

template <typename Derived>
struct Tick<Derived, EB_ENC> {

    Tick(uint8_t PinEncA, uint8_t PinEncB, uint8_t mode = INPUT) {
        d()->setButtonLevel(LOW);
        setPins(mode, PinEncA, PinEncB);
    }

    // установить пины
    void setPins(uint8_t PinEncA, uint8_t PinEncB, uint8_t mode = INPUT) {
        pinMode(PinEncA, mode);
        pinMode(PinEncB, mode);
        _pins[0] = PinEncA;
        _pins[1] = PinEncB;
    }

    // подтянуть пины внутренней подтяжкой
    void pullUp() { pinMode(_pins[0], INPUT_PULLUP), pinMode(_pins[1], INPUT_PULLUP); }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие

    uint8_t tick() {

        tickISR();
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR() {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->poolEnc(xr::fastRead(_pins[0]) | (xr::fastRead(_pins[1]) << 1));
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
    uint8_t _pins[Encoder];
};

template <typename Derived>
struct Tick<Derived, EB_BTN> {
    Tick(uint8_t PinBtn, uint8_t mode = INPUT) {
        d()->setButtonLevel(LOW);
        setPins(mode, PinBtn);
    }

    // установить пины
    void setPins(uint8_t PinBtn, uint8_t mode = INPUT) {
        pinMode(PinBtn, mode);
        _pin = PinBtn;
    }

    // подтянуть пины внутренней подтяжкой
    void pullUp() { pinMode(_pin, INPUT_PULLUP); }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие

    uint8_t tick() {

        tickISR();
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR() {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->_btnState = xr::fastRead(_pin);
            d()->poolBtn();
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
    uint8_t _pin;
};

/////////////////////////////////////////////////////////////////////////////////////
/// \brief Virtual Pin defined Tick structures
template <typename Derived>
struct Tick<Derived, EB_VIRT_ENCBTN> {

    Tick() {
        d()->setButtonLevel(LOW);
    }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие
    uint8_t tick(uint8_t PinEncA, uint8_t PinEncB, uint8_t PinBtn) {
        tickISR(PinEncA, PinEncB, PinBtn);
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR(uint8_t PinEncA, uint8_t PinEncB, uint8_t PinBtn) {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->poolEnc(xr::fastRead(PinEncA) | (xr::fastRead(PinEncB) << 1));
            d()->_btnState = xr::fastRead(PinBtn);
            d()->poolBtn();
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
};

template <typename Derived>
struct Tick<Derived, EB_VIRT_ENC> {
    Tick() {
        d()->setButtonLevel(LOW);
    }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие

    uint8_t tick(uint8_t PinEncA, uint8_t PinEncB) {
        tickISR(PinEncA, PinEncB);
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR(uint8_t PinEncA, uint8_t PinEncB) {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->poolEnc(xr::fastRead(PinEncA) | (xr::fastRead(PinEncB) << 1));
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
};

template <typename Derived>
struct Tick<Derived, EB_VIRT_BTN> {
    Tick() {
        d()->setButtonLevel(LOW);
    }

    // ===================================== TICK =====================================
    // тикер, вызывать как можно чаще
    // вернёт отличное от нуля значение, если произошло какое то событие

    uint8_t tick(uint8_t PinBtn) {
        tickISR(PinBtn);
        d()->checkCallback();
        return d()->EBState;
    }

    // тикер специально для прерывания, не проверяет коллбэки
    uint8_t tickISR(uint8_t PinBtn) {
        if (!d()->_isrFlag) {
            d()->_isrFlag = 1;
            d()->_btnState = xr::fastRead(PinBtn);
            d()->poolBtn();
        }
        d()->_isrFlag = 0;
        return d()->EBState;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }
};

/////////////////////////////////////////////////////////////////////////////////////
/// \brief The Callbacks struct
///
template <class Derived>
struct Callbacks {
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
        if (d()->turn())
            d()->exec(TURN_HANDLER);
        if (d()->turnH())
            d()->exec(TURN_H_HANDLER);
        if (d()->EBState > 0 && d()->EBState <= 8)
            d()->exec(EbCallback(d()->EBState));
        if (d()->release())
            d()->exec(RELEASE_HANDLER);
        if (d()->hold())
            d()->exec(HOLD_HANDLER);
        if (d()->template checkFlag<Derived::ClicksGet>()) {
            d()->exec(CLICKS_HANDLER);
            if (d()->clicks == d()->_amount)
                d()->exec(EbCallback(13)); // CLICKS_AMOUNT
        }
        d()->EBState = 0;
    }

private:
    Derived* d() { return static_cast<Derived*>(this); }

    void exec(EbCallback num) {
        if (_callbacks[num])
            _callbacks[num]();
    }

    uint8_t _amount = 0;
    Callback _callbacks[14] {};
};

template <>
struct Callbacks<void> {
    inline void checkCallback() { }
};

#endif
