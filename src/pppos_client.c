#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_log.h"
#include "sim800.h"
#include "pppos_client.h"

static const char *TAG = "pppos_example";
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int STOP_BIT = BIT1;
static const int DISCONNECT_BIT = BIT2;

static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case ESP_MODEM_EVENT_PPP_START:
        ESP_LOGI(TAG, "Modem PPP Started");
        break;
    case ESP_MODEM_EVENT_PPP_STOP:
        ESP_LOGW(TAG, "Modem PPP Stopped");
        ESP_LOGI(TAG, "STOP_BIT set ");
        xEventGroupSetBits(event_group, STOP_BIT);
        ESP_LOGI(TAG, "DISCONNECT_BIT set ");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
        break;
    case ESP_MODEM_EVENT_UNKNOWN:
        ESP_LOGW(TAG, "Unknown line received: %s", (char *)event_data);
        break;
    default:
        break;
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGW(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = *(esp_netif_t **)event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "CONNECT_BIT set ");
        xEventGroupSetBits(event_group, CONNECT_BIT);
        ESP_LOGI(TAG, "GOT ip event!!!");
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGW(TAG, "Modem Disconnect from PPP Server");
        ESP_LOGI(TAG, "DISCONNECT_BIT set ");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
        ESP_LOGI(TAG, "STOP_BIT set ");
        xEventGroupSetBits(event_group, STOP_BIT);
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

void start_pppos(void)
{

    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_PAP;

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    event_group = xEventGroupCreate();

    /* create dte object */
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
    config.rts_io_num = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
    config.cts_io_num = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;
    config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
    config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
    config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
    config.event_task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
    config.event_task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
    config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;

    modem_dte_t *dte = esp_modem_dte_init(&config);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler, ESP_EVENT_ANY_ID, NULL));

    // Init netif object
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    pppos_netif = esp_netif_new(&cfg);
    assert(pppos_netif);

    void *modem_netif_adapter = esp_modem_netif_setup(dte);
    esp_modem_netif_set_default_handlers(modem_netif_adapter, pppos_netif);

    while (1)
    {
        modem_dce_t *dce = NULL;
        /* create dce object */
        dce = sim800_init(dte);

        if (dce == NULL)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGE(TAG, "dce null,. restarting module");
            continue;
        }
        ESP_ERROR_CHECK(dce->set_flow_ctrl(dce, MODEM_FLOW_CONTROL_NONE));
        ESP_ERROR_CHECK(dce->store_profile(dce));
        /* Print Module ID, Operator, IMEI, IMSI */
        ESP_LOGI(TAG, "Module: %s", dce->name);
        ESP_LOGI(TAG, "Operator: %s", dce->oper);
        ESP_LOGI(TAG, "IMEI: %s", dce->imei);
        ESP_LOGI(TAG, "IMSI: %s", dce->imsi);
        /* Get signal quality */
        uint32_t rssi = 0, ber = 0;
        ESP_ERROR_CHECK(dce->get_signal_quality(dce, &rssi, &ber));
        ESP_LOGI(TAG, "rssi: %d, ber: %d", rssi, ber);
        /* Get battery voltage */
        uint32_t voltage = 0, bcs = 0, bcl = 0;
        ESP_ERROR_CHECK(dce->get_battery_status(dce, &bcs, &bcl, &voltage));
        ESP_LOGI(TAG, "Battery voltage: %d mV", voltage);

        /* attach the modem to the network interface */
        if (esp_netif_attach(pppos_netif, modem_netif_adapter))
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGE(TAG, "cant attach to netif");
            continue;
        }
        /* Wait for IP address */
        ESP_LOGI(TAG, "-------Waiting to connect------");

        xEventGroupWaitBits(event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "--------connected--------------");

        xEventGroupWaitBits(event_group, STOP_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_LOGW(TAG, "--------DISCONNECT -- restaarting---------");
    }

    /* Unregister events, destroy the netif adapter and destroy its esp-netif instance */
    esp_modem_netif_clear_default_handlers(modem_netif_adapter);
    esp_modem_netif_teardown(modem_netif_adapter);
    esp_netif_destroy(pppos_netif);

    ESP_ERROR_CHECK(dte->deinit(dte));
}