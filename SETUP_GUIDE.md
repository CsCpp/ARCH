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

| Пин | Функция | Режим | Примечание |
| :--- | :--- | :--- | :--- |
| **PA10** | Digit 1 (Анод) | Output Push-Pull | Дисплей |
| **PA11** | Digit 2 (Анод) | Output Push-Pull | Дисплей |
| **PA12** | Digit 3 (Анод) | Output Push-Pull | Дисплей |
| **PA15** | Digit 4 (Анод) | Output Push-Pull | **Проверьте Debug: Serial Wire в SYS!** |
| **PB0-PB6** | Segments A-G | Output Push-Pull | Катоды дисплея через резисторы 220 Ом |
| **PB8** | Gas Valve | Output Push-Pull | Реле/транзистор клапана газа |
| **PB9** | Oscillator | Output Push-Pull | Реле включения поджига |
| **PA0** | Torch Button | **Input Pull-up** | Кнопка на горелке (замыкание на GND) |
| **PA1** | Encoder A | **Input Pull-up** | Фаза А энкодера |
| **PA2** | Encoder B | **Input Pull-up** | Фаза B энкодера |
| **PA3** | Encoder SW | **Input Pull-up** | Кнопка выбора энкодера |

---

## 5. Project Manager (Генерация кода)

1. Перейдите во вкладку **Code Generator**.
2. Установите галочку: **Generate peripheral initialization as a pair of '.c/.h' files per peripheral**.
3. Нажмите `Ctrl + S` для генерации кода.
