/*

*/
#include <Arduino.h>
#include <FS.h>
#include "Zone.h"
#include "Centrale.h"
#ifdef ESP32
#include "SPIFFS.h"
#endif

TFSconfig FSconfig;

Centrale::Centrale() {
  _nbZone = 0;
  _PtrZones = nullptr;
}

void Centrale::checkConfigFile(bool forcerewrite) {
  Tconfig cnf;

  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file || forcerewrite) {
    file = SPIFFS.open(CONFIG_FILE, "w");
    if (file) {
      memset(&FSconfig, 0, sizeof(FSconfig));
      strcpy(FSconfig.data.SSID,"Freebox-8B7BFF");
      strcpy(FSconfig.data.PWD,"laclewpadeJOJO04290");
      file.write((byte*)&FSconfig, sizeof(FSconfig));

      memset(&cnf, 0xFF, sizeof(cnf));
      // config file prepared for 32 channels max
      for (byte n = 0; n < MAX_NB_CHANNEL; n++) {
        int nn = file.write((byte*)&cnf, sizeof(cnf));
      }
      file.close();
      Serial.println(F("Création fichier de config"));
    } else {
      Serial.print(CONFIG_FILE);
      Serial.println(F(" : Erreur de création"));
    }
  } else {
    file.read((byte*)&FSconfig, sizeof(FSconfig));
    file.close();
  }
}

void Centrale::saveConfigFile() {
  File file = SPIFFS.open(CONFIG_FILE, "r+");
  if (file) {
    file.write((byte*)&FSconfig, sizeof(FSconfig));
    file.close();
    Serial.println(F("Fichier de config enregistré"));
  } else {
    Serial.println(F("Erreur sauvegarde config"));
  }
}

void Centrale::addZone(byte z, byte evpin) {
  if (_nbZone >= NB_CHANNEL)
    return;
  _PtrZones = (Zone**) realloc(_PtrZones, (_nbZone + 1) * sizeof(Zone*));
  _PtrZones[_nbZone] = new Zone(z, evpin);
  _PtrZones[_nbZone]->loadConfig();
  _nbZone++;
}

void Centrale::setZoneForced(byte zone, bool forced) {
  if (zone < NB_CHANNEL) {
    _PtrZones[zone]->setForced(forced);
  }
}

void Centrale::process(const RtcDateTime& dt, byte curMoisture) {
  for (byte z = 0; z < NB_CHANNEL; z++) {
    _PtrZones[z]->process(dt, curMoisture);
  }
}

Zone* Centrale::getZone(byte z) {
  return _PtrZones[z % _nbZone];
}

byte Centrale::getNbZone() {
  return _nbZone;
}

bool Centrale::isMQTT() {
  return strlen(FSconfig.data.mqttserver) > 0;
}

bool Centrale::isMoistureChirp() {
  return (FSconfig.data.moisturemode == 1);
}

bool Centrale::isMoistureMQTT() {
  return (FSconfig.data.moisturemode == 2);
}
