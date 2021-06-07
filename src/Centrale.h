/*
 * 
 */
#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <Arduino.h>
#include <RtcDS3231.h>
#include "Zone.h"

typedef union FSconfig
{
    struct {
      char SSID[0x20];
      char PWD[0x20];
      char mqttserver[0x20];
      char mqttuser[0x20];
      char mqttpass[0x20];
      byte moisturemode;
      byte b1;
      byte b2;
      byte b3;
      uint16_t i0;
      uint16_t i1;
      uint32_t l0;
      uint32_t l1;
      byte reserved[0x10];
    } data;
    byte bloc[FSCONFIG_SIZE];
} TFSconfig;

// max 32
#define MAX_NB_CHANNEL 32
#define NB_CHANNEL      8

class Centrale {
  public:
    Centrale();
    void checkConfigFile(bool forcerewrite);
    void saveConfigFile();
    void addZone(byte z, byte evpin);
    Zone* getZone(byte z);
    void setZoneForced(byte zone, bool forced);
    byte getNbZone();
    bool isMQTT();
    bool isMoistureChirp();
    bool isMoistureMQTT();
    //
    void process(const RtcDateTime& dt, byte curMoisture);
  private:
    // config
    byte _nbZone;
    Zone** _PtrZones;
};

#endif // SCHEDULER_H_
