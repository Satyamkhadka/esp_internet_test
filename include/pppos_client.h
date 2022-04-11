
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_netif.h"
#include "esp_netif_ppp.h"

#include "esp_modem.h"
#include "esp_modem_netif.h"

TaskHandle_t pppos_handle;

esp_netif_t *pppos_netif;

void start_pppos(void);
static bool ready = false;