/* Ethernet Basic Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "eth.h"
static const char *TAG = "eth_example";
#define MY_DNS_IP_ADDR 0x08080808 // 8.8.8.8

static esp_err_t set_dhcps_dns(esp_netif_t *netif)
{
    // esp_netif_ip_info_t ip;
    // IP4_ADDR(&ip.ip, 192, 168, 2, 5);
    // IP4_ADDR(&ip.gw, 192, 168, 2, 5);
    // IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

    // esp_netif_dhcps_stop(netif);
    ESP_LOGI(TAG, "%s", esp_err_to_name(esp_netif_dhcpc_stop(netif)));

    ESP_LOGI(TAG, "%s", esp_err_to_name(esp_netif_dhcps_stop(netif)));

    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = htonl(MY_DNS_IP_ADDR);
    dns.ip.type = IPADDR_TYPE_V4;
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
    ESP_LOGI(TAG, "%s", esp_err_to_name(esp_netif_dhcps_start(netif)));
    ESP_LOGI(TAG, "%s", esp_err_to_name(esp_netif_dhcpc_stop(netif)));

    return ESP_OK;
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

void start_eth(void)
{

    esp_netif_ip_info_t my_ap_ip = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 11, 1)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 11, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
    };
    ESP_LOGI(TAG, "Ethernet Trying to set IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, IPSTR, IP2STR(&(my_ap_ip.ip)));
    ESP_LOGI(TAG, IPSTR, IP2STR(&(my_ap_ip.netmask)));
    ESP_LOGI(TAG, IPSTR, IP2STR(&(my_ap_ip.gw)));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    // Create new default instance of esp-netif for Ethernet
    // esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_inherent_config_t eth_behav_cfg = {
        .get_ip_event = IP_EVENT_ETH_GOT_IP,
        .lost_ip_event = 0,
        .flags = (ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP),
        .ip_info = (esp_netif_ip_info_t *)&my_ap_ip,
        .if_key = "ETHDHCPS",
        .if_desc = "eth",
        .route_prio = 5};
    eth_behav_cfg.flags &= ~ESP_NETIF_DHCP_CLIENT;
    esp_netif_config_t my_eth_cfg = {.base = &eth_behav_cfg, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    eth_netif = esp_netif_new(&my_eth_cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ETH_PHY_RST_GPIO;
    mac_config.smi_mdc_gpio_num = CONFIG_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);

    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    esp_eth_ioctl(eth_handle, ETH_CMD_S_FLOW_CTRL, (void *)true);

    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    assert(eth_netif);

    set_dhcps_dns(eth_netif);
    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
    /* start Ethernet driver state machine */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}