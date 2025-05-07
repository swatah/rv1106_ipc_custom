/*
 * Copyright (c) 2025 NeuralSense AI Private Limited
 * Trading as swatah.ai. All rights reserved.
 *
 * This file is part of the swatah.ai software stack and is licensed under
 * the terms defined in the accompanying LICENSE file. Unauthorized copying,
 * distribution, or modification of this file, via any medium, is strictly prohibited.
 *
 * For more information, visit: https://swatah.ai
*/

// Import libraries

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Define constants
#define ROCKIVA_AREA_NUM_MAX 10
#define ROCKIVA_BA_MAX_RULE_NUM 10
#define MAX_TRIPWIRE_RULES 10

// Define structures

/* Point coordinates */
typedef struct {
    int16_t x; /* Horizontal coordinate, represented as ten-thousandths, range 0~9999 */
    int16_t y; /* Vertical coordinate, represented as ten-thousandths, range 0~9999 */
} RockIvaPoint;

/* Line coordinates */
typedef struct {
    RockIvaPoint head; /* Head coordinate (top of the line in the vertical direction) */
    RockIvaPoint tail; /* Tail coordinate (bottom of the line in the vertical direction) */
} RockIvaLine;

/* Area coordinates */
typedef struct {
    RockIvaPoint points[4]; /* Four points defining an area */
} RockIvaArea;

/* Tripwire Rule */
typedef struct {
    RockIvaLine line;  // Line coordinates
    bool ruleEnable;   // Enable/Disable flag for the rule
} TripwireRule;

/* Area Invasion Rule */
typedef struct {
    RockIvaArea area;  // Area coordinates
    bool ruleEnable;   // Enable/Disable flag for the rule
} AreaInvasionRule;

/* Task Rule */
typedef struct {
    TripwireRule tripwireRules[MAX_TRIPWIRE_RULES];
    AreaInvasionRule areaInvasionRules[ROCKIVA_BA_MAX_RULE_NUM];
} TaskRule;

typedef void (*EventCallback)(int ruleId, const char* eventType, const char* details);

// Register the callback
void register_event_callback(EventCallback callback);

// Global task rule
TaskRule g_task_rule;

// Function declarations
void normalize_point(RockIvaPoint* point);
void normalize_line(RockIvaLine* line);
void normalize_area(RockIvaArea* area);
void set_tripwire_rule(int index, RockIvaLine* line, bool enable);
void set_area_invasion_rule(int index, RockIvaArea* area, bool enable);
void clear_all_rules();
void normalize_rules();
bool read_line_config(int index, RockIvaLine* line);
bool read_area_config(int index, RockIvaArea* area);
void load_all_configs();
int init_tripwire();

// Event handling

static EventCallback g_event_callback = NULL;

// Function to register the callback
void register_event_callback(EventCallback callback) {
    g_event_callback = callback;
}

// Function definitions

void normalize_point(RockIvaPoint* point) {
    if (!point) return;
    // Normalize the point (logic to be implemented)
    if (point->x < 0) point->x = 0;
    if (point->x > 9999) point->x = 9999;
    if (point->y < 0) point->y = 0;
    if (point->y > 9999) point->y = 9999;
}

void normalize_line(RockIvaLine* line) {
    if (!line) return;
    normalize_point(&line->head);
    normalize_point(&line->tail);
}

void normalize_area(RockIvaArea* area) {
    if (!area) return;
    for (int i = 0; i < 4; i++) {
        normalize_point(&area->points[i]);
    }
}

void set_tripwire_rule(int index, RockIvaLine* line, bool enable) {
    if (index < 0 || index >= MAX_TRIPWIRE_RULES || !line) return;
    g_task_rule.tripwireRules[index].line = *line;
    g_task_rule.tripwireRules[index].ruleEnable = enable;
}

void set_area_invasion_rule(int index, RockIvaArea* area, bool enable) {
    if (index < 0 || index >= ROCKIVA_BA_MAX_RULE_NUM || !area) return;
    g_task_rule.areaInvasionRules[index].area = *area;
    g_task_rule.areaInvasionRules[index].ruleEnable = enable;
}

const char* saix_get_current_timestamp() {
    static char timestamp[64];
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now); // Use gmtime for UTC or localtime for local time
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return timestamp;
}

void clear_all_rules() {
    for (int i = 0; i < MAX_TRIPWIRE_RULES; i++) {
        g_task_rule.tripwireRules[i].ruleEnable = false;
    }
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        g_task_rule.areaInvasionRules[i].ruleEnable = false;
    }
}

// Main logic for normalizing rules
void normalize_rules() {
    for (int i = 0; i < MAX_TRIPWIRE_RULES; i++) {
        if (g_task_rule.tripwireRules[i].ruleEnable) {
            normalize_line(&g_task_rule.tripwireRules[i].line);
        }
    }
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        if (g_task_rule.areaInvasionRules[i].ruleEnable) {
            normalize_area(&g_task_rule.areaInvasionRules[i].area);
        }
    }
}

// Function to read line configuration
bool read_line_config(int index, RockIvaLine* line) {
    if (!line || index < 0 || index >= MAX_TRIPWIRE_RULES) return false;

    char entry[128] = {0};
    snprintf(entry, sizeof(entry), "tripwire.%d:head_x", index);
    line->head.x = rk_param_get_int(entry, -1);

    snprintf(entry, sizeof(entry), "tripwire.%d:head_y", index);
    line->head.y = rk_param_get_int(entry, -1);

    snprintf(entry, sizeof(entry), "tripwire.%d:tail_x", index);
    line->tail.x = rk_param_get_int(entry, -1);

    snprintf(entry, sizeof(entry), "tripwire.%d:tail_y", index);
    line->tail.y = rk_param_get_int(entry, -1);

    // Validate the configuration
    if (line->head.x == -1 || line->head.y == -1 || line->tail.x == -1 || line->tail.y == -1) {
        return false;
    }

    return true;
}

// Function to read area configuration
bool read_area_config(int index, RockIvaArea* area) {
    if (!area || index < 0 || index >= ROCKIVA_BA_MAX_RULE_NUM) return false;

    char entry[128] = {0};
    for (int i = 0; i < 4; i++) {
        snprintf(entry, sizeof(entry), "area.%d:point%d_x", index, i);
        area->points[i].x = rk_param_get_int(entry, -1);

        snprintf(entry, sizeof(entry), "area.%d:point%d_y", index, i);
        area->points[i].y = rk_param_get_int(entry, -1);

        // Validate the configuration
        if (area->points[i].x == -1 || area->points[i].y == -1) {
            return false;
        }
    }

    return true;
}

// Function to load all configurations
void load_all_configs() {
    for (int i = 0; i < MAX_TRIPWIRE_RULES; i++) {
        RockIvaLine line;
        if (read_line_config(i, &line)) {
            set_tripwire_rule(i, &line, true); // Enable the rule after loading
        } else {
            set_tripwire_rule(i, NULL, false); // Disable the rule if config is invalid
        }
    }

    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        RockIvaArea area;
        if (read_area_config(i, &area)) {
            set_area_invasion_rule(i, &area, true); // Enable the rule after loading
        } else {
            set_area_invasion_rule(i, NULL, false); // Disable the rule if config is invalid
        }
    }
}

// Function to initialize with provided configurations
void init_tripwire_and_area_configs_with_params(TripwireRule* tripwires, int tripwire_count, 
    AreaInvasionRule* areas, int area_count) {
LOG_INFO("Initializing Tripwire and Area Configurations with parameters...\n");

// Clear all existing rules
clear_all_rules();

// Set tripwire rules
for (int i = 0; i < tripwire_count && i < MAX_TRIPWIRE_RULES; i++) {
set_tripwire_rule(i, &tripwires[i].line, tripwires[i].ruleEnable);
}

// Set area invasion rules
for (int i = 0; i < area_count && i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
set_area_invasion_rule(i, &areas[i].area, areas[i].ruleEnable);
}

// Normalize all rules
normalize_rules();

LOG_INFO("Initialization with parameters complete. Tripwires: %d, Areas: %d\n", 
tripwire_count, area_count);
}

// Original initialization function - now calls the parameterized version if no parameters provided
void init_tripwire_and_area_configs() {
LOG_INFO("Initializing Tripwire and Area Configurations...\n");
load_all_configs(); // Load all tripwire and area invasion configurations
normalize_rules();  // Normalize the loaded configurations
LOG_INFO("Initialization Complete.\n");
}

// Function to detect tripwire events
#include <string.h>

#define MAX_TRACKED_OBJECTS 128

typedef struct {
    char tracking_id[32];
    int active;
    float prev_cx, prev_cy;
    float curr_cx, curr_cy;
} TrackedObject;

TrackedObject tracked_objects[MAX_TRACKED_OBJECTS];
int tracked_object_count = 0;

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

void detect_tripwire_events() {
    for (int i = 0; i < MAX_TRIPWIRE_RULES; i++) {
        if (!g_task_rule.tripwireRules[i].ruleEnable)
            continue;

        RockIvaLine line = g_task_rule.tripwireRules[i].line;

        for (int j = 0; j < tracked_object_count; j++) {
            TrackedObject obj = tracked_objects[j];

            bool crosses = (obj.prev_cy < line.head.y && obj.curr_cy >= line.head.y) ||
                           (obj.prev_cy > line.head.y && obj.curr_cy <= line.head.y);

            if (crosses && g_event_callback) {
                const char* direction = (obj.curr_cy > obj.prev_cy) ? "top_to_bottom" : "bottom_to_top";

                char json_message[512];
                snprintf(json_message, sizeof(json_message),
                         "{\n"
                         "  \"timestamp\": \"%s\",\n"
                         "  \"camera_id\": \"CAMERA_001\",\n"
                         "  \"client_id\": \"CLIENT_ABC\",\n"
                         "  \"event_type\": \"tripwire_event\",\n"
                         "  \"sequence_number\": %d,\n"
                         "  \"tripwire_event\": {\n"
                         "    \"direction\": \"%s\",\n"
                         "    \"object\": {\n"
                         "      \"class\": \"object\",\n"
                         "      \"tracking_id\": \"%s\"\n"
                         "    },\n"
                         "    \"counters\": {\n"
                         "      \"left_to_right\": 0,\n"
                         "      \"right_to_left\": 0,\n"
                         "      \"top_to_bottom\": %d,\n"
                         "      \"bottom_to_top\": %d\n"
                         "    }\n"
                         "  }\n"
                         "}\n",
                         saix_get_current_timestamp(),
                         i,
                         direction,
                         obj.tracking_id,
                         (strcmp(direction, "top_to_bottom") == 0) ? 1 : 0,
                         (strcmp(direction, "bottom_to_top") == 0) ? 1 : 0);

                g_event_callback(i, "Tripwire", json_message);
            }
        }
    }
}

void detect_area_invasion_events() {
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        if (!g_task_rule.areaInvasionRules[i].ruleEnable)
            continue;

        RockIvaArea* area = &g_task_rule.areaInvasionRules[i].area;

        int x_min = area->points[0].x;
        int y_min = area->points[0].y;
        int x_max = area->points[2].x;
        int y_max = area->points[2].y;

        for (int j = 0; j < tracked_object_count; j++) {
            TrackedObject obj = tracked_objects[j];

            if (obj.curr_cx >= x_min && obj.curr_cx <= x_max &&
                obj.curr_cy >= y_min && obj.curr_cy <= y_max && g_event_callback) {

                char json_message[512];
                snprintf(json_message, sizeof(json_message),
                         "{\n"
                         "  \"timestamp\": \"%s\",\n"
                         "  \"camera_id\": \"CAMERA_001\",\n"
                         "  \"client_id\": \"CLIENT_ABC\",\n"
                         "  \"event_type\": \"area_invasion_event\",\n"
                         "  \"sequence_number\": %d,\n"
                         "  \"area_invasion_event\": {\n"
                         "    \"area_id\": %d,\n"
                         "    \"points\": [\n"
                         "      {\"x\": %d, \"y\": %d},\n"
                         "      {\"x\": %d, \"y\": %d},\n"
                         "      {\"x\": %d, \"y\": %d},\n"
                         "      {\"x\": %d, \"y\": %d}\n"
                         "    ]\n"
                         "  }\n"
                         "}\n",
                         saix_get_current_timestamp(),
                         i,
                         i,
                         area->points[0].x, area->points[0].y,
                         area->points[1].x, area->points[1].y,
                         area->points[2].x, area->points[2].y,
                         area->points[3].x, area->points[3].y);

                g_event_callback(i, "Area Invasion", json_message);
            }
        }
    }
}


void process_events() {
    while (true) {
        // Detect tripwire events
        detect_tripwire_events();

        // Detect area invasion events
        detect_area_invasion_events();

        // Sleep for a short duration to simulate periodic processing
        usleep(100 * 1000); // 100 ms
    }
}


int init_tripwire() {
    printf("Initializing Tripwire functionality...\n");

    // Load all tripwire and area invasion configurations
    load_all_configs();

    // Normalize the loaded configurations
    normalize_rules();

    printf("Tripwire initialization complete.");
}

//deinit tripwire
int deinit_tripwire() {
    printf("Deinitializing Tripwire functionality...");

    // Clear all tripwire and area invasion rules
    clear_all_rules();

    printf("Tripwire deinitialization complete.");
}