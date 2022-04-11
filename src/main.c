#include "esp_log.h"
#include "esp_err.h"
#include "eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "wiphy.h"
#include "veryhotspot.h"
#include "esp_event.h"
#include "test.h"
#include "pppos_client.h"
const char *TAG = "MAIN";

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // start whatever you want to start
    // make sure to set priority of netif routes if multiple interface are used.
    // xTaskCreate(start_pppos, "pppos", 4096, NULL, 2, NULL);
    // start_eth();
    // start_wifi();
    // test_internet();
    // startveryhotspot();
}