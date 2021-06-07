/*
  Chirp Pinout
              RST   SCL
    [BTN]      o     o     o      \
                                   >
               o     o     o      /
              GND   SDA   VCC


  https://github.com/Miceuz/PlantWateringAlarm
*/
#ifndef CHIRP_H_
#define CHIRP_H_

#include <Arduino.h>
#include <Wire.h>

#ifdef ESP32
const byte CHRIPPin_RST  = 17;
#endif
#ifdef ESP8266
const byte CHRIPPin_RST  = 3;
#endif

void ChirpSetup() {
  // reset module to 
  digitalWrite(CHRIPPin_RST, LOW);
  pinMode(CHRIPPin_RST, OUTPUT);  
  delay(1);
  digitalWrite(CHRIPPin_RST, HIGH);
  delay(50);
}

void writeI2CRegister8bit(int addr, int value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  Wire.endTransmission();
}

unsigned int readI2CRegister16bit(int addr, int reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission();
  delay(20);
  Wire.requestFrom(addr, 2);
  unsigned int t = Wire.read() << 8;
  t = t | Wire.read();
  return t;
}

int ChirpReadMoisture() {
  int capa = readI2CRegister16bit(0x20, 0);
  Serial.println("hum:" + String(capa));
  // scale mapping
  int capamap = map(capa,270,520,0,100);
  return capamap;
}


int ChirpReadTemperature() {
  int temp = readI2CRegister16bit(0x20, 5);
  Serial.println("temp:" + String(temp));
  // scale mapping
  int tempmap = temp / 180;
  return tempmap;
}

int ChirpReadLight() {
  writeI2CRegister8bit(0x20, 3);//request light measurement 
  delay(100); // if delay is too long the module freeze
  int light = readI2CRegister16bit(0x20, 4);
  Serial.println("light:" + String(light));
  // scale mapping
  int lightmap = map(light,7200,0,0,100);
  return lightmap;
}

#endif // CHIRP_H_
