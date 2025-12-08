#include "ota_server.hpp"
#include <string.h>
#include <sys/param.h>  // FIXED: Required for MIN macro
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"    // FIXED: Required for MACSTR/MAC2STR
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "OTA_SERVER";

// --- Configuration ---
#define WIFI_SSID      "WizardChess_Update"
#define WIFI_PASS      "password123" 
#define MAX_HTTP_RECV_BUFFER 1024

// --- HTML for the upload page ---
static const char *upload_script_html = 
    "<!DOCTYPE html><html>"
    "<body><h1>Wizard Chess OTA</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='update'><br><br>"
    "<input type='submit' value='Update Firmware'>"
    "</form></body></html>";

/*
 * URI Handler: GET /
 * serves the upload form
 */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, upload_script_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/*
 * URI Handler: POST /update
 * Receives the binary file and writes to OTA partition
 */
static esp_err_t update_post_handler(httpd_req_t *req)
{
    char buf[1024];
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    int received;
    int remaining = req->content_len;
    bool image_header_checked = false;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx",
             update_partition->subtype, update_partition->address);

    while (remaining > 0) {
        // Read data from the request
        if ((received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Retry
            }
            ESP_LOGE(TAG, "File upload failed!");
            if (update_handle) esp_ota_end(update_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        // Initialize OTA on first data packet
        if (!image_header_checked) {
            if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle) != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed");
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            image_header_checked = true;
        }

        // Write chunk to flash
        if (esp_ota_write(update_handle, buf, received) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed");
            esp_ota_end(update_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        remaining -= received;
    }

    // Finalize OTA
    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Set new boot partition
    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA Success! Restarting...");
    httpd_resp_sendstr(req, "Update Complete. Rebooting...");
    
    // Allow response to be sent before restarting
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

static void start_webserver()
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t update_uri = {
            .uri       = "/update",
            .method    = HTTP_POST,
            .handler   = update_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &update_uri);
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void start_ota_server()
{
    // Initialize NVS
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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, WIFI_SSID);
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    strcpy((char*)wifi_config.ap.password, WIFI_PASS);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
    
    // Start Web Server
    start_webserver();
}