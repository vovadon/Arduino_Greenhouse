#ifndef DHT_H
#define DHT_H

#include <Arduino.h>

#define DHT_OK 0
#define DHT_WAIT_FOR_SECOND 1
#define DHT_ERROR_CHECKSUM 2
#define DHT_ERROR_TIMEOUT 3

class DHT11
{
public:
	DHT11(uint8_t pin);

	float readTemperature();
	float readHumidity();
private:
	int8_t read();
	unsigned long checkPin(bool voltage, unsigned int timeout_us);

	byte data[5];
	float _temperature;
	float _humidity;

	uint8_t _pin;
};

#endif