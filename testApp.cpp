// пример с библиотекой EncButton2

// Опциональные дефайн-настройки (показаны по умолчанию)
//#define EB_FAST 30     // таймаут быстрого поворота, мс
//#define EB_DEB 50      // дебаунс кнопки, мс
//#define EB_HOLD 1000   // таймаут удержания кнопки, мс
//#define EB_STEP 500    // период срабатывания степ, мс
//#define EB_CLICK 400   // таймаут накликивания, мс

//#include <EncButton.h>
#include <EncButton2.h>

enum {
    A = 10,
    B = 11,
    BT = 2
};

/*
Data:        356 bytes (17.4% Full)
Program:    4658 bytes (14.2% Full)

Data:        356 bytes (17.4% Full)
Program:    4748 bytes (14.5% Full)
*/

EncButton2<EB_ENCBTN> enc(A, B, BT, INPUT); // энкодер с кнопкой
//EncButton2<EB_ENCBTN> enc(A, B, BT, INPUT); // энкодер с кнопкой

// EncButton2<EB_ENC> enc(INPUT, 2, 3);        // просто энкодер
// EncButton2<EB_BTN> enc(INPUT, 4);           // просто кнопка
//  для изменения направления энкодера поменяй A и B при инициализации

// по умолчанию пины настроены в INPUT_PULLUP
// Если используется внешняя подтяжка - лучше перевести в INPUT
// EncButton<EB_TICK, 2, 3, 4> enc(INPUT);

void setup() {
    Serial.begin(9600);
    // ещё настройки
    // enc.counter = 100;        // изменение счётчика энкодера
    // enc.setHoldTimeout(500);  // установка таймаута удержания кнопки
    // enc.setButtonLevel(HIGH); // LOW - кнопка подключает GND (умолч.), HIGH - кнопка подключает VCC
}

void loop() {
    enc.tick(); // опрос происходит здесь

    // =============== ЭНКОДЕР ===============
    // обычный поворот
    if (enc.turn()) {
        Serial.println("turn");

        // можно опросить ещё:
        // Serial.println(enc.counter);  // вывести счётчик
        // Serial.println(enc.fast());   // проверить быстрый поворот
        Serial.println(enc.getDir()); // направление поворота
    }

    // "нажатый поворот"
    if (enc.turnH()) {
        Serial.println("hold + turn");

        // можно опросить ещё:
        // Serial.println(enc.counter);  // вывести счётчик
        // Serial.println(enc.fast());   // проверить быстрый поворот
        Serial.println(enc.getDir()); // направление поворота
    }

    if (enc.left())
        Serial.println("left"); // поворот налево
    if (enc.right())
        Serial.println("right"); // поворот направо
    if (enc.leftH())
        Serial.println("leftH"); // нажатый поворот налево
    if (enc.rightH())
        Serial.println("rightH"); // нажатый поворот направо

    // =============== КНОПКА ===============
    if (enc.press())
        Serial.println("press");
    if (enc.click())
        Serial.println("click");
    if (enc.release())
        Serial.println("release");

    if (enc.held())
        Serial.println("held"); // однократно вернёт true при удержании
    // if (enc.hold()) Serial.println("hold");   // будет постоянно возвращать true после удержания
    if (enc.step())
        Serial.println("step"); // импульсное удержание

    // проверка на количество кликов
    if (enc.hasClicks(1))
        Serial.println("action 1 clicks");
    if (enc.hasClicks(2))
        Serial.println("action 2 clicks");
    if (enc.hasClicks(3))
        Serial.println("action 3 clicks");
    if (enc.hasClicks(5))
        Serial.println("action 5 clicks");

    // вывести количество кликов
    if (enc.hasClicks()) {
        Serial.print("has clicks ");
        Serial.println(enc.clicks);
    }
}

//#include <EncButton.h>

// enum {
//     A = 10,
//     B = 11,
//     BT = 2
// };

// EncButton<EB_CALLBACK, A, B, BT> enc;
////EncButton<EB_CALLBACK, A, B> enc;
////EncButton<EB_CALLBACK, BT> enc;

// void CLICK() { Serial.println(__FUNCTION__); }
// void CLICKS() { Serial.println(__FUNCTION__), Serial.println(enc.clicks); }
// void HOLD() { Serial.println(__FUNCTION__); }
// void HOLDED() { Serial.println(__FUNCTION__); }
// void LEFT() { Serial.println(__FUNCTION__); }
// void LEFT_H() { Serial.println(__FUNCTION__); }
// void PRESS() { Serial.println(__FUNCTION__); }
// void RELEASE() { Serial.println(__FUNCTION__); }
// void RIGHT() { Serial.println(__FUNCTION__); }
// void RIGHT_H() { Serial.println(__FUNCTION__); }
// void STEP() { Serial.println(__FUNCTION__); }
// void TURN() { Serial.println(__FUNCTION__), Serial.println(enc.counter), Serial.println(enc.fast()); }
// void TURN_H() { Serial.println(__FUNCTION__), Serial.println(enc.getDir()), Serial.println(enc.fast()); }
// void CLICKS_5() { Serial.println(__FUNCTION__), Serial.println(enc.clicks); }

// void setup() {
//     Serial.begin(9600);

//    enc.attach(CLICKS_HANDLER, CLICKS);
//    enc.attach(CLICK_HANDLER, CLICK);
//    enc.attach(HOLDED_HANDLER, HOLDED);
//    enc.attach(HOLD_HANDLER, HOLD);
//    enc.attach(LEFT_HANDLER, LEFT);
//    enc.attach(LEFT_H_HANDLER, LEFT_H);
//    enc.attach(PRESS_HANDLER, PRESS);
//    enc.attach(RELEASE_HANDLER, RELEASE);
//    enc.attach(RIGHT_HANDLER, RIGHT);
//    enc.attach(RIGHT_H_HANDLER, RIGHT_H);
//    enc.attach(STEP_HANDLER, STEP);
//    enc.attach(TURN_HANDLER, TURN);
//    enc.attach(TURN_H_HANDLER, TURN_H);

//    enc.attachClicks(5, CLICKS_5);
//}

//// =============== LOOP =============
// void loop() {
//     enc.tick(); // обработка всё равно здесь
// }

///*

//Начальное состояние
// Data:        332 bytes (16.2% Full)
// Program:    4710 bytes (14.4% Full)

//вынес функциональность солбэков в отдельный класс
// Data:        332 bytes (16.2% Full)
// Program:    4710 bytes (14.4% Full)

//флаг макро сделал встроенными методами
// Data:        332 bytes (16.2% Full)
// Program:    4676 bytes (14.3% Full)

//заменил магические константы на enum Flags / State
// Data:        332 bytes (16.2% Full)
// Program:    4610 bytes (14.1% Full)

// Data:        333 bytes (16.3% Full)
// Program:    4612 bytes (14.1% Full)

//*/
