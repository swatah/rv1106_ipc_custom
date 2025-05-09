#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_QUEUE_SIZE 100
#define MAX_TOPIC_LENGTH 128

/**
 * @brief Types of MQTT events to handle and publish.
 */
typedef enum {
    EVENT_REGISTER = 0,
    EVENT_HEARTBEAT,
    EVENT_SYSTEM_METRICS,
    EVENT_CONFIG_UPDATE,
    EVENT_ONLINE_TARGET,
    EVENT_TRIPWIRE,
    EVENT_AREA_RULE,
    EVENT_MOTION,
    EVENT_ERROR,
    EVENT_MAX
} EventType;

/**
 * @brief Structure to hold an outgoing MQTT message.
 */
typedef struct {
    EventType event_type;     /**< Event type */
    char payload[2048];       /**< JSON payload */
    int qos;                  /**< MQTT QoS level */
    int retained;             /**< Retained flag */
} MqttMessage;

/**
 * @brief Structure to hold MQTT configuration loaded from ini/parameters.
 */
typedef struct {
    int enabled;                        /**< MQTT enabled flag */
    char broker_ip[128];                /**< Broker IP or hostname */
    uint16_t broker_port;               /**< Broker port */
    char username[64];                  /**< Username */
    char password_hash[65];              /**< Password hash */
    int keepalive_interval;             /**< Keepalive interval */
    int cleansession;                   /**< Clean session flag */
    int use_tls;                        /**< TLS enabled */
    char ca_cert_path[256];              /**< CA certificate path */
    char last_will_topic[128];           /**< Last Will topic */
    char last_will_message[256];         /**< Last Will message */

    char publish_topics[EVENT_MAX][MAX_TOPIC_LENGTH]; /**< Publish topics per event */

    int max_subscriptions;                       /**< Max allowed subscriptions */
    char (*subscription_topics)[MAX_TOPIC_LENGTH]; /**< Dynamically allocated subscription topics */
    int subscription_count;                      /**< Actual subscription count loaded */
} MqttConfig;

/**
 * @brief System Metrics payload for reporting device statistics.
 */
typedef struct {
    float sensor_fps;            /**< Sensor frames per second */
    float inference_fps;         /**< Inference frames per second */
    uint32_t ram_usage_mb;        /**< RAM usage in MB */
    float npu_usage_percent;     /**< NPU utilization percentage */
    float cpu_load_percent;      /**< CPU load percentage */
    float network_load_mbps;     /**< Network bandwidth usage in Mbps */
} SystemMetrics;

/**
 * @brief Payload for Tripwire event.
 */
typedef struct {
    const char *direction;        /**< Direction of movement (e.g., left_to_right) */
    const char *object_class;     /**< Class name of object (e.g., "person") */
    uint32_t tracking_id;         /**< Tracking ID of the object */
    uint32_t left_to_right;       /**< Counter for left to right crossings */
    uint32_t right_to_left;       /**< Counter for right to left crossings */
    uint32_t top_to_bottom;       /**< Counter for top to bottom crossings */
    uint32_t bottom_to_top;       /**< Counter for bottom to top crossings */
} TripwirePayload;

/**
 * @brief Payload for Area Rule event.
 */
typedef struct {
    uint32_t zone_id;             /**< Zone ID where event occurred */
    const char *type;             /**< Type of area event (areaIn, areaOut, areaStay) */
    const char *object_class;     /**< Class name of object */
    uint32_t tracking_id;         /**< Tracking ID of the object */
    uint32_t duration_seconds;    /**< Time spent in area (in seconds) */
} AreaRulePayload;

/**
 * @brief Payload for Motion detection event.
 */
typedef struct {
    const char *motion_area;      /**< Area where motion was detected */
    uint32_t motion_strength;     /**< Motion strength score */
} MotionEventPayload;

/**
 * @brief Payload for Error reporting.
 */
typedef struct {
    const char *error_code;       /**< Error code identifier */
    const char *error_message;    /**< Human-readable error message */
} ErrorPayload;

/**
 * @brief Initialize MQTT client, read configuration and start worker thread.
 */
void mqtt_init(void);

/**
 * @brief Push an event and optional payload into the MQTT outgoing queue.
 * 
 * @param event Event type.
 * @param payload Pointer to event-specific payload structure.
 */
void mqtt_queue_push(EventType event, void *payload);

/**
 * @brief Build a JSON-formatted message based on event type and payload.
 * 
 * @param event Event type.
 * @param payload Optional pointer to payload data.
 * @param out_buffer Output JSON buffer.
 * @param out_buffer_size Size of output buffer.
 */
void build_message(EventType event, void *payload, char *out_buffer, size_t out_buffer_size);

#ifdef __cplusplus
}
#endif

#endif // MQTT_MANAGER_H
