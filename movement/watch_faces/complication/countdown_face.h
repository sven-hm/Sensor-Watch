/*
 * MIT License
 *
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

#ifndef COUNTDOWN_FACE_H_
#define COUNTDOWN_FACE_H_

/*
 * COUNTDOWN TIMER face with 3 countdown slots
 *
 * Slight extension of the original countdown face by Wesley Ellis.
 *
 * Usage:
 *   - Press the light button to cycle through the timer slots
 *   - Long press the light button to enter setting mode for the current slot:
 *     - Press the light button to cycle through settings
 *       (hour -> minute -> second -> finish settings)
 *     - Long press the light button to reset current value to zero
 *     - Press the alarm button to adjust the current value
 *   - Start and pause the countdown using the alarm button, similar
 *     to the stopwatch face.
 *   - When paused, long press the light button to restore the
 *     last entered countdown.
 *   - When one of the countdown timers finishes the watch jumps to the
 *     according slot in the countdown timer face.
 *
 * Max countdown is 23 hours, 59 minutes and 59 seconds.
 *
 * Note: we have to prevent the watch from going to deep sleep using
 * movement_schedule_background_task() while the timer is running.
 */

#include "movement.h"

#define COUNTDOWN_SLOTS 5

typedef enum {
    cd_paused,
    cd_running,
    cd_setting,
    cd_reset
} countdown_mode_t;

typedef struct {
    uint32_t target_ts;
    uint8_t hours : 5;
    uint8_t minutes : 6;
    uint8_t seconds : 6;
    uint8_t set_hours : 5;
    uint8_t set_minutes : 6;
    uint8_t set_seconds : 6;
    countdown_mode_t mode;
} countdown_slot_state_t;

typedef struct {
    uint32_t now_ts;
    uint8_t selection : 2;
    uint8_t current_slot_idx : 6;
    uint8_t next_alarm_slot_idx : 6;
    uint8_t watch_face_idx;
    countdown_slot_state_t slots[COUNTDOWN_SLOTS];
} countdown_state_t;

void countdown_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr);
void countdown_face_activate(movement_settings_t *settings, void *context);
bool countdown_face_loop(movement_event_t event, movement_settings_t *settings, void *context);
void countdown_face_resign(movement_settings_t *settings, void *context);

#define countdown_face ((const watch_face_t){ \
    countdown_face_setup, \
    countdown_face_activate, \
    countdown_face_loop, \
    countdown_face_resign, \
    NULL, \
})

#endif // COUNTDOWN_FACE_H_
