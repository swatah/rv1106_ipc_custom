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
 #define ROCKIVA_AREA_NUM_MAX 10
 #define ROCKIVA_BA_MAX_RULE_NUM 10
 #define MAX_TRIPWIRE_RULES 10
 
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
 
 /* Note: The following function is not defined in the provided source but is referenced */
 /**
  * @brief Get an integer parameter from the system configuration
  * @param entry Parameter name
  * @param default_value Default value to return if parameter not found
  * @return Parameter value or default value
  */
 int rk_param_get_int(const char* entry, int default_value);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* ROCKIVA_DETECTION_H */