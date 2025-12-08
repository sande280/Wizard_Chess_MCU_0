#include "ota_server.hpp"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "OTA_SERVER";

// --- Configuration ---
#define WIFI_SSID      "WizardChess_boad"
#define WIFI_PASS      "Wizrd"
#define LOG_BUFFER_SIZE (4 * 1024) 

static RingbufHandle_t log_ringbuf = NULL;
static httpd_handle_t server = NULL;

// --- HTML for the OTA Upload Page ---
static const char *upload_script_html = 
    "<!DOCTYPE html><html><body>"
    "<h1>Wizard Chess OTA</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='update'><br><br>"
    "<input type='submit' value='Update Firmware'>"
    "</form>"
    "<br><a href='/log'><button>Go to Live Logs</button></a>"
    "</body></html>";

// --- HTML for the Log Viewer ---
static const char *log_viewer_html = 
    "<!DOCTYPE html><html><head><title>ESP32 Logs</title>"
    "<style>"
    "body { background-color: #1e1e1e; color: #00ff00; font-family: monospace; }"
    "#logs { white-space: pre-wrap; }"
    "</style></head><body>"
    "<h1>Live Log Stream</h1>"
    "<div id='logs'></div>"
    "<script>"
    "var ws = new WebSocket('ws://' + location.host + '/ws');"
    "ws.onmessage = function(event) {"
    "  var logDiv = document.getElementById('logs');"
    "  logDiv.innerHTML += event.data;"
    "  window.scrollTo(0, document.body.scrollHeight);"
    "};"
    "</script></body></html>";

// Custom vprintf handler to divert logs to RingBuffer
int web_log_vprintf(const char *fmt, va_list args) {
    // 1. Print to Serial (Standard UART)
    int ret = vprintf(fmt, args);

    // 2. Print to RingBuffer for Web
    if (log_ringbuf) {
        char buf[128];
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        if (len > 0) {
            // Send to ringbuffer (don't wait if full, just drop)
            xRingbufferSend(log_ringbuf, buf, len, 0);
        }
    }
    return ret;
}

// Helper for other files to write to web log
void send_to_web_log(const char* fmt, va_list args) {
    web_log_vprintf(fmt, args);
}

// Handler: GET / (OTA Page)
static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, upload_script_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler: GET /log (Log Viewer)
static esp_err_t log_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, log_viewer_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler: WebSocket /ws (Stream logs)
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // Handshake
        return ESP_OK;
    }

    // We only care about sending data OUT, not receiving
    // But we must handle the request to keep connection alive
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // Read (drain) any incoming data
    uint8_t buf[16];
    ws_pkt.payload = buf;
    httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
    
    return ESP_OK;
}

// Task to push logs from RingBuffer to WebSocket clients
static void log_pusher_task(void *param) {
    char *item;
    size_t item_size;
    
    while (1) {
        // Wait for data in the ring buffer
        item = (char *)xRingbufferReceive(log_ringbuf, &item_size, pdMS_TO_TICKS(100));
        
        if (item != NULL) {
            // Get all WebSocket fds
            size_t fds = 4; // Max clients
            int client_fds[4];
            if (httpd_get_client_list(server, &fds, client_fds) == ESP_OK) {
                for (int i = 0; i < fds; i++) {
                    // Check if this client is a WebSocket
                    if (httpd_ws_get_fd_info(server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                        httpd_ws_frame_t ws_pkt;
                        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                        ws_pkt.payload = (uint8_t *)item;
                        ws_pkt.len = item_size;
                        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                        
                        // Send asynchronously
                        httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
                    }
                }
            }
            vRingbufferReturnItem(log_ringbuf, (void *)item);
        } else {
            // No logs, yield
            vTaskDelay(pdMS_TO_TICKS(50)); 
        }
    }
}

static esp_err_t update_post_handler(httpd_req_t *req) {
    // ... (Keep existing OTA logic from previous answer here) ...
    // Note: For brevity, I am not repeating the full OTA logic here.
    // Insert the full update_post_handler code from the previous turn.
    
    // Placeholder to prevent compile error if you copy-paste blindly:
    httpd_resp_sendstr(req, "OTA Code Placeholder"); 
    return ESP_OK;
}

static void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 10; // Increase handler count

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { "/", HTTP_GET, index_get_handler, NULL };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t update_uri = { "/update", HTTP_POST, update_post_handler, NULL };
        httpd_register_uri_handler(server, &update_uri);

        httpd_uri_t log_uri = { "/log", HTTP_GET, log_get_handler, NULL };
        httpd_register_uri_handler(server, &log_uri);

        httpd_uri_t ws_uri = { "/ws", HTTP_GET, ws_handler, NULL, true };
        httpd_register_uri_handler(server, &ws_uri);
    }
}

// ... (Keep existing wifi_event_handler from previous answer) ...
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    // Insert your existing WiFi event handler logic here
}

void start_ota_server() {
    // 1. Initialize Log Ringbuffer
    log_ringbuf = xRingbufferCreate(LOG_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    
    // 2. Redirect ESP_LOG calls to our custom handler
    esp_log_set_vprintf(web_log_vprintf);

    // 3. Start task to push logs to WebSocket
    xTaskCreate(log_pusher_task, "log_pusher", 4096, NULL, 5, NULL);

    // 4. Standard Init (NVS, Netif, Loop, AP Config)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, WIFI_SSID);
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    strcpy((char*)wifi_config.ap.password, WIFI_PASS);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    if (strlen(WIFI_PASS) == 0) wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 5. Start Web Server
    start_webserver();
}