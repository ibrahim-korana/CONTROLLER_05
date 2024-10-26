#ifndef _NETWORK_H
#define _NETWORK_H

#include "esp_log.h"
#include "esp_system.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "../core.h"
//#include "AsyncUDP.h"
//#include "Arduino.h"

#define BTT0 0x001
#define BTT1 0x002

#define WIFI_CONNECTED_BIT BTT0
#define WIFI_FAIL_BIT      BTT1


#define DEFAULT_AP_CHANNEL 1
#define AP_MAX_STA_CONN   20
//#define WIFI_MAXIMUM_RETRY  10


class Network {
    public:
      Network() {
    	  Event = xEventGroupCreate();
      };
      ~Network() {};

      bool init(home_network_config_t cnf);
      esp_err_t Station_Start(void);
      void Station_connect(void) {esp_wifi_connect();};
      void set_connection_bit(void) {xEventGroupSetBits(Event, WIFI_CONNECTED_BIT);retry=0;}
      void set_fail_bit(void) {xEventGroupSetBits(Event, WIFI_FAIL_BIT);}

      uint8_t retry = 0;

      esp_err_t ap_connect(void);
      bool ap_init(void);


      //bool start(void);
      

    private:
       EventGroupHandle_t Event;
       home_network_config_t NetConfig;
       bool net_init(void);
             
       bool sta_init(void);
       
       void set_full_error(void);
       esp_timer_handle_t clock_timer;
       
       
    protected:  
       
        
};



#endif
