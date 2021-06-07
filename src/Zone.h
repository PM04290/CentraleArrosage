/*
 * 
 */
#ifndef ZONE_H_
#define ZONE_H_

#include <Arduino.h>
#include <RtcDS3231.h>

#define CONFIG_FILE "/config.dat"

#define FSCONFIG_SIZE 0x100

#define MAX_CYCLE_PER_DAY 4

#define HOUR_MN           60
#define DAY_MN            1440

#define OVERLAP_WEEK_TIME 99999

typedef struct cycle
{
  uint8_t startHour;  // 0..23 hr
  uint8_t startMinute;// 0..59 mn
  uint8_t Duration;   // 1..254 mn (0 and 255 reserved for inactive)
  uint8_t reserved;
} Tcycle;

typedef struct config
{
  byte evpin;
  byte runningDays;
  byte moistureMin;
  byte reserved;
  Tcycle cycles[MAX_CYCLE_PER_DAY];
} Tconfig;

class Zone {
  private:
    // config
    byte _evnum;
    byte _evpin;
    byte _runningDays;                // bit 0..6  (eg 5 : Dim+Mar
    byte _moistureMin;                // watering under value
    Tcycle _cycle[MAX_CYCLE_PER_DAY];   
    // schedule data
    bool _forced;
    byte _cycleIdx;                       // 0..MAX_CYCLE_PER_DAY (cycle running)
    int  _stopTime;

  public:
    Zone(byte evnum, byte evpin);
    //
    byte getDays();
    void setDays(byte runningdays);
    void setMoisture(byte moisture);
    byte getMoisture();
    Tcycle getCycle(byte idxcycle);
    void setCycle(byte idxcycle, byte starthour, byte startmin, byte duration);
    void setCycle(Tcycle cycle, byte idxcycle);
    String getCycleString(byte idxcycle);
    byte getNum();
    void saveConfig();
    void loadConfig();
    
    bool isConfigValid();
    byte getEVstate();
    byte getNbCycle();
    void process(const RtcDateTime& dt, const byte moisture);
    void setForced(bool forced);
    bool isForced();
    bool isActive();
};


#endif // ZONE_H_
