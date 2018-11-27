#include "dht11.h"

DHT11::DHT11(uint8_t pin)
{
	_pin = pin;
}

int8_t DHT11::read()
{
	for (int8_t i = 0; i < 5; i++)
		data[i] = 0;

	pinMode(_pin, OUTPUT);
	digitalWrite(_pin, LOW);
	delay(18);

	pinMode(_pin, INPUT_PULLUP);

	if (checkPin(HIGH, 40) == 0)
	{
		return DHT_ERROR_TIMEOUT;
	}
	if (checkPin(LOW, 80) == 0)
	{
		return DHT_ERROR_TIMEOUT;
	}
	if (checkPin(HIGH, 80) == 0)
	{
		return DHT_ERROR_TIMEOUT;
	}

	for (int8_t i = 0; i < 40; i++)
	{
		unsigned long lowCycle = checkPin(LOW, 50);
		unsigned long highCycle = checkPin(HIGH, 70);
		
		if (lowCycle == 0 || highCycle == 0)
			return DHT_ERROR_TIMEOUT;

		data[i / 8] <<= 1;
		if (highCycle > lowCycle)
			data[i / 8] |= 1;
	}

	if (data[4] != (data[0] + data[1] + data[2] + data[3]))
		return DHT_ERROR_CHECKSUM;

	_humidity = data[0];
	_temperature = data[3] & 0x80 ? (data[2] + (1 - (data[3] & 0x7F) * 0.1)) * -1 : (data[2] + (data[3] & 0x7F) * 0.1);

	return DHT_OK;
}

float DHT11::readTemperature()
{
	read();
	return _temperature;
}

float DHT11::readHumidity()
{
	read();
	return _humidity;
}

unsigned long DHT11::checkPin(bool voltage, unsigned int timeout_us)
{
	unsigned long lastreadtime = micros();
	while (digitalRead(_pin) == voltage)
	{
		if (micros() - lastreadtime > timeout_us)
			return 0;
	}
	return micros() - lastreadtime;
}