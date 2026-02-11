#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include "Waterfall.h"

#include <LittleFS.h>
#include <FS.h>

static uint8_t wfBuffer[WF_TIME_ROWS][WF_FREQ_BINS];
static uint8_t wfState = 0;  // 0=idle, 1=recording
static bool wfStopRequested = false;

// File header values (always in kHz)
static uint32_t wfMinFreqKhz, wfMaxFreqKhz, wfStepKhz;
// Tuning values (in band units: 10kHz for FM, kHz for AM/SW/MW)
static uint32_t wfMinBU, wfStepBU, wfMaxBU;
static uint16_t wfBins;
static uint32_t wfStartTime, wfIntervalMs;
static uint16_t wfFreqIndex, wfRowIndex;
static bool wfTunePending;
static uint16_t wfSavedFreq;  // User's frequency before recording

#define WF_IDLE       0
#define WF_RECORDING  1

bool waterfallIsRecording(void)
{
  return (wfState == WF_RECORDING);
}

void waterfallRequestStop(void)
{
  wfStopRequested = true;
}

bool waterfallFileExists(void)
{
  return LittleFS.exists(WF_FILE_PATH);
}

static bool writeWaterfallFile(void)
{
  if (wfRowIndex == 0 || wfBins == 0) return false;

  fs::File f = LittleFS.open(WF_FILE_PATH, "wb");
  if (!f) return false;

  const uint32_t magic = 0x31465757;  // "WF01" little-endian
  f.write((uint8_t*)&magic, 4);
  f.write((uint8_t*)&wfBins, 2);
  f.write((uint8_t*)&wfRowIndex, 2);
  f.write((uint8_t*)&wfMinFreqKhz, 4);
  f.write((uint8_t*)&wfMaxFreqKhz, 4);
  f.write((uint8_t*)&wfStepKhz, 4);
  f.write((uint8_t*)&wfIntervalMs, 4);

  for (uint16_t r = 0; r < wfRowIndex; r++)
    f.write(wfBuffer[r], wfBins);

  f.close();
  return true;
}

bool waterfallStart(void)
{
  const Band *band = getCurrentBand();
  uint32_t minF = band->minimumFreq;
  uint32_t maxF = band->maximumFreq;
  if (maxF <= minF) return false;

  // Band freq units: FM = 10 kHz, AM/SW/MW = 1 kHz
  // Convert both to kHz for the file header
  uint32_t minKhz = (currentMode == FM) ? (minF * 10) : minF;
  uint32_t maxKhz = (currentMode == FM) ? (maxF * 10) : maxF;

  // Number of bins to cover the band, capped at WF_FREQ_BINS
  uint32_t range = maxF - minF;  // In band units (10kHz for FM, kHz for AM)
  wfBins = (uint16_t)(range > WF_FREQ_BINS ? WF_FREQ_BINS : range);
  if (wfBins < 1) wfBins = 1;

  // Step in band units (what rx.setFrequency expects)
  uint32_t stepBandUnits = (range + wfBins - 1) / wfBins;
  if (stepBandUnits < 1) stepBandUnits = 1;

  // Convert step to kHz for the file header
  uint32_t stepKhz = (currentMode == FM) ? (stepBandUnits * 10) : stepBandUnits;

  wfMinFreqKhz = minKhz;
  wfMaxFreqKhz = maxKhz;
  wfStepKhz = stepKhz;
  // Band units for rx.setFrequency
  wfMinBU = minF;
  wfMaxBU = maxF;
  wfStepBU = stepBandUnits;
  wfFreqIndex = 0;
  wfRowIndex = 0;
  wfIntervalMs = 0;
  wfStopRequested = false;
  wfState = WF_RECORDING;
  wfTunePending = true;
  wfStartTime = millis();

  // Save user's frequency and mute
  wfSavedFreq = rx.getFrequency();
  muteOn(MUTE_TEMP, true);
  rx.setMaxDelaySetFrequency(currentMode == FM ? 60 : 80);
  rx.setFrequency((uint16_t)minF);
  return true;
}

static void waterfallStop(void)
{
  wfState = WF_IDLE;
  if (wfRowIndex > 0 && wfBins > 0)
    writeWaterfallFile();

  // Restore user's original frequency
  rx.setFrequency(wfSavedFreq);
  rx.setMaxDelaySetFrequency(30);
  muteOn(MUTE_TEMP, false);
}

bool waterfallTick(void)
{
  if (wfState != WF_RECORDING) return false;

  if (wfStopRequested || (millis() - wfStartTime >= WF_DURATION_MS))
  {
    waterfallStop();
    return false;
  }

  if (wfTunePending)
  {
    rx.getStatus(0, 0);
    if (!rx.getTuneCompleteTriggered())
      return true;
    wfTunePending = false;
  }

  // Read RSSI at the current frequency (tuned in previous iteration)
  rx.getCurrentReceivedSignalQuality();
  uint8_t rssi = rx.getCurrentRSSI();
  wfBuffer[wfRowIndex][wfFreqIndex] = rssi;

  // Advance to next bin
  wfFreqIndex++;
  if (wfFreqIndex >= wfBins)
  {
    wfFreqIndex = 0;
    wfRowIndex++;
    wfIntervalMs = (millis() - wfStartTime) / wfRowIndex;
    if (wfRowIndex >= WF_TIME_ROWS)
    {
      waterfallStop();
      return false;
    }
  }

  // Tune to next frequency using band units (what rx.setFrequency expects)
  uint32_t nextFreq = wfMinBU + (uint32_t)wfFreqIndex * wfStepBU;
  if (nextFreq > wfMaxBU) nextFreq = wfMaxBU;
  rx.setFrequency((uint16_t)nextFreq);
  wfTunePending = true;
  return true;
}
