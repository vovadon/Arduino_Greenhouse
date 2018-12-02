#include <dht11.h>
#include <OneWire.h>

// i/o pins
#define DHT_PIN 12
#define DS_PIN 2
#define FAN_PIN 11
#define LAMP_PIN 9
#define PUMP_PIN 8
#define HUMIDITY_PIN A0

#define TEMP_AIR_MIN 24			// Если температура воздуха ниже указанной включаются лампы накаливания
#define HUMIDITY_AIR_MAX 80		// Если влажность воздуха больше указанной включается вентилятор
#define HUMIDITY_SOIL_MIN 300	// Минимальноче значение влажности почвы
#define HUMIDITY_SOIL_MAX 700	// range(0, 1023) большей сухости почвы соответствует большее значение. Если влажность почвы достигает указанного значения включается полив
#define TEMP_SOIL_MAX 25		// Если температура почвы больше указанной включается полив

#define PUMP_TIME_DELAY 5000	// На сколько влючать устройство (мс)
#define PUMP_WAIT 60000			// Время ожидания после полива

// Экземпляры классов, в которых инкапсулированны данные и методы для работы с датчиками
DHT11 dht(DHT_PIN);
OneWire ds(DS_PIN);

uint8_t state; // Состояние программы
int16_t temp_ds, hmdt_soil; // Переменные для хранения значений с датчиков
float temp_dht, hmdt_air;

unsigned long lastReadTime, lastReadSensors; // Переменные для измерения задержки
bool temp_flag, hmdt_flag, pump_flag, waitUpdateHmdtSoil;

void setup() {
	Serial.begin(9600);

	// установка портов
	pinMode(HUMIDITY_PIN, INPUT);
	pinMode(LAMP_PIN, OUTPUT);
	pinMode(FAN_PIN, OUTPUT);
	pinMode(PUMP_PIN, OUTPUT);

	lastReadSensors = -1000;
	lastReadTime = -PUMP_TIME_DELAY;

	temp_flag = true;
	hmdt_flag = true;
	pump_flag = true;
	waitUpdateHmdtSoil = false;
	state = 0; // начальное состояние
}

// Функция вывода данных в терминал
void printData() {
	Serial.print("Temp dht=");
	Serial.print(temp_dht);
	Serial.print(" Humidity air=");
	Serial.print(hmdt_air);
	Serial.print(" Temp ds=");
	Serial.print(temp_ds);
	Serial.print(", Humidity soil=");
	Serial.println(hmdt_soil);
}

void loop() {
	switch (state) {
		case 0:
			state = 1;
			break;
		case 1:
			// Получение данных с датчиков
			if (millis() - lastReadSensors >= 1000) {
				lastReadSensors = millis();

				hmdt_soil = analogRead(HUMIDITY_PIN);
				readDataDHT(temp_dht, hmdt_air);
				readDataDS(temp_ds);

				if (waitUpdateHmdtSoil && millis() - lastReadTime >= PUMP_WAIT) {
					waitUpdateHmdtSoil = false;
				}

				printData();
				state = 2;
			}

			break;
		case 2:
			if (temp_dht < TEMP_AIR_MIN && hmdt_flag) {
				if (temp_flag) {
					digitalWrite(LAMP_PIN, temp_flag);
					temp_flag = false;
				}
			}
			else {
				digitalWrite(LAMP_PIN, temp_flag);
				temp_flag = true;
			}
			state = 3;
			break;
		case 3: // Проверка температуры воздуха
			if (hmdt_air > HUMIDITY_AIR_MAX && temp_flag) {
				if (hmdt_flag) {
					digitalWrite(FAN_PIN, hmdt_flag);
					hmdt_flag = false;
				}
			}
			else {
				digitalWrite(FAN_PIN, hmdt_flag);
				hmdt_flag = true;
			}
			state = 4;
			break;
		case 4: // Проверка температуры и влажности почвы
			// Ждем время чтобы вода успела впитаться
			if (waitUpdateHmdtSoil) {
				if (millis() - lastReadTime >= PUMP_WAIT) {
					waitUpdateHmdtSoil = false;
				}
				// Иначе выходим
				else {
					state = 1;
					break;
				}
			}

			if (hmdt_air < HUMIDITY_SOIL_MIN || temp_ds > TEMP_SOIL_MAX) {
				if (pump_flag) {
					digitalWrite(PUMP_PIN, pump_flag);

					lastReadTime = millis();
					pump_flag = false;
				}

				if (millis() - lastReadTime > PUMP_TIME_DELAY) {
					digitalWrite(PUMP_PIN, pump_flag);
					lastReadTime = millis();

					pump_flag = true;
					waitUpdateHmdtSoil = true;
				}
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

	delay(1000);

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