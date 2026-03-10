# Инструкция по настройке среды STM32CubeIDE для проекта ARCH

Этот документ описывает шаги конфигурирования микроконтроллера **STM32F103C8T6** в графическом редакторе `.ioc` для обеспечения совместимости с предоставленным кодом.

## 1. System Core (Системные настройки)

### RCC (Тактирование)
- **High Speed Clock (HSE):** Crystal/Ceramic Resonator.
- *Перейдите во вкладку Clock Configuration:* Установите **HCLK** на **72 MHz**. Нажмите Enter, чтобы IDE рассчитала множители.

### SYS (Отладка)
- **Debug:** Serial Wire (обязательно для прошивки через ST-Link и освобождения пина PA15).

---

## 2. Timers (Таймеры)

### TIM1 (Силовой ШИМ)
*Этот таймер управляет H-мостом. Настройка Dead-time критична для выживания модулей Mitsubishi.*
- **Slave Mode:** Disable.
- **Clock Source:** Internal Clock.
- **Channel 1 & 2:** PWM Generation CH1 & CH2.
- **Parameter Settings:**
  - **Prescaler:** 0
  - **Counter Period (ARR):** 4500 (дает частоту 16 кГц).
  - **PWM Mode:** Mode 1.
  - **CH Polarity:** High (если ваш драйвер прямой) или Low (если инверсный).
- **Dead Time:** - **Dead Time DTG:** 72 (дает ~1 мкс паузы). Уточните по даташиту ACPL-316J и модулей.

### TIM2 (Таймер AC частоты)
- **Clock Source:** Internal Clock.
- **Prescaler:** 71 (1 тик = 1 мкс).
- **Counter Period:** 10000 (будет меняться динамически кодом).
- **NVIC Settings:** Включить **TIM2 global interrupt**.

### TIM3 (Динамическая индикация)
- **Clock Source:** Internal Clock.
- **Prescaler:** 71.
- **Counter Period:** 2000 (обновление сегмента каждые 2 мс).
- **NVIC Settings:** Включить **TIM3 global interrupt**.

---

## 3. Analog (АЦП)

### ADC1 (Датчик тока)
- **IN1:** Поставьте галочку (пин PA1).
- **Parameter Settings:**
  - **Data Alignment:** Right alignment.
  - **Scan Conversion Mode:** Disabled.
  - **Continuous Conversion Mode:** Disabled (запускаем программно).
  - **Sampling Time:** 71.5 Cycles (для точности на больших токах).

---

## 4. GPIO (Назначение выводов)

Настройте следующие пины в режиме **GPIO_Output**:

| Пин | Функция | Режим | Примечание |
|:---|:---|:---|:---|
| **PA10** | Digit 1 (Анод) | Output Push-Pull | Дисплей |
| **PA11** | Digit 2 (Анод) | Output Push-Pull | Дисплей |
| **PA12** | Digit 3 (Анод) | Output Push-Pull | Дисплей |
| **PA15** | Digit 4 (Анод) | Output Push-Pull | Дисплей (SW-Debug должен быть включен) |
| **PB0-PB6**| Segments A-G | Output Push-Pull | Катоды дисплея |
| **PB8** | Gas Valve | Output Push-Pull | Реле клапана газа |
| **PB9** | Oscillator | Output Push-Pull | Реле осциллятора |

Настройте следующие пины в режиме **Input**:

| Пин | Функция | Режим | Примечание |
|:---|:---|:---|:---|
| **PA0** | Torch Button | Input Pull-up | Кнопка горелки (замыкание на GND) |
| **PA1** | Encoder A | Input Pull-up | Фаза А энкодера |
| **PA2** | Encoder B | Input Pull-up | Фаза B энкодера |
| **PA3** | Encoder SW | Input Pull-up | Кнопка энкодера |

---

## 5. Project Manager (Генерация кода)
1. Вкладка **Code Generator**.
2. Установить галочку: **Generate peripheral initialization as a pair of '.c/.h' files per peripheral**.
3. Это позволит аккуратно заменить `main.c` целиком, сохранив настройки периферии в отдельных файлах.
