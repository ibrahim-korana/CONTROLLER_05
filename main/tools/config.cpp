
#include "tool.cpp"
#include "button_config.cpp"

#include "../comp/cron/cron.h"




void config(void)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<LED1) | (1ULL << LED2) | (1ULL << LED3);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE; 
    gpio_config(&io_conf);
    ESP_LOGI(TAG,"LEDS Init");
    gpio_set_level(LED1,0);
    gpio_set_level(LED2,0);
    gpio_set_level(LED3,0);

    led_test();

    ESP_LOGI(TAG,"Buttons Init");
    Button_Config(BUTTON1,"1",&btn1);
    Button_Config(BUTTON2,"2",&btn2);
    Button_Config(BUTTON3,"3",&btn3);


    ESP_LOGI(TAG,"I2C BUS Init");
    bus.init_bus(SDA,SCL, (i2c_port_t)0);
    bus.device_list();

#ifdef SSD1306 
    ESP_LOGI(TAG, "Panel is 128x32");
    i2c_master_init(&ekran,&bus,-1);
    ssd1306_init(&ekran, 128, 32);
    ssd1306_clear_screen(&ekran, false);
    ssd1306_contrast(&ekran, 0xff);
    display_write(0,"Initializing..");
#endif    

#ifdef CLOCK
    ESP_ERROR_CHECK(saat.init_device(&bus,0x51));
    ESP_LOGI(TAG,"0x51 PCF85763 ile iletişim kuruldu");
    sntp_set_time_sync_notification_cb(time_sync);
    struct tm rtcinfo;
    bool Valid=false;

    if (saat.get_time(&rtcinfo, &Valid)==ESP_OK) {
        ESP_LOGI(TAG,"Saat okundu. Degerler Stabil %s",(!Valid)?"Degil":"");
        if (!Valid)
        {
            ESP_LOGW(TAG, "Saat default degere ayarlaniyor");
            struct tm tt0 = {};
            tt0.tm_sec = 0;
            tt0.tm_min = 59;
            tt0.tm_hour = 13;
            tt0.tm_mday = 1;
            tt0.tm_mon = 3;
            tt0.tm_year = 2024;
            saat.set_time(&tt0,true);
            saat.get_time(&rtcinfo, &Valid); 
            //pcf8563_set_sync(true);
            if (Valid) ESP_LOGE(TAG,"Saat AYARLANAMADI");
        } else {
            //rtc degerlerini espye yazıyor
            saat.device_to_esp();
            char *rr = (char *)malloc(50);
            struct tm tt0 = {};
            saat.Get_current_date_time(rr,&tt0);
            ESP_LOGW(TAG, "Tarih/Saat %s", rr);
            free(rr);
        }
    }
#endif


    ESP_LOGI(TAG,"Disc Init");
    //disk.format();
    ESP_ERROR_CHECK(!disk.init());
    disk.read_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig), 0);
	if (GlobalConfig.open_default==0 ) {
		//Global ayarlar diskte kayıtlı değil. Kaydet.
		 default_config();
		 disk.read_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig),0);
		 FATAL_MSG(GlobalConfig.open_default,"Global Initilalize File ERROR !...");
	}

    disk.read_file(NETWORK_FILE,&NetworkConfig,sizeof(NetworkConfig), 0);
	if (NetworkConfig.home_default==0 ) {
		//Network ayarları diskte kayıtlı değil. Kaydet.
		 network_default_config();
		 disk.read_file(NETWORK_FILE,&NetworkConfig,sizeof(NetworkConfig),0);
		 FATAL_MSG(NetworkConfig.home_default, "Network Initilalize File ERROR !...");
	}

    if (!disk.file_control(GEAR_FILE))
      {
        for (int i=0;i<64;i++)
          {
            gear_t kk={};
            kk.short_addr = 200;
            kk.type = 0;
            disk.write_file(GEAR_FILE,&kk,sizeof(gear_t),i);
          }
      }
    if (!disk.file_control(GURUP_FILE))
      {
        for (int i=0;i<16;i++)
          {
            uint8_t kk=0;
            disk.write_file(GURUP_FILE,&kk,sizeof(uint8_t),i);
          }
      } 

    if (!disk.file_control(ZAMAN_FILE))
      {
        for (int i=0;i<32;i++)
          {
            zaman_t kk={};
            disk.write_file(ZAMAN_FILE,&kk,sizeof(zaman_t),i);
          }
      }   

    disk.list("/config","*.*");  

    ESP_LOGI(TAG,"RS485 Initialize");
    rs485_cfg.uart_num = 1;
    rs485_cfg.dev_num  = 254;
    rs485_cfg.rx_pin   = RS485_RX;
    rs485_cfg.tx_pin   = RS485_TX;
    rs485_cfg.oe_pin   = RS485_DIR;
    rs485_cfg.baud     = 115200;
    rs485.initialize(&rs485_cfg, (gpio_num_t)LED1);

    ESP_LOGI(TAG,"Dali-1 Initialize");
    dali.initialize(DALI1_TX, DALI1_RX, dali_callback, LED2, NULL);
}

void cron_callback(cron_job *job)
{
   zaman_job_t *jb = (zaman_job_t *)job->data;
   char *kk = (char *)calloc(1,100);
   sprintf(kk,"%d %s %d %s",jb->addr,(jb->type==1)?"Gurup":"Senaryo",jb->power,(jb->command==1)?"Aciliyor":"Kapatiliyor");
   ESP_LOGI(TAG,"%s",kk);
   free(kk);
   ESP_LOGI(TAG,"JOB EXEC %d", job->id);

  if (jb->type==1)
  { 
    if (jb->command==1)
      {
        //Gurup ac
        arc_power(jb->addr, 1, jb->power);
      }
    if (jb->command==2)
      {
        //Gurup kapat
        off(jb->addr, 1);
      }  
  } else {
    if (jb->command==3)
      {
        //Tum lambaları kapat
        //senaryo calıştır        
        off(255, 0);
        command_action(255,0,jb->addr+5);
      }  
  }




}

void add_cron(uint8_t saat, uint8_t dak, zaman_job_t *data )
{
    char *mm = (char *)calloc(1,20);
    sprintf(mm,"0 %d %d * * *",dak,saat);
    jobs[cron_count++]=cron_job_create(mm,cron_callback,(void *)data);
    ESP_LOGI(TAG,"          %s type:%s addr:%d comm:%d",mm,(data->type==1)?"gurup":"scene",data->addr, data->command);
    free(mm);
}

zaman_job_t *create_zjob(uint8_t adr, uint8_t cm)
{
  zaman_job_t *jb = (zaman_job_t *) calloc(1,sizeof(zaman_job_t));
  if (adr<16) {jb->type=1; jb->addr = adr;} else {jb->type=2;jb->addr = adr-16;} 
  jb->command=cm; 
  return jb;
}

void init_cron(void)
{
  cron_stop();
  cron_job_clear_all();
  ESP_LOGI(TAG,"Cron STOP");
  cron_count = 0;

  for (int i=0;i<32;i++)
  {
      zaman_t kk={};
      disk.read_file(ZAMAN_FILE,&kk,sizeof(zaman_t),i);
      if (kk.active>0) 
        {
          if (kk.on1saat>0 && kk.on1dakika>0) {
                  uint8_t km = 1;
                  if (kk.gurup==0) km=3;
                  zaman_job_t *jb = create_zjob(i,km);
                  jb->power = kk.power1;
                  add_cron(kk.on1saat, kk.on1dakika, jb);
              }
          if (kk.on2saat>0 && kk.on2dakika>0) {
                  uint8_t km = 1;
                  if (kk.gurup==0) km=3;
                  zaman_job_t *jb = create_zjob(i,km);
                  jb->power = kk.power2;
                  add_cron(kk.on2saat, kk.on2dakika, jb);
            }
          if (kk.off1saat>0 && kk.off1dakika>0) {
                  zaman_job_t *jb = create_zjob(i,2);
                  jb->power = 0;
                  add_cron(kk.off1saat, kk.off1dakika, jb);
            }
          if (kk.off2saat>0 && kk.off2dakika>0) {
                  zaman_job_t *jb = create_zjob(i,2);
                  jb->power = 0;
                  add_cron(kk.off2saat, kk.off2dakika, jb);
            }
        }
  }
 if (cron_count>0) {
  cron_start();
  ESP_LOGI(TAG,"Cron START");
 }

}