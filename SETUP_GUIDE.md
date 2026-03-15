# Инструкция по настройке среды STM32CubeIDE для проекта ARCH

Этот документ описывает шаги конфигурирования микроконтроллера **STM32F103C8T6** в графическом редакторе `.ioc` для обеспечения совместимости с предоставленным кодом.

## 1. System Core (Системные настройки)

### RCC (Тактирование)
* **High Speed Clock (HSE):** Crystal/Ceramic Resonator.
* **Clock Configuration:** Установите **HCLK на 72 MHz**. Нажмите Enter, чтобы IDE рассчитала множители (PLL Source = HSE, PLLMul = x9).

### SYS (Отладка)
* **Debug:** **Serial Wire** (Критически важно! Это освобождает пин PA15 для дисплея и позволяет прошивать плату по двум проводам).

---

## 2. Timers (Таймеры)

### TIM1 (Силовой ШИМ)
*Этот таймер управляет H-мостом. Настройка Dead-time критична для выживания модулей Mitsubishi.*
* **Slave Mode:** Disable.
* **Clock Source:** Internal Clock.
* **Channel 1 & 2:** **PWM Generation CH1 CH2**.
* **Parameter Settings:**
    * **Prescaler:** 0
    * **Counter Period (ARR):** 4500 (частота 16 кГц).
    * **PWM Mode:** Mode 1.
    * **CH Polarity:** High.
    * **Dead Time (DTG):** **150** (дает ~2.1 мкс паузы). *Для таких тяжелых модулей как CM600, 1 мкс может быть недостаточно.*

### TIM2 (Таймер AC частоты)
* **Clock Source:** Internal Clock.
* **Prescaler:** 71 (1 тик = 1 мкс).
* **Counter Period:** 10000 (автоматически меняется кодом).
* **NVIC Settings:** Включить **TIM2 global interrupt**.

### TIM3 (Динамическая индикация)
* **Clock Source:** Internal Clock.
* **Prescaler:** 71.
* **Counter Period:** 2000 (обновление разряда каждые 2 мс).
* **NVIC Settings:** Включить **TIM3 global interrupt**.

---

## 3. Analog (АЦП)

### ADC1 (Датчик тока ABB ES1000)
* **IN4:** Поставьте галочку (пин **PA4**).
* **Parameter Settings:**
    * **Data Alignment:** Right alignment.
    * **Continuous Conversion Mode:** Disabled (запуск программно в цикле ПИ-регулятора).
    * **Sampling Time:** **71.5 Cycles** (минимум, для фильтрации шумов силовой части).

---

## 4. GPIO (Назначение выводов)
### 1. Силовая часть (PWM - TIM1)
| Pin  | Function   | Mode               | Hardware Connection        |
|:-----|:-----------|:-------------------|:---------------------------|
| PA8  | TIM1_CH1   | AF Push-Pull       | 74HCT541 (A0) -> High Side A|
| PA7  | TIM1_CH1N  | AF Push-Pull       | 74HCT541 (A1) -> Low Side A |
| PA9  | TIM1_CH2   | AF Push-Pull       | 74HCT541 (A2) -> High Side B|
| PB0  | TIM1_CH2N  | AF Push-Pull       | 74HCT541 (A3) -> Low Side B |

### 2. Защита и буфер
| Pin  | Function   | Mode               | Hardware Connection        |
|:-----|:-----------|:-------------------|:---------------------------|
| PB12 | Buffer OE  | GPIO Output PP     | 74HCT541 (Pins 1 & 19)     |

### 3. Дисплей (7-Seg Dynamic)
| Pin  | Function   | Mode               | Hardware Connection        |
|:-----|:-----------|:-------------------|:---------------------------|
| PB1  | Segment A  | GPIO Output PP     | Resistor 220R -> Display   |
| PB2  | Segment B  | GPIO Output PP     | Resistor 220R -> Display   |
| PB3  | Segment C  | GPIO Output PP     | Resistor 220R -> Display   |
| PB4  | Segment D  | GPIO Output PP     | Resistor 220R -> Display   |
| PB5  | Segment E  | GPIO Output PP     | Resistor 220R -> Display   |
| PB6  | Segment F  | GPIO Output PP     | Resistor 220R -> Display   |
| PB7  | Segment G  | GPIO Output PP     | Resistor 220R -> Display   |
| PA10 | Digit 1    | GPIO Output PP     | 2N7002 Gate (Anode/Cathode)|
| PA11 | Digit 2    | GPIO Output PP     | 2N7002 Gate (Anode/Cathode)|
| PA12 | Digit 3    | GPIO Output PP     | 2N7002 Gate (Anode/Cathode)|
| PA15 | Digit 4    | GPIO Output PP     | 2N7002 (Set SWD in SYS!)   |

### 4. Периферия и датчики
| Pin  | Function   | Mode               | Hardware Connection        |
|:-----|:-----------|:-------------------|:---------------------------|
| PA4  | Current Sn | ADC1_IN4           | Current Sensor (Hall/Shunt)|
| PB8  | Gas Valve  | GPIO Output PP     | Relay / Gas Valve Driver   |
| PB9  | Oscillator | GPIO Output PP     | HV Ignition (Oscillator)   |
| PA0  | Torch Btn  | Input Pull-up      | Torch Button (to GND)      |

### 5. Энкодер (Настройка)
| Pin  | Function   | Mode               | Hardware Connection        |
|:-----|:-----------|:-------------------|:---------------------------|
| PA1  | Encoder A  | Input Pull-up      | Encoder Phase A            |
| PA2  | Encoder B  | Input Pull-up      | Encoder Phase B            |
| PA3  | Encoder SW | Input Pull-up      | Encoder Button             |
---

## 5. Project Manager (Генерация кода)

1. Перейдите во вкладку **Code Generator**.
2. Установите галочку: **Generate peripheral initialization as a pair of '.c/.h' files per peripheral**.
3. Нажмите `Ctrl + S` для генерации кода.
