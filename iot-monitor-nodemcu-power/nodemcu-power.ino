/*
  Mauricio
  Julho2020

  English, Portuguese

  POWER MONITOR
  - NodeMCU lolin
      - Current sensor 100A:50mA
          - 2x 10k power-split-resitor
          - 1x burn-resistor 22ohm

  FUTURE WORK
  - Only english comments and strings
  - Add external configuration file + gitignore
  - Test for memory leak
*/

// BUILT IN LED
#define BUILT_IN_LED D4

// LED
#define LED1 D0
#define LED2 D2
#define LED3 D3

// WIFI CONNECTION
#include <ESP8266WiFi.h>
String WIFI_SS_ID = "";
String WIFI_SS_PASSWORD = "";

// REMOTE HTTP SERVER
String SERVER_HOST_URL = "www.mauriciodz.com";
int SERVER_PORT = 33000;

// CURRENT SENSOR
// CT YHDC 100A:50mA + Emon libray
#define TOTAL_SAMPLES 120
#define HOME_VOLTAGE 230.0
// analog converter: GND=4 e 3.33V=1024
#define CURRENT_ANALOG_IN A0
// current when there is 1v across burnen resistor:
#define CURRENT_CALIBRATION 111.1
// number of samples to take (can be very uncertain since the wave can have
// distortions):
#define CURRENT_NUMBER_OF_SAMPLES 1480
#include "EmonLib.h"
EnergyMonitor emon1;

// SERVER DATA JSON STRUCTURE
struct sensor_values_t
{
	double a0 = -1.0; // a0 raw read
	double current = -1.0; // current in amperes
	double voltage = -1.0; // voltage in volts
	double power = -1.0; // power in watts
	double energy = -1.0; // consumed energy in joules
	double duration = -1.0; // amout of seconds to measure all the wave samples
};

void setupCurrentSensor()
{
	emon1.current(CURRENT_ANALOG_IN, CURRENT_CALIBRATION);
}

void hardResetNodemcu()
{
	// ESP.reset();
	ESP.restart();
}

void setupWifi()
{
	Serial.print("Tentando conectar na rede wifi...");
	WiFi.begin(WIFI_SS_ID, WIFI_SS_PASSWORD);
	while(WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("Conectado!");
	Serial.print("Meu IP: ");
	Serial.println(WiFi.localIP().toString());
}

void setupSerialMonitor()
{
	Serial.begin(115200);
	Serial.println();
	Serial.println("Hi");
}

void blinkLed(int LED_PIN, int duration, int repeat)
{
	if(repeat < 1)
		return;
	if(duration > 5000)
		return;
	if(duration < 50)
		return;

	for(int i = 0; i < repeat; i++)
	{
		// turn on
		digitalWrite(LED_PIN, HIGH);
		delay(duration);

		// turn off
		digitalWrite(LED_PIN, LOW);
		delay(duration);
	}
}

void readCurrentFromSensor(struct sensor_values_t* ptr_sensor_values)
{
	double rmsCurrent = emon1.calcIrms(CURRENT_NUMBER_OF_SAMPLES);
	ptr_sensor_values->current = rmsCurrent;
	double a0 = (double)analogRead(CURRENT_ANALOG_IN);
	ptr_sensor_values->a0 = a0;
	ptr_sensor_values->voltage = HOME_VOLTAGE;
	ptr_sensor_values->power = (rmsCurrent * HOME_VOLTAGE);
}

void setupBuiltInLed()
{
	pinMode(BUILT_IN_LED, OUTPUT);
	digitalWrite(BUILT_IN_LED, LOW);
}

int sendValuesToServer(struct sensor_values_t* ptr_sensor_values)
{
	WiFiClient client;
	Serial.println(SERVER_HOST_URL);
	Serial.print("Tentando conectar no servidor...");
	int connectStatus = 999;

	if((connectStatus = client.connect(SERVER_HOST_URL, SERVER_PORT)))
	{
		Serial.println("OK");

		char query[300];
		sprintf(query,
				"GET "
				"/savepower?current=%.2f&a0=%.2f&"
				"power=%.2f&voltage=%.2f&energy=%."
				"2f&duration=%.2f "
				"HTTP/1.0",
				ptr_sensor_values->current,
				ptr_sensor_values->a0,
				ptr_sensor_values->power,
				ptr_sensor_values->voltage,
				ptr_sensor_values->energy,
				ptr_sensor_values->duration);

		Serial.printf("QUERY: %s\n", query);

		Serial.println("-- SENDING QUERY TO SERVER --");

		client.println(query);
		client.println();

		Serial.println("-- QUERY SENT DONE --:");

		Serial.println("RESPONSE HEADER:");
		while(client.connected())
		{
			String response = client.readStringUntil('\n');
			if(response == "\r")
			{
				Serial.println(response);
				break;
			}
		}
		Serial.println("-- HEADER DONE --");

		Serial.println("RESPONSE BODY:");

		String response = client.readStringUntil('\n');
		Serial.println(response);

		Serial.println("-- RESPONSE DONE --");
	}
	else
	{
		Serial.print("ERROR: ");
		Serial.println(connectStatus);
		return -1;
	}

	return 0;
}

void setupLed()
{
	pinMode(LED1, OUTPUT);
	digitalWrite(LED1, LOW);

	pinMode(LED2, OUTPUT);
	digitalWrite(LED2, LOW);

	pinMode(LED3, OUTPUT);
	digitalWrite(LED3, LOW);
}

void waveSamplesOnPlotter()
{
	double is[1000];
	for(int i = 0; i < 1000; i++)
	{
		double a0 = (double)analogRead(CURRENT_ANALOG_IN);
		is[i] = a0;
	}
	for(int i = 0; i < 1000; i++)
	{
		Serial.println(is[i]);
		delay(500);
	}
	delay(2000);
}

void setup()
{
	setupSerialMonitor();
	setupBuiltInLed();
	setupLed();
	setupWifi();
}

void loop()
{
	digitalWrite(LED1, HIGH);

	// read sensors
	double a0 = (double)analogRead(CURRENT_ANALOG_IN);
	double current = emon1.calcIrms(1000);
	double voltage = HOME_VOLTAGE;
	double power = current * voltage; // volts * amperes = watts
	double timeDurationSeconds = 1.0; // 1sec
	double energy = power * timeDurationSeconds; // watts * seconds = joules

	// print to serial monitor
	Serial.print("CURRENT_NUMBER_OF_SAMPLES: ");
	Serial.println(CURRENT_NUMBER_OF_SAMPLES);
	Serial.print("CURRENT_ANALOG_IN: ");
	Serial.println(CURRENT_ANALOG_IN);
	Serial.print("CURRENT_CALIBRATION: ");
	Serial.println(CURRENT_CALIBRATION);
	Serial.print("a0: ");
	Serial.println(a0);
	Serial.print("Current: ");
	Serial.println(current);

	digitalWrite(LED1, LOW);
	delay(700);

	// mount structure
	struct sensor_values_t* ptr_sensor_values = (struct sensor_values_t*)malloc(sizeof(struct sensor_values_t));
	ptr_sensor_values->a0 = a0; // media dos valores lidos em a0
	ptr_sensor_values->current = current; // media da corrente lida
	ptr_sensor_values->voltage = voltage; // media da voltage lida
	ptr_sensor_values->power = power; // media da potencia
	ptr_sensor_values->energy = energy; // soma da energia total usada
	ptr_sensor_values->duration = 1.0; // soma do tempo usado em second

	digitalWrite(LED1, LOW);
	digitalWrite(LED2, HIGH);

	// send to server
	int response = sendValuesToServer(ptr_sensor_values);

	digitalWrite(LED2, LOW);

	// check response
	if(response >= 0)
	{
		// success
		digitalWrite(LED3, HIGH);
		delay(1000);
		digitalWrite(LED3, LOW);
		delay(1000);
	}
	else
	{
		// error
		blinkLed(BUILT_IN_LED, 250, 16);
		delay(1000);
		hardResetNodemcu();
	}
}

void loop2()
{
	// faz 1 leitura
	// readCurrentFromSensor(&sensor_values);

	// 14A, 220V, 300W, 100j, 104500J
	// 16A, 220V, 300W, 300j, 104900J

	// memory for all samples
	double sum_current = 0.0;
	double sum_voltage = 0.0;
	double sum_power = 0.0;
	double sum_energy = 0.0;
	double sum_a0 = 0.0;
	unsigned long sum_time = 0;
	unsigned long startTime = millis();
	unsigned long endTime = 0;
	unsigned long timeDuration = 0;

	// take TOTAL_SAMPLES readings, one per half second, 1 minute total
	for(int counter = 0; counter < TOTAL_SAMPLES; counter++)
	{
		digitalWrite(LED1, HIGH);

		// compute 1 reading
		double a0 = (double)analogRead(CURRENT_ANALOG_IN);
		double current = emon1.calcIrms(CURRENT_NUMBER_OF_SAMPLES);
		double voltage = HOME_VOLTAGE;
		double power = current * voltage; // volts * amperes = watts

		endTime = millis();
		timeDuration = endTime - startTime;
		double timeDurationSeconds = ((double)timeDuration / 1000.0);
		double energy = power * timeDurationSeconds; // watts * seconds = joules
		sum_time += timeDuration;

		// store the sum
		sum_current += current;
		sum_voltage += voltage;
		sum_power += power;
		sum_energy += energy;
		sum_a0 += a0;

		// print
		// Serial.print("CURRENT: ");Serial.println(current);
		// Serial.print("VOLTAGE: ");Serial.println(voltage);
		// Serial.print("POWER: ");Serial.println(power);
		// Serial.print("ENERGY: ");Serial.println(energy);
		// Serial.print("SUM_ENERGY: ");Serial.println(sum_energy);

		digitalWrite(LED1, LOW);

		startTime = millis();
		delay(500);
	}

	// compute averange value from 60 readings
	double media_current = sum_current / (double)TOTAL_SAMPLES;
	double media_voltage = sum_voltage / (double)TOTAL_SAMPLES;
	double media_power = sum_power / (double)TOTAL_SAMPLES;
	double media_a0 = sum_a0 / (double)TOTAL_SAMPLES;

	// send the media
	struct sensor_values_t* ptr_sensor_values = (struct sensor_values_t*)malloc(sizeof(struct sensor_values_t));
	ptr_sensor_values->a0 = media_a0; // media dos valores lidos em a0
	ptr_sensor_values->current = media_current; // media da corrente lida
	ptr_sensor_values->voltage = media_voltage; // media da voltage lida
	ptr_sensor_values->power = media_power; // media da potencia
	ptr_sensor_values->energy = sum_energy; // soma da energia total usada
	ptr_sensor_values->duration = ((double)sum_time / 1000.0); // soma do tempo usado em second

	// send to server
	digitalWrite(LED2, HIGH);
	int response = sendValuesToServer(ptr_sensor_values);
	digitalWrite(LED2, LOW);

	// check response
	if(response > 0)
	{
		// success
		digitalWrite(LED3, HIGH);
		delay(1000);
		digitalWrite(LED3, LOW);
		delay(1000);
	}
	else
	{
		// error
		blinkLed(BUILT_IN_LED, 250, 8);
		delay(1000);
		hardResetNodemcu();
	}
}
