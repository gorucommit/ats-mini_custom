#ifndef WATERFALL_H
#define WATERFALL_H

#include <stdint.h>
#include <stdbool.h>

#define WF_FREQ_BINS  400
#define WF_TIME_ROWS  200
#define WF_DURATION_MS (10 * 60 * 1000)

#define WF_FILE_PATH  "/waterfall.raw"

// Start recording (full band, muted). Call when long-press Scan.
bool waterfallStart(void);

// Call every loop() when recording. Returns true while still recording.
bool waterfallTick(void);

// True while recording (so UI can show status and main loop can call Tick).
bool waterfallIsRecording(void);

// Call from main loop when user presses button to stop early.
void waterfallRequestStop(void);

// True if a waterfall file exists and can be downloaded.
bool waterfallFileExists(void);

#endif
