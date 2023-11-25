#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <dolphin/dolphin.h>

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

static void dvd_logo_bounce(DvdLogoState* dvd_logo_state) {
    if(dvd_logo_state->position.x == MAX_WIDTH) {
        dvd_logo_state->stepX = -1;
    } else if(dvd_logo_state->position.x == 0) {
        dvd_logo_state->stepX = 1;
    }

    if(dvd_logo_state->position.y == MAX_HEIGHT) {
        dvd_logo_state->stepY = -1;
    } else if(dvd_logo_state->position.y == 0) {
        dvd_logo_state->stepY = 1;
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
    dvd_logo_bounce(dvd_logo_state);

    // Init view
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, dvd_logo_render_callback, dvd_logo_state);
    view_port_input_callback_set(view_port, dvd_logo_input_callback, event_queue);

    // Init timer
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
    free(dvd_logo_state);

    return 0;
}
