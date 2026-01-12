# Pulse Analytics - AI Handover Context

**Created:** 2026-01-10
**Previous Context:** Separated from `archery-pulse-main` monorepo.

## 🚀 Project Overview
**Pulse Analytics** is a specialized Data Science and Firmware Engineering project focused on developing a high-precision HRV (Heart Rate Variability) measurement device.

**Goal:** Establish a "Ground Truth" device for HRV analysis to calibrate commercial solutions (like the Archery Pulse mobile app).

## 🛠️ Hardware Stack
- **Microcontroller:** ESP32 (Dev Module / S3)
- **Sensor:** MAX30105 (High-Sensitivity Pulse Oximeter & Heart-Rate Sensor)
- **Communication:** Serial/UART for data logging, potentially Bluetooth/BLE for App connection.
- **Sampling Rate Target:** 200Hz-400Hz.
- **Session Duration:** 120 seconds (24,000 samples per channel).

## 📂 Repository Structure
- **/firmware:** Contains the C++ source code for the ESP32.
- **/firmware/legacy_versions:** Contains historical iterations of the firmware code (v1 through v6). Use these for reference on what has been tried.
- **/data_analysis:** Python scripts/Notebooks for validating signals (Pandas, SciPy, NumPy).

## 🎯 Strategic Objectives
1.  **Metric Validation:** Validate RMSSD, SDNN, and Stress Metrics against reference devices (e.g., Polar H10, Welltory).
2.  **Signal Processing:** Implement robust filtering (Butterworth Bandpass 0.5-5.0Hz) to remove motion artifacts and DC offset.
3.  **Algorithm Development:** Refine Peak Detection (Pan-Tompkins or similar derivative-based approaches) to achieve sub-millisecond precision on RR intervals.
4.  **Data Science Pipeline:** Create a seamless flow to dump raw PPG data from the ESP32 to Python validation scripts.

## 📝 Current Status & History
- **Refactoring:** The project was recently extracted from a larger monorepo to focus purely on the "Hard Tech" aspects.
- **Version History Summary:**
    - **v1-v4:** Basic heart rate detection and local display experiments.
    - **v5:** Introduced local logging to SPIFFS and multiple session management. Proved to be stable but lacked high-fidelity raw data upload.
    - **v6_DS (Initial):** Focused on "Data Science" (3-channel raw data upload to Supabase). Encountered RAM issues (fixed via Chunked Upload) and BPM inaccuracies.
    - **v6_DS (Current Fix):** Identified and resolved "Sampling Jitter". Moved from `loop()`-based reading to a rigid isochronous `micros()` guard (200Hz). Added **Live Tagging** support.
- **Immediate Challenge:** Starting a clean data collection campaign with precise timing and contextual tags.
- **Database Status:** Full backup of legacy data stored in `docs/backups/`. Supabase table reset for a fresh start.

## 🤖 Instructions for the Next AI Agent
1.  **Read this file first.**
2.  **Assume "Data Science Mode":** The user values accuracy, statistical validation, and robust code over UI aesthetics for this project.
3.  **Firmware Focus:** Work primarily in `firmware/` but be ready to write Python scripts in `data_analysis/` to plot and verify the data dumped by the ESP32.
4.  **Git:** This folder is intended to be its own Git repository. If not yet initialized, suggest doing so.

## 📌 To-Do List
- [x] Initialize Git repository for `pulse-analytics`.
- [x] Specific compilation check of the latest firmware code.
- [x] Implement rigid isochronous sampling (200Hz) to fix BPM inaccuracies.
- [x] Implement Real-time Tagging support via Serial.
- [x] Reset Supabase DB and backup legacy data.
- [ ] Create a Python script to visualize the Serial plotter data in real-time or from a CSV dump.
- [ ] Perform a full 60-second comparative session between v5 (stable) and v6_DS (precision).

## 🧠 Technical Implementation Details

### High-Volume Data Upload (Chunked Streaming)
To support the "Data Science" requirement of uploading 120 seconds of raw 3-channel data @ 200Hz (approx. 72,000 samples + RR intervals), we encountered ESP32 RAM limitations when attempting to allocate a single JSON string (approx. 400KB+).

**Solution:** Implemented **Chunked Transfer Encoding** using `NetworkClientSecure` directly, bypassing the standard `HTTPClient` payload buffering.
- **Protocol:** HTTP/1.1 `Transfer-Encoding: chunked` over SSL.
- **Strategy:** The JSON object is constructed piece-by-piece and sent in small chunks (200 array items per flush).
- **Benefit:** Allows uploading arbitrarily large datasets (limited only by Supabase timeouts, not RAM) with minimal memory footprint (~4KB buffer).
- **Key Code:** `sendChunk()` helper and lambda-based array streaming in `uploadSession()`.

### Live Tagging & Demographics System (BLE / Serial)
To add context to measurements for Data Science analysis, a live configuration system was implemented via Serial and Bluetooth (BLE).

**Usage:**
1. **Bluetooth:** Connect via your smartphone using "Serial Bluetooth Terminal" (Device: `PulseAnalytics`).
2. **Serial:** Open the Serial Monitor at 115200 baud.
3. **Commands:**
   - `USER:name`: (Persistent)
   - `AGE:value`: (Persistent, Validated 15-80)
   - `SEX:M` or `SEX:F`: (Persistent, Only 'm' or 'f' accepted)
   - `TAG:value`: e.g., `TAG:pos_cafe` (**One-Shot**: Resets after each session)
4. **Behavior Details:**
   - **Persistent:** User name, Age, and Gender are remembered until manually changed.
   - **One-Shot:** The `TAG` resets automatically to null after each session to avoid context contamination.
