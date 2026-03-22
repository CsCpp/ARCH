#include "config.h"

/* Текущее целевое значение тока, генерируемое автоматом (учитывает нарастание и спад). */
volatile uint32_t current_setpoint = 0;

/* Фиксация значения тока в момент отпускания кнопки (от этой точки рассчитывается спад). */
volatile uint32_t amp_at_release = 0;

/* Основной конечный автомат сварочного процесса (вызывается каждую 1 мс из прерывания TIM3) */
void Welder_Process(void) {
    bool btn = BTN_TORCH_PRESSED(); // Чтение триггера горелки

    /* Приведение токов к рабочему масштабу (умножение на 10 для сглаживания математики ШИМ) */
    uint32_t target_amp_x10 = (uint32_t)set_amp * 10;
    uint32_t start_amp_x10 = (uint32_t)modes[cur_mode].start_amp * 10;

    switch (machine_state) {

        /* 1. РЕЖИМ ПРОСТОЯ */
        case IDLE:
            current_setpoint = 0; // Тока нет, силовые ключи заперты
            if (btn) {
                GAS_ON(); // Открываем клапан подачи аргона
                state_timer = ms_ticks;
                machine_state = PRE_GAS;
            }
            break;

        /* 2. ПРЕД-ГАЗ (Вытеснение кислорода из зоны шва перед поджигом) */
        case PRE_GAS:
            if (ms_ticks - state_timer >= modes[cur_mode].pre_gas) {
                current_setpoint = start_amp_x10; // Установка стартового тока для безопасного пробоя
                OSC_ON(); // Запуск высоковольтного осциллятора
                state_timer = ms_ticks;
                machine_state = START_HF;
            }
            /* Если сварщик передумал и бросил кнопку — отмена пробоя, уходим на продувку */
            if (!btn) machine_state = POST_GAS;
            break;

        /* 3. ПОДЖИГ ДУГИ (Работа осциллятора ограничена по времени для защиты электроники) */
        case START_HF:
            if (ms_ticks - state_timer >= 500) { // Осциллятор работает максимум 500 мс
                OSC_OFF();
                state_timer = ms_ticks;
                machine_state = WELD_WORK; // Переход к основному току
            }
            if (!btn) machine_state = POST_GAS;
            break;

        /* 4. ОСНОВНАЯ СВАРКА (Нарастание тока и поддержание дуги) */
        case WELD_WORK: {
            uint32_t up_time = modes[cur_mode].up_slope;
            uint32_t elapsed = ms_ticks - state_timer;

            /* Алгоритм линейной интерполяции для мягкого старта (Up-Slope) */
            if (up_time > 10 && elapsed < up_time) {
                current_setpoint = start_amp_x10 +
                    ((target_amp_x10 - start_amp_x10) * elapsed) / up_time;
            } else {
                current_setpoint = target_amp_x10; // Выход на номинальный ток
            }

            /* Если кнопку отпустили (в не-Spot режиме) — начинаем заваривать кратер */
            if (!btn && cur_mode != 3) {
                amp_at_release = current_setpoint; // Фиксируем ток, на котором прервали сварку
                state_timer = ms_ticks;
                machine_state = DOWN_SLOPE;
                break;
            }

            /* Обработка импульсного режима (чередование Work / Pause) */
            if (modes[cur_mode].is_pulse) {
                if (ms_ticks - state_timer >= modes[cur_mode].p_work) {
                    state_timer = ms_ticks;
                    machine_state = (cur_mode == 3) ? DOWN_SLOPE : WELD_PAUSE;
                }
            }
        } break;

        /* 5. БАЗОВЫЙ ТОК (Пауза в импульсном режиме для кристаллизации ванны) */
        case WELD_PAUSE:
            current_setpoint = target_amp_x10 / 5; // Базовый ток фиксированно = 20% от основного (можно вынести в меню)

            if (!btn) {
                amp_at_release = current_setpoint;
                state_timer = ms_ticks;
                machine_state = DOWN_SLOPE;
                break;
            }
            if (ms_ticks - state_timer >= modes[cur_mode].p_pause) {
                state_timer = ms_ticks;
                machine_state = WELD_WORK;
            }
            break;

        /* 6. ЗАВАРКА КРАТЕРА (Плавное гашение дуги для предотвращения трещин в конце шва) */
        case DOWN_SLOPE:
            OSC_OFF(); // На всякий случай дублируем отключение осциллятора
            {
                uint32_t down_time = modes[cur_mode].down_slope;
                uint32_t elapsed = ms_ticks - state_timer;

                if (down_time > 10 && elapsed < down_time) {
                    if (amp_at_release > 50) { // Ток отрыва дуги ~ 5 Ампер (50 единиц)
                        current_setpoint = amp_at_release -
                            ((amp_at_release - 50) * elapsed) / down_time;
                    }
                } else {
                    current_setpoint = 0; // Ток иссяк — глушим ШИМ аппаратно
                    state_timer = ms_ticks;
                    machine_state = POST_GAS;
                }
            }
            break;

        /* 7. ПОСТ-ГАЗ (Охлаждение вольфрамового электрода и защита кристаллизующегося шва от кислорода) */
        case POST_GAS:
            current_setpoint = 0;
            OSC_OFF();

            if (ms_ticks - state_timer >= modes[cur_mode].post_gas) {
                if (!btn) { // Если кнопка всё еще не нажата — перекрываем газ
                    GAS_OFF();
                    machine_state = IDLE;
                }
            }

            /* Быстрый рестарт дуги, если сварщик нажал кнопку до окончания продувки */
            if (btn) {
                GAS_ON();
                state_timer = ms_ticks;
                machine_state = PRE_GAS;
            }
            break;
    }
}
