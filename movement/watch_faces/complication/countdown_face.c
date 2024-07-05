/*
 * MIT License
 *
 * Copyright (c) 2023 Konrad Rieck
 * Copyright (c) 2022 Wesley Ellis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "countdown_face.h"
#include "watch.h"
#include "watch_utility.h"

#define CD_SELECTIONS 3
#define DEFAULT_MINUTES 3

static bool quick_ticks_running;

static void abort_quick_ticks(countdown_state_t *state) {
    if (quick_ticks_running) {
        quick_ticks_running = false;
        if (state->slots[state->current_slot_idx].mode == cd_setting)
            movement_request_tick_frequency(4);
        else
            movement_request_tick_frequency(1);
    }
}

static inline int32_t get_tz_offset(movement_settings_t *settings) {
    return movement_timezone_offsets[settings->bit.time_zone] * 60;
}

static inline void store_countdown(countdown_state_t *state) {
    // Store set_{hours,minutes,seconds}
    countdown_slot_state_t* slot_state = &state->slots[state->current_slot_idx];

    slot_state->set_hours = slot_state->hours;
    slot_state->set_minutes = slot_state->minutes;
    slot_state->set_seconds = slot_state->seconds;
}

static inline void load_countdown(countdown_state_t *state) {
    /* Load set countdown time */
    countdown_slot_state_t* slot_state = &state->slots[state->current_slot_idx];

    slot_state->hours = slot_state->set_hours;
    slot_state->minutes = slot_state->set_minutes;
    slot_state->seconds = slot_state->set_seconds;
}

static inline void button_beep(movement_settings_t *settings) {
    // play a beep as confirmation for a button press (if applicable)
    if (settings->bit.button_should_sound)
        watch_buzzer_play_note(BUZZER_NOTE_C7, 50);
}

static void update_next_alarm(countdown_state_t *state, movement_settings_t *settings) {
    uint32_t next_alarm = UINT32_MAX;

    // pick next ts from running slots
    for (uint8_t ii = 0; ii < COUNTDOWN_SLOTS; ii++) {
        countdown_slot_state_t* current_slot_state = &state->slots[ii];
        if (current_slot_state->mode == cd_running) {
            if (current_slot_state->target_ts < next_alarm) {
                next_alarm = current_slot_state->target_ts;
                state->next_alarm_slot_idx = ii;
            }
        }
    }

    if (next_alarm == UINT32_MAX) {
        movement_cancel_background_task_for_face(state->watch_face_idx);
    } else {
        watch_date_time target_dt = watch_utility_date_time_from_unix_time(
                next_alarm, get_tz_offset(settings));
        movement_schedule_background_task_for_face(state->watch_face_idx, target_dt);
    }
}

static void start(countdown_state_t *state, movement_settings_t *settings) {
    watch_date_time now = watch_rtc_get_date_time();
    countdown_slot_state_t* current_slot_state = &state->slots[state->current_slot_idx];

    current_slot_state->mode = cd_running;
    state->now_ts = watch_utility_date_time_to_unix_time(now, get_tz_offset(settings));
    current_slot_state->target_ts = watch_utility_offset_timestamp(
            state->now_ts, current_slot_state->hours, current_slot_state->minutes, current_slot_state->seconds);
    update_next_alarm(state, settings);
}

static void draw(countdown_state_t *state, uint8_t subsecond) {
    char buf[16];

    uint32_t delta;
    div_t result;

    countdown_slot_state_t* current_slot_state = &state->slots[state->current_slot_idx];

    switch (current_slot_state->mode) {
        case cd_running:
            delta = current_slot_state->target_ts - state->now_ts;
            result = div(delta, 60);
            current_slot_state->seconds = result.rem;
            result = div(result.quot, 60);
            current_slot_state->hours = result.quot;
            current_slot_state->minutes = result.rem;
            sprintf(buf, "CD%2d%2d%02d%02d",
                    state->current_slot_idx + 1,
                    current_slot_state->hours,
                    current_slot_state->minutes,
                    current_slot_state->seconds);
            break;
        case cd_reset:
        case cd_paused:
            sprintf(buf, "CD%2d%2d%02d%02d",
                    state->current_slot_idx + 1,
                    current_slot_state->hours,
                    current_slot_state->minutes,
                    current_slot_state->seconds);
            break;
        case cd_setting:
            sprintf(buf, "CD%2d%2d%02d%02d",
                    state->current_slot_idx + 1,
                    current_slot_state->hours,
                    current_slot_state->minutes,
                    current_slot_state->seconds);
            if (!quick_ticks_running && subsecond % 2) {
                switch(state->selection) {
                    case 0:
                        buf[4] = buf[5] = ' ';
                        break;
                    case 1:
                        buf[6] = buf[7] = ' ';
                        break;
                    case 2:
                        buf[8] = buf[9] = ' ';
                        break;
                    default:
                        break;
                }
            }
            break;
    }
    if (current_slot_state->mode == cd_running) {
        watch_set_indicator(WATCH_INDICATOR_BELL);
    } else {
        watch_clear_indicator(WATCH_INDICATOR_BELL);
    }
    watch_display_string(buf, 0);
}

static void pause(countdown_state_t *state, movement_settings_t *settings) {
    state->slots[state->current_slot_idx].mode = cd_paused;
    update_next_alarm(state, settings);
    watch_clear_indicator(WATCH_INDICATOR_BELL);
}

static void reset(countdown_state_t *state, movement_settings_t *settings) {
    state->slots[state->current_slot_idx].mode = cd_reset;
    update_next_alarm(state, settings);
    watch_clear_indicator(WATCH_INDICATOR_BELL);
    load_countdown(state);
}

static void ring(countdown_state_t *state, movement_settings_t *settings) {
    state->current_slot_idx = state->next_alarm_slot_idx;
    reset(state, settings);
    movement_play_alarm();
}

static void settings_increment(countdown_state_t *state) {
    countdown_slot_state_t* current_slot_state = &state->slots[state->current_slot_idx];

    switch(state->selection) {
        case 0:
            current_slot_state->hours = (current_slot_state->hours + 1) % 24;
            break;
        case 1:
            current_slot_state->minutes = (current_slot_state->minutes + 1) % 60;
            break;
        case 2:
            current_slot_state->seconds = (current_slot_state->seconds + 1) % 60;
            break;
        default:
            // should never happen
            break;
    }
    return;
}

void countdown_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr) {
    (void) settings;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(countdown_state_t));
        countdown_state_t *state = (countdown_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(countdown_state_t));
        state->current_slot_idx = 0;
        state->next_alarm_slot_idx = COUNTDOWN_SLOTS;
        state->watch_face_idx = watch_face_index;
        for (uint8_t ii = 0; ii < COUNTDOWN_SLOTS; ii++) {
            state->slots[ii].mode = cd_reset;
        }
        store_countdown(state);
    }
}

void countdown_face_activate(movement_settings_t *settings, void *context) {
    (void) settings;
    countdown_state_t *state = (countdown_state_t *)context;

    for (uint8_t ii = 0; ii < COUNTDOWN_SLOTS; ii++) {
        // update state->now_ts if at least one slot is running
        if (state->slots[ii].mode == cd_running) {
            watch_date_time now = watch_rtc_get_date_time();
            state->now_ts = watch_utility_date_time_to_unix_time(now, get_tz_offset(settings));
            break;
        }
    }
    watch_set_colon();

    movement_request_tick_frequency(1);
    quick_ticks_running = false;
}

bool countdown_face_loop(movement_event_t event, movement_settings_t *settings, void *context) {
    countdown_state_t *state = (countdown_state_t *)context;
    countdown_slot_state_t* current_slot_state = &state->slots[state->current_slot_idx];

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            draw(state, event.subsecond);
            break;
        case EVENT_TICK:
            if (quick_ticks_running) {
                if (watch_get_pin_level(BTN_ALARM))
                    settings_increment(state);
                else
                    abort_quick_ticks(state);
            }

            for (uint8_t ii = 0; ii < COUNTDOWN_SLOTS; ii++) {
                if (state->slots[ii].mode == cd_running) {
                    state->now_ts++;
                    break;
                }
            }
            draw(state, event.subsecond);
            break;
        case EVENT_MODE_BUTTON_UP:
            abort_quick_ticks(state);
            movement_move_to_next_face();
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (current_slot_state->mode == cd_setting) {
                state->selection++;
                if(state->selection >= CD_SELECTIONS) {
                    state->selection = 0;
                    current_slot_state->mode = cd_reset;
                    store_countdown(state);
                    movement_request_tick_frequency(1);
                    button_beep(settings);
                }
            } else {
                state->current_slot_idx = (state->current_slot_idx + 1) % COUNTDOWN_SLOTS;
            }
            draw(state, event.subsecond);
            break;
        case EVENT_ALARM_BUTTON_UP:
            switch(current_slot_state->mode) {
                case cd_running:
                    pause(state, settings);
                    button_beep(settings);
                    break;
                case cd_reset:
                case cd_paused:
                    if (!(current_slot_state->hours == 0 
                                && current_slot_state->minutes == 0 
                                && current_slot_state->seconds == 0)) {
                        // Only start the timer if we have a valid time.
                        start(state, settings);
                        button_beep(settings);
                    }
                    break;
                case cd_setting:
                    settings_increment(state);
                    break;
            }
            draw(state, event.subsecond);
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (current_slot_state->mode == cd_setting) {
                quick_ticks_running = true;
                movement_request_tick_frequency(8);
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            switch (current_slot_state->mode) {
                case cd_setting:
                    switch (state->selection) {
                        case 0:
                            current_slot_state->hours = 0;
                            // intentional fallthrough
                        case 1:
                            current_slot_state->minutes = 0;
                            // intentional fallthrough
                        case 2:
                            current_slot_state->seconds = 0;
                            break;
                    }
                    break;
                case cd_paused:
                    reset(state, settings);
                    button_beep(settings);
                    break;
                default:
                    current_slot_state->mode = cd_setting;
                    movement_request_tick_frequency(4);
                    button_beep(settings);
                    break;
            }
            break;
        case EVENT_ALARM_LONG_UP:
            abort_quick_ticks(state);
            break;
        case EVENT_BACKGROUND_TASK:
            movement_move_to_face(state->watch_face_idx);
            ring(state, settings);
            break;
        case EVENT_TIMEOUT:
            abort_quick_ticks(state);
            movement_move_to_face(0);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
        // intentionally squelch the light default event; we only show the light when cd is running
        case EVENT_LIGHT_BUTTON_DOWN:
            break;
        default:
            movement_default_loop_handler(event, settings);
            break;
    }

    return true;
}

void countdown_face_resign(movement_settings_t *settings, void *context) {
    (void) settings;
    countdown_state_t *state = (countdown_state_t *)context;
    countdown_slot_state_t* current_slot_state = &state->slots[state->current_slot_idx];

    if (current_slot_state->mode == cd_setting) {
        state->selection = 0;
        current_slot_state->mode = cd_reset;
        store_countdown(state);
    }
}
