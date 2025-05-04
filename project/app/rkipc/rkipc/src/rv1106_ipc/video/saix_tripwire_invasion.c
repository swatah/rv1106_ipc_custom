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
void detect_tripwire_events() {
    for (int i = 0; i < MAX_TRIPWIRE_RULES; i++) {
        if (g_task_rule.tripwireRules[i].ruleEnable) {
            // Simulate event detection (replace with actual detection logic)
            bool eventTriggered = (rand() % 10 == 0); // Randomly trigger events for testing
            if (eventTriggered && g_event_callback) {
                char details[128];
                snprintf(details, sizeof(details), "Tripwire %d triggered at coordinates: Head(%d, %d), Tail(%d, %d)",
                         i,
                         g_task_rule.tripwireRules[i].line.head.x,
                         g_task_rule.tripwireRules[i].line.head.y,
                         g_task_rule.tripwireRules[i].line.tail.x,
                         g_task_rule.tripwireRules[i].line.tail.y);
                g_event_callback(i, "Tripwire", details);
            }
        }
    }
}

// Function to detect area invasion events
void detect_area_invasion_events() {
    for (int i = 0; i < ROCKIVA_BA_MAX_RULE_NUM; i++) {
        if (g_task_rule.areaInvasionRules[i].ruleEnable) {
            // Simulate event detection (replace with actual detection logic)
            bool eventTriggered = (rand() % 15 == 0); // Randomly trigger events for testing
            if (eventTriggered && g_event_callback) {
                char details[128];
                snprintf(details, sizeof(details), "Area Invasion %d triggered at points: (%d, %d), (%d, %d), (%d, %d), (%d, %d)",
                         i,
                         g_task_rule.areaInvasionRules[i].area.points[0].x,
                         g_task_rule.areaInvasionRules[i].area.points[0].y,
                         g_task_rule.areaInvasionRules[i].area.points[1].x,
                         g_task_rule.areaInvasionRules[i].area.points[1].y,
                         g_task_rule.areaInvasionRules[i].area.points[2].x,
                         g_task_rule.areaInvasionRules[i].area.points[2].y,
                         g_task_rule.areaInvasionRules[i].area.points[3].x,
                         g_task_rule.areaInvasionRules[i].area.points[3].y);
                g_event_callback(i, "Area Invasion", details);
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