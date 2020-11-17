/*
  Device 1
  Take a 2Mb photo + Weather information (temp, humi, press) + lux
  And send both through wifi + propagate weather string via 433MHz radio
  transmitter
  @ Jan 2020
*/

#define LED D4

#define BTO_BUILTIN 0

// SPI
//#include <SPI.h>

// ARDUCAM
// lib at /Users/ize/Documents/Arduino/libraries
// Edit memorysaver.h to uncoment the camera model you have #define
// OV2640_MINI_2MP ArduCAM.h - Arduino library support for CMOS Image Sensor
#include <ArduCAM.h> // i2c and spi

// i2c light sensor BH1750
#include <BH1750FVI.h>
BH1750FVI LightSensor(BH1750FVI::k_DevModeContLowRes);

// CAMERA OV2640
const int CS = D3;
ArduCAM myCAM(OV2640, CS);

// WEATHER BME280
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // i2c

// radio tx 433 MHz
#define TX_ADDRESS D0 // pino do tx
#define RX_ADDRESS D4 // pino do rx // nao usa aqui
#include <RH_ASK.h>   // RadioHead lib para o modulo FS1000a RF433
RH_ASK
radio(2000, RX_ADDRESS,
      TX_ADDRESS); // speed, rx, tx, tx-controller pin, invert controller?
// RH_ASK radio;
// RHReliableDatagram radioReliable(radio, TX_ADDRESS);

// ESP12E WIFI AND HTTP
// Library at
// /Users/ize/Library/Arduino15/packages/esp8266/hardware/esp8266/2.6.3
//#include <WiFiClient.h>
// HTTPClient http; // to create get request to the remote server running nodejs
// lib pro esp conectar no wifi do quarto, WiFi.begin()
#include <ESP8266WiFi.h>
String serverIP = "192.168.0.188";
String MYSSID = "wifi ssd";
String MYSSPASSWORD = "wifi senha";

// 2k SRAM inside uno, so malloc is not enought for 1929 bytes
char *ptr_foto;
size_t tamanho_ptr_foto = 0;

int transferefoto() {
  WiFiClient client;
  if (client.connect("192.168.0.188", 8080)) {
    const size_t bufferSize = 1024;
    size_t bytes_restantes = tamanho_ptr_foto;
    uint8_t buffer[bufferSize] = {0xFF};

    client.print("POST ");
    client.print("/upload");
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println("192.168.0.188");
    client.print("Content-Length: ");
    client.println(tamanho_ptr_foto);
    client.println("Content-Type: image/jpeg");
    client.println("Content-Transfer-Encoding: binary");
    client.println();
    client.write(&ptr_foto[0], tamanho_ptr_foto);
    client.stop();
  }
}

// testing if device can hold a whole photo at one time
int batefotosalvapointer() {
  // bate foto and wait
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
    ;

  // pega tamanho da foto
  const size_t tamanho_foto = myCAM.read_fifo_length();
  Serial.print("Tamanho da foto: ");
  Serial.println(tamanho_foto);
  if (tamanho_foto >= 524280) {
    Serial.print("Imagem muito grande");
    return -1;
  }
  tamanho_ptr_foto = tamanho_foto;

  // aloca
  ptr_foto = (char *)malloc(tamanho_foto);
  if (!ptr_foto) {
    Serial.println("malloc error!");
    return -1;
  }

  // bota camera em modo leitura
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
#if !(defined(ARDUCAM_SHIELD_V2) && defined(OV2640_CAM))
  SPI.transfer(0xFF);
#endif

  // read each byte from cam memory
  SPI.transferBytes((uint8_t *)&ptr_foto[0], (uint8_t *)&ptr_foto[0],
                    tamanho_foto);

  return 0;
}

// read chunk of data from cam memory using spi and send it to server
void f_transfere_foto() {
  WiFiClient client;

  Serial.printf("Tentando conectar at %s para enviar foto post...\n",
                "192.168.0.188");

  if (client.connect("192.168.0.188", 8080)) {
    Serial.println("OK");

    long total_time = millis();

    // bate foto
    myCAM.flush_fifo();
    delayMicroseconds(150);
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
      ;
    Serial.println("CAM Capture Done.");

    // pega tamango da foto
    const size_t tamanho_foto = myCAM.read_fifo_length();
    Serial.print("Tamanho da foto: ");
    Serial.println(tamanho_foto);
    if (tamanho_foto >= 524280) {
      Serial.print("Imagem muito grande");
      return;
    }

    // bota camera em modo leitura
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
#if !(defined(ARDUCAM_SHIELD_V2) && defined(OV2640_CAM))
    SPI.transfer(0xFF);
#endif

    digitalWrite(LED, HIGH);
    delay(50);
    Serial.println("Iniciando transmissao para server...");

    // inicia post request no servidor
    size_t bytes_restantes = tamanho_foto;
    const size_t bufferSize = 1024;
    uint8_t buffer[bufferSize] = {0xFF};
    client.print("POST ");
    client.print("/upload");
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(serverIP);
    client.print("Content-Length: ");
    client.println(tamanho_foto);
    client.println("Content-Type: image/jpeg");
    client.println("Content-Transfer-Encoding: binary");
    client.println();
    // data chuncks
    while (bytes_restantes) {
      // verifica se tem 1024 cheio ou tem que pegar menos dados
      size_t copy_size =
          (bytes_restantes < bufferSize) ? bytes_restantes : bufferSize;

      SPI.transferBytes(&buffer[0], &buffer[0], copy_size);
      // temp =  SPI.transfer(0x00);

      // checa conexao
      if (!client.connected()) {
        Serial.println("Conexao wifi perdida!");
        break;
      }
      client.write(&buffer[0], copy_size);

      bytes_restantes -= copy_size;

      delayMicroseconds(100);
    }

    // finaliza wifi
    client.stop();
    // finaliza leitura da memoria da cam
    myCAM.CS_HIGH();

    Serial.println("Done");
    digitalWrite(LED, LOW);
    delay(50);

    // computa tempo
    total_time = millis() - total_time;
    Serial.print("upload time = ");
    Serial.print(total_time, DEC);
    Serial.println(" ms");

    // tenta ler header da resposta do server
    while (client.connected()) {
      String response = client.readStringUntil('\n');
      if (response == "\r") {
        Serial.println("HEADER:");
        Serial.println(response);
        break;
      }
    }

    // tenta ler body da resposta do server
    String response = client.readStringUntil('\n');
    Serial.println("RESPONSE:");
    Serial.println(response);

  } else {
    Serial.println("ERROR");
  }
}

void setup_camera() {
  uint8_t vid, pid, temp;

  // Inicia camera
  Wire.begin(D2, D1); // Wire.begin(SDA, SCL)
  pinMode(CS, OUTPUT);
  SPI.begin();
  SPI.setFrequency(
      1000000); // 1 MHz instead of 4 MHz to avoid noise no fio de 30 cm

  // Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55) {
    while (1) {
      Serial.println("ERRO SPI INTERFACE");
      delay(3000);
    }
  }
  Serial.println("SPI BUS OK");

  // Check if the camera module type is OV2640
  myCAM.wrSensorReg8_8(0xff, 0x01);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if ((vid != 0x26) && ((pid != 0x41) || (pid != 0x42))) {
    while (1) {
      Serial.println("Can't find OV2640 module!");
      Serial.print("vid: ");
      Serial.println(vid);
      Serial.print("pid: ");
      Serial.println(pid);
      delay(3000);
    }
  }
  Serial.println("OV2640 detected.");

  // Configura CAMERA
  myCAM.set_format(JPEG);
  // myCAM.set_format(RAW);
  myCAM.InitCAM();
  // myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  // myCAM.OV2640_set_JPEG_size(OV2640_160x120);
  // myCAM.OV2640_set_JPEG_size(OV2640_352x288);
  // myCAM.OV2640_set_JPEG_size(OV2640_640x480);
  // myCAM.OV2640_set_JPEG_size(OV2640_1024x768);
  // OV2640_1280x1024
  myCAM.OV2640_set_JPEG_size(OV2640_1600x1200);

  delay(1000);

  myCAM.clear_fifo_flag();
  // myCAM.write_reg(ARDUCHIP_FRAMES, 0x00);
}

// init wifi
void setupWifi() {
  Serial.print("Tentando conectar na rede wifi...");
  WiFi.begin(MYSSID, MYSSPASSWORD); // ssid, password
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Conectado!");
  Serial.print("Meu IP: ");
  Serial.println(WiFi.localIP().toString());
}

// send weather string to server using get request
void sendGetRequest(float temp, float humi, float pres, float alti, float lumi,
                    long diff_time) {
  WiFiClient client;
  Serial.printf("Conectando no server %s...\n", "192.168.0.188:8080");
  if (client.connect("192.168.0.188", 8080)) {
    Serial.println("OK");

    char query[200];
    sprintf(query,
            "GET /save?t=%.2f&h=%.2f&p=%.2f&a=%.2f&l=%.2f&df=%d HTTP/1.0", temp,
            humi, pres, alti, lumi, diff_time);
    Serial.printf("QUERY: %s\n", query);
    client.println(query);
    client.println();

    while (client.connected()) {
      String response = client.readStringUntil('\n');
      if (response == "\r") {
        Serial.println("HEADER:");
        Serial.println(response);
        break;
      }
    }
    String response = client.readStringUntil('\n');
    Serial.println("RESPONSE:");
    Serial.println(response);

  } else {
    Serial.println("ERROR");
  }
}

// propagate the radio string
void propagaRadio(float temp, float humi, float pres, float alti, float lux) {
  Serial.printf("RH_ASK_MAX_MESSAGE_LEN = %d\n", RH_ASK_MAX_MESSAGE_LEN);
  char query[RH_ASK_MAX_MESSAGE_LEN] = {0};
  sprintf(query, "t=%.2f h=%.2f p=%.2f l=%.2f", temp, humi, pres, lux);
  radio.send((uint8_t *)query, strlen((char *)query));
  radio.waitPacketSent();
}

void checkWifi() {
  Serial.print("WiFi.status(): ");
  Serial.println(WiFi.status());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wifi is ok!");
  } else {
    Serial.print("Wifi is off, restarting....");
    // reseta esp
    delay(5000);
    ESP.reset();
    delay(5000);
  }
}

void setup() {
  // botao do nodemcu amica built in
  // pinMode(BTO_BUILTIN, INPUT_PULLUP); // error

  // inicia serial monitor
  Serial.begin(115200); // 115200 em vez de 9600 pq esp gosta de high bandwidth
  Serial.println();
  Serial.println("Hi");

  // inicia radio
  if (!radio.init()) {
    Serial.println("radio setup error");
  } else {
    Serial.println("radio setup ok");
  }

  // inicia arducam
  setup_camera();

  // inicia sensor bme280
  if (!bme.begin(0x76)) {
    Serial.println("bme.begin() falhou...");
    while (1)
      ;
  }

  // inicia ldr sensor
  // pinMode(A0, INPUT);
  LightSensor.begin();

  // inicia led
  pinMode(LED, OUTPUT);

  // inicia wifi
  setupWifi();
}

long start_time = 0, end_time = 0, diff_time = 0, goal_sleep = 0, sum_time = 0;
void loop() {
  // checa wifi
  checkWifi();

  start_time = millis();

  digitalWrite(LED, HIGH);

  // le sensor bme280
  float temp = 0.0f + bme.readTemperature();      // C
  float humi = 0.0f + bme.readHumidity();         // %
  float pres = 0.0f + bme.readPressure();         // 100.0f; // hPa
  float alti = 0.0f + bme.readAltitude(1013.25f); // m
  uint16_t lux = LightSensor.GetLightIntensity(); // lux

  // imprime no serial
  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" *C");
  Serial.print("Humidity = ");
  Serial.print(humi);
  Serial.println(" %");
  Serial.print("Pressure = ");
  Serial.print(pres);
  Serial.println(" hPa");
  Serial.print("Approx altitude = ");
  Serial.print(alti);
  Serial.println(" m");
  Serial.print("LDR resistor = ");
  Serial.print(lux);
  Serial.println(" lux");
  Serial.println();

  // propaga string via radio 433
  propagaRadio(temp, humi, pres, alti, lux);

  // envia dados do tempo
  sendGetRequest(temp, humi, pres, alti, lux, diff_time);

  // bate e envia foto
  f_transfere_foto();

  digitalWrite(LED, LOW);

  // dorme por 30s enviando radio
  end_time = millis();
  diff_time = end_time - start_time;
  goal_sleep = 30000 - diff_time;
  for (;;) {
    long xx_start = millis();

    // work
    digitalWrite(LED, HIGH);
    propagaRadio(temp, humi, pres, alti, lux);
    delay(250);
    digitalWrite(LED, LOW);
    delay(250);

    long xx_end = millis();
    long work_time = xx_end - xx_start;

    // check
    sum_time += work_time;
    if (sum_time > goal_sleep) {
      sum_time = 0;
      break;
    }
  }
}

bool jaBateu = false;
void loop2() {
  checkWifi();

  if (jaBateu == false) {
    Serial.println("jaBateu is false");
    int ret = batefotosalvapointer();
    jaBateu = true;
  } else {
    Serial.println("jaBateu is true");
    transferefoto();
  }

  delay(3000);
}
