#include "mqtt_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <MQTTClient.h>
#include <unistd.h>
#include <time.h>
#include "rockiva.h"

// External rk_param functions
extern int rk_param_get_int(const char *entry, int default_val);
extern const char *rk_param_get_string(const char *entry, const char *default_val);

// Static Globals
static MqttConfig mqtt_cfg;
static MQTTClient mqtt_client;
static pthread_t mqtt_thread_id;

static MqttMessage mqtt_queue[MQTT_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

volatile int mqtt_connected = 0;
volatile int mqtt_register_ack = 0;

char camera_id[64] = "CAMERA_001";
char client_id[64] = "CLIENT_ABC";
static uint64_t sequence_number = 0;

/**
 * @brief Get current UTC timestamp formatted.
 */
static void get_current_timestamp(char *buf, size_t buflen) {
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    strftime(buf, buflen, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

/*
Example Register Message:
{
  "timestamp": "2025-04-28T14:30:15Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "register",
  "sequence_number": 0
}
*/
/**
 * @brief Build register event message.
 */
void build_register_message(char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
             "{\n"
             "  \"timestamp\": \"%s\",\n"
             "  \"camera_id\": \"%s\",\n"
             "  \"client_id\": \"%s\",\n"
             "  \"event_type\": \"register\",\n"
             "  \"sequence_number\": %llu\n"
             "}",
             timestamp, camera_id, client_id, (unsigned long long)sequence_number++);
}

/*
Example Heartbeat Message:
{
  "timestamp": "2025-04-28T14:31:45Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "heartbeat",
  "sequence_number": 10
}
*/
/**
 * @brief Build heartbeat event message.
 */
void build_heartbeat_message(char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
             "{\n"
             "  \"timestamp\": \"%s\",\n"
             "  \"camera_id\": \"%s\",\n"
             "  \"client_id\": \"%s\",\n"
             "  \"event_type\": \"heartbeat\",\n"
             "  \"sequence_number\": %llu\n"
             "}",
             timestamp, camera_id, client_id, (unsigned long long)sequence_number++);
}

/*
Example System Metrics Message:
{
  "timestamp": "2025-04-28T14:32:30Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "system_metrics",
  "sequence_number": 11,
  "metrics": {
    "sensor_fps": 30.2,
    "inference_fps": 29.8,
    "ram_usage_mb": 512,
    "npu_usage_percent": 65.0,
    "cpu_load_percent": 35.0,
    "network_load_mbps": 1.5
  }
}
*/
/**
 * @brief Build system metrics event message.
 */
void build_system_metrics_message(SystemMetrics *metrics, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
             "{\n"
             "  \"timestamp\": \"%s\",\n"
             "  \"camera_id\": \"%s\",\n"
             "  \"client_id\": \"%s\",\n"
             "  \"event_type\": \"system_metrics\",\n"
             "  \"sequence_number\": %llu,\n"
             "  \"metrics\": {\n"
             "    \"sensor_fps\": %.2f,\n"
             "    \"inference_fps\": %.2f,\n"
             "    \"ram_usage_mb\": %u,\n"
             "    \"npu_usage_percent\": %.2f,\n"
             "    \"cpu_load_percent\": %.2f,\n"
             "    \"network_load_mbps\": %.2f\n"
             "  }\n"
             "}",
             timestamp, camera_id, client_id, (unsigned long long)sequence_number++,
             metrics->sensor_fps, metrics->inference_fps, metrics->ram_usage_mb,
             metrics->npu_usage_percent, metrics->cpu_load_percent, metrics->network_load_mbps);
}

/*
Example Config Update Message:
{
  "timestamp": "2025-04-28T14:33:10Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "config_update",
  "sequence_number": 12,
  "config_text": "[camera]\\nresolution=1920x1080\\nfps=30\\n..."
}
*/
/**
 * @brief Build config update event message.
 */
void build_config_update_message(const char *config_text, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
             "{\n"
             "  \"timestamp\": \"%s\",\n"
             "  \"camera_id\": \"%s\",\n"
             "  \"client_id\": \"%s\",\n"
             "  \"event_type\": \"config_update\",\n"
             "  \"sequence_number\": %llu,\n"
             "  \"config_text\": \"%s\"\n"
             "}",
             timestamp, camera_id, client_id, (unsigned long long)sequence_number++, config_text);
}


/*
Example Online Target Message:
{
  "timestamp": "2025-04-28T14:34:15Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "online_target",
  "sequence_number": 13,
  "targets": [
    {
      "class": "person",
      "score": 0.95,
      "bbox": {"x": 100, "y": 150, "width": 60, "height": 180},
      "tracking_id": "TID_00001"
    }
  ]
}
*/
/**
 * @brief Build online target event message.
 */
void build_online_target_message(RockIvaBaResult *result, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    size_t offset = snprintf(out_buffer, out_buffer_size,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"camera_id\": \"%s\",\n"
        "  \"client_id\": \"%s\",\n"
        "  \"event_type\": \"online_target\",\n"
        "  \"sequence_number\": %llu,\n"
        "  \"targets\": [\n",
        timestamp, camera_id, client_id, (unsigned long long)sequence_number++);

        for (uint32_t i = 0; i < result->objNum; ++i) {
            if (i > 0) {
                offset += snprintf(out_buffer + offset, out_buffer_size - offset, ",\n");
            }
        
            RockIvaObjectInfo *obj = &result->triggerObjects[i].objInfo;
            RockIvaRectangle *r = &obj->rect;
        
            offset += snprintf(out_buffer + offset, out_buffer_size - offset,
                "    {\n"
                "      \"class\": %u,\n"  // Assuming type is numeric ID; replace with lookup if needed
                "      \"score\": %.2f,\n"
                "      \"bbox\": {\"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d},\n"
                "      \"tracking_id\": \"TID_%05u\"\n"
                "    }",
                obj->type,                            // class/type
                obj->score / 100.0f,                  // convert score to float 0.0–1.0
                r->topLeft.x, r->topLeft.y,          // top-left x, y
                r->bottomRight.x - r->topLeft.x,     // width
                r->bottomRight.y - r->topLeft.y,     // height
                obj->objId                            // use objId as tracking ID
            );
        }

    snprintf(out_buffer + offset, out_buffer_size - offset, "\n  ]\n}");
}

/*
Example Tripwire Event Message:
{
  "timestamp": "2025-04-28T14:35:20Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "tripwire_event",
  "sequence_number": 14,
  "tripwire_event": {
    "direction": "left_to_right",
    "object": {
      "class": "person",
      "tracking_id": "TID_00005"
    },
    "counters": {
      "left_to_right": 10,
      "right_to_left": 3,
      "top_to_bottom": 0,
      "bottom_to_top": 0
    }
  }
}
*/
/**
 * @brief Build tripwire event message.
 */
void build_tripwire_event_message(TripwirePayload *payload, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"camera_id\": \"%s\",\n"
        "  \"client_id\": \"%s\",\n"
        "  \"event_type\": \"tripwire_event\",\n"
        "  \"sequence_number\": %llu,\n"
        "  \"tripwire_event\": {\n"
        "    \"direction\": \"%s\",\n"
        "    \"object\": {\n"
        "      \"class\": \"%s\",\n"
        "      \"tracking_id\": \"TID_%05u\"\n"
        "    },\n"
        "    \"counters\": {\n"
        "      \"left_to_right\": %u,\n"
        "      \"right_to_left\": %u,\n"
        "      \"top_to_bottom\": %u,\n"
        "      \"bottom_to_top\": %u\n"
        "    }\n"
        "  }\n"
        "}",
        timestamp, camera_id, client_id, (unsigned long long)sequence_number++,
        payload->direction, payload->object_class, payload->tracking_id,
        payload->left_to_right, payload->right_to_left, payload->top_to_bottom, payload->bottom_to_top);
}

/*
Example Area Rule Event Message:
{
  "timestamp": "2025-04-28T14:36:25Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "area_rule_event",
  "sequence_number": 15,
  "area_event": {
    "zone_id": 2,
    "type": "areaIn",
    "object": {
      "class": "vehicle",
      "tracking_id": "TID_00015",
      "duration_seconds": 12
    }
  }
}
*/
/**
 * @brief Build area rule event message.
 */
void build_area_rule_event_message(AreaRulePayload *payload, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"camera_id\": \"%s\",\n"
        "  \"client_id\": \"%s\",\n"
        "  \"event_type\": \"area_rule_event\",\n"
        "  \"sequence_number\": %llu,\n"
        "  \"area_event\": {\n"
        "    \"zone_id\": %u,\n"
        "    \"type\": \"%s\",\n"
        "    \"object\": {\n"
        "      \"class\": \"%s\",\n"
        "      \"tracking_id\": \"TID_%05u\",\n"
        "      \"duration_seconds\": %u\n"
        "    }\n"
        "  }\n"
        "}",
        timestamp, camera_id, client_id, (unsigned long long)sequence_number++,
        payload->zone_id, payload->type, payload->object_class, payload->tracking_id, payload->duration_seconds);
}

/*
Example Motion Event Message:
{
  "timestamp": "2025-04-28T14:37:30Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "motion_event",
  "sequence_number": 16,
  "motion": {
    "area": "Zone A",
    "strength": 78
  }
}
*/
/**
 * @brief Build motion event message.
 */
void build_motion_event_message(MotionEventPayload *payload, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"camera_id\": \"%s\",\n"
        "  \"client_id\": \"%s\",\n"
        "  \"event_type\": \"motion_event\",\n"
        "  \"sequence_number\": %llu,\n"
        "  \"motion\": {\n"
        "    \"area\": \"%s\",\n"
        "    \"strength\": %u\n"
        "  }\n"
        "}",
        timestamp, camera_id, client_id, (unsigned long long)sequence_number++,
        payload->motion_area, payload->motion_strength);
}

/*
Example Error Event Message:
{
  "timestamp": "2025-04-28T14:38:00Z",
  "camera_id": "CAMERA_001",
  "client_id": "CLIENT_ABC",
  "event_type": "error",
  "sequence_number": 17,
  "error": {
    "code": "E001",
    "message": "Sensor disconnected"
  }
}
*/
/**
 * @brief Build error event message.
 */
void build_error_message(ErrorPayload *payload, char *out_buffer, size_t out_buffer_size) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));
    snprintf(out_buffer, out_buffer_size,
        "{\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"camera_id\": \"%s\",\n"
        "  \"client_id\": \"%s\",\n"
        "  \"event_type\": \"error\",\n"
        "  \"sequence_number\": %llu,\n"
        "  \"error\": {\n"
        "    \"code\": \"%s\",\n"
        "    \"message\": \"%s\"\n"
        "  }\n"
        "}",
        timestamp, camera_id, client_id, (unsigned long long)sequence_number++,
        payload->error_code, payload->error_message);
}

/**
 * @brief Build full event message based on event type.
 */
void build_message(EventType type, void *payload, char *out_buffer, size_t out_buffer_size) {
    switch (type) {
        case EVENT_REGISTER:
            build_register_message(out_buffer, out_buffer_size);
            break;
        case EVENT_HEARTBEAT:
            build_heartbeat_message(out_buffer, out_buffer_size);
            break;
        case EVENT_SYSTEM_METRICS:
            build_system_metrics_message((SystemMetrics*)payload, out_buffer, out_buffer_size);
            break;
        case EVENT_CONFIG_UPDATE:
            build_config_update_message((const char*)payload, out_buffer, out_buffer_size);
            break;
        case EVENT_ONLINE_TARGET:
            build_online_target_message((RockIvaBaResult*)payload, out_buffer, out_buffer_size);
            break;
        case EVENT_TRIPWIRE:
            build_tripwire_event_message((TripwirePayload*)payload, out_buffer, out_buffer_size);
            break;
        case EVENT_AREA_RULE:
            build_area_rule_event_message((AreaRulePayload*)payload, out_buffer, out_buffer_size);
            break;
        case EVENT_MOTION:
            build_motion_event_message((MotionEventPayload*)payload, out_buffer, out_buffer_size);
            break;
        case EVENT_ERROR:
            build_error_message((ErrorPayload*)payload, out_buffer, out_buffer_size);
            break;
        default:
            snprintf(out_buffer, out_buffer_size, "{}");
            break;
    }
}

/**
 * @brief Load MQTT configuration from rk_param.
 */
static void load_mqtt_config(void) {
    mqtt_cfg.enabled = rk_param_get_int("mqtt.common:enabled", 0);
    strncpy(mqtt_cfg.broker_ip, rk_param_get_string("mqtt.common:broker_ip", "127.0.0.1"), sizeof(mqtt_cfg.broker_ip) - 1);
    mqtt_cfg.broker_port = (uint16_t)rk_param_get_int("mqtt.common:broker_port", 1883);
    strncpy(mqtt_cfg.username, rk_param_get_string("mqtt.common:username", ""), sizeof(mqtt_cfg.username) - 1);
    strncpy(mqtt_cfg.password_hash, rk_param_get_string("mqtt.common:password_hash", ""), sizeof(mqtt_cfg.password_hash) - 1);
    mqtt_cfg.keepalive_interval = rk_param_get_int("mqtt.common:keepalive_interval", 20);
    mqtt_cfg.cleansession = rk_param_get_int("mqtt.common:cleansession", 1);
    mqtt_cfg.use_tls = rk_param_get_int("mqtt.common:use_tls", 0);
    strncpy(mqtt_cfg.ca_cert_path, rk_param_get_string("mqtt.common:ca_cert_path", ""), sizeof(mqtt_cfg.ca_cert_path) - 1);
    strncpy(mqtt_cfg.last_will_topic, rk_param_get_string("mqtt.common:last_will_topic", "status/lastwill"), sizeof(mqtt_cfg.last_will_topic) - 1);
    strncpy(mqtt_cfg.last_will_message, rk_param_get_string("mqtt.common:last_will_message", "Astla la Vista Baby"), sizeof(mqtt_cfg.last_will_message) - 1);

    // Load publish topics
    const char *publish_keys[] = {
        "register", "heartbeat", "system_metrics", "config_update",
        "online_target", "tripwire_event", "area_rule_event", "motion_event", "error"
    };
    for (int i = 0; i < EVENT_MAX; ++i) {
        snprintf(mqtt_cfg.publish_topics[i], sizeof(mqtt_cfg.publish_topics[i]),
                 "%s", rk_param_get_string((const char *)publish_keys[i], ""));
    }

    // Load subscriptions
    mqtt_cfg.max_subscriptions = rk_param_get_int("mqtt.common:max_subscriptions", 10);
    mqtt_cfg.subscription_topics = malloc(mqtt_cfg.max_subscriptions * MAX_TOPIC_LENGTH);
    mqtt_cfg.subscription_count = 0;

    for (int i = 0; i < mqtt_cfg.max_subscriptions; ++i) {
        char key[64];
        snprintf(key, sizeof(key), "mqtt.subscribe:topic%d", i);
        const char *sub = rk_param_get_string(key, "");
        if (sub && sub[0] != '\0') {
            strncpy(mqtt_cfg.subscription_topics[mqtt_cfg.subscription_count], sub, MAX_TOPIC_LENGTH - 1);
            mqtt_cfg.subscription_count++;
        }
    }
}

/**
 * @brief Push an MQTT event message to the internal queue.
 */
void mqtt_queue_push(EventType type, void *payload) {
    if (type >= EVENT_MAX || mqtt_cfg.publish_topics[type][0] == '\0') {
        // No topic defined for this event, drop silently
        return;
    }
    pthread_mutex_lock(&queue_lock);
    int next_tail = (queue_tail + 1) % MQTT_QUEUE_SIZE;
    if (next_tail == queue_head) {
        // Queue full, drop oldest
        queue_head = (queue_head + 1) % MQTT_QUEUE_SIZE;
    }
    build_message(type, payload, mqtt_queue[queue_tail].payload, sizeof(mqtt_queue[queue_tail].payload));
    mqtt_queue[queue_tail].event_type = type;
    mqtt_queue[queue_tail].qos = 1;
    mqtt_queue[queue_tail].retained = 0;
    queue_tail = next_tail;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_lock);
}

/**
 * @brief Dummy handler for incoming subscribed MQTT messages.
 */
void mqtt_message_arrived(char *topic, char *payload) {
    printf("[MQTT] Incoming message on topic [%s]: %s\n", topic, payload);
}

/**
 * @brief MQTT background thread for publishing and subscribing.
 */
void *mqtt_thread(void *arg) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions will_opts = MQTTClient_willOptions_initializer;
    MQTTClient_create(&mqtt_client, mqtt_cfg.broker_ip, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    conn_opts.keepAliveInterval = mqtt_cfg.keepalive_interval;
    conn_opts.cleansession = mqtt_cfg.cleansession;
    conn_opts.username = mqtt_cfg.username;
    conn_opts.password = mqtt_cfg.password_hash;

    if (mqtt_cfg.last_will_topic[0] != '\0') {
        will_opts.topicName = mqtt_cfg.last_will_topic;
        will_opts.message = mqtt_cfg.last_will_message;
        will_opts.qos = 1;
        will_opts.retained = 0;
        conn_opts.will = &will_opts;
    }

    if (MQTTClient_connect(mqtt_client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("[MQTT] Connect failed!\n");
        return NULL;
    }
    mqtt_connected = 1;

    // Subscribe to topics
    for (int i = 0; i < mqtt_cfg.subscription_count; ++i) {
        MQTTClient_subscribe(mqtt_client, mqtt_cfg.subscription_topics[i], 1);
    }

    while (1) {
        pthread_mutex_lock(&queue_lock);
        while (queue_head == queue_tail) {
            pthread_cond_wait(&queue_cond, &queue_lock);
        }
        MqttMessage msg = mqtt_queue[queue_head];
        queue_head = (queue_head + 1) % MQTT_QUEUE_SIZE;
        pthread_mutex_unlock(&queue_lock);

        if (mqtt_connected && mqtt_register_ack) {
            const char *topic = mqtt_cfg.publish_topics[msg.event_type];
            MQTTClient_publish(mqtt_client, topic, strlen(msg.payload), msg.payload, msg.qos, msg.retained, NULL);
        }
    }
    return NULL;
}

/**
 * @brief Initialize MQTT Manager.
 */
void mqtt_init(void) {
    load_mqtt_config();
    if (!mqtt_cfg.enabled) {
        printf("[MQTT] MQTT disabled by config.\n");
        return;
    }
    pthread_create(&mqtt_thread_id, NULL, mqtt_thread, NULL);
}
