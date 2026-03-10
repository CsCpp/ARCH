/*
 * PROJECT: ARCH (ARC-CORE H-Bridge)
 * HARDWARE: STM32F103C8T6 (Blue Pill), Mitsubishi CM600HA-24H, ACPL-316J
 * SENSOR: ABB ES1000-9676 (1:1000)
 * ----------------------------------------------------------------------------
 * ОПИСАНИЕ: 
 * Управление инвертором TIG AC/DC. Реализован полный цикл сварки: 
 * пред-газ, ВЧ-поджиг, горячий старт, рабочий ток, спад тока и пост-газ.
 * ----------------------------------------------------------------------------
 */

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* --- 1. КОНСТАНТЫ И АДРЕСАЦИЯ --- */
// Адрес последней страницы Flash (64KB модель) для хранения настроек
#define FLASH_USER_START_ADDR 0x0800FC00  
#define SETTINGS_MAGIC        0xDEADBEEF  

// Настройки датчика тока ABB ES1000-9676
#define SENSOR_RATIO    1000.0f  // Коэффициент 1:1000
#define R_SHUNT         6.2f     // Нагрузочный резистор (Ом)
#define V_REF           3.3f     // Опорное напряжение АЦП
#define V_OFFSET        1.65f    // Смещение (виртуальный ноль для AC)

// Параметры ШИМ (TIM1 на 72МГц)
#define PWM_PERIOD      4500     // 72MHz / 4500 = 16kHz частота ШИМ

// Назначение выводов (GPIO)
#define GAS_VALVE_PIN   GPIO_PIN_8
#define GAS_VALVE_PORT  GPIOB
#define OSC_RELAY_PIN   GPIO_PIN_9
#define OSC_RELAY_PORT  GPIOB
#define TORCH_BTN_PIN   GPIO_PIN_0
#define TORCH_BTN_PORT  GPIOA

/* --- 2. СТРУКТУРЫ И ПЕРЕМЕННЫЕ --- */
typedef struct {
    uint32_t magic;
    uint32_t main_current;  // Уставка тока (А)
    uint32_t freq_ac;       // Частота AC (Гц)
    uint32_t ac_balance;    // Баланс (%)
    uint32_t post_gas;      // Время пост-газа (мс)
    uint32_t pre_gas;       // Время пред-газа (мс)
    uint32_t down_slope;    // Спад тока (мс)
} WeldingSettings;

typedef enum {
    STATE_IDLE, STATE_PRE_GAS, STATE_IGNITION, 
    STATE_WELDING, STATE_DOWN_SLOPE, STATE_POST_GAS, STATE_ERROR
} State_t;

// Системные объекты
WeldingSettings settings;
State_t machine_state = STATE_IDLE;
volatile uint32_t current_duty = 0;
volatile uint8_t ac_phase = 0;
float current_setpoint = 0;

// Дисплей: 0-9, 10:'A', 11:'F', 12:'b', 13:'-', 14:Пусто
const uint8_t segment_map[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 
    0x88, 0x8E, 0x83, 0xBF, 0xFF 
};
volatile uint8_t disp_buff[4] = {14, 14, 14, 14};

/* --- 3. ФУНКЦИИ ПАМЯТИ И ИЗМЕРЕНИЙ --- */

// Сохранение во Flash (Blue Pill)
void Save_Settings(void) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase;
    uint32_t err;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = FLASH_USER_START_ADDR;
    erase.NbPages = 1;
    HAL_FLASHEx_Erase(&erase, &err);
    
    uint32_t *p = (uint32_t*)&settings;
    for(int i=0; i < sizeof(settings)/4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_START_ADDR + (i*4), p[i]);
    }
    HAL_FLASH_Lock();
}

void Load_Settings(void) {
    WeldingSettings *f = (WeldingSettings*)FLASH_USER_START_ADDR;
    if(f->magic != SETTINGS_MAGIC) {
        settings = (WeldingSettings){SETTINGS_MAGIC, 80, 100, 30, 3000, 500, 2000};
        Save_Settings();
    } else {
        settings = *f;
    }
}

// Прецизионное чтение тока с датчика ABB
float Get_Current_Amps(void) {
    extern ADC_HandleTypeDef hadc1;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
        uint32_t raw = HAL_ADC_GetValue(&hadc1);
        float v = (raw * V_REF) / 4095.0f;
        float amps = ((v - V_OFFSET) / R_SHUNT) * SENSOR_RATIO;
        return (amps < 0) ? -amps : amps; // Модуль тока для ПИ-регулятора
    }
    return 0;
}

/* --- 4. ИНТЕРФЕЙС И ЗАЩИТА ЭКРАНА --- */

// Статичные прочерки для исключения мерцания при помехах от осциллятора
void Display_Protection(bool enable) {
    extern TIM_HandleTypeDef htim3;
    if(enable) {
        HAL_TIM_Base_Stop_IT(&htim3); 
        // Включаем все аноды (PA10, 11, 12, 15)
        GPIOA->ODR &= ~(GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_15);
        // Выводим прочерк на PB0-6
        GPIOB->ODR = (GPIOB->ODR & 0xFF80) | (segment_map[13] & 0x7F);
    } else {
        HAL_TIM_Base_Start_IT(&htim3);
    }
}

// Опрос энкодера (PA1, PA2) и кнопки (PA3)
void UI_Handler(void) {
    if(machine_state != STATE_IDLE) return; // Блокировка UI во время сварки

    static uint8_t last_enc = 0;
    static uint32_t save_timer = 0;
    static uint8_t menu_mode = 0; // 0:Ток, 1:Частота

    // Чтение фаз
    uint8_t curr_enc = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) << 1) | HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);
    if(curr_enc != last_enc) {
        int8_t step = 0;
        if(last_enc == 0x03 && curr_enc == 0x01) step = 1;
        if(last_enc == 0x03 && curr_enc == 0x02) step = -1;

        if(step != 0) {
            save_timer = HAL_GetTick();
            if(menu_mode == 0) settings.main_current += step * 5;
            else settings.freq_ac += step * 5;
            
            // Лимиты
            if(settings.main_current < 10) settings.main_current = 10;
            if(settings.main_current > 200) settings.main_current = 200;
        }
        last_enc = curr_enc;
    }

    // Кнопка выбора
    static bool btn_lock = false;
    if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET && !btn_lock) {
        menu_mode = !menu_mode;
        btn_lock = true;
    } else if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) btn_lock = false;

    // Автосохранение
    if(save_timer && (HAL_GetTick() - save_timer > 5000)) {
        Save_Settings();
        save_timer = 0;
    }

    // Обновление буфера дисплея
    uint16_t v = (menu_mode == 0) ? settings.main_current : settings.freq_ac;
    disp_buff[0] = menu_mode ? 11 : 10; // 'F' или 'A'
    disp_buff[1] = (v / 100) % 10;
    disp_buff[2] = (v / 10) % 10;
    disp_buff[3] = v % 10;
}

/* --- 5. СИЛОВАЯ ЧАСТЬ И ПИ-РЕГУЛЯТОР --- */

// ПИ-регулятор (вызывать строго 1 раз в мс)
void PI_Controller(void) {
    static float integral = 0;
    // Работает только в режимах горения дуги
    if(machine_state < STATE_IGNITION || machine_state > STATE_DOWN_SLOPE) {
        current_duty = 0;
        integral = 0;
        return;
    }

    float error = current_setpoint - Get_Current_Amps();
    integral += error * 0.05f; // Ki
    if(integral > 500) integral = 500; // Ограничение накопителя
    if(integral < -500) integral = -500;

    float output = (error * 0.15f) + integral; // Kp + Ki
    
    // Безопасное ограничение: оставляем Dead-time для Mitsubishi
    if(output > 4000) output = 4000;
    if(output < 0) output = 0;
    current_duty = (uint32_t)output;
}

// Прерывание AC (TIM2) и Дисплея (TIM3)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if(htim->Instance == TIM2) {
        extern TIM_HandleTypeDef htim1;
        ac_phase = !ac_phase;
        if(ac_phase) {
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, current_duty);
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
        } else {
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, current_duty);
        }
        // Установка времени фазы по балансу
        uint32_t p = 1000000 / settings.freq_ac;
        uint32_t t = ac_phase ? (p * settings.ac_balance / 100) : (p * (100 - settings.ac_balance) / 100);
        __HAL_TIM_SET_AUTORELOAD(htim, t);
    }

    if(htim->Instance == TIM3) {
        static uint8_t d = 0;
        GPIOA->ODR |= (GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_15); // Выкл аноды
        GPIOB->ODR = (GPIOB->ODR & 0xFF80) | (segment_map[disp_buff[d]] & 0x7F);
        if(d==0) GPIOA->ODR &= ~GPIO_PIN_10;
        if(d==1) GPIOA->ODR &= ~GPIO_PIN_11;
        if(d==2) GPIOA->ODR &= ~GPIO_PIN_12;
        if(d==3) GPIOA->ODR &= ~GPIO_PIN_15;
        d = (d + 1) % 4;
    }
}

/* --- 6. ГЛАВНЫЙ ЦИКЛ СВАРКИ --- */
void Welding_SM(void) {
    static uint32_t t_step = 0;
    bool btn = (HAL_GPIO_ReadPin(TORCH_BTN_PORT, TORCH_BTN_PIN) == GPIO_PIN_RESET);

    switch(machine_state) {
        case STATE_IDLE:
            if(btn) {
                Display_Protection(true);
                HAL_GPIO_WritePin(GAS_VALVE_PORT, GAS_VALVE_PIN, GPIO_PIN_SET);
                t_step = HAL_GetTick();
                machine_state = STATE_PRE_GAS;
            }
            break;

        case STATE_PRE_GAS:
            if(HAL_GetTick() - t_step > settings.pre_gas) {
                HAL_GPIO_WritePin(OSC_RELAY_PORT, OSC_RELAY_PIN, GPIO_PIN_SET);
                t_step = HAL_GetTick();
                machine_state = STATE_IGNITION;
            }
            break;

        case STATE_IGNITION:
            // Поджиг: если ток пошел (>10A) или прошло 2 сек
            if(Get_Current_Amps() > 10.0f || (HAL_GetTick() - t_step > 2000)) {
                HAL_GPIO_WritePin(OSC_RELAY_PORT, OSC_RELAY_PIN, GPIO_PIN_RESET);
                machine_state = btn ? STATE_WELDING : STATE_DOWN_SLOPE;
                t_step = HAL_GetTick();
            }
            break;

        case STATE_WELDING:
            current_setpoint = settings.main_current;
            if(!btn) {
                t_step = HAL_GetTick();
                machine_state = STATE_DOWN_SLOPE;
            }
            break;

        case STATE_DOWN_SLOPE:
            {
                uint32_t elapsed = HAL_GetTick() - t_step;
                if(elapsed > settings.down_slope) {
                    current_setpoint = 0;
                    t_step = HAL_GetTick();
                    machine_state = STATE_POST_GAS;
                } else {
                    current_setpoint = settings.main_current * (1.0f - (float)elapsed/settings.down_slope);
                }
            }
            break;

        case STATE_POST_GAS:
            if(HAL_GetTick() - t_step > settings.post_gas) {
                HAL_GPIO_WritePin(GAS_VALVE_PORT, GAS_VALVE_PIN, GPIO_PIN_RESET);
                Display_Protection(false);
                machine_state = STATE_IDLE;
            }
            break;
            
        default: machine_state = STATE_ERROR; break;
    }
}

int main(void) {
    HAL_Init();
    // SystemClock_Config(); // 72MHz
    // MX_GPIO_Init(); MX_ADC1_Init(); MX_TIM1_Init(); MX_TIM2_Init(); MX_TIM3_Init();
    
    Load_Settings();
    
    extern TIM_HandleTypeDef htim1, htim2, htim3;
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_Base_Start_IT(&htim3);

    uint32_t pi_timer = 0;

    while (1) {
        // Контроль тока (1 мс)
        if(HAL_GetTick() - pi_timer >= 1) {
            PI_Controller();
            pi_timer = HAL_GetTick();
        }
        
        Welding_SM();
        UI_Handler();
    }
}
