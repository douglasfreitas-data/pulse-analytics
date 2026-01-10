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
- **Sampling Rate Target:** 400Hz+ for accurate R-peak detection.

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
- **Legacy Code:** Several versions (`dispositivo_vX.txt`) exist. The latest attempts focused on fixing compilation errors related to `HRVSession` structs and optimizing I2C speeds.
- **Immediate Challenge:** ensuring stable compilation and verifying the raw signal quality.

## 🤖 Instructions for the Next AI Agent
1.  **Read this file first.**
2.  **Assume "Data Science Mode":** The user values accuracy, statistical validation, and robust code over UI aesthetics for this project.
3.  **Firmware Focus:** Work primarily in `firmware/` but be ready to write Python scripts in `data_analysis/` to plot and verify the data dumped by the ESP32.
4.  **Git:** This folder is intended to be its own Git repository. If not yet initialized, suggest doing so.

## 📌 To-Do List
- [ ] Initialize Git repository for `pulse-analytics`.
- [ ] Specific compilation check of the latest firmware code.
- [ ] Create a Python script to visualize the Serial plotter data in real-time or from a CSV dump.
