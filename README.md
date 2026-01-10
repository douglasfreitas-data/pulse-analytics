# Pulse Analytics 🫀

**Pulse Analytics** is a high-precision HRV (Heart Rate Variability) measurement project designed to establish a "Ground Truth" for validating commercial pulse oximetry solutions.

This project focuses on the intersection of **Firmware Engineering** and **Data Science**, prioritizing signal accuracy, statistical rigor, and robust algorithm development over user interface design.

## 🎯 Project Goals

*   **Metric Validation:** Rigorous validation of RMSSD, SDNN, and Stress Metrics against reference medical-grade devices (e.g., Polar H10).
*   **Signal Processing:** Implementation of advanced filtering (Butterworth Bandpass) to isolate the cardiac signal from motion artifacts and noise.
*   **High-Resolution Sampling:** Achieving consistent 400Hz+ sampling rates with the MAX30105 sensor for precise R-peak detection.
*   **Algorithm Development:** Refinement of peak detection algorithms (e.g., Pan-Tompkins) to achieve sub-millisecond precision on RR intervals.

## 🛠️ Tech Stack

### Hardware
*   **Microcontroller:** ESP32 (Dev Module / S3)
*   **Sensor:** MAX30105 (Particle Sensing Module)

### Firmware
*   **Language:** C++
*   **Framework:** Arduino / ESP-IDF

### Data Analysis
*   **Language:** Python
*   **Libraries:** Pandas, NumPy, SciPy, Matplotlib
*   **Tools:** Jupyter Notebooks for signal visualization and statistical analysis.

## 📂 Repository Structure

*   `/firmware`: Source code for the ESP32 microcontroller.
*   `/data_analysis`: Python scripts and notebooks for post-processing and validating PPG signals.
*   `/docs`: Documentation and reference materials.
*   `/database`: (To be defined - likely for storing session logs).

## 🚀 Getting Started

1.  **Firmware:** Navigate to `firmware/` and open the project in your preferred IDE (PlatformIO or Arduino IDE).
2.  **Analysis:** Check `data_analysis/` for Jupyter notebooks to visualize the raw data output from the serial port.

## 📝 License

[Insert License Here - e.g., MIT, Proprietary]
