#ifndef _ETHERNET_J60_H
#define _ETHERNET_J60_H

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_eth_enc28j60.h"
#include "driver/spi_master.h"

#include "esp_event.h"
#include "freertos/event_groups.h"

#include "core.h"

#define ETH_CONNECTED_BIT BIT0
#define ETH_FAIL_BIT      BIT1




class Ethj60 {
    public:
      Ethj60() {};
      ~Ethj60() {};

      esp_err_t start(home_network_config_t cnf, home_global_config_t gcnf);
      void set_connect_bit(void) {
    	                           xEventGroupSetBits(Event, ETH_CONNECTED_BIT);
                                   Active=true;
                                 }
      void set_fail_bit(void) {
    	                        xEventGroupSetBits(Event, ETH_FAIL_BIT);
    	                        Active=false;
                              }
      bool is_active(void){return Active;}

    private:
       EventGroupHandle_t Event;
       home_network_config_t mConfig;
       home_global_config_t gConfig;
       bool Active=false;
    protected:  
        
};

#endif
