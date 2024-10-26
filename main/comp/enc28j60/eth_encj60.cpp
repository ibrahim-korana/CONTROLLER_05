
#include "eth_encj60.h"

static const char *TAG = "ETH_ENCJ60";

#define EXAMPLE_ENC28J60_MISO_GPIO 16
#define EXAMPLE_ENC28J60_MOSI_GPIO 17
#define EXAMPLE_ENC28J60_SCLK_GPIO 5
#define EXAMPLE_ENC28J60_SPI_HOST (spi_host_device_t)1
#define EXAMPLE_ENC28J60_SPI_CLOCK_MHZ 8
#define EXAMPLE_ENC28J60_CS_GPIO 18
#define EXAMPLE_ENC28J60_INT_GPIO 35

/*
#define ETH_MISO 16
#define ETH_MOSI 17
#define ETH_CS 18
#define ETH_HOST (spi_host_device_t) 1
#define ETH_SCLK 5
#define ETH_INT 35 
#define ETH_CLOCK_MHZ 8
#define ETH_SPI_PHY_RST0_GPIO 19
#define ETH_SPI_PHY_ADDR0 1
*/

//ESP_EVENT_DEFINE_BASE(LED_EVENTS);

esp_err_t Ethj60::start(home_network_config_t cnf, home_global_config_t gcnf)
{     
    mConfig = cnf;
    gConfig = gcnf;
    Event = xEventGroupCreate();
    xEventGroupClearBits(Event, ETH_CONNECTED_BIT | ETH_FAIL_BIT);
    Active = false;

    gpio_install_isr_service(0);
    if(esp_netif_init()!=ESP_OK) return ESP_FAIL;
    
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

     if (eth_netif)
    {
        if (mConfig.ipstat==STATIC_IP)
            {  
                esp_netif_ip_info_t info_t = {};
                memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
                if(esp_netif_dhcpc_stop(eth_netif)!=ESP_OK) return false;
                info_t.ip.addr = esp_ip4addr_aton((const char *)mConfig.ip);
                info_t.netmask.addr = esp_ip4addr_aton((const char *)mConfig.netmask);
                info_t.gw.addr = esp_ip4addr_aton((const char *)mConfig.gateway);
                esp_netif_set_ip_info(eth_netif, &info_t);
            }
    }

    spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = EXAMPLE_ENC28J60_MISO_GPIO;
        buscfg.mosi_io_num = EXAMPLE_ENC28J60_MOSI_GPIO;
        buscfg.sclk_io_num = EXAMPLE_ENC28J60_SCLK_GPIO;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
    if(spi_bus_initialize(EXAMPLE_ENC28J60_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO)!=ESP_OK) return ESP_FAIL;

    spi_device_interface_config_t spi_devcfg = {};
        spi_devcfg.mode = 0;
        spi_devcfg.clock_speed_hz = EXAMPLE_ENC28J60_SPI_CLOCK_MHZ * 1000 * 1000;
        spi_devcfg.spics_io_num = EXAMPLE_ENC28J60_CS_GPIO;
        spi_devcfg.queue_size = 20;
        spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(EXAMPLE_ENC28J60_SPI_CLOCK_MHZ);

    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(EXAMPLE_ENC28J60_SPI_HOST, &spi_devcfg);
    enc28j60_config.int_gpio_num = EXAMPLE_ENC28J60_INT_GPIO;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 0; 
    phy_config.reset_gpio_num = -1; 
    esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    if(esp_eth_driver_install(&eth_config, &eth_handle)!=ESP_OK) return ESP_FAIL;

    uint8_t aaa[] = {
        0x02, 0x00, 0x00, 0x12, 0x34, 0x56
    };
    aaa[2] = gConfig.odano._room.binano;
    aaa[3] = gConfig.odano._room.katno;
    aaa[4] = gConfig.odano._room.odano;
    aaa[5] = gConfig.odano._room.altodano;
    mac->set_addr(mac, aaa);

    if (emac_enc28j60_get_chip_info(mac) < ENC28J60_REV_B5 && EXAMPLE_ENC28J60_SPI_CLOCK_MHZ < 8) {
        ESP_LOGE(TAG, "SPI frequency must be at least 8 MHz for chip revision less than 5");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    if(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle))!=ESP_OK) return ESP_FAIL;
    // Register user defined event handers
    //ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    //ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* It is recommended to use ENC28J60 in Full Duplex mode since multiple errata exist to the Half Duplex mode */

    eth_duplex_t duplex = ETH_DUPLEX_FULL;
    if(esp_eth_ioctl(eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex)!=ESP_OK) return ESP_FAIL;


    const TickType_t xTicksToWait = 60000 / portTICK_PERIOD_MS;
    EventBits_t uxBits;
    led_events_data_t ld={};
    ld.state = 1;
    ESP_ERROR_CHECK(esp_event_post(LED_EVENTS, LED_EVENTS_ETH, &ld, sizeof(led_events_data_t), portMAX_DELAY));

    /* start Ethernet driver state machine */
    if(esp_eth_start(eth_handle)!=ESP_OK) return ESP_FAIL;
    
    
    uxBits =  xEventGroupWaitBits(Event,
    		ETH_CONNECTED_BIT | ETH_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            xTicksToWait);

      //printf("UBITS %ld\n",uxBits);
      if (uxBits==0) return ESP_FAIL;
      ld.state = 0;
      ESP_ERROR_CHECK(esp_event_post(LED_EVENTS, LED_EVENTS_ETH, &ld, sizeof(led_events_data_t), portMAX_DELAY));

    return ESP_OK;
};
