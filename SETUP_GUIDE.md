# 📑 PROJECT ARCH: FINAL PINOUT SPECIFICATION (v4.4)
**Device:** TIG Welding Controller  
**MCU:** STM32F103C8T6 (Blue Pill)

---

## 🔴 1. POWER STAGE & PROTECTION
*Управление инвертором и аппаратная защита.*

| Pin | Function | Mode | Description |
| :--- | :--- | :--- | :--- |
| **PA8** | **TIM1_CH1** | AF PP | High Side A (Верхнее плечо A) |
| **PA7** | **TIM1_CH1N**| AF PP | Low Side A (Нижнее плечо A) |
| **PA9** | **TIM1_CH2** | AF PP | High Side B (Верхнее плечо B) |
| **PB0** | **TIM1_CH2N**| AF PP | Low Side B (Нижнее плечо B) |
| **PB11** | **BUFFER OE** | Out PP | **1=Stop, 0=Run**. Блокировка 74HCT541 |
| **PB1** | **DRV_FAULT** | Input | **Ошибка драйверов**. Мгновенный стоп ШИМ |
| **PB2** | **RESERVE** | - | Свободен (BOOT1 pin) |

---

## 🔵 2. USER INTERFACE (7-SEG DISPLAY)
*Динамическая индикация (Общий Анод). Линейный порядок сегментов.*

### 2.1 Cathodes (Segment Bus - LINEAR)
| Pin | Segment | Mask Bit | Logic | Hardware |
| :--- | :--- | :--- | :--- | :--- |
| **PB3** | **Seg A** | bit 0 | 0 = ON | Res 220R (JTAG OFF Required) |
| **PB4** | **Seg B** | bit 1 | 0 = ON | Res 220R (JTAG OFF Required) |
| **PB5** | **Seg C** | bit 2 | 0 = ON | Res 220R |
| **PB6** | **Seg D** | bit 3 | 0 = ON | Res 220R |
| **PB7** | **Seg E** | bit 4 | 0 = ON | Res 220R |
| **PB8** | **Seg F** | bit 5 | 0 = ON | Res 220R |
| **PB9** | **Seg G** | bit 6 | 0 = ON | Res 220R |

### 2.2 Anodes (Digit Select)
| Pin | Digit | Logic | Hardware |
| :--- | :--- | :--- | :--- |
| **PB12** | **Digit 1** | 1 = ON | Res 330R -> PC817 -> Anode 1 |
| **PB13** | **Digit 2** | 1 = ON | Res 330R -> PC817 -> Anode 2 |
| **PB14** | **Digit 3** | 1 = ON | Res 330R -> PC817 -> Anode 3 |
| **PB15** | **Digit 4** | 1 = ON | Res 330R -> PC817 -> Anode 4 |

---

## 🟡 3. PERIPHERALS & SENSORS
| Pin | Function | Mode | Description |
| :--- | :--- | :--- | :--- |
| **PA4** | **CURR_SN** | ADC_IN4 | Датчик тока (ABB ES1000, 0–3.3V) |
| **PA5** | **GAS_VLV** | Out PP | Реле газового клапана |
| **PA6** | **OSC_HV** | Out PP | Реле осциллятора (HF Start) |

---

## 🟢 4. CONTROL INPUTS (ENCODER & BUTTON)
| Pin | Function | Mode | Description |
| :--- | :--- | :--- | :--- |
| **PA0** | **TRCH_BTN**| Input PU | Кнопка горелки (2T/4T) |
| **PA1** | **ENC_A** | Input PU | Энкодер: Фаза А |
| **PA2** | **ENC_B** | Input PU | Энкодер: Фаза B |
| **PA3** | **ENC_SW** | Input PU | Кнопка энкодера (Menu) |

---

## ⚙️ SOFTWARE IMPLEMENTATION NOTES
1. **Remap:** Вызвать `__HAL_AFIO_REMAP_SWJ_NOJTAG()` для работы **PB3/PB4**.
2. **Safety:** Пин **PB11** при старте установить в `HIGH`.
3. **Fault Handling:** Рекомендуется настроить **PB1** как EXTI-прерывание для защиты.
4. **Fast Display:** `GPIOB->ODR = (GPIOB->ODR & ~(0x7F << 3)) | (segment_map[value] << 3);`
