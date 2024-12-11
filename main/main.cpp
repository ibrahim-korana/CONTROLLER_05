
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cjson.h"
#include "esp_event.h"
#include <rom/ets_sys.h>

#include "iot_button.h"
#include "home_i2c.h"
#include "home_8563.h"
#include "core.h"
#include "comp/storage.h"
#include "comp/rs485.h"
#include "comp/ww5500.h"
#include "comp/enc28j60/ethernet.h"
#include "comp/dali.h"
#include "dali_global.h"
#include "ssd1306.h"
#include "esp32Time.h"
#include "IPTool.h"
#include "esp_timer.h"
#include "comp/tcpserver.h"
#include "comp/tcpclient.h"
#include "comp/udpserver.h"
#include "comp/jsontool.h"
#include "comp/network.h"
#include "comp/cron/cron.h"


#define FATAL_MSG(a, str)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        abort();                                         \
    }


#define LED1 GPIO_NUM_2
#define LED2 GPIO_NUM_15

#define BUTTON1 GPIO_NUM_39
#define BUTTON2 GPIO_NUM_36
#define BUTTON3 GPIO_NUM_14

#define SDA GPIO_NUM_26
#define SCL GPIO_NUM_27

#define RS485_TX GPIO_NUM_13
#define RS485_RX GPIO_NUM_4
#define RS485_DIR GPIO_NUM_12

#define DALI1_TX GPIO_NUM_33
#define DALI1_RX GPIO_NUM_32
#define DALI1_ADC GPIO_NUM_34

#define DALI2_TX GPIO_NUM_17
#define DALI2_RX GPIO_NUM_16
#define DALI2_ADC GPIO_NUM_35

#define ETHERNET
#define ETH_W5500
#define SSD1306
#define CLOCK


ESP_EVENT_DEFINE_BASE(RS485_DATA_EVENTS);
ESP_EVENT_DEFINE_BASE(LED_EVENTS);
ESP_EVENT_DEFINE_BASE(TCP_SERVER_EVENTS);
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENTS);

#define BEKLE vTaskDelay(20 / portTICK_PERIOD_MS);
#define MAX_GATEWAY 40

#define MBEKLE vTaskDelay(5 / portTICK_PERIOD_MS);


cron_job *jobs[20] = {};
uint8_t cron_count = 0;
button_handle_t btn1, btn2, btn3;
Home_i2c bus = Home_i2c();
Home_8563 saat =  Home_8563();
Storage disk;
RS485_config_t rs485_cfg={};
RS485 rs485 = RS485();
Dali dali;
#ifdef ETHERNET
    #ifdef ETH_W5500
        WW5500 w5500 = WW5500(); //w5500
    #else    
        Ethernet w5500 = Ethernet(); //enc28j60
    #endif 
#endif    

SSD1306_t ekran;
IPAddr Addr = IPAddr();
Network wifi = Network();
char *BroadcastAdr=NULL; 

config_t GlobalConfig;
home_network_config_t NetworkConfig = {};
TcpServer tcpserver = TcpServer();
UdpServer udpserver = UdpServer();


bool Net_Connect = false;

const char *TAG ="MAIN";

void arc_power(uint8_t adr, uint8_t gurup, uint8_t power);
void off(uint8_t adr, uint8_t gurup);
void command_action(uint8_t adr, uint8_t gurup, uint8_t comm);

#define GLOBAL_FILE "/config/config.bin"
#define NETWORK_FILE "/config/network.bin"
#define GEAR_FILE "/config/gear.bin"
#define GURUP_FILE "/config/gurup.bin"
#define ZAMAN_FILE "/config/zaman.bin"
#define CLIENT_FILE "/config/client.bin"

void dali_callback(package_t *data, backword_t *backword);
void all_on(void);
void all_off(void);

void display_write(int satir, const char *txt)
{
#ifdef CLOCK    
    ssd1306_clear_line(&ekran,satir,false);
    ssd1306_display_text(&ekran, satir, (char*)txt, strlen(txt), false);
#endif    
}

#include "tools/smqmdns.cpp"
#include "tools/events.cpp"
#include "tools/config.cpp"
#include "tools/dali_tool.cpp"
#include "tools/tcp_events.cpp"


#include "comp/http.c"
#include "smq_tftp.h"



void webwrite(home_network_config_t net, config_t glob)
{
    disk.write_file(NETWORK_FILE,&net,sizeof(net),0);
    disk.write_file(GLOBAL_FILE,&glob,sizeof(glob),0);
}

void defreset(void *)
{
    network_default_config();
    default_config();
}

void udp_callback(uint8_t *data, uint8_t datalen, uint8_t *recv, uint8_t *reclen)
{
    ESP_LOGI(TAG,"UDP GELEN << %d %s",datalen,data);
    cJSON *rcv_json = cJSON_Parse((char*)data);
    char *command = (char *)calloc(1,30);
    JSON_getstring(rcv_json,"com", command,28);

    if (strcmp(command,"reqserver")==0)
      {
        
            char *ss0 = (char*)calloc(1,100);
            ss0 = Addr.to_string(NetworkConfig.home_ip);
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "com", "server");
            cJSON_AddStringToObject(root, "ip", ss0);
            char *dat = cJSON_PrintUnformatted(root);
            strcpy((char*)recv,dat);
            *reclen = strlen(dat);
            cJSON_free(dat);
            cJSON_Delete(root);
            free(ss0);
            
           printf("%s\n",recv);
      }
    free(command);
    cJSON_Delete(rcv_json); 
}

static void heard_task(void *args)
{  
    static uint8_t ss=0;  
    for (;;) {
        gpio_set_level(LED1,1);
        vTaskDelay(50/portTICK_PERIOD_MS);
        gpio_set_level(LED1,0); 
        if (++ss>6) {
            char *tt = (char*)calloc(1,64);
            Get_current_time(tt);
            display_write(3,tt);
            free(tt);
            ss=0;
            ssd1306_clear_line(&ekran,2,false);
        }
        vTaskDelay(5000/portTICK_PERIOD_MS);
        }
}

void query_dns(const char *name)
{
    struct addrinfo *address_info;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int res = getaddrinfo(name, NULL, &hints, &address_info);
    if (res != 0 || address_info == NULL) {
        ESP_LOGE(TAG, "couldn't get hostname for :%s: "
                      "getaddrinfo() returns %d, addrinfo=%p", name, res, address_info);
    } else {
        if (address_info->ai_family == AF_INET) {
            struct sockaddr_in *p = (struct sockaddr_in *)address_info->ai_addr;
            ESP_LOGI(TAG, "Resolved %s IPv4 address: %s", name,ipaddr_ntoa((const ip_addr_t*)&p->sin_addr.s_addr));
        }
    }    
}




extern "C" void app_main()
{
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
   // esp_log_level_set("phy_init", ESP_LOG_NONE);
    esp_log_level_set("gpio", ESP_LOG_NONE);
    esp_log_level_set("SOCKET_SERVER", ESP_LOG_NONE);
    
    ESP_LOGI(TAG, "INITIALIZING...");
    
    gpio_install_isr_service(0);
    if(esp_event_loop_create_default()!=ESP_OK) {ESP_LOGE(TAG,"esp_event_loop_create_default ERROR "); }
    ESP_ERROR_CHECK(esp_event_handler_instance_register(LED_EVENTS, ESP_EVENT_ANY_ID, led_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL, NULL));

    config();

    Net_Connect = false; 

            ESP_LOGI(TAG,"Wan Type %s",(NetworkConfig.wan_type==1)?"Ethernet":"Wifi");
            
            #ifdef ETHERNET
			if (NetworkConfig.wan_type==WAN_ETHERNET)
			{
				//Wan haberleşmesi ethernettir. Wifi kapalı
				if (w5500.start(NetworkConfig, &GlobalConfig)!=ESP_OK) FATAL_MSG(0,"Ethernet Baslatilamadi..");
                
				Net_Connect=true;
			}
            #endif
            
			if (NetworkConfig.wan_type==WAN_WIFI)
			{
				//Wan haberleşmesi Wifi. Etherneti kapat. Wifi STA olarak start et.
				wifi.init(NetworkConfig);				
				if (wifi.Station_Start()!=ESP_OK) {
                    ESP_LOGE(TAG,"WIFI Station Baslatilamadi. Wifi AP olarak açılıyor");
                    //ESP_ERROR_CHECK(wifi.ap_init());                   
                } 
                esp_wifi_set_ps(WIFI_PS_NONE);
                Net_Connect=true;
			}  

    GlobalConfig.random_mac = 0;
    disk.write_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig),0);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(TCP_SERVER_EVENTS, ESP_EVENT_ANY_ID, tcp_handler, NULL, NULL)); 

    if (Net_Connect)
      {
        if (GlobalConfig.http_start==1)
		{
			ESP_LOGI(TAG, "WEB START");
			ESP_ERROR_CHECK(start_rest_server("/config",NetworkConfig, GlobalConfig, &webwrite));
		}

		if (GlobalConfig.tcpserver_start==1)
		{
			ESP_LOGI(TAG, "TCP 5717 SOCKET SERVER START");
			tcpserver.start(5717);
            BroadcastAdr = (char *)calloc(1,50);
            strcpy(BroadcastAdr, Addr.to_string(NetworkConfig.home_broadcast));
            udpserver.start(5000,udp_callback);
		}
        query_dns("smartq.com.tr");
        if (GlobalConfig.time_sync==1) {
            Set_SystemTime_SNTP_diff();
        }
      }
  
#ifdef SSD1306
    ssd1306_clear_screen(&ekran,false);
#endif    
    char *ss = (char*)calloc(1,100);
    sprintf(ss,"%s",Addr.to_string(NetworkConfig.home_ip));
    display_write(0,ss);

#ifdef CLOCK
    char *rr = (char *)malloc(50);
    struct tm tt0 = {};
    saat.Get_current_date_time(rr,&tt0);
    ESP_LOGW(TAG, "Tarih/Saat %s", rr);
    free(rr);
#endif    

    char *ss0 = (char*)calloc(1,100);
    ss0 = Addr.to_string(NetworkConfig.home_ip);
    ESP_LOGW(TAG,"           IP : %s",ss0);
    ESP_LOGW(TAG,"BROADCAST ADR : %s",BroadcastAdr);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "com", "server");
    cJSON_AddStringToObject(root, "ip", ss0);
    char *dat = cJSON_PrintUnformatted(root);
    ss0 = Addr.to_string(NetworkConfig.home_broadcast);   
    udpserver.send_and_receive(BroadcastAdr,(uint8_t *)dat,strlen(dat),NULL,NULL);
    cJSON_free(dat);
    cJSON_Delete(root);
    free(ss0);

ESP_LOGI(TAG, "Init CRON..");
init_cron();
xTaskCreatePinnedToCore(heard_task, "task_00", 2048, NULL, 10, NULL,1);

//Saati Yaz
char *tt = (char*)calloc(1,64);
Get_current_time(tt);
display_write(3,tt);
free(tt);

initialise_mdns();
//smq_tftp_start();


ESP_LOGI(TAG, "Init Stop..");


//   dali_out_test();

    
/*
    package_t pk = {};
    address_t bb = {};  
    bb.short_adr = true;
    bb.arc_power = false;
    bb.data = 1;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 0x90;
    pk.data.type = BLOCK_16;
    for (int i=0;i<100;i++)
    {
    uint8_t kk = dali.send_receive(&pk);
    ESP_LOGI(TAG,"%02x %02x Get Level %02X Power %02x",pk.data.data0, pk.data.data1, 1,kk);
    ets_delay_us(416*22);
    ets_delay_us(416*22);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    }
*/

    while(true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

}