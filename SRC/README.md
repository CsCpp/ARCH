# TIG Welding Controller (STM32F103)

A professional control system for a welding inverter supporting AC/DC modes, pulse welding, and a complete TIG process cycle. Powered by the 32-bit STM32F103 microcontroller.

## 🚀 Key Features

* **Complete TIG Cycle:** Fully adjustable stages including Pre-gas, Start Current, Up-Slope, Work (Peak) Current, Down-Slope, and Post-gas.
* **5 Welding Presets:** Quick switching between operating modes (including MMA, Pulse, COLD, SPOT, and AC).
* **AC Balance & Frequency:** Adjustable AC frequency (20-200 Hz) and polarity balance for high-quality aluminum welding.
* **Hardware PWM (16 kHz):** Signal generation via the `TIM1` timer with hardware Dead-Time insertion (~2.2 µs) for robust protection of power IGBT/MOSFET transistors against shoot-through currents.
* **Wear-Leveling Flash Memory:** Smart settings retention algorithm using `memcmp`. The MCU's flash memory does not wear out when navigating the UI; writing only occurs when parameters are actually changed and strictly after the welding cycle is completed.
* **Smart UI:** Instant access to settings by holding the encoder (no release wait time required) and smooth dynamic 7-segment display multiplexing.

---

## 🔌 Hardware Mapping (Pinout)

**Inputs (Controls & Sensors):**
* `PA0` — Torch trigger button (Active Low / Pull-to-GND).
* `PA1`, `PA2` — Rotary Encoder (Phases A and B).
* `PA3` — Encoder push button (Menu entry / Mode switching).
* `PA4` — ADC Input (Current sensor / Shunt via Op-Amp).

**Outputs (Power & Relays):**
* `PA8`, `PA9` — PWM High/Low sides of the H-bridge (Complementary channels `TIM1_CH1` and `TIM1_CH2`).
* `PB11` — Half-bridge driver block (Inverse logic: 1 - STOP, 0 - RUN).
* `PA5` — Gas valve solenoid relay.
* `PA6` — Oscillator relay (HF Arc Start).

**Display (7-Segment, 4 Digits):**
* `PB12 - PB15` — Common Anodes (driven via transistor switches).
* `PB3 - PB9` — Segment Cathodes (driven via 220 Ohm current-limiting resistors).

---

## 🕹️ Controls

The interface is designed to be operated with welding gloves on—all controls are managed by a single rotary encoder:

1.  **Main Screen:** Turning the encoder adjusts the main work current (Amps).
2.  **Short Press:** Cycles through the 5 saved welding presets.
3.  **Long Press (1.2 sec):** Instantly enters the advanced cycle settings menu.
    * *Turn:* Select a parameter (`PrG`, `StA`, `UpS`, `dnS`, `PoG`, etc.).
    * *Short Press:* Edit the selected parameter (the value will start blinking).
    * *`5AvE`* — Force save changes and exit to the main screen.
    * *`E5c`* — Exit to the main screen (changes will be auto-saved after the next welding operation).

---

## 🛠️ Build & Compilation

The project uses standard CMSIS libraries and is ready to be compiled in **STM32CubeIDE** or **Keil uVision** (GCC compiler).
* **Core Clock:** 72 MHz (via 8 MHz external HSE crystal).
* **Optimization:** `-O1` or `-O2` is highly recommended.
