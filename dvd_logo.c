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
    Point step;
    int speed;

    NotificationApp* notification;
    FuriMutex* mutex;
} DvdLogoState;

static void dvd_logo_render_callback(Canvas* const canvas, void* ctx) {
    DvdLogoState* dvd_logo_state = ctx;

    furi_mutex_acquire(dvd_logo_state->mutex, FuriWaitForever);

    canvas_draw_icon(
        canvas, dvd_logo_state->position.x, dvd_logo_state->position.y, &I_DvdLogo_46x26);

    furi_mutex_release(dvd_logo_state->mutex);
}

static void dvd_logo_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;

    DvdLogoEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void dvd_logo_update_timer_callback(void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;

    DvdLogoEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static Point dvd_logo_get_next_step(DvdLogoState* dvd_logo_state) {
    Point next_step = dvd_logo_state->position;

    next_step.x += dvd_logo_state->step.x * dvd_logo_state->speed;
    next_step.y += dvd_logo_state->step.y * dvd_logo_state->speed;

    return next_step;
}

const NotificationSequence sequence_corner = {
    // &message_vibro_on,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    // &message_vibro_off,
    NULL,
};

const NotificationSequence sequence_wall = {
    // &message_vibro_on,
    &message_note_c4,
    &message_delay_50,
    &message_sound_off,
    // &message_vibro_off,
    NULL,
};

static void dvd_logo_bounce_sfx(DvdLogoState* dvd_logo_state, bool is_corner) {
    if(is_corner) {
        notification_message(dvd_logo_state->notification, &sequence_set_only_green_255);
        notification_message(dvd_logo_state->notification, &sequence_corner);
    } else {
        notification_message(dvd_logo_state->notification, &sequence_set_only_blue_255);
        notification_message(dvd_logo_state->notification, &sequence_wall);
    }
    notification_message(dvd_logo_state->notification, &sequence_reset_rgb);
}

static void dvd_logo_bounce(DvdLogoState* dvd_logo_state) {
    bool has_bounced = false;

    if(dvd_logo_state->position.x >= MAX_WIDTH) {
        dvd_logo_state->step.x = -1; // Invert step
        dvd_logo_state->position.x = MAX_WIDTH; // Cap position
        has_bounced = true;
    } else if(dvd_logo_state->position.x <= 0) {
        dvd_logo_state->step.x = 1; // Invert step
        dvd_logo_state->position.x = 0; // Cap position
        has_bounced = true;
    }

    if(dvd_logo_state->position.y >= MAX_HEIGHT) {
        dvd_logo_state->step.y = -1; // Invert step
        dvd_logo_state->position.y = MAX_HEIGHT; // Cap position
        has_bounced = true;
    } else if(dvd_logo_state->position.y <= 0) {
        dvd_logo_state->step.y = 1; // Invert step
        dvd_logo_state->position.y = 0; // Cap position
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

static void dvd_logo_init_state(DvdLogoState* const dvd_logo_state) {
    dvd_logo_state->position.x = rand() % MAX_WIDTH;
    dvd_logo_state->position.y = rand() % MAX_HEIGHT;
    dvd_logo_state->step.x = (rand() % 2) ? 1 : -1;
    dvd_logo_state->step.y = (rand() % 2) ? 1 : -1;
    dvd_logo_state->speed = 2;
}

int32_t dvd_logo_app(void* p) {
    UNUSED(p);

    // Init event queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(DvdLogoEvent));

    // Enforce display on
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message_block(notification, &sequence_display_backlight_enforce_on);

    // Init state
    DvdLogoState* dvd_logo_state = malloc(sizeof(DvdLogoState));
    dvd_logo_init_state(dvd_logo_state);
    dvd_logo_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    dvd_logo_state->notification = notification;

    // Check for first bounce
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

        furi_mutex_release(dvd_logo_state->mutex);

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

        furi_mutex_release(dvd_logo_state->mutex);
        view_port_update(view_port);
    }

    // Return backlight to normal state
    notification_message(notification, &sequence_display_backlight_enforce_auto);

    // Cleanup
    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(dvd_logo_state->mutex);
    free(dvd_logo_state);

    return 0;
}
