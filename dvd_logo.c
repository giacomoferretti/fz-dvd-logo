#include <furi.h>
#include <furi_hal.h>
#include <dolphin/dolphin.h>

#include <gui/gui.h>
#include <gui/icon_i.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <dvd_logo_icons.h>

#define MAX_WIDTH (128 - I_DvdLogo_46x26.width)
#define MAX_HEIGHT (64 - I_DvdLogo_46x26.height)

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} DvdLogoEvent;

typedef struct {
    uint8_t x;
    uint8_t y;
} Point;

typedef struct {
    Point position;
    uint8_t stepX;
    uint8_t stepY;

    NotificationApp* notifications;
    FuriMutex* mutex;
} DvdLogoState;

static void dvd_logo_render_callback(Canvas* const canvas, void* ctx) {
    DvdLogoState* dvd_logo_state = ctx;

    canvas_draw_icon(
        canvas, dvd_logo_state->position.x, dvd_logo_state->position.y, &I_DvdLogo_46x26);
}

static void dvd_logo_input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    DvdLogoEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void dvd_logo_update_timer_callback(FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    DvdLogoEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static Point dvd_logo_get_next_step(DvdLogoState* dvd_logo_state) {
    Point next_step = dvd_logo_state->position;

    next_step.x += dvd_logo_state->stepX;
    next_step.y += dvd_logo_state->stepY;

    return next_step;
}

int rand_int(int min, int max) {
    return min + rand() % (max - min);
}

// Static list of frequencies
// static const float frequencies[] = {
//     261.63f, // C4
//     // 293.66f, // D4
//     // 329.63f, // E4
//     // 349.23f, // F4
//     // 392.00f, // G4
//     // 440.00f, // A4
//     // 493.88f, // B4
//     523.25f, // C5
//     // 587.33f, // D5
//     // 659.25f, // E5
//     // 698.46f, // F5
//     // 783.99f, // G5
//     // 880.00f, // A5
//     // 987.77f, // B5
//     // 1046.50f, // C6
// };

static void dvd_logo_bounce_sfx(DvdLogoState* dvd_logo_state, bool is_corner) {
    if(is_corner) {
        notification_message(dvd_logo_state->notifications, &sequence_set_only_green_255);
    } else {
        notification_message(dvd_logo_state->notifications, &sequence_set_only_blue_255);
    }
    // notification_message(dvd_logo_state->notifications, &sequence_set_vibro_on);

    if(furi_hal_speaker_acquire(1000)) {
        // Play random frequency
        // furi_hal_speaker_start(frequencies[rand() % (sizeof(frequencies) / sizeof(float))], 1.0f);
        if(is_corner) {
            furi_hal_speaker_start(523.25f, 1.0f);
        } else {
            furi_hal_speaker_start(261.63f, 1.0f);
        }
    }

    furi_delay_ms(50);

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    // notification_message(dvd_logo_state->notifications, &sequence_reset_vibro);
    notification_message(dvd_logo_state->notifications, &sequence_reset_rgb);
}

static void dvd_logo_bounce(DvdLogoState* dvd_logo_state) {
    bool has_bounced = false;

    if(dvd_logo_state->position.x == MAX_WIDTH) {
        dvd_logo_state->stepX = -1;
        has_bounced = true;
    } else if(dvd_logo_state->position.x == 0) {
        dvd_logo_state->stepX = 1;
        has_bounced = true;
    }

    if(dvd_logo_state->position.y == MAX_HEIGHT) {
        dvd_logo_state->stepY = -1;
        has_bounced = true;
    } else if(dvd_logo_state->position.y == 0) {
        dvd_logo_state->stepY = 1;
        has_bounced = true;
    }

    if(dvd_logo_state->position.x == MAX_WIDTH) {
        dvd_logo_state->stepX = -1;
        has_bounced = true;
    } else if(dvd_logo_state->position.x == 0) {
        dvd_logo_state->stepX = 1;
        has_bounced = true;
    }

    bool is_corner =
        (dvd_logo_state->position.x == 0 || dvd_logo_state->position.x == MAX_WIDTH) &&
        (dvd_logo_state->position.y == 0 || dvd_logo_state->position.y == MAX_HEIGHT);

    if(has_bounced) {
        dvd_logo_bounce_sfx(dvd_logo_state, is_corner);
    }
}

static void dvd_logo_process_step(DvdLogoState* dvd_logo_state) {
    dvd_logo_state->position = dvd_logo_get_next_step(dvd_logo_state);
    dvd_logo_bounce(dvd_logo_state);
}

int32_t dvd_logo_app(void* p) {
    UNUSED(p);

    // Init event queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(DvdLogoEvent));

    // Init state
    DvdLogoState* dvd_logo_state = malloc(sizeof(DvdLogoState));
    dvd_logo_state->position.x = 0; //rand() % MAX_WIDTH;
    dvd_logo_state->position.y = 0; //rand() % MAX_HEIGHT;
    dvd_logo_state->stepX = -1; //rand() % MAX_WIDTH;
    dvd_logo_state->stepY = -1; //rand() % MAX_HEIGHT;
    dvd_logo_state->notifications = furi_record_open(RECORD_NOTIFICATION);
    dvd_logo_bounce(dvd_logo_state);

    // Init view
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, dvd_logo_render_callback, dvd_logo_state);
    view_port_input_callback_set(view_port, dvd_logo_input_callback, event_queue);

    // Init timer (tick every 1/8 second)
    FuriTimer* timer =
        furi_timer_alloc(dvd_logo_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 8);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    dolphin_deed(DolphinDeedPluginGameStart);

    DvdLogoEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        if(event_status == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress && event.input.key == InputKeyBack) {
                    processing = false;
                }
            } else if(event.type == EventTypeTick) {
                dvd_logo_process_step(dvd_logo_state);
            }
        } else {
            // Event timeout
        }

        view_port_update(view_port);
    }

    // Cleanup
    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    free(dvd_logo_state);

    return 0;
}
