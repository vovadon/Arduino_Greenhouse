#include <dht11.h>
#include <Wire.h> // DS1307 подключается через I2C интерфейс и библиотеку Wire 
#include <OneWire.h>
#include "RTClib.h"

// i/o pins
#define DHT_PIN 7
#define DS_PIN 2
#define FAN_PIN 11
#define LAMP_PIN 9
#define PUMP_PIN 8
#define EN_HMDT_SOIL_PIN 4
#define LED_PIN 10
#define HUMIDITY_PIN A0

#define TEMP_AIR_MIN 24         // Если температура воздуха ниже указанной включаются лампы накаливания
#define HUMIDITY_AIR_MAX 60     // Если влажность воздуха больше указанной включается вентилятор
#define HUMIDITY_SOIL_MIN 300   // Минимальноче значение влажности почвы
#define HUMIDITY_SOIL_MAX 700   // range(0, 1023) большей сухости почвы соответствует большее значение. Если влажность почвы достигает указанного значения включается полив
#define TEMP_SOIL_MAX 25        // Если температура почвы больше указанной включается полив

#define PUMP_TIME_DELAY 5000    // На сколько влючать устройство (мс)
#define PUMP_WAIT 60000         // Время ожидания после полива
#define READ_HMDT_SOIL_MS 3600000 // 1 час

// Экземпляры классов, в которых инкапсулированны данные и методы для работы с датчиками
DHT11 dht(DHT_PIN);
OneWire ds(DS_PIN);
RTC_DS1307 rtc;
DateTime now;

uint8_t state; // Состояние программы
int16_t temp_ds, hmdt_soil; // Переменные для хранения значений с датчиков
float temp_dht, hmdt_air;

unsigned long lastReadTime, lastReadSensors; // Переменные для измерения задержки
bool temp_flag, hmdt_flag, pump_flag, waitUpdateHmdtSoil, isSoilOk;

void setup() {
    Serial.begin(9600); // 57600
    //rtc.adjust(DateTime(__DATE__, __TIME__));

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
    }

    if (!rtc.isrunning()) {
        Serial.println("RTC is NOT running!");
    }

    // установка портов
    pinMode(HUMIDITY_PIN, INPUT);
    pinMode(LAMP_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(EN_HMDT_SOIL_PIN, OUTPUT);
    digitalWrite(EN_HMDT_SOIL_PIN, HIGH);

    state = 0; // начальное состояние
}

// Функция вывода данных в терминал
void printData() {
    Serial.println();
    Serial.print("Temp dht=");
    Serial.print(temp_dht);
    Serial.print(", Humidity air=");
    Serial.print(hmdt_air);
    Serial.print(", Temp ds=");
    Serial.print(temp_ds);
    Serial.print(", Humidity soil=");
    Serial.println(hmdt_soil);
    Serial.println();
}

void loop() {
    switch (state) {
        case 0:
            lastReadSensors = -1000;
            lastReadTime = -READ_HMDT_SOIL_MS;

            temp_flag = true;
            hmdt_flag = true;
            pump_flag = true;
            isSoilOk = true;
            waitUpdateHmdtSoil = false;
            state = 1;
            break;
        case 1:
            // Получение данных с датчиков
            now = rtc.now(); // Получение текущего времени с часов

            if (pump_flag) { // pump_flag = true когда насос выключен
                if (!isSoilOk || millis() - lastReadTime >= READ_HMDT_SOIL_MS) {
                    lastReadTime = millis();

                    digitalWrite(EN_HMDT_SOIL_PIN, LOW);
                    readDataDS(ds);

                    // Для повышения точности считывание выполняется несколько раз
                    for (uint8_t i = 0; i < 5; i++) {
                        hmdt_soil += analogRead(HUMIDITY_PIN);
                        delay(100);
                    }
                    digitalWrite(EN_HMDT_SOIL_PIN, HIGH);
                    hmdt_soil /= 5; // Получаем усредненное значение
                }
            }

            if (millis() - lastReadSensors >= 1000) {
                lastReadSensors = millis();

                readDataDHT(temp_dht, hmdt_air);

                // if (waitUpdateHmdtSoil && millis() - lastReadTime >= PUMP_WAIT) {
                //     waitUpdateHmdtSoil = false;
                // }

                printData();
                state = 2;
            }
            break;
        case 2: // Проверка температуры воздуха
            if (temp_dht < TEMP_AIR_MIN && hmdt_flag) {
                if (temp_flag) {
                    Serial.println("Enable lamp");
                    digitalWrite(LAMP_PIN, HIGH);
                    temp_flag = false;
                }
            }
            else {
                Serial.println("Disable lamp");
                digitalWrite(LAMP_PIN, LOW);
                temp_flag = true;
            }
            state = 3;
            break;
        case 3: // Проверка влажности воздуха
            if (hmdt_air > HUMIDITY_AIR_MAX && temp_flag) {
                if (hmdt_flag) {
                    Serial.println("Enable fan");
                    digitalWrite(FAN_PIN, HIGH);
                    hmdt_flag = false;
                }
            }
            else {
                Serial.println("Disable fan");
                digitalWrite(FAN_PIN, LOW);
                hmdt_flag = true;
            }
            state = 4;
            break;
        case 4: // Включение светодиодой ленты по времени
            Serial.print(now.day(), DEC);
            Serial.print('/');
            Serial.print(now.month(), DEC);
            Serial.print('/');
            Serial.print(now.year(), DEC);
            Serial.print(' ');
            Serial.print(now.hour(), DEC);
            Serial.print(':');
            Serial.print(now.minute(), DEC);
            Serial.print(':');
            Serial.print(now.second(), DEC);
            Serial.println();

            if (now.hour() >= 6 && now.hour() < 18) {
                digitalWrite(LED_PIN, HIGH);
            }
            else {
                digitalWrite(LED_PIN, LOW);
            }
            break;
        case 5: // Проверка температуры и влажности почвы
            // Ждем чтобы вода успела впитаться
            if (waitUpdateHmdtSoil) {
                if (millis() - lastReadTime >= PUMP_WAIT) {
                    Serial.println("waitUpdateHmdtSoil = false");
                    waitUpdateHmdtSoil = false;
                    lastReadTime = millis();
                }
                // Иначе выходим
                else {
                    state = 1;
                    Serial.println("Wait for 60 second");
                    break;
                }
            }

            if (hmdt_soil < HUMIDITY_SOIL_MIN || temp_ds > TEMP_SOIL_MAX) {
                if (pump_flag) {
                    Serial.println("Enable pump");
                    digitalWrite(PUMP_PIN, HIGH);

                    lastReadTime = millis();
                    pump_flag = false;
                    isSoilOk = false;
                }

                if (millis() - lastReadTime > PUMP_TIME_DELAY) {
                    digitalWrite(PUMP_PIN, LOW);
                    lastReadTime = millis();
                    Serial.println("Disable pump, ds18b20 wait 60 seconds");

                    pump_flag = true;
                    waitUpdateHmdtSoil = true;
                }
            }
            else {
                Serial.println("soid hmdt and temp are normal");
                isSoilOk = true;
            }
            state = 1;
            break;
    }
}

// Функция получает и записывает температуру и влажность с датчика DHT22
// Возвращает true если данные успешно получены, иначе false
bool readDataDHT(float &temp, float &humidity) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Провека данных с датчика
    if (isnan(h) || isnan(t)) {
        Serial.println("Couldn't read data from dht");
        return false;
    }
    else {
        temp = t;
        humidity = h;
    }
    return true;
}

// Функция получает и записывает температуру с датчика ds18b20 
// Возвращает true если данные успешно получены, иначе false
bool readDataDS(int16_t &temp) {
    byte addr[8], data[12];

    if (!ds.search(addr)) {
        ds.reset_search();
        return false;
    }

    if (OneWire::crc8(addr, 7) != addr[7]) {
        Serial.println("CRC is not valid!");
        return false;
    }

    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);

    delay(800);

    ds.reset();
    ds.select(addr);
    ds.write(0xBE);

    Serial.print("Addr= ");
    for(int i=0; i<8; i++){
        Serial.print(addr[i], HEX);
    }
    Serial.println();

    for (int8_t i = 0; i < 9; i++) {
        data[i] = ds.read();
    }

    Serial.print(" Data=");
    Serial.print(data[1], HEX);
    Serial.print(data[0], HEX);
    Serial.println();

    temp = ((data[1] << 8) | data[0]) * 0.0625;
    return true;
}
