#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"

//
// Default-style layout with tuning scale at bottom and signal strength
// markers on the scale when scan data is available (Berndt-style).
//
void drawLayoutSignalScale(const char *statusLine1, const char *statusLine2)
{
  drawLayoutTop();

  // Draw left-side menu/info bar
  drawSideBar(currentCmd, MENU_OFFSET_X, MENU_OFFSET_Y, MENU_DELTA_X);

  // Draw S-meter
  drawSMeter(getStrength(rssi), METER_OFFSET_X, METER_OFFSET_Y);

  // Indicate FM pilot detection (stereo indicator)
  drawStereoIndicator(METER_OFFSET_X, METER_OFFSET_Y, (currentMode==FM) && rx.getCurrentPilot());

  // Draw bottom section; if not consumed, show scale with signal markers
  if(!drawLayoutBottom(statusLine1, statusLine2))
    drawScaleWithSignals(isSSB() ? (currentFrequency + currentBFO/1000) : currentFrequency);
}
