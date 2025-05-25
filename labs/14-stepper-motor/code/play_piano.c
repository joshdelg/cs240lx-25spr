#include "rpi.h"
#include "a4988.h"
#include "notes.h"

#define BPM 120

typedef enum {
    EIGHT = (30 * 1000000) / BPM,
    QUARTER = (60 * 1000000) / BPM,
    DOTTED_QUARTER = (90 * 1000000) / BPM,
    HALF = (120 * 1000000) / BPM,
    DOTTED_HALF = (180 * 1000000) / BPM,
    WHOLE = (240 * 1000000) / BPM,
} note_t;

void play_note(step_t *s, float note, note_t duration) {
    uint32_t play_for_us = duration;
    uint32_t start_time = timer_get_usec();
    output("playing note %f for %d us\n", note, play_for_us);
    while(timer_get_usec() - start_time < play_for_us) {
        // Step forward
        step(s, forward);

        // Wait for adequate delay
        uint32_t delay = (uint32_t)(1000000.0 / note);
        delay_us(delay / 2);

        // Step backward
        step(s, backward);

        // Wait for adequate delay
        delay_us(delay / 2);
    }
}

void notmain(void) {
    enum { dir_delay = 1, step_delay = 1 };
    enum { dir = 21, step = 20 };

    step_t s = step_mk(dir, dir_delay, step, step_delay);

    while(1) {
        play_note(&s, NOTE_C4, QUARTER);
        play_note(&s, NOTE_D4, QUARTER);
        play_note(&s, NOTE_E4, QUARTER);
        play_note(&s, NOTE_F4, QUARTER);
        play_note(&s, NOTE_G4, QUARTER);
        play_note(&s, NOTE_A4, QUARTER);
        play_note(&s, NOTE_B4, QUARTER);
        play_note(&s, NOTE_C5, QUARTER);
    }
}