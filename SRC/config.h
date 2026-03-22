#ifndef CONFIG_H_
#define CONFIG_H_

#include "stm32f1xx.h"
#include <stdbool.h>

/* --- АППАРАТНЫЕ НАСТРОЙКИ СИСТЕМЫ --- */

/* Разрешение ШИМ (Значение регистра ARR).
   При тактировании 72 МГц значение 4500 дает ровно 16 кГц несущей частоты. */
#define PWM_MAX           4500

/* Ограничение максимального заполнения ШИМ (Duty Cycle) для защиты IGBT/MOSFET транзисторов.
   Значение 4200 от 4500 дает ~93%. Оставшееся время необходимо транзисторам для полного закрытия. */
#define PWM_MAX_DUTY      4200

/* Аппаратное мертвое время (Dead-Time) для защиты полумоста от сквозных токов (КЗ).
   Значение 160 в регистре BDTR при 72 МГц дает задержку около 2.2 мкс. */
#define DEAD_TIME         160

/* Адрес страницы Flash-памяти для хранения пользовательских настроек (конец памяти 64КБ чипа). */
#define FLASH_SETTINGS_ADDR  0x0800FC00

/* Магическое число для проверки инициализации памяти. Если при чтении совпадает — данные валидны. */
#define SETTINGS_MAGIC       0xABCD


/* --- МАКРОСЫ УПРАВЛЕНИЯ ПЕРИФЕРИЕЙ --- */
/* Использование регистров BSRR/BRR гарантирует атомарность операций (выполнение за 1 такт).
   Это исключает сбои при прерываниях в момент изменения состояния пинов. */
#define GAS_ON()          (GPIOA->BSRR = GPIO_BSRR_BS5)
#define GAS_OFF()         (GPIOA->BRR  = GPIO_BRR_BR5)

#define OSC_ON()          (GPIOA->BSRR = GPIO_BSRR_BS6)
#define OSC_OFF()         (GPIOA->BRR  = GPIO_BRR_BR6)

/* Управление драйвером силовых ключей (PB11). Логика инверсная (Active-Low). */
#define BRIDGE_RUN()      (GPIOB->BRR  = GPIO_BSRR_BS11)
#define BRIDGE_STOP()     (GPIOB->BSRR = GPIO_BSRR_BS11)

/* Чтение дискретных входов с защитой от инверсии логики (кнопки замыкают на землю). */
#define BTN_TORCH_PRESSED() (!(GPIOA->IDR & GPIO_IDR_IDR0))
#define BTN_ENC_PRESSED()   (!(GPIOA->IDR & GPIO_IDR_IDR3))


/* --- КОНЕЧНЫЕ АВТОМАТЫ (FSM) --- */

/* Состояния циклограммы сварочного процесса (TIG Cycle) */
typedef enum {
    IDLE,       /* Простой, ожидание команды на старт */
    PRE_GAS,    /* Предварительная продувка зоны сварки защитным газом */
    START_HF,   /* Работа осциллятора (пробой дуги высоким напряжением) */
    WELD_WORK,  /* Удержание основного или импульсного тока сварки */
    WELD_PAUSE, /* Фаза базового тока в режиме Pulse (поддержание ванны) */
    DOWN_SLOPE, /* Плавное снижение тока для заварки кратера (Down-Slope) */
    POST_GAS    /* Пост-продувка для защиты остывающего вольфрама и сварочного шва */
} state_t;

/* Состояния пользовательского интерфейса */
typedef enum {
    UI_WORK,     /* Главный экран (управление основным током) */
    UI_MENU_SEL, /* Выбор пункта меню настроек (PrG, UpS, dnS и т.д.) */
    UI_MENU_SET  /* Редактирование выбранного параметра */
} ui_state_t;


/* --- СТРУКТУРЫ И ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ --- */

/* Пакет параметров для одного режима сварки (DC, AC, Pulse и т.д.) */
typedef struct {
    uint16_t pre_gas;       /* Время пред-газа (мс) */
    uint16_t start_amp;     /* Стартовый ток (А) */
    uint16_t up_slope;      /* Время нарастания тока (мс) */
    uint16_t down_slope;    /* Время спада тока / заварки кратера (мс) */
    uint16_t post_gas;      /* Время пост-газа (мс) */
    uint32_t p_work;        /* Время импульса тока в режиме Pulse (мс) */
    uint32_t p_pause;       /* Время паузы (базового тока) в режиме Pulse (мс) */
    uint16_t ac_freq;       /* Частота переменного тока (Гц) */
    uint16_t ac_balance;    /* Баланс переменного тока (очистка/проплавление, %) */
    bool is_pulse;          /* Флаг включения импульсного режима */
    bool is_ac;             /* Флаг переменного тока (для сварки алюминия) */
} WeldParams;

extern WeldParams modes[];
extern volatile state_t machine_state;
extern volatile ui_state_t ui_mode;
extern volatile uint8_t cur_mode;
extern volatile uint32_t ms_ticks, state_timer, mode_show_timer;
extern volatile int32_t set_amp, real_amp, adc_raw;
extern volatile uint8_t cur_digit, display_buf[4];
extern volatile uint32_t current_setpoint;

/* --- ПРОТОТИПЫ СИСТЕМНЫХ ФУНКЦИЙ --- */
void PWM_Init(void);
void PWM_SetUpdate(bool active);
void Display_Init(void);
void Display_Refresh_Step(void);
void set_display_mode_name(uint8_t mode);
void update_display_numbers(int32_t val);
void Welder_Process(void);
void Settings_Save(void);
void Settings_Load(void);
void set_display_text(const char* txt);

#endif /* CONFIG_H_ */
