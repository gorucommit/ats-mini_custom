#ifndef WEBCONTROL_H
#define WEBCONTROL_H

/**
 * WebControl - REST API + SSE Web Remote Control
 * 
 * Provides a web-based remote control interface for the ATS-Mini radio.
 * Uses REST API for commands and Server-Sent Events (SSE) for real-time
 * status updates.
 * 
 * Endpoints:
 *   GET  /control       - Serves the control page HTML
 *   GET  /api/options   - Static configuration (bands, steps, etc.)
 *   GET  /api/status    - Current radio state
 *   GET  /events        - SSE stream for real-time updates
 *   POST /api/tune      - Tune frequency
 *   POST /api/seek      - Seek up/down
 *   POST /api/volume    - Set volume
 *   POST /api/squelch   - Set squelch
 *   POST /api/agc       - Set AGC/attenuation
 *   POST /api/avc       - Set AVC
 *   POST /api/band      - Change band
 *   POST /api/mode      - Change mode
 *   POST /api/step      - Change step
 *   POST /api/bandwidth - Change bandwidth
 *   POST /api/mute      - Mute/unmute
 *   GET  /api/memories  - List memory slots
 *   POST /api/memory/store?slot=N - Store current to memory
 *   POST /api/memory/tune?slot=N  - Tune to memory
 *   POST /api/memory/clear?slot=N - Clear memory
 */

#include <ESPAsyncWebServer.h>

/**
 * Initialize web control routes and SSE handler.
 * Call this from webInit() in Network.cpp after creating the server.
 * 
 * @param server Reference to the AsyncWebServer instance
 */
void webControlInit(AsyncWebServer &server);

/**
 * Process SSE broadcasts. Call this from the main loop.
 * Handles throttled broadcasting of status changes to connected clients.
 * Should be called frequently (every loop iteration is fine).
 */
void webControlTick();

/**
 * Mark the radio state as changed.
 * Call this after any state change (volume, frequency, band, etc.)
 * to trigger an SSE broadcast to connected clients.
 */
void webControlNotify();

/**
 * Get the number of connected SSE clients.
 * Useful for debugging or conditional behavior.
 * 
 * @return Number of connected clients
 */
uint8_t webControlClientCount();

#endif // WEBCONTROL_H
