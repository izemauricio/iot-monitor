/*
  18julho2020, 5agosto2020, 16nov2020

  Mauricio Ize - mauriciodcjz@gmail.com

  WEATHER MONITOR
  - NodeMCU 12E LOLIN-V3 (SDA = D2 = GPIO4, SCL = D1 = GPIO5)
      - BME280 GY-BM (vcc gnd scl sda csb sdd) [Celsius, hPA, %]
      - BMP280 HW-611 (vcc gnd scl sda csb sdd) [Celsius, hPA]
      - Luminosity GY-30 (gnd vcc sda scl ad0) [lux] threshold configurado no resistor) [ppm]
      - GAS CO MQ-7 (gnd vcc d0 a0) (d0 = avisa que ppm ultrapassou threshold configurado no resistor) [ppm]
      - RGB-LED (gnd d0 d1 d2)
      - Bussola-XYZ GY-273 (chip hmc5883L) (gnd vcc scl sda drdy) (drdy = digital = data-ready) [uT] Adicionado 5agosto2020:
      - Tiny RTC i2c DS1307 (gnd, vcc, sda, scl)x2(repetido nos dois lados) + (sq (output do gerador de onda quadrada), +bt(output da bateria)) de um lado apenas
      - level shift 3.3v 5v (nao tem modelo) (h1 h2 h3 h4 hVcc hGnd l1 l2 l3 l4 lVcc lGnd)
      - ADC expander: 4adc to i2c ADS1115 (ad0 ad1 ad2 ad3 sda scl gnd vcc)

  PODERIA TER:
      - LCD 16x2 [i2c]
      - Radio TX e Radio RX [a0]
      - Camera 2mb [i2c e spi]
      - Relê para controlar 220v [gpio]
      - Magnetronico, Acelerometro e Gyroscopico [i2c]
      - GPS
      - Celular 3g
      - Receive time rtc from WWV/DCF radio signal
          - WWVB is a time signal radio station near Fort Collins, Colorado
          - WWV is a shortwave (also known as "high frequency" (HF)) radio station, located near Fort Collins, Colorado.
          - DCF77 is a German longwave time signal and standard-frequency radio station.

  FUTURE WORK:
  - Enviar json no body com auth token no header
  - Implementar lib crypto no nodemcu
  - Importar token de auth do arquivo sem subir para github
  - Remover PT from comments and strings
*/

// LED BUILT IN DO NODEMCU
#define BUILT_IN_LED D4

// ADS1115 A0 EXPANDER (ANALOG TO DIGITAL (0-3.3V to 0-65536))
#include <Adafruit_ADS1015.h>
Adafruit_ADS1115 ads; // 0-65536 graduacoes  (16-bit version)

// MQ-7 CO and MQ-5 GLP,GN sensor
// inside readADS

// Tiny RTC
#include "RTClib.h" // RTC library
// initialize RTC library
RTC_DS1307 rtc;
// arduino time library
DateTime now;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/*
   TimeLibrary Old: https://playground.arduino.cc/Code/DateTime/
	byte Hour;
	byte Minute;
	byte Second;
	byte Day;
	byte DayofWeek; // Sunday is day 0
	byte Month;     // Jan is month 0
	byte Year;      // the Year minus 1900

	TimeLibrary versao 2: https://playground.arduino.cc/Code/Time/
	hour(); // The hour now (0-23)
	minute(); // The minute now (0-59)
	second(); // The second now (0-59)
	day(); // The day now (1-31)
	weekday(); // Day of the week (1-7), Sunday is day 1
	month(); // The month now (1-12)
	year(); // The full four digit year: (2009,2010 etc)

	hourFormat12(); // The hour now in 12 hour format
	isAM(); // Returns true if time now is AM
	isPM(); // Returns true if time now is PM
	now(); // Returns the current time as seconds since midnight Jan 1 197
*/

// LED RGB
#define RGB_R D5
#define RGB_G D6
#define RGB_B D7
enum COLORID
{
	RED,
	GREEN,
	BLUE,
	MAGENTA,
	CYAN,
	YELLOW,
	WHITE
};
int COLORRGB[7][3] = {
	{1, 0, 0}, // red
	{0, 1, 0}, // green
	{0, 0, 1}, // blue
	{1, 0, 1}, // magenta
	{0, 1, 1}, // cyan
	{1, 1, 0}, // yellow
	{1, 1, 1} // white
};

/*
  // bussola hmc5883L GY-273
  #include <Adafruit_HMC5883_U.h>
  #include <Adafruit_Sensor.h>
  #include <Wire.h>
  // Assign a unique ID to this sensor at the same time
  Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
*/
// bussola QMC5883 (CHINA) GY-273
#include <QMC5883LCompass.h>
QMC5883LCompass compass;

// BME280 tripe sensor E BMP280 double
#define SEALEVELPRESSURE_HPA (1013.25)
#include <Adafruit_Sensor.h>
// BME280 triple sensor
#include <Adafruit_BME280.h>
#define BME280_I2C_ADDR 0x76 // bme can use 0x76 and some bme uses 0x77
Adafruit_BME280 bme;
// BMP280 double sensor
#include <Adafruit_BMP280.h>
#define BMP280_I2C_ADDR 0x77 // default is 0x77 para bmp
Adafruit_BMP280 bmp;
struct BME_VALUES
{
	double temp;
	double humi;
	double pres;
	double alti;
};

// Luminosity GY-30 BH1750 (gnd vcc sda scl ad0)
#include <BH1750.h>
BH1750 lightMeter;

// NODEMCU WIFI CONNECTION
#include <ESP8266WiFi.h>
// String WIFI_SS_ID = "Árvore 6";
String WIFI_SS_ID = "Amarelo C6";
String WIFI_SS_PASSWORD = "1122334455";

// NODEMCU REMOTE HTTP SERVER
String SERVER_HOST_URL = "www.mauriciodz.com";
int SERVER_PORT = 33000;

// data wrapper
struct sensor_values_t
{
	// wifi
	bool isWifiOk = false;
	bool isServerOk = false;
	bool isSensorOk1 = false; // bme
	bool isSensorOk2 = false; // bmp
	bool isSensorOk3 = false; // lux
	bool isSensorOk4 = false; // mag
	bool isSensorOk5 = false; // tiny rtc
	bool isSensorOk6 = false; // a0 hub com mq5 no ad0 e mq7 no ad1

	// bme280
	double temp = -1.0;
	double humi = -1.0;
	double pres = -1.0;
	double alti = -1.0;

	// bmp280
	double temp2 = -1.0;
	double pres2 = -1.0;
	double alti2 = -1.0;

	// gy-30
	double lux = -1.0;

	// mag
	int mag_x = -1;
	int mag_y = -1;
	int mag_z = -1;
	int mag_azimuth = -1; // int degree
	int mag_bearing = -1; // 0-15
	char mag_direction[4] = "a\n"; // N S E W ...

	// tiny rtc
	int year = -1;
	int mes = -1;
	int day = -1;
	int hour = -1;
	int minute = -1;
	int second = -1;
	int dayofweek = -1;
	long secondsSince1970 = -1;
	long daysSince1970 = -1;

	// a0 expansor (mq5 e mq7)
	int mq5 = -1; // ad0 raw 0-65535
	double mq5ppm = -1.0;
	int mq7 = -1; // ad0 raw 0-65535
	double mq7ppm = -1.0;
};

void setup()
{
	setupSerialMonitor();
	setupWifi();
	setupBuiltInLed();
	setupRGB();
	setupRTC();
	setupLumi();
	setupBME280();
	setupBMP280();
	// setupBussola(); // comprei esse, mas veio uma verso da china, o QMC
	setupBussolaQMC();
	setupADS();
}

void loop()
{
	struct sensor_values_t sensor_values;

	// blink blue 1s once to tell we are starting the loop
	blinkRGB(BLUE, 2000, 1); // (color, duration in ms, quantity)

	// rainbow();
	// printScannerI2C();
	blinkBuiltInLed();
	blinkRGB(CYAN, 500, 1);
	printBME280(&sensor_values);
	blinkRGB(CYAN, 500, 1);
	printBMP280(&sensor_values);
	blinkRGB(CYAN, 500, 1);
	printLumi(&sensor_values);
	blinkRGB(CYAN, 500, 1);
	printBussolaQMC(&sensor_values);
	blinkRGB(CYAN, 500, 1);
	printRTCstring(&sensor_values);
	blinkRGB(CYAN, 500, 1);
	readADS(&sensor_values); // no expansor de a0 tem o mq5 e mq7 conectado
	blinkRGB(CYAN, 500, 1);

	// blink red 500ms twice to tell some sensor went wrong
	// blink green 500ms twice to tell all sensor are good
	blinkRGB(GREEN, 2000, 1);

	// blink white 1s once to tell we are going to send to server
	blinkRGB(WHITE, 2000, 1);
	int r = sendValuesToServer(&sensor_values);
	if(r < 0)
	{
		blinkRGB(RED, 10000, 1);
		hardResetNodemcu();
	}
	else
	{
		blinkRGB(GREEN, 2000, 1);
	}
}

void printRTCstring(struct sensor_values_t* ptr_sensor_values)
{
	DateTime now = rtc.now();

	int year = (int)now.year();
	int mes = (int)now.month();
	int day = (int)now.day();
	int hour = (int)now.hour();
	int minute = (int)now.minute();
	int second = (int)now.second();
	int dayofweek = (int)now.dayOfTheWeek();
	long secondsSince1970 = now.unixtime();
	long daysSince1970 = now.unixtime() / 86400L;

	ptr_sensor_values->year = year;
	ptr_sensor_values->mes = mes;
	ptr_sensor_values->day = day;
	ptr_sensor_values->hour = hour;
	ptr_sensor_values->minute = minute;
	ptr_sensor_values->second = second;
	ptr_sensor_values->dayofweek = dayofweek;
	ptr_sensor_values->secondsSince1970 = secondsSince1970;
	ptr_sensor_values->daysSince1970 = daysSince1970;

	// Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
	// DateTime future (now + TimeSpan(1, 12, 30, 6)); // Compute date now +
	// (1day, 12h, 30min, 6s) Serial.print(future.year(), DEC);
}

void setupRTC()
{
	if(!rtc.begin())
	{
		Serial.println("Couldn't find RTC");
		Serial.flush();
		// abort();
	}
	else
	{
		Serial.println("RTC is OK");
	}

	if(!rtc.isrunning())
	{
		Serial.println("RTC is NOT running, let's set the time!");
		// When time needs to be set on a new device, or after a power loss, the
		// following line sets the RTC to the date & time this sketch was compiled
		// rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		// This line sets the RTC with an explicit date & time, for example to set
		// January 21, 2014 at 3am you would call:
		// rtc.adjust(DateTime(2020, 8, 8, 1, 52, 0));
	}
	else
	{
		Serial.println("RTC is running!");
	}
	// rtc.adjust(DateTime(2020, 8, 8, 1, 52, 0)); // coloquei UTC time
}

void rainbow()
{
	for(int i = 0; i < 7; i++)
	{
		blinkRGB((enum COLORID)i, 1000, 1);
	}
}

void readADS(struct sensor_values_t* ptr_sensor_values)
{
	// read ADC
	int16_t adc0, adc1, adc2, adc3;
	adc0 = ads.readADC_SingleEnded(0); // mq5 (glp, gn)
	adc1 = ads.readADC_SingleEnded(1); // mq7 (co)
	adc2 = ads.readADC_SingleEnded(2); // not used, grounded
	adc3 = ads.readADC_SingleEnded(3); // not used, grounded

	ptr_sensor_values->mq5 = adc0;
	ptr_sensor_values->mq7 = adc1;

	// mq7 reader
	double coeficienteA = 19.32; // datasheet
	double coeficienteB = -0.64; // datasheet
	double v_in = 5.0;
	double maxRangeA0 = 65535.0;
	double v_out = adc1 * (v_in / maxRangeA0);
	double ratio = (v_in - v_out) / v_out;
	double ppm = (coeficienteA * pow(ratio, coeficienteB));
	Serial.print("MQ7 (ppm): ");
	Serial.println(ppm);

	ptr_sensor_values->mq7ppm = ppm;

	Serial.print("AIN0 mq5 (GN e GLP): ");
	Serial.println(adc0);
	Serial.print("AIN1 mq7 (CO): ");
	Serial.println(adc1);
	Serial.print("AIN2: ");
	Serial.println(adc2);
	Serial.print("AIN3: ");
	Serial.println(adc3);
	Serial.println(" ");
}

void setupADS()
{
	ads.begin();
}

void printScannerI2C()
{
	byte error, address;
	int nDevices;

	Serial.println("Scanning...");
	nDevices = 0;
	for(address = 1; address < 127; address++)
	{
		Wire.beginTransmission(address);
		error = Wire.endTransmission();

		if(error == 0)
		{
			Serial.print("I2C device found at address 0x");

			if(address < 16)
				Serial.print("0");

			Serial.print(address, HEX);
			Serial.println(" !");

			nDevices++;
		}

		else if(error == 4)
		{
			Serial.print("Unknow error at address 0x");
			if(address < 16)
				Serial.print("0");
			Serial.println(address, HEX);
		}
	}

	if(nDevices == 0)
		Serial.println("-- NO i2C FOUND --\n");
	else
		Serial.println("DONE\n");

	delay(5000);
}

void setupBussolaQMC()
{
	compass.init();
}

void printBussolaQMC(struct sensor_values_t* ptr_sensor_values)
{
	compass.read();
	int x = compass.getX();
	int y = compass.getY();
	int z = compass.getZ();
	int azimuth = compass.getAzimuth();
	int bearing = (int)compass.getBearing(azimuth); // byte to int
	char ptr_mag_direction[4];
	ptr_mag_direction[3] = '\0';
	compass.getDirection(ptr_mag_direction, azimuth);

	ptr_sensor_values->mag_x = x;
	ptr_sensor_values->mag_y = y;
	ptr_sensor_values->mag_z = z;
	ptr_sensor_values->mag_azimuth = azimuth;
	ptr_sensor_values->mag_bearing = bearing;
	ptr_sensor_values->mag_direction[0] = ptr_mag_direction[0];
	ptr_sensor_values->mag_direction[1] = ptr_mag_direction[1];
	ptr_sensor_values->mag_direction[2] = ptr_mag_direction[2];
	ptr_sensor_values->mag_direction[3] = '\0';

	Serial.print("X: ");
	Serial.print(x);
	Serial.print("  ");
	Serial.print("Y: ");
	Serial.print(y);
	Serial.print("  ");
	Serial.print("Z: ");
	Serial.print(z);
	Serial.print("  ");
	Serial.println("uT");

	Serial.print("AZIMUTH: ");
	Serial.print(azimuth);
	Serial.print("  ");
	Serial.print("BEARING: ");
	Serial.print(bearing);
	Serial.print("  ");
	Serial.print("DIRECTION: ");
	Serial.print(ptr_mag_direction);
	Serial.println("");

	struct _event
	{
		struct _magnetic
		{
			int x;
			int y;
			int z;
		} magnetic;
	};

	struct _event event = {{x, y, z}};

	Serial.print("X: ");
	Serial.print(event.magnetic.x);
	Serial.print("  ");
	Serial.print("Y: ");
	Serial.print(event.magnetic.y);
	Serial.print("  ");
	Serial.print("Z: ");
	Serial.print(event.magnetic.z);
	Serial.print("  ");
	Serial.println("uT");
}

void printBussola()
{
	/*
    sensors_event_t event;
    mag.getEvent(&event);

    // RAW VALUES
    Serial.print("X: "); Serial.print(event.magnetic.x); Serial.print("  ");
    Serial.print("Y: "); Serial.print(event.magnetic.y); Serial.print("  ");
    Serial.print("Z: "); Serial.print(event.magnetic.z); Serial.print("  ");
    Serial.println("uT");

    // HEADING
    // Verifique a declinacao magnetica da tua cidade:
    http://www.magnetic-declination.com/
    // 18*36'W => -18.6* => -0.32rads
    // O norte magnetico em aru esta 18 graus para
    float heading = atan2(event.magnetic.y, event.magnetic.x);
    float declinationAngle = -0.3246;
    heading += declinationAngle;

    // Correct for when signs are reversed.
    if (heading < 0)
    heading += 2 * PI;

    // Check for wrap due to addition of declination.
    if (heading > 2 * PI)
    heading -= 2 * PI;

    // Convert radians to degrees for readability.
    float headingDegrees = heading * 180 / M_PI;

    Serial.print("Heading (degrees): "); Serial.println(headingDegrees);
  */
}

void printLumi(struct sensor_values_t* ptr_sensor_values)
{
	float lux = lightMeter.readLightLevel();
	ptr_sensor_values->lux = lux;
	Serial.print("Light: ");
	Serial.print(lux);
	Serial.println(" lx");
}

void setupBussola()
{
	/*
    if (!mag.begin())
    {
    Serial.println("Ooops, no HMC5883 detected ... Check your wiring!");
    while (1);
    }
    // Display some basic information on this sensor
    displaySensorDetails();
  */
}

void displaySensorDetails()
{
	/*
    sensor_t sensor;
    mag.getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.print  ("Sensor:       "); Serial.println(sensor.name);
    Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print  ("Max Value:    "); Serial.print(sensor.max_value);
    Serial.println(" uT"); Serial.print  ("Min Value:    ");
    Serial.print(sensor.min_value); Serial.println(" uT"); Serial.print
    ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" uT");
    Serial.println("------------------------------------");
    Serial.println("");
    delay(500);
  */
}

void blinkRGB(enum COLORID color, int duration, int quantity)
{
	if(quantity < 1)
		return;
	if(duration > 5000)
		return;
	if(duration < 50)
		return;

	int* RGB = COLORRGB[color];

	/*
    Serial.println("blinkRGB:");
    Serial.print(" R:");Serial.println(RGB[0]);
    Serial.print(" G:");Serial.println(RGB[1]);
    Serial.print(" B:");Serial.println(RGB[2]);
    Serial.print(" Duration:");Serial.println(duration);
    Serial.print(" Quantity:");Serial.println(quantity);
    Serial.print(" color:");Serial.println(color);
  */

	for(int i = 0; i < quantity; i++)
	{
		// turn on
		digitalWrite(RGB_R, RGB[0]);
		digitalWrite(RGB_G, RGB[1]);
		digitalWrite(RGB_B, RGB[2]);
		delay(duration);

		// turn off
		digitalWrite(RGB_R, LOW);
		digitalWrite(RGB_G, LOW);
		digitalWrite(RGB_B, LOW);
		delay(duration);
	}
}

void setupRGB()
{
	pinMode(RGB_R, OUTPUT);
	pinMode(RGB_G, OUTPUT);
	pinMode(RGB_B, OUTPUT);
}

void setupLumi()
{
	lightMeter.begin();
	Serial.println(F("BH1750 Test"));
}

void printBME280(struct sensor_values_t* ptr_sensor_values)
{
	struct BME_VALUES* ptrBmeValues = (struct BME_VALUES*)malloc(sizeof(struct BME_VALUES));
	readBME280(ptrBmeValues);

	ptr_sensor_values->temp = ptrBmeValues->temp;
	ptr_sensor_values->humi = ptrBmeValues->humi;
	ptr_sensor_values->pres = ptrBmeValues->pres;
	ptr_sensor_values->alti = ptrBmeValues->alti;

	Serial.println("******* BME: ");
	Serial.print("Temp: ");
	Serial.print(ptrBmeValues->temp);
	Serial.println(" *C");

	Serial.print("Humi: ");
	Serial.print(ptrBmeValues->humi);
	Serial.println(" %");

	Serial.print("Pres: ");
	Serial.print(ptrBmeValues->pres);
	Serial.println(" hPa");

	Serial.print("Alti: ");
	Serial.print(ptrBmeValues->alti);
	Serial.println(" m");

	free(ptrBmeValues);
}

void printBMP280(struct sensor_values_t* ptr_sensor_values)
{
	struct BME_VALUES* ptrBmeValues = (struct BME_VALUES*)malloc(sizeof(struct BME_VALUES));
	readBMP280(ptrBmeValues);

	ptr_sensor_values->temp2 = ptrBmeValues->temp;
	ptr_sensor_values->pres2 = ptrBmeValues->pres;
	ptr_sensor_values->alti2 = ptrBmeValues->alti;

	Serial.println("******* BMP: ");
	Serial.print("Temp: ");
	Serial.print(ptrBmeValues->temp);
	Serial.println(" *C");

	Serial.print("Humi: ");
	Serial.print(ptrBmeValues->humi);
	Serial.println(" %");

	Serial.print("Pres: ");
	Serial.print(ptrBmeValues->pres);
	Serial.println(" hPa");

	Serial.print("Alti: ");
	Serial.print(ptrBmeValues->alti);
	Serial.println(" m");

	free(ptrBmeValues);
}

void readBMP280(struct BME_VALUES* ptrBmpValues)
{
	ptrBmpValues->temp = bmp.readTemperature(); // C
	// ptrBmpValues->humi = bmp.readHumidity();
	ptrBmpValues->humi = -1.0;
	ptrBmpValues->pres = bmp.readPressure(); // pa
	ptrBmpValues->alti = bmp.readAltitude(SEALEVELPRESSURE_HPA); // m
}

void readBME280(struct BME_VALUES* ptrBmeValues)
{
	ptrBmeValues->temp = bme.readTemperature(); // C
	ptrBmeValues->humi = bme.readHumidity(); // %
	ptrBmeValues->pres = bme.readPressure(); // pa (divide by 100 to get hPa)
	ptrBmeValues->alti = bme.readAltitude(SEALEVELPRESSURE_HPA); // m
}

int setupBME280()
{
	if(!bme.begin(BME280_I2C_ADDR))
	{
		Serial.println("bme.begin() falhou...");
		return -1;
	}
	else
	{
		Serial.println("bme.begin() OK");
		return 0;
	}
}

int setupBMP280()
{
	if(!bmp.begin())
	{
		Serial.println("bmp.begin() falhou...");
		return -1;
	}
	else
	{
		Serial.println("bmp.begin() OK");
		return 0;
	}
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
				"/saveweather?temp=%.2f&humi=%.2f&pres=%.2f&alti=%.2f&lux=%.2f&temp2=%."
				"2f&pres2=%.2f&alti2=%.2f&magx=%d&magy=%d&magz=%d&azimuth=%d&bearing=%"
				"d&direction=%s&mq5=%d&mq5ppm=%.2f&mq7=%d&mq7ppm=%.2f&unixtime=%lu "
				"HTTP/1.0",
				ptr_sensor_values->temp,
				ptr_sensor_values->humi,
				ptr_sensor_values->pres,
				ptr_sensor_values->alti,
				ptr_sensor_values->lux,
				ptr_sensor_values->temp2,
				ptr_sensor_values->pres2,
				ptr_sensor_values->alti2,
				ptr_sensor_values->mag_x,
				ptr_sensor_values->mag_y,
				ptr_sensor_values->mag_z,
				ptr_sensor_values->mag_azimuth,
				ptr_sensor_values->mag_bearing,
				// ptr_sensor_values->mag_direction,
				"AAA",
				ptr_sensor_values->mq5,
				ptr_sensor_values->mq5ppm,
				ptr_sensor_values->mq7,
				ptr_sensor_values->mq7ppm,
				/*
          ptr_sensor_values->year,
          ptr_sensor_values->mes,
          ptr_sensor_values->day,
          ptr_sensor_values->hour,
          ptr_sensor_values->minute,
          ptr_sensor_values->second,
          ptr_sensor_values->dayofweek,
          ptr_sensor_values->daysSince1970
        */
				ptr_sensor_values->secondsSince1970 // &unixtime=%ld // tirei isso e parou de resetar
		);

		Serial.printf("QUERY: %s\n", query);
		Serial.println("-- SENDING QUERY TO SERVER --");

		client.println(query);

		client.println();
		Serial.println("-- QUERY SENT DONE --:");

		while(client.connected())
		{
			String response = client.readStringUntil('\n');
			if(response == "\r")
			{
				Serial.println("RESPONSE HEADER:");
				Serial.println(response);
				break;
			}
		}
		Serial.println("-- HEADER DONE --");

		String response = client.readStringUntil('\n');

		Serial.println("RESPONSE BODY:");
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

void hardResetNodemcu()
{
	// ESP.reset(); // hard reset that can leave some register in the old state
	ESP.restart(); // recomended method to clean everything
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
	Serial.println("Conectado.");
	Serial.print("Meu IP: ");
	Serial.println(WiFi.localIP().toString());
}

void setupSerialMonitor()
{
	Serial.begin(115200);
	Serial.println();
	Serial.println("Hi");
}

void setupBuiltInLed()
{
	pinMode(BUILT_IN_LED, OUTPUT);
	digitalWrite(BUILT_IN_LED, LOW);
}

void blinkBuiltInLed()
{
	digitalWrite(BUILT_IN_LED, LOW);
	delay(1000);
	digitalWrite(BUILT_IN_LED, HIGH);
	delay(1000);
}
