

void time_sync(struct timeval *tv)
{   
    ESP_LOGI(TAG,"Time Sync");
    time_t now;
	struct tm tt1;
	time(&now);
	localtime_r(&now, &tt1);
	setenv("TZ", "UTC-03:00", 1);
	tzset();
	localtime_r(&now, &tt1);
    saat.esp_to_device();

    char *rr = (char *)malloc(50);
    struct tm tt0 = {};
    saat.Get_current_date_time(rr,&tt0);
    ESP_LOGW(TAG, "Tarih/Saat %s", rr);
    free(rr);
}


void led_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    //ESP_LOGW(TAG, "LED %ld %ld", id , base, id);
    led_events_data_t* event = (led_events_data_t*) event_data;
    bool et=false, mq=false;
    
    if (id==LED_EVENTS_ETH) {gpio_set_level(LED2, event->state);}
    if (id==LED_EVENTS_MQTT) {gpio_set_level(LED1, event->state);}
    vTaskDelay(10/portTICK_PERIOD_MS);
    mq = gpio_get_level(LED1);
    et = gpio_get_level(LED2);

    if (!mq && !et) display_write(2,"          ");
    if (et) display_write(2,"ETH ERROR");
    if (!et && mq) display_write(2,"MQTT ERROR"); 
/*
    if (id==LED_EVENTS_ETH && event->state) {
        display_write(2,"ETH ERROR");
        return;
    }
    if (id==LED_EVENTS_MQTT && event->state) {        
        display_write(2,"MQTT ERROR");
        return;
    }
*/    
    
}

void ip_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    //ESP_LOGW(TAG, "IP %ld %ld", id , base, id);
 
    if ((id==IP_EVENT_STA_GOT_IP || id==IP_EVENT_ETH_GOT_IP))
              {
                // Net.wifi_update_clients();
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                NetworkConfig.home_ip = event->ip_info.ip.addr;
                NetworkConfig.home_netmask = event->ip_info.netmask.addr;
                NetworkConfig.home_gateway = event->ip_info.gw.addr;
                NetworkConfig.home_broadcast = (uint32_t)(NetworkConfig.home_ip) | (uint32_t)0xFF000000UL;
                //tcpclient.wait = false;
                #ifdef ETHERNET
                    if (id==IP_EVENT_ETH_GOT_IP) {
                        w5500.set_connect_bit();
                    }
                #endif
                if (id==IP_EVENT_STA_GOT_IP) wifi.set_connection_bit();
                ESP_LOGI(TAG, "IP Received");
                char *ss = (char*)calloc(1,100);
                sprintf(ss,"%s",Addr.to_string(NetworkConfig.home_ip));
                display_write(0,ss);
                free(ss);            
              }
}

void eth_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    //ESP_LOGW(TAG, "ETH %ld %ld", id , base, id);
    if (id==ETHERNET_EVENT_CONNECTED)
        	{
        		esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
        		esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, NetworkConfig.mac);
                
        		ESP_LOGI(TAG, "Ethernet Link Up");
        	}
    if(id==ETHERNET_EVENT_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Ethernet Link Down");
        #ifdef ETHERNET
            w5500.set_fail_bit();
        #endif    
    }
}

void wifi_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{


    if (id==13) {
                char *ss = (char*)calloc(1,100);
                sprintf(ss,"192.168.7.1");
                display_write(0,ss);
                free(ss);
    }
    if (id==WIFI_EVENT_STA_DISCONNECTED)
              {
                 // tcpclient.wait = true;
                  if (wifi.retry < NetworkConfig.WIFI_MAXIMUM_RETRY) {
                	  wifi.Station_connect();;
                      wifi.retry++;
                      ESP_LOGW(TAG, "Tekrar Baglanıyor %d",NetworkConfig.WIFI_MAXIMUM_RETRY-wifi.retry);
                                                      } else {
                      ESP_LOGE(TAG,"Wifi Başlatılamadı.. Wifi AP Modda açılıyor.");
                      wifi.ap_init();
                                                             }
              }

    if (id==WIFI_EVENT_STA_START)
        {
        wifi.retry=0;
        ESP_LOGW(TAG, "Wifi Network Connecting..");
        wifi.Station_connect();
        }

}

