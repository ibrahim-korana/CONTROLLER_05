

void default_config(void)
{
    ESP_LOGW(TAG,"DEFAULT CONFIG");
     GlobalConfig.open_default = 1;
     GlobalConfig.atama_sirasi = 1;
     GlobalConfig.random_mac = 0;

     strcpy((char *)GlobalConfig.mqtt_server,"mqtt.smartq.com.tr");
     GlobalConfig.mqtt_keepalive = 30;
     GlobalConfig.device_id = 1;
     GlobalConfig.project_number=1;
     GlobalConfig.http_start=1;
     GlobalConfig.tcpserver_start=1;
     GlobalConfig.daliserver_start=1;
     GlobalConfig.comminication=0;
     GlobalConfig.time_sync=1;
     strcpy((char *)GlobalConfig.mdnshost,"SMQ_01");
     

     GlobalConfig.tus1=13;
     GlobalConfig.tus2=13;
     GlobalConfig.tus3=13;

     GlobalConfig.pow1=200;
     GlobalConfig.pow2=200;
     GlobalConfig.pow3=200;

     disk.file_control(GLOBAL_FILE);
     disk.write_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig),0);
}

void network_default_config(void)
{
    ESP_LOGW(TAG,"NETWORK DEFAULT CONFIG");
     NetworkConfig.home_default = 1;
     NetworkConfig.wifi_type = HOME_WIFI_AP;
     #ifdef ETHERNET
        NetworkConfig.wan_type = WAN_ETHERNET;
     #else
        NetworkConfig.wan_type = WAN_WIFI;
     #endif
        
     NetworkConfig.ipstat = STATIC_IP;
     //strcpy((char*)NetworkConfig.wifi_ssid, "Lords Palace");
     strcpy((char*)NetworkConfig.wifi_pass, "");
     strcpy((char*)NetworkConfig.ip,"192.168.1.220");
     strcpy((char*)NetworkConfig.netmask,"255.255.255.0");
     strcpy((char*)NetworkConfig.gateway,"192.168.1.1");
     strcpy((char*)NetworkConfig.dns,"8.8.8.8");
     strcpy((char*)NetworkConfig.backup_dns,"4.4.4.4");
     NetworkConfig.channel = 1;
     NetworkConfig.WIFI_MAXIMUM_RETRY=20;
/*
    NetworkConfig.wifi_type = HOME_WIFI_STA;
    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"Akdogan_2.4G");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"651434_2.4");
*/
    NetworkConfig.wifi_type = HOME_WIFI_STA;
    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"Baguette Modem");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"Baguette2024");

    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"IMS_YAZILIM");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"mer6514a4c");
    
    disk.file_control(NETWORK_FILE);
    disk.write_file(NETWORK_FILE,&NetworkConfig,sizeof(NetworkConfig),0);
}

void led_test(void)
{
    bool kk = false;
    for (int i=0;i<3;i++)
    {
        gpio_set_level(LED1,kk);
        gpio_set_level(LED2,kk);
        kk=!kk;
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
}