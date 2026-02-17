#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "EIBI.h"

// CB frequency range
#define MIN_CB_FREQUENCY 26060
#define MAX_CB_FREQUENCY 27995

//
// Named frequencies, sorted by increasing frequency!
//
static const NamedFreq namedFrequencies[] =
{
  {  1840, "FT8"  }, {  3573, "FT8"  }, {  5357, "FT8"  }, {  7074, "FT8" },
  {  7165, "SSTV" }, {  7171, "SSTV" }, { 10136, "FT8"  }, { 14074, "FT8" },
  { 14230, "SSTV" }, { 18100, "FT8"  }, { 21074, "FT8"  }, { 24915, "FT8" },
  { 27700, "SSTV" }, { 28074, "FT8"  }, { 28680, "SSTV" },
};

//
// CB channel mappings
//
static const char *cbChannelNumber[] =
{
  "1",  "2",  "3",  "41",
  "4",  "5",  "6",  "7",  "42",
  "8",  "9",  "10", "11", "43",
  "12", "13", "14", "15", "44",
  "16", "17", "18", "19", "45",
  "20", "21", "22", "23",
  "24", "25", "26", "27",
  "28", "29", "30", "31",
  "32", "33", "34", "35",
  "36", "37", "38", "39",
  "40",
};

//
// RDS program types
//
const char *rdsProgramTypes[32] =
{
  0, "News", "Current Affairs", "Information",
  "Sport", "Education", "Drama", "Culture",
  "Science", "Varied", "Pop Music", "Rock Music",
  "Easy Listening", "Light Classical", "Serious Classical", "Other Music",
  "Weather", "Finance", "Children's Program", "Social Affairs",
  "Religion", "Phone-In", "Travel", "Leisure",
  "Jazz Music", "Country Music", "National Music", "Oldies Music",
  "Folk Music", "Documentary", "TEST", "! ALERT !"
};

const char *rbdsProgramTypes[32] =
{
  0, "News", "Information", "Sports",
  "Talk", "Rock", "Classic Rock", "Adult Hits",
  "Soft Rock", "Top 40", "Country", "Oldies",
  "Soft Music", "Nostalgia", "Jazz", "Classical",
  "R & B", "Soft R & B", "Foreign Language", "Religious Music",
  "Religious Talk", "Personality", "Public", "College",
  "Spanish Talk", "Spanish Music", "Hip Hop", 0,
  0, "Weather", "TEST", "! ALERT !"
};

static char bufStationName[50]  = "";
static char bufRadioText[100]   = "";
static char bufProgramInfo[100] = "";
static uint16_t piCode = 0x0000;

const char *getStationName()
{
  if(switchThemeEditor())
    return("*STATION*");
  else
    return(getRDSMode() & RDS_PS? bufStationName : "");
}

const char *getRadioText()
{
  return(getRDSMode() & RDS_RT? bufRadioText : "");
}

const char *getProgramInfo()
{
  return(getRDSMode() & RDS_RT? bufProgramInfo : "");
}

uint16_t getRdsPiCode()
{
  return(getRDSMode() & RDS_PI? piCode : 0x0000);
}

void clearStationInfo()
{
  bufStationName[0] = '\0';
  bufProgramInfo[0] = '\0';
  bufRadioText[0]   = '\0'; // Multiline!
  bufRadioText[1]   = '\0';
  piCode = 0x0000;
}

static bool showStationName(const char *stationName, bool isLong = false)
{
  if(stationName && strcmp((isLong && bufStationName[0] == 0xFF) ? bufStationName + 1 : bufStationName, stationName))
  {
    // If the name is explicitly marked as long, add 0xFF in front of it
    // This is done to display the EiBi names differently
    if(isLong)
    {
      bufStationName[0] = 0xFF;
      strcpy(bufStationName + 1, stationName);
    }
    else
      strcpy(bufStationName, stationName);
    return(true);
  }

  return(false);
}

// Helper: Check if Radio Text contains end-of-message marker (0x0D)
static bool hasEndOfMessage(const char *text)
{
  if(!text) return false;
  for(int i = 0; i < 64 && text[i]; i++)
  {
    if(text[i] == 0x0D) return true;
  }
  return false;
}

// Helper: Check if string is mostly printable (quality check)
static bool isValidRadioText(const char *text)
{
  if(!text || !text[0]) return false;
  int printable = 0;
  int total = 0;
  for(int i = 0; i < 64 && text[i]; i++)
  {
    if(text[i] >= 32 || text[i] == 0x0D || text[i] == 0x0A)
      printable++;
    total++;
  }
  // Require at least 80% printable characters
  return (total > 0 && (printable * 100 / total) >= 80);
}

// Helper: Get effective length (before 0x0D or end)
static int getEffectiveLength(const char *text)
{
  if(!text) return 0;
  int len = 0;
  for(int i = 0; i < 64 && text[i] && text[i] != 0x0D; i++)
    len++;
  return len;
}

static bool showRadioText(const char *radioText, uint8_t width = 32)
{
  bool changed = false;
  int i, j;
  char c;

  // Must have text
  if(!radioText) return(false);

  // PERSISTENCE: Only update if new text is "better" than current
  // This prevents partial/scrolled text from overwriting good data
  int currentLen = getEffectiveLength(bufRadioText);
  bool currentHasEOM = hasEndOfMessage(bufRadioText);
  int newLen = getEffectiveLength(radioText);
  bool newHasEOM = hasEndOfMessage(radioText);
  
  // Quality check: reject obviously bad data
  if(!isValidRadioText(radioText)) return(false);

  // Skip leading whitespace
  int start;
  for(start = 0; (start < 64) && radioText[start] && (radioText[start] <= ' '); start++);

  // If nothing left after whitespace skip, don't update
  if(!radioText[start]) return(false);

  // PERSISTENCE RULE: Only replace if:
  // 1. Current is empty, OR
  // 2. New has EOM and current doesn't, OR
  // 3. New is longer AND (current has no EOM OR new also has EOM)
  // 4. Same length but new has EOM (prefer complete messages)
  if(currentLen > 0)
  {
    bool shouldReplace = false;
    
    if(!currentHasEOM && newHasEOM)
      shouldReplace = true;  // Prefer complete message
    else if(newLen > currentLen && (!currentHasEOM || newHasEOM))
      shouldReplace = true;  // Longer and not making things worse
    else if(newLen == currentLen && newHasEOM && !currentHasEOM)
      shouldReplace = true;  // Same length but now complete
    else if(currentHasEOM && newHasEOM && newLen >= currentLen)
      shouldReplace = true;  // Both complete, prefer longer/newer
    
    if(!shouldReplace) return(false);
  }

  // Terminate at 0x0D, split into lines by 0x0A
  for(i = start, j = 0; (i < 64) && radioText[i] && (radioText[i] != 0x0D); i++)
  {
    if((radioText[i] == 0x0A) || ((radioText[i] == ' ') && (j >= width)))
    {
      c = '\0';
      j = 0;
    }
    else
    {
      c = radioText[i];
      j++;
    }

    changed |= c != bufRadioText[i - start];
    bufRadioText[i - start] = c;
  }

  // Skip trailing whitespace
  int finalLen = i - start;
  while((finalLen > 0) && (bufRadioText[finalLen - 1] <= ' ')) finalLen--;

  // Check the end of the buffer for changes
  changed |= bufRadioText[finalLen] || bufRadioText[finalLen + 1];

  // Terminate multiline text with two zeros
  bufRadioText[finalLen++] = '\0';
  bufRadioText[finalLen++] = '\0';

  // Done
  return(changed);
}

static bool showProgramInfo(const char *programInfo)
{
  if(programInfo && strcmp(bufProgramInfo, programInfo))
  {
    strcpy(bufProgramInfo, programInfo);
    return(true);
  }

  return(false);
}

static bool showRdsProgramType(uint8_t pgmType, bool useRBDS = false)
{
  // Use US-based RBDS system if requested
  const char *text =
    useRBDS?
      (pgmType<ITEM_COUNT(rbdsProgramTypes)? rbdsProgramTypes[pgmType] : 0)
    : (pgmType<ITEM_COUNT(rdsProgramTypes)? rdsProgramTypes[pgmType] : 0);

  // Filter out invalid or non-existant RDS program types
  return(showProgramInfo(text? text:""));
}

static bool showRdsPiCode(uint16_t rdsPiCode)
{
  if(rdsPiCode != piCode)
  {
    // PI code changed - this is a new station, clear stale RT/PS
    // This prevents showing previous station's text on new station
    if(rdsPiCode != 0x0000 && piCode != 0x0000)
    {
      bufRadioText[0] = '\0';
      bufRadioText[1] = '\0';
      bufStationName[0] = '\0';
    }
    piCode = rdsPiCode;
    return(true);
  }

  return(false);
}

static bool showRdsTime(const char *rdsTime)
{
  // If NTP time available, do not use RDS time
  if(!rdsTime || ntpIsAvailable()) return(false);

  // The standard RDS time format is “HH:MM”.
  // or sometimes more complex like “DD.MM.YY,HH:MM”.
  const char *timeField = strstr(rdsTime, ":");

  // If we find a valid time format...
  if(timeField && (timeField>=rdsTime+2) && timeField[1] && timeField[2])
  {
    // Extract hours and minutes
    int hours = (timeField[-2] - '0') * 10 + timeField[-1] - '0';
    int mins  = (timeField[1] - '0') * 10 + timeField[2] - '0';

    // If hours and minutes are valid, update clock
    if(hours>=0 && hours<24 && mins>=0 && mins<60)
      return(clockSet(hours, mins));
  }

  // No time
  return(false);
}

bool checkRds()
{
  bool needRedraw = false;
  uint8_t mode = getRDSMode();

  rx.getRdsStatus();

  if(rx.getRdsReceived() && rx.getRdsSync() && rx.getRdsSyncFound())
  {
    needRedraw |= (mode & RDS_PS) && showStationName(rx.getRdsStationName());
    needRedraw |= (mode & RDS_RT) && showRadioText(rx.getRdsVersionCode()? rx.getRdsText2B() : rx.getRdsText2A());
    needRedraw |= (mode & RDS_PI) && showRdsPiCode(rx.getRdsPI());
    needRedraw |= (mode & RDS_CT) && showRdsTime(rx.getRdsTime());
    needRedraw |= (mode & RDS_PT) && showRdsProgramType(rx.getRdsProgramTypeX(), !!(mode & RDS_RBDS));
  }

  // Return TRUE if any RDS information changes
  return(needRedraw);
}

static const char *findCBChannelByFreq(uint16_t freq)
{
  const int column_step = 450; // In kHz
  const int row_step    = 10;
  const int max_columns = 8;   // A-H
  const int max_rows    = 45;
  static char buf[50];

  if(freq<MIN_CB_FREQUENCY || freq>MAX_CB_FREQUENCY) return(0);

  int offset = freq - MIN_CB_FREQUENCY;
  char type  = 'R';
  if((offset%10) == 5)
  {
    type = 'E';
    offset -= 5;
  }

  int column_index = offset / column_step;
  int remainder    = offset % column_step;
  if((column_index>=max_columns) || (remainder%row_step)) return(0);

  int row_number = remainder / row_step;
  if((row_number>=max_rows) || (row_number<0)) return(0);

  sprintf(buf, "%c%s%c", 'A' + column_index, cbChannelNumber[row_number], type);
  return(buf);
}

static const char *findNameByFreq(uint16_t freq, const NamedFreq *db, uint16_t dbSize)
{
  int r, l;

  for(l=0, r=dbSize-1 ; l <= r ; )
  {
    int m = (l + r) >> 1;
    if(db[m].freq < freq)      l = m + 1;
    else if(db[m].freq > freq) r = m - 1;
    else return(db[m].name);
  }

  return(0);
}

static const char *findScheduleByFreq(uint16_t freq, bool periodic)
{
  uint8_t hour, minute;

  if(currentMode==FM) return(0);

  // Must have valid time
  if(!clockGetHM(&hour, &minute)) return(0);

  static uint16_t last_freq = 0;
  static uint8_t last_minute = 255;
  static size_t first_offset = (size_t)-1;
  static size_t last_offset = (size_t)-1;
  const StationSchedule *entry = NULL;

  // Try EIBI lookup at the next offset and same freq
  if(periodic && freq == last_freq && last_offset != (size_t)-1)
  {
    entry = eibiAtSameFreq(hour, minute, &last_offset, false);

    // Try EIBI lookup at the first offset and same freq
    if(!entry)
    {
      last_offset = first_offset;
      entry = eibiAtSameFreq(hour, minute, &last_offset, true);
    }
  }

  // Try new EIBI lookup if not found or once per minute
  if(!periodic || (!entry && last_offset != (size_t)-1) || last_minute != minute)
  {
    last_freq = freq;
    last_minute = minute;
    last_offset = (size_t)-1;
    entry = eibiLookup(freq, hour, minute, &last_offset);
    first_offset = last_offset = entry ? last_offset : (size_t)-1;
  }

  // Return just the station name
  return(entry? entry->name : 0);
}

bool identifyFrequency(uint16_t freq, bool periodic)
{
  const char *name;
  static uint16_t last_freq = 0;
  static bool name_found = false;

  // RDS has priority on FM
  if(currentMode==FM) return(false);

  // Do not try to look up static names more than once for the same freq
  if(periodic && last_freq==freq && name_found) return(false);
  last_freq = freq;
  name_found = false;

  // For non-periodic calls the name will be found earlier
  if(!periodic)
  {
    // Try list of named frequencies first
    name = findNameByFreq(freq, namedFrequencies, ITEM_COUNT(namedFrequencies));
    if(name)
    {
      name_found = true;
      return(showStationName(name));
    }

    // Try CB channel names
    name = findCBChannelByFreq(freq);
    if(name)
    {
      name_found = true;
      return(showStationName(name));
    }
  }

  // Try EIBI schedule
  name = findScheduleByFreq(freq, periodic);
  return(showStationName(name? name : "", true));
}
