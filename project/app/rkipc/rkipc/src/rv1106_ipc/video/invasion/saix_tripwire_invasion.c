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

void init_tripwire_and_area_configs() {
    LOG_INFO("Initializing Tripwire and Area Configurations...\n");
    load_all_configs(); // Load all tripwire and area invasion configurations
    normalize_rules();  // Normalize the loaded configurations
    LOG_INFO("Initialization Complete.\n");
}

