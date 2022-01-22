#include <EncButton.h>

enum {
    A = 10,
    B = 11,
    BT = 2
};

EncButton<EB_CALLBACK, A, B, BT> enc;
//EncButton<EB_CALLBACK, A, B> enc;
//EncButton<EB_CALLBACK, BT> enc;

void CLICK() { Serial.println(__FUNCTION__); }
void CLICKS() { Serial.println(__FUNCTION__), Serial.println(enc.clicks); }
void HOLD() { Serial.println(__FUNCTION__); }
void HOLDED() { Serial.println(__FUNCTION__); }
void LEFT() { Serial.println(__FUNCTION__); }
void LEFT_H() { Serial.println(__FUNCTION__); }
void PRESS() { Serial.println(__FUNCTION__); }
void RELEASE() { Serial.println(__FUNCTION__); }
void RIGHT() { Serial.println(__FUNCTION__); }
void RIGHT_H() { Serial.println(__FUNCTION__); }
void STEP() { Serial.println(__FUNCTION__); }
void TURN() { Serial.println(__FUNCTION__), Serial.println(enc.counter), Serial.println(enc.fast()); }
void TURN_H() { Serial.println(__FUNCTION__), Serial.println(enc.getDir()), Serial.println(enc.fast()); }
void CLICKS_5() { Serial.println(__FUNCTION__), Serial.println(enc.clicks); }

void setup()
{
    Serial.begin(9600);

    enc.attach(CLICKS_HANDLER, CLICKS);
    enc.attach(CLICK_HANDLER, CLICK);
    enc.attach(HOLDED_HANDLER, HOLDED);
    enc.attach(HOLD_HANDLER, HOLD);
    enc.attach(LEFT_HANDLER, LEFT);
    enc.attach(LEFT_H_HANDLER, LEFT_H);
    enc.attach(PRESS_HANDLER, PRESS);
    enc.attach(RELEASE_HANDLER, RELEASE);
    enc.attach(RIGHT_HANDLER, RIGHT);
    enc.attach(RIGHT_H_HANDLER, RIGHT_H);
    enc.attach(STEP_HANDLER, STEP);
    enc.attach(TURN_HANDLER, TURN);
    enc.attach(TURN_H_HANDLER, TURN_H);

    enc.attachClicks(5, CLICKS_5);
}

// =============== LOOP =============
void loop() {
    enc.tick(); // обработка всё равно здесь
}

/*

Начальное состояние
Data:        332 bytes (16.2% Full)
Program:    4710 bytes (14.4% Full)

вынес функциональность солбэков в отдельный класс
Data:        332 bytes (16.2% Full)
Program:    4710 bytes (14.4% Full)

флаг макро сделал встроенными методами
Data:        332 bytes (16.2% Full)
Program:    4676 bytes (14.3% Full)

заменил магические константы на enum Flags / State
Data:        332 bytes (16.2% Full)
Program:    4610 bytes (14.1% Full)


*/
