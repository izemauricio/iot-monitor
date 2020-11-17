/*
  Device 3
  Arduino + LCD + RadioRX
  Receive weather data from 433MHz radio and print it at 16x2 lcd
  @ Jan 2020
*/

#include <LiquidCrystal.h>
LiquidCrystal lcd(2, 3, 4, 5, 6, 7); // RS, EN, d4, d5, d6 e d7

#include <RH_ASK.h>
RH_ASK driver(2000, 8,
              9); // speed, rx, tx, tx-controller pin, invert controller

void setup() {
  Serial.begin(9600);
  Serial.println("Serial monitor ok");

  // radio
  if (!driver.init()) {
    Serial.println("radio failed");
  } else {
    Serial.println("radio ok");
  }

  // lcd
  lcd.begin(16, 2);
}

long radio_counter = 0;
long radio_error_counter = 0;
uint8_t buf[RH_ASK_MAX_MESSAGE_LEN] = {0};
uint8_t buflen = sizeof(buf);

void loop() {
  Serial.print(".");

  // radio rx
  // after checksum ok
  // count number of received msg or errors
  if (driver.recv(buf, &buflen)) // Non-blocking
  {
    int i;
    Serial.print("Got:(");
    Serial.print((char *)buf);
    Serial.println(").");
    radio_counter++;
  } else {
    radio_error_counter++;
  }

  // static build the lcd lines from the radio char array
  // t=26.71 h=45.42 p=1013.09 l=13.00
  char line1[17] = {0}, line2[17] = {0};
  if (buf[0] == 't') {
    // t
    line1[0] = buf[2];
    line1[1] = buf[3];
    line1[2] = buf[4];
    line1[3] = buf[5];
    line1[4] = buf[6];
    line1[5] = ' ';
    // h
    line1[6] = buf[10];
    line1[7] = buf[11];
    line1[8] = buf[12];
    line1[9] = buf[13];
    line1[10] = buf[14];
    // counter at end
    sprintf(line2, "        %d", radio_counter);
    // p
    line2[0] = buf[18];
    line2[1] = buf[19];
    line2[2] = buf[20];
    line2[3] = buf[21];
    line2[4] = buf[22];
    line2[5] = buf[23];
    line2[6] = buf[24];
  } else {
    sprintf(line1, "E=%d", radio_error_counter);
    sprintf(line2, "A=%d", radio_counter);
  }
  line1[16] = 0;
  line2[16] = 0;

  // lcd
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);

  // sleep
  delay(1000);
}
