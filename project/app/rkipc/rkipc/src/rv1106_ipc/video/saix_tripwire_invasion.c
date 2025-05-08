#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <rockiva.h>


// Define constants
#define MAX_TRACKED_OBJECTS 128

// Global task rule
RockIvaBaTaskRule g_task_rule;

// Event callback type
typedef void (*EventCallback)(int ruleId, const char* eventType, const char* details);

// Global event callback
static EventCallback g_event_callback = NULL;

// Function declarations
void register_event_callback(EventCallback callback);
void clear_all_rules();
void load_all_configs();
void normalize_rules();
void detect_tripwire_events();
void detect_area_invasion_events();
void process_events();
int init_tripwire();
int deinit_tripwire();

// Function to register the callback
void register_event_callback(EventCallback callback) {
    g_event_callback = callback;
}

// Function to clear all rules
void clear_all_rules() {
    memset(&g_task_rule, 0, sizeof(g_task_rule));
}

// Function to load all configurations
void load_all_configs() {
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        // Example: Load tripwire rule configuration
        g_task_rule.tripWireRule[i].ruleEnable = 1; // Enable rule
        g_task_rule.tripWireRule[i].ruleID = i;
        g_task_rule.tripWireRule[i].line.head.x = 1000;
        g_task_rule.tripWireRule[i].line.head.y = 2000;
        g_task_rule.tripWireRule[i].line.tail.x = 8000;
        g_task_rule.tripWireRule[i].line.tail.y = 2000;
        g_task_rule.tripWireRule[i].event = ROCKIVA_BA_TRIP_EVENT_BOTH;
    }
}

// Function to normalize rules (if needed)
void normalize_rules() {
    // Example: Ensure coordinates are within valid ranges
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        if (g_task_rule.tripWireRule[i].ruleEnable) {
            RockIvaLine* line = &g_task_rule.tripWireRule[i].line;
            if (line->head.x < 0) line->head.x = 0;
            if (line->head.y < 0) line->head.y = 0;
            if (line->tail.x > 9999) line->tail.x = 9999;
            if (line->tail.y > 9999) line->tail.y = 9999;
        }
    }
}

// Tracked object structure
typedef struct {
    char tracking_id[32];
    int active;
    float prev_cx, prev_cy;
    float curr_cx, curr_cy;
} TrackedObject;

TrackedObject tracked_objects[MAX_TRACKED_OBJECTS];
int tracked_object_count = 0;

// Function to update tracked objects
void update_tracked_object(const char* id, float cx, float cy) {
    for (int i = 0; i < tracked_object_count; i++) {
        if (strcmp(tracked_objects[i].tracking_id, id) == 0) {
            tracked_objects[i].prev_cx = tracked_objects[i].curr_cx;
            tracked_objects[i].prev_cy = tracked_objects[i].curr_cy;
            tracked_objects[i].curr_cx = cx;
            tracked_objects[i].curr_cy = cy;
            tracked_objects[i].active = 1;
            return;
        }
    }
    if (tracked_object_count < MAX_TRACKED_OBJECTS) {
        TrackedObject* obj = &tracked_objects[tracked_object_count++];
        strncpy(obj->tracking_id, id, sizeof(obj->tracking_id) - 1);
        obj->tracking_id[sizeof(obj->tracking_id) - 1] = '\0';
        obj->prev_cx = cx;
        obj->prev_cy = cy;
        obj->curr_cx = cx;
        obj->curr_cy = cy;
        obj->active = 1;
    }
}

// Function to detect tripwire events
void detect_tripwire_events() {
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        if (!g_task_rule.tripWireRule[i].ruleEnable)
            continue;

        RockIvaLine line = g_task_rule.tripWireRule[i].line;

        for (int j = 0; j < tracked_object_count; j++) {
            TrackedObject obj = tracked_objects[j];

            bool crosses = (obj.prev_cy < line.head.y && obj.curr_cy >= line.head.y) ||
                           (obj.prev_cy > line.head.y && obj.curr_cy <= line.head.y);

            if (crosses && g_event_callback) {
                const char* direction = (obj.curr_cy > obj.prev_cy) ? "top_to_bottom" : "bottom_to_top";

                char json_message[512];
                snprintf(json_message, sizeof(json_message),
                         "{\n"
                         "  \"rule_id\": %d,\n"
                         "  \"event_type\": \"tripwire_event\",\n"
                         "  \"direction\": \"%s\",\n"
                         "  \"tracking_id\": \"%s\"\n"
                         "}\n",
                         g_task_rule.tripWireRule[i].ruleID,
                         direction,
                         obj.tracking_id);

                g_event_callback(g_task_rule.tripWireRule[i].ruleID, "Tripwire", json_message);
            }
        }
    }
}

// Function to detect area invasion events
void detect_area_invasion_events() {
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        if (!g_task_rule.areaInRule[i].ruleEnable)
            continue;

        RockIvaArea* area = &g_task_rule.areaInRule[i].area;

        for (int j = 0; j < tracked_object_count; j++) {
            TrackedObject obj = tracked_objects[j];

            bool inside = (obj.curr_cx >= area->points[0].x && obj.curr_cx <= area->points[2].x &&
                           obj.curr_cy >= area->points[0].y && obj.curr_cy <= area->points[2].y);

            if (inside && g_event_callback) {
                char json_message[512];
                snprintf(json_message, sizeof(json_message),
                         "{\n"
                         "  \"rule_id\": %d,\n"
                         "  \"event_type\": \"area_invasion_event\",\n"
                         "  \"tracking_id\": \"%s\"\n"
                         "}\n",
                         g_task_rule.areaInRule[i].ruleID,
                         obj.tracking_id);

                g_event_callback(g_task_rule.areaInRule[i].ruleID, "Area Invasion", json_message);
            }
        }
    }
}

// Function to process events
void process_events() {
    while (true) {
        detect_tripwire_events();
        detect_area_invasion_events();
        usleep(100 * 1000); // 100 ms
    }
}

// Function to initialize tripwire functionality
int init_tripwire() {
    printf("Initializing Tripwire functionality...\n");
    clear_all_rules();
    load_all_configs();
    normalize_rules();
    printf("Tripwire initialization complete.\n");
    return 0;
}

// Function to deinitialize tripwire functionality
int deinit_tripwire() {
    printf("Deinitializing Tripwire functionality...\n");
    clear_all_rules();
    printf("Tripwire deinitialization complete.\n");
    return 0;
}