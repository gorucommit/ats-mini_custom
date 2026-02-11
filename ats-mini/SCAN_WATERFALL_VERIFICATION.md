# Scan / Waterfall Button Logic – Verification

## Button semantics (Button.cpp)
- **wasClicked**: release after < 0.5 s
- **wasShortPressed**: release after 0.5 s–2 s
- **wasLongPressed**: release after ≥ 2 s
- **isLongPressed**: still held ≥ 2 s (used only for sleep toggle when `currentCmd == CMD_NONE`)

## Path 1: Enter Scan from sidebar menu
- User has **currentCmd = CMD_MENU**, menuIdx = MENU_SCAN (Scan highlighted).
- User **presses** → main loop: `clickHandler(CMD_MENU, !pb1st.wasLongPressed)` → **clickMenu(menuIdx, shortPress)**.
- **clickMenu**: sets `currentCmd = CMD_NONE`, then `switch(cmd)` → **MENU_SCAN**: calls **clickScan(shortPress)** (no other assignment).
- So when **clickScan** runs here, **currentCmd is still CMD_NONE**.

  - **Short press** (`shortPress == true`):
    - Condition `currentCmd == CMD_SCAN && (scanHasData() || waterfallFileExists())` is **false** (currentCmd is CMD_NONE).
    - **Else**: sets `currentCmd = CMD_SCAN`, runs **normal scan**. ✓
  - **Long press** (`shortPress == false`):
    - **waterfallStart()**; on success sets `currentCmd = CMD_SCAN`, shows "Waterfall... (press to stop)". ✓

## Path 2: Already on Scan screen (results or idle)
- User has **currentCmd = CMD_SCAN** (scan done or waterfall done).
- User **presses** → main loop: `clickHandler(CMD_SCAN, !pb1st.wasLongPressed)` → **clickScan(shortPress)**.

  - **Short press**:
    - `currentCmd == CMD_SCAN && (scanHasData() || waterfallFileExists())` → **true** (we have data/file).
    - Sets **currentCmd = CMD_NONE**, **drawScreen()** → **exit to main screen**. ✓
  - **Long press**:
    - **waterfallStart()** (new waterfall); on success `currentCmd = CMD_SCAN`, message. ✓

## Path 3: During waterfall recording
- **waterfallIsRecording()** is true.
- Main loop hits **first**: `if(waterfallIsRecording()) { waterfallRequestStop(); needRedraw = true; }` and does **not** call **clickHandler**.
- So **button press stops recording** and does not run Scan/menu logic. ✓

## Path 4: Sleep toggle
- **isLongPressed** branch runs only when **currentCmd == CMD_NONE**.
- So long-press sleep is **only** from main screen; when Scan is selected in menu or on Scan screen, **no sleep toggle**. ✓

## Path 5: Timeout (ELAPSED_COMMAND)
- `if (currentCmd != CMD_NONE && currentCmd != CMD_SEEK && currentCmd != CMD_SCAN && currentCmd != CMD_MEMORY)` → then clear.
- **CMD_SCAN is excluded** → staying on Scan screen is not cleared by timeout. ✓

## Summary table

| Context              | Short press (0.5–2 s release) | Long press (≥2 s release)   |
|----------------------|-------------------------------|-----------------------------|
| Menu, Scan selected  | Start **normal scan**         | Start **waterfall**         |
| Scan screen (has data) | **Exit** to main screen     | Start **new waterfall**     |
| Waterfall recording  | **Stop** recording (handled before clickHandler) | Same |

All paths match the intended behaviour; no contradictions found.
