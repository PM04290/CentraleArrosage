/*

*/
#include <Arduino.h>
#include <FS.h>
#include "Zone.h"
#ifdef ESP32
#include "SPIFFS.h"
#endif

Zone::Zone(byte evnum, byte evpin)
  : _evnum(evnum),
    _evpin(evpin) {
  pinMode(evpin, OUTPUT);
  digitalWrite(evpin, LOW);
  _cycleIdx = -1;
  _stopTime = -1;
  _forced = false;
  _runningDays = 0xFF;
  _moistureMin = 0xFF;
  for (byte n = 0; n < MAX_CYCLE_PER_DAY; n++) {
    _cycle[n].startHour = 20;
    _cycle[n].startMinute = 0;
    _cycle[n].Duration = 0;
  }
}

byte Zone::getDays() {
  return _runningDays;
}

void Zone::setDays(byte runningdays) {
  _runningDays = runningdays;
}

void Zone::setMoisture(byte moisture) {
  _moistureMin = moisture;
}

byte Zone::getMoisture() {
  return _moistureMin;
}

void Zone::setCycle(byte idxcycle, byte starthour, byte startmin, byte duration) {
  if (idxcycle < MAX_CYCLE_PER_DAY) {
    _cycle[idxcycle].startHour = starthour;
    _cycle[idxcycle].startMinute = startmin;
    _cycle[idxcycle].Duration = duration;
  }
}

void Zone::setCycle(Tcycle cycle, byte idxcycle) {
  if (idxcycle < MAX_CYCLE_PER_DAY) {
    _cycle[idxcycle] = cycle;
  }
}

Tcycle Zone::getCycle(byte idxcycle) {
  return _cycle[idxcycle];
}

String Zone::getCycleString(byte idxcycle) {
  char buf[10];
  char s[] = "off";
  if (_cycle[idxcycle].Duration > 0 && _cycle[idxcycle].Duration < 0xFF) {
    sprintf(s, "%d", _cycle[idxcycle].Duration);
  }
  if (_cycle[idxcycle].startHour < 24 && _cycle[idxcycle].startMinute < 60) {
    sprintf(buf, "%2d:%02d=%s", _cycle[idxcycle].startHour, _cycle[idxcycle].startMinute, s);
  } else {
    sprintf(buf, "--:--=%s", s);
  }
  return String(buf);
}

byte Zone::getNum() {
  return _evnum;
}

void Zone::saveConfig() {
  int idx = FSCONFIG_SIZE + ((_evnum - 1) * 0x20);
  Tconfig cnf;

  cnf.evpin = _evpin;
  cnf.runningDays = _runningDays;
  cnf.moistureMin = _moistureMin;
  cnf.reserved = 0xFF;
  for (byte n = 0; n < MAX_CYCLE_PER_DAY; n++) {
    cnf.cycles[n] = _cycle[n];
  }

  File file = SPIFFS.open(CONFIG_FILE, "r+");
  if (file) {
    file.seek(idx, SeekSet);
    file.write((byte*)&cnf, sizeof(cnf));
    file.close();
  }
}

void Zone::loadConfig() {
  int idx = FSCONFIG_SIZE + ((_evnum - 1) * 0x20);
  Tconfig cnf;

  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (file) {
    file.seek(idx, SeekSet);
    file.read((byte*)&cnf, sizeof(cnf));
    file.close();

    if (cnf.evpin != 0xFF) {
      //_evpin = cnf.evpin;
      _runningDays = cnf.runningDays;
      _moistureMin = cnf.moistureMin;
      for (byte n = 0; n < MAX_CYCLE_PER_DAY; n++) {
        _cycle[n] = cnf.cycles[n];
      }
    }
  }

  _stopTime = -1;
  _forced = false;
}

bool Zone::isConfigValid() {
  return (_evpin != 0xFF) && (_runningDays > 0x00) && (_runningDays < 0x80) && (getNbCycle() > 0);
}

byte Zone::getEVstate() {
  return digitalRead(_evpin);
}

byte Zone::getNbCycle() {
  byte nb = 0;
  for (byte n = 0; n < MAX_CYCLE_PER_DAY; n++) {
    if ((_cycle[n].Duration > 0) && (_cycle[n].Duration <= 240)) {
      nb++;
    }
  }
  return nb;
}

void Zone::process(const RtcDateTime& dt, const byte moisture) {
  if (_forced) {
    return;
  }
  if (isConfigValid() == false) {
    return;
  }
  //Serial.println("EV process");
  byte today = dt.DayOfWeek();
  // detect end of cycle
  if (_stopTime >= 0) {
    if (((dt.Hour() * HOUR_MN) + dt.Minute()) == _stopTime) {
      _stopTime = -1;
      digitalWrite(_evpin, LOW);
      Serial.print(F("Stop EV "));
      Serial.println(_evnum);
    }
  }
  // detect new cycle to start if free
  if (_stopTime < 0) {
    if ((moisture < _moistureMin) && (_runningDays & (1 << today))) { // is day to run ?
      for (byte c = 0; c < MAX_CYCLE_PER_DAY && _stopTime < 0; c++) {
         // is it time to run ?
        if (_cycle[c].Duration > 0
         && _cycle[c].startHour == dt.Hour()
         && _cycle[c].startMinute == dt.Minute()) {
          _stopTime = (dt.Hour() * HOUR_MN) + dt.Minute() + _cycle[c].Duration;
          if (_stopTime >= DAY_MN) {
            _stopTime -= DAY_MN; // 24*60 day overlap
          }
          // run EV
          digitalWrite(_evpin, HIGH);
          Serial.print(F("Start EV "));
          Serial.println(_evnum);
        }
      }
    }
  }
}

void Zone::setForced(bool forced) {
  _forced = forced;
  if (forced) {
    _stopTime = -1;
    Serial.print(F("Start EV "));
    digitalWrite(_evpin, HIGH);
  } else {
    Serial.print(F("Stop EV "));
    digitalWrite(_evpin, LOW);
  }
  Serial.println(_evnum);
}

bool Zone::isForced() {
  return _forced;
}

bool Zone::isActive() {
  return _forced || (_stopTime > 0);
}
