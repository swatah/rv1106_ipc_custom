/**
 * Copyright (c) 2025 NeuralSense AI Private Limited
 * Trading as swatah.ai. All rights reserved.
 *
 * This file is part of the swatah.ai software stack and is licensed under
 * the terms defined in the accompanying LICENSE file. Unauthorized copying,
 * distribution, or modification of this file, via any medium, is strictly prohibited.
 *
 * For more information, visit: https://swatah.ai
*/

/**
 * @file rockiva_detection.h
 * @brief Header file for tripwire detection module
 * @details Includes definitions for tripwire and area invasion detection
 */

 #ifndef ROCKIVA_DETECTION_H
 #define ROCKIVA_DETECTION_H
 
 #include <stdint.h>
 #include <stdbool.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /* Define constants */
 #ifndef MAX_TRIPWIRE_RULES
#define MAX_TRIPWIRE_RULES 10
#endif
 
 /* Point coordinates */
/* typedef struct {
     int16_t x; 
     int16_t y; 
 } RockIvaPoint;
 */

/**
 * @brief Type definition for the event callback function
 * @param ruleId The ID of the triggered rule
 * @param eventType The type of the event (e.g., "Tripwire", "Area Invasion")
 * @param details Additional details about the event
 */
typedef void (*EventCallback)(int ruleId, const char* eventType, const char* details);

 
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
 
 /* External function declarations */
 extern TaskRule g_task_rule;
 
 /**
  * @brief Normalize a point's coordinates to valid range (0-9999)
  * @param point Pointer to the point to normalize
  */
 void normalize_point(RockIvaPoint* point);
 
 /**
  * @brief Normalize a line's coordinates
  * @param line Pointer to the line to normalize
  */
 void normalize_line(RockIvaLine* line);
 
 /**
  * @brief Normalize an area's coordinates
  * @param area Pointer to the area to normalize
  */
 void normalize_area(RockIvaArea* area);
 
 /**
  * @brief Set a tripwire rule with specified parameters
  * @param index Index of the rule to set
  * @param line Pointer to line coordinates
  * @param enable Flag to enable/disable the rule
  */
 void set_tripwire_rule(int index, RockIvaLine* line, bool enable);
 
 /**
  * @brief Set an area invasion rule with specified parameters
  * @param index Index of the rule to set
  * @param area Pointer to area coordinates
  * @param enable Flag to enable/disable the rule
  */
 void set_area_invasion_rule(int index, RockIvaArea* area, bool enable);
 
 /**
  * @brief Clear all tripwire and area invasion rules
  */
 void clear_all_rules();
 
 /**
  * @brief Normalize all active rules' coordinates
  */
 void normalize_rules();
 
 /**
  * @brief Read a tripwire line configuration from system parameters
  * @param index Index of the line configuration
  * @param line Pointer to store the line configuration
  * @return true if read successfully, false otherwise
  */
 bool read_line_config(int index, RockIvaLine* line);
 
 /**
  * @brief Read an area configuration from system parameters
  * @param index Index of the area configuration
  * @param area Pointer to store the area configuration
  * @return true if read successfully, false otherwise
  */
 bool read_area_config(int index, RockIvaArea* area);
 
 /**
  * @brief Load all tripwire and area invasion configurations
  */
 void load_all_configs();
 
 /**
  * @brief Initialize tripwire and area invasion configurations
  */
 void init_tripwire_and_area_configs();

 /**
 * @brief Register a callback function for event handling
 * @param callback The callback function to register
 */
void register_event_callback(EventCallback callback);

/**
 * @brief Detect tripwire events and invoke the callback if triggered
 */
void detect_tripwire_events();

/**
 * @brief Detect area invasion events and invoke the callback if triggered
 */
void detect_area_invasion_events();

/**
 * @brief Process events periodically (tripwire and area invasion)
 */
void process_events();
 
 /**
  * @brief Initialize tripwire functionality
  */
 int init_tripwire();

 /**
  * @brief Deinitialize tripwire functionality
  */
    int deinit_tripwire();
 
 /* Note: The following function is not defined in the provided source but is referenced */
 /**
  * @brief Get an integer parameter from the system configuration
  * @param entry Parameter name
  * @param default_value Default value to return if parameter not found
  * @return Parameter value or default value
  */
 int rk_param_get_int(const char* entry, int default_value);

void update_tracked_object(const char* id, float cx, float cy);
void detect_tripwire_events();
void detect_area_invasion_events();
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* ROCKIVA_DETECTION_H */