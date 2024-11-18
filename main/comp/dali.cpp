#include "dali.h"
#include "math.h"

#include <rom/ets_sys.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"

#define HALF_PERIOD 416
#define FULL_PERIOD 832
/* %20 sapma
#define HP_SAPMA 42
#define FP_SAPMA 84
*/
//%30
#define HP_SAPMA 125
#define FP_SAPMA 250



#define HALF_PERIOD_UP HALF_PERIOD+HP_SAPMA
#define HALF_PERIOD_DOWN HALF_PERIOD-HP_SAPMA
#define FULL_PERIOD_UP (FULL_PERIOD+FP_SAPMA)
#define FULL_PERIOD_DOWN (FULL_PERIOD-FP_SAPMA)

#define BIT16_BIT 19
#define BIT16_FORWARD_TIME_MAX (BIT16_BIT * FULL_PERIOD_UP) 
#define BIT16_FORWARD_TIME_MIN (BIT16_BIT * FULL_PERIOD_DOWN)

#define FULL_PERIOD_DOWN_3 1746

#define PARAZIT 100
#define PER_BEKLE 104

const char *DALI_TAG = "DALI_TAG"; 

ESP_EVENT_DECLARE_BASE(DALI_EVENTS);


int Dali::Adc_Olcum(void)
{
    int adc_raw, voltage;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &adc_raw);   
    if (do_calibration1_chan0) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw, &voltage));
        return voltage;
    } 
    return 0;
};

void Dali::adc_task(void *args)
{
    Dali *self = (Dali *)args;   
    for (;;) {
        if (self->Olcum_Ready)
        {
            int adc_reading = self->Adc_Olcum();
           //ESP_LOGI(DALI_TAG,"Hat %d",adc_reading);
            if (adc_reading<500) {
                vTaskDelay(100 / portTICK_PERIOD_MS); adc_reading = self->Adc_Olcum();
                if (adc_reading<500) {vTaskDelay(100 / portTICK_PERIOD_MS); adc_reading = self->Adc_Olcum();}
                if (adc_reading<500) {
                    if (!self->hat_error) {
                        esp_event_post(DALI_EVENTS, DALI_HAT_ERROR, NULL, 0, 0);
                    }
                    self->hat_error = true; 
                    gpio_intr_disable(self->_rx);
                }
                //dali hattının voltajı gitti. lambanın systemFailureLevel seviyesinde yakılması gerekiyor.
            }else {
                if (self->hat_error) {
                    esp_event_post(DALI_EVENTS, DALI_HAT_NORMAL, NULL, 0,0);
                }
                self->hat_error = false;
                gpio_intr_enable(self->_rx);
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);         
        }
}

bool Dali::adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI("DALI", "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {};
            cali_config.unit_id = unit;
            cali_config.atten = atten;
            cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI("DALI", "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW("DALI", "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE("DALI", "Invalid arg or no memory");
    }

    return calibrated;
}


void Dali::initialize(gpio_num_t tx, gpio_num_t rx,  adc_channel_t chan, dali_callback_t cb, gpio_num_t ld)
{
            _tx = tx;
            _rx = rx;
            callback = cb;
            Led = ld;
           // channel = chan;
            gpio_config_t io_conf = {};
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL<<_tx) ;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE; 
            gpio_config(&io_conf);
            gpio_set_level(_tx, BUS_IDLE);
            gpio_set_drive_capability(_tx,GPIO_DRIVE_CAP_3);

            gpio_config_t intConfig;
            intConfig.pin_bit_mask = (1ULL<<_rx);
            intConfig.mode         = GPIO_MODE_INPUT;
            intConfig.pull_up_en   = GPIO_PULLUP_ENABLE;// DISABLE;
            intConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;// GPIO_PULLDOWN_ENABLE;
            gpio_config(&intConfig);
            
            
            gptimer_config_t gpc = {};
                gpc.clk_src = GPTIMER_CLK_SRC_DEFAULT;
                gpc.direction = GPTIMER_COUNT_UP;
                gpc.resolution_hz = 1 * 1000 * 1000; // 1MHz, 1 tick = 1us
            ESP_ERROR_CHECK(gptimer_new_timer(&gpc, &per_timer));
            ESP_ERROR_CHECK(gptimer_new_timer(&gpc, &kom_timer));

            
            gptimer_alarm_config_t alarm_cfg0 = {};
                    alarm_cfg0.reload_count = 0;
                    alarm_cfg0.alarm_count = 1000000; 
                    alarm_cfg0.flags.auto_reload_on_alarm = true;
            ESP_ERROR_CHECK(gptimer_set_alarm_action(kom_timer, &alarm_cfg0));

            
            gptimer_alarm_config_t alarm_cfg = {};
            alarm_cfg.reload_count = 0;
            alarm_cfg.alarm_count = FULL_PERIOD_UP*2; 
            alarm_cfg.flags.auto_reload_on_alarm = false;
            gptimer_event_callbacks_t cbs = {};
            cbs.on_alarm = timer_alarm_cb; // register user callback
          
            ESP_ERROR_CHECK(gptimer_register_event_callbacks(per_timer, &cbs, (void *)this));
            ESP_ERROR_CHECK(gptimer_set_alarm_action(per_timer, &alarm_cfg));

            ESP_ERROR_CHECK(gptimer_enable(per_timer));
            ESP_ERROR_CHECK(gptimer_enable(kom_timer));
            ESP_ERROR_CHECK(gptimer_start(kom_timer));
                    
            adc_oneshot_unit_init_cfg_t init_config1 = {};
               init_config1.unit_id = ADC_UNIT_1;   
               init_config1.ulp_mode = ADC_ULP_MODE_DISABLE;        
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle)); 

            adc_oneshot_chan_cfg_t config = {};
               config.bitwidth = ADC_BITWIDTH_DEFAULT;
               config.atten = ADC_ATTEN_DB_11;        
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &config));  

            adc1_cali_chan0_handle = NULL;
            do_calibration1_chan0 = adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_7, ADC_ATTEN_DB_11, &adc1_cali_chan0_handle);

            start_bit_begin = false;

            semaphore = xSemaphoreCreateBinary();
            end_sem = xSemaphoreCreateBinary();
            back_sem = xSemaphoreCreateBinary();

            xTaskCreate(sem_task, "task_02", 4096, (void *)this, 10, NULL);
            xTaskCreate(adc_task, "task_03", 2048, (void *)this, 10, NULL);
            xTaskCreate(paket_task, "task_04", 4096, (void *)this, 5, NULL);

            #ifdef LOG_DETAY 
              xTaskCreate(print_task, "task_05", 2048, (void *)this, 10, NULL);
            #endif

            gpio_set_intr_type(_rx, GPIO_INTR_ANYEDGE);
	        gpio_isr_handler_add(_rx, int_handler, (void *)this);
            gpio_intr_enable(_rx);           
}


void Dali::_send_one(void)
{
     gpio_set_level(_tx, 0);
     ets_delay_us(HALF_PERIOD);
     gpio_set_level(_tx, 1);
     ets_delay_us(HALF_PERIOD);
}

void Dali::_send_zero(void)
{
     gpio_set_level(_tx, 1);
     ets_delay_us(HALF_PERIOD);
     gpio_set_level(_tx, 0);
     ets_delay_us(HALF_PERIOD);  
}

void Dali::_send_stop(void)
{
     gpio_set_level(_tx, 1);
     ets_delay_us(FULL_PERIOD*2);   
}

void Dali::_send_byte(uint8_t data)
{
   uint8_t tmp=data, ii=0xFF;
   while(ii>0)
   {
      if ((tmp&0x80)==0x80)
      {
         _send_one(); 
      }
      else
      {
         _send_zero();
      }
      tmp=tmp<<1;           
      ii=ii>>1;
   } 
}
void Dali::send(package_t *package)
{
   Olcum_Ready = false;
   period_us = esp_timer_get_time();
   gpio_intr_disable(_rx);
   _send_one(); 
   //Adress
   _send_byte(package->data.data0);
   if (package->data.type==BLOCK_24 || package->data.type==BLOCK_16) _send_byte(package->data.data1);
   if (package->data.type==BLOCK_24) _send_byte(package->data.data2);
   //Stop bits
   _send_stop();
   vTaskDelay(1/portTICK_PERIOD_MS);
    gpio_intr_enable(_rx);
    Olcum_Ready = true;
}

#ifdef LOG_DETAY
void Dali::print_task(void *arg)
{
  Dali *self = (Dali *)arg;
  example_queue_element_t ele;
  while(1)
  {
    if (xQueueReceive(self->queue, &ele, portMAX_DELAY))
    {
       // ESP_LOGI("AA","count=%5llu %d %d", ele.event_count,ele.lv,ele.st);
        printf("count=%5llu %d %d %08jx\n", ele.event_count,ele.lv,ele.st, (uintmax_t)ele.gln);
    }
  }
}
#endif

bool Dali::timer_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_awoken = pdTRUE;
    Dali *self = (Dali *)user_ctx;

    uint64_t kk = 0;
    gptimer_get_raw_count(self->kom_timer,&kk);
    gptimer_set_raw_count(self->kom_timer,0);
    
    gptimer_stop(self->per_timer);
  //  gptimer_stop(self->kom_timer);
    paket_queue_element_t gg = {};
    gg.gln = self->Hgelen;
    gg.bit_cnt = self->bit_count;
    self->start_bit_begin = false;
    //if (gg.bit_cnt<16) return pdTRUE;
    xQueueSendFromISR(self->paket_queue, &gg, &high_task_awoken);
   // gpio_intr_disable(self->_rx);
    //ets_delay_us(FULL_PERIOD * 2); 

    return high_task_awoken == pdTRUE;
}

void Dali::int_start(void)
{
   // start_bit_begin = false;
    //gptimer_set_raw_count(kom_timer,0);
    gpio_intr_enable(_rx);
}
void Dali::int_stop(void)
{
   // gptimer_stop(kom_timer);
    gpio_intr_disable(_rx);
}


void IRAM_ATTR  Dali::int_handler(void *args)
{
    Dali *self = (Dali *)args;
    bool level = gpio_get_level(self->_rx);
    static bool gec=false; 
    BaseType_t high_task_awoken = pdFALSE;
    
    if (!self->start_bit_begin && !level) { 
        //Start biti için hat sıfıra düştü
        //period ve frame timerlarını sıfırlayıp 
        //stop bitleri için alarm konuluyor ve
        //timerlar başlatılıyor.
        //2 full period boyunca int aktive olmazsa ki bu durum 
        //stop bitlerinde olur timer alarm çalışır.
        //Paketin bittiğini timer alarmdan alabiliriz.
        //start biti, en az 3 full period boyunca 1 de 
        //durduktan sonra sıfıra düşen lojik 1 şeklindedir.
        //  ------------    ----
        //             |    |
        //             |    |
        //             ------  
        //Bunu garanti etmek gerekir. 
    
        uint64_t kt = 0;
        gptimer_get_raw_count(self->kom_timer,&kt);
               
        if (kt>(FULL_PERIOD_DOWN_3))
          {          
            //ESP_ERROR_CHECK(gptimer_enable(self->per_timer));
            gptimer_set_raw_count(self->per_timer,0); 
            ESP_ERROR_CHECK(gptimer_start(self->per_timer));
            self->bit_count=0;  
            if (self->Led!=-1) gpio_set_level(self->Led,level); 
            //ESP_ERROR_CHECK(gptimer_start(self->kom_timer));
           // if (self->Led!=GPIO_NUM_NC) gpio_set_level(self->Led,1); 
          }     
        gptimer_set_raw_count(self->kom_timer,0);
        #ifdef LOG_DETAY2
            example_queue_element_t ele;
            ele.event_count = kt;
            ele.st = self->bit_count;
            ele.lv = 98;
            ele.gln = self->Hgelen;
            xQueueSendFromISR(self->queue, &ele, &high_task_awoken); 
            #endif

        self->Hgelen = 0x000000;          
        return;
    };
    if (!self->start_bit_begin && level) {
      //start bitinin yükselen kenarı geldi. 
      //period sayacını sıfırlayıp ilk bit verisi için 
      //hazır hale getiriliyor
      uint64_t kk = 0;     
      gptimer_get_raw_count(self->per_timer,&kk);
      if (self->Led!=-1) gpio_set_level(self->Led,level); 
            #ifdef LOG_DETAY2
            example_queue_element_t ele;
            ele.event_count = kk;
            ele.st = self->bit_count;
            ele.lv = 99;
            ele.gln = self->Hgelen;
            xQueueSendFromISR(self->queue, &ele, &high_task_awoken); 
            #endif

      // kk nın yarım period olması şart
      if (kk>HALF_PERIOD_DOWN && kk<HALF_PERIOD_UP)
        {
            gptimer_set_raw_count(self->per_timer,0);
            self->start_bit_begin =true;
            self->bit_count=0;      
            
            /*
            gptimer_alarm_config_t alarm_cfg = {};
            alarm_cfg.alarm_count = FULL_PERIOD_UP*2; 
            alarm_cfg.flags.auto_reload_on_alarm = false;
            ESP_ERROR_CHECK(gptimer_set_alarm_action(self->per_timer, &alarm_cfg));
            */
            gpio_set_level(GPIO_NUM_15,level);
        }     
      return;         
    }

    if (self->start_bit_begin) {
        
        uint64_t kk = 0;        
        gptimer_get_raw_count(self->per_timer,&kk);
        if (kk<=PARAZIT) gec=true; else gec=false;
        if (kk>PARAZIT && !gec) {
            if (self->Led!=-1) gpio_set_level(self->Led,level); 
            gec=false;   
            if (kk>FULL_PERIOD_DOWN) self->bit_count+=2; else self->bit_count++; 
            if ((self->bit_count%2)==0) self->Hgelen = (self->Hgelen<<1) | level; 

            #ifdef LOG_DETAY2
            example_queue_element_t ele;
            ele.event_count = kk;
            ele.st = self->bit_count;
            ele.lv = level;
            ele.gln = self->Hgelen;
            xQueueSendFromISR(self->queue, &ele, &high_task_awoken); 
            #endif

            gptimer_set_raw_count(self->per_timer,0);
            /*
            gptimer_alarm_config_t alarm_cfg = {};
            alarm_cfg.alarm_count = FULL_PERIOD_UP*2; 
            alarm_cfg.flags.auto_reload_on_alarm = false;
            ESP_ERROR_CHECK(gptimer_set_alarm_action(self->per_timer, &alarm_cfg));
            */
        };
    }
    
}

void Dali::backword_yesno(backword_t data)
{   

}

backword_t Dali::_send_receive(package_t *package)
{
    backword_t ret = {};
    ret.backword = BACK_NONE;
    
   ets_delay_us(816 * 80); //Son gonderilen paketten sonra en az bu kadar beklemeli 
  // ets_delay_us(HALF_PERIOD * 60);

   //Start Bit
   ESP_ERROR_CHECK(gpio_intr_disable(_rx));
   ets_delay_us(FULL_PERIOD * 2);
   _send_one(); 
   _send_byte(package->data.data0);
   if (package->data.type==BLOCK_24 || package->data.type==BLOCK_16) _send_byte(package->data.data1);
   if (package->data.type==BLOCK_24) _send_byte(package->data.data2);
   _send_stop();
   ets_delay_us(FULL_PERIOD);
   if (Led!=-1) gpio_set_level(Led,1);     
   ESP_ERROR_CHECK( gpio_intr_enable(_rx));

   //Data Gönderildi. Cevap bekleniyor
   if (xSemaphoreTake(back_sem, 25/portTICK_PERIOD_MS))
   {
        ret.backword = BACK_DATA;
        ret.data = gelen.data.data0;
   } else {
    ret.backword = BACK_TIMEOUT;
   }
   if (Led!=-1) gpio_set_level(Led,0);

    return ret;
}

void Dali::paket_task(void *arg)
{
  Dali *self = (Dali *)arg;
  paket_queue_element_t gg;
  while(1)
  {
        if (xQueueReceive(self->paket_queue, &gg, portMAX_DELAY))
        {
            package_t g0 = {};
            g0.package = gg.gln;
            g0.data.type = BLOCK_ERR;
            if(gg.bit_cnt==17 || gg.bit_cnt==16) {
               // printf("8 BIT DATA %08jx %d\n",(uintmax_t)gg.gln, gg.bit_cnt);
                g0.package = g0.package<<16;g0.data.type = BLOCK_8;
                }
            if(gg.bit_cnt==32 || gg.bit_cnt==33) {
               // printf("16 BIT DATA %08jx %d\n",(uintmax_t)gg.gln, gg.bit_cnt);
                g0.package = g0.package<<8;g0.data.type = BLOCK_16;
                }
            if(gg.bit_cnt>40) {g0.data.type = BLOCK_24;} 

            if (g0.data.type == BLOCK_ERR)
            {
                //Collesion
                if (gg.bit_cnt<32) {
                  // printf("COLLESION %08jx %d\n",(uintmax_t)gg.gln, gg.bit_cnt);
                   gg.bit_cnt = 16;
                   g0.package = 0xFF<<16;g0.data.type = BLOCK_8;

                }
            }


            self->gelen.package = g0.package;
            self->gelen.data.error = 0; 
            if (g0.data.type == BLOCK_8)
            {
                    xSemaphoreGive(self->back_sem);
                    //printf("DATA %02X %d\n",self->gelen.data.data0, gg.bit_cnt);
            }
            else if (self->callback!=NULL && g0.data.type == BLOCK_16 ) {
                    backword_t rt = {};
                    rt.backword = BACK_NONE;
                    if (self->callback!=NULL) self->callback(&self->gelen, &rt);            
                    if (rt.backword!=BACK_NONE) {
                            //callback gönderiliyor
                            ets_delay_us(FULL_PERIOD*5);
                            self->_send_one(); 
                            self->_send_byte((rt.backword==BACK_DATA)?rt.data:(uint8_t)rt.backword);
                            self->_send_stop();
                            ets_delay_us(FULL_PERIOD*2);
                            self->gelen.package = 0;
                                } else self->gelen.package = 0;
                    if (self->Led!=GPIO_NUM_NC) gpio_set_level(self->Led,0);  
            }
          //  gpio_intr_enable(self->_rx);
         //   printf("GELDI %2d %08jx\n", gg.bit_cnt, (uintmax_t)gg.gln);
        
        }
  }
}

void Dali::sem_task(void *args)
{
    Dali *self = (Dali *)args;   
    for (;;) {
        if (xSemaphoreTake(self->semaphore, portMAX_DELAY)) {
           backword_t rt = {};
           rt.backword = BACK_NONE;
           if (self->gelen.data.type==BLOCK_8)
            {
                xSemaphoreGive(self->back_sem);
               // printf("DATA %02X\n",self->gelen.data.data0);

            } else if (self->callback!=NULL) {
                self->callback(&self->gelen, &rt);            
                if (rt.backword!=BACK_NONE) {
                        //callback gönderiliyor
                        ets_delay_us(FULL_PERIOD*5);
                        self->_send_one(); 
                        self->_send_byte((rt.backword==BACK_DATA)?rt.data:(uint8_t)rt.backword);
                        self->_send_stop();
                        ets_delay_us(FULL_PERIOD*3);
                        self->gelen.package = 0; 
                            }
            }           
           if (self->Led!=GPIO_NUM_NC) gpio_set_level(self->Led,0);                                                 
          }           
        }
}

backword_tt Dali::send_receive(package_t *package)
{
    /*
    backword_tt ret = BACK_NONE;
    backword_t kk = _send_receive(package);
    //if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);
    if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);
    if (kk.backword==BACK_NONE) ret = BACK_TIMEOUT; 
    if (kk.backword==BACK_TIMEOUT) ret = BACK_TIMEOUT;    
    if (kk.backword==BACK_DATA) {
        if (kk.data==0xFF) ret= BACK_YES;
        if (kk.data==0x00) ret= BACK_NO;
    }

   // if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);
   // if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);
    
    return ret;
    */
   /*
   backword_tt kk = _esend_receive(package);
   if (kk==BACK_TIMEOUT) kk = _esend_receive(package);
   if (kk==BACK_TIMEOUT) kk = _esend_receive(package);
   if (kk==BACK_TIMEOUT) kk = _esend_receive(package);
   */
   
   backword_tt aa;
   Olcum_Ready = false;
   backword_t kk = _send_receive(package);
   if (kk.backword==BACK_TIMEOUT) {
    ets_delay_us(FULL_PERIOD*5);
    kk = _send_receive(package);
    }
   if (kk.backword==BACK_TIMEOUT) {ets_delay_us(FULL_PERIOD*5); kk = _send_receive(package);}
   if (kk.backword!=BACK_TIMEOUT)
   {
       switch (kk.data) {
            case 0xFF : {aa=BACK_YES;break;}
            case 0x00 : {aa=BACK_NO;break;}
            default: aa=(backword_tt)kk.data;
            } 
   } else aa = BACK_TIMEOUT;
  // printf("RET %02X\n",aa);


   Olcum_Ready = true;
 
   return aa;
}

backword_t Dali::ysend_receive(package_t *package)
{
    backword_t kk = _send_receive(package);
   // if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);
   // if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);
   // if (kk.backword==BACK_TIMEOUT) kk = _send_receive(package);    
    return kk;
}




backword_tt Dali::_esend_receive(package_t *package)
{
   gpio_intr_disable(_rx);
  // printf("Dali send_rec %02X %02X\n", package->data.data0, package->data.data1);
   if (Led!=-1) gpio_set_level(Led,1);

   //Start Bit
   _send_one(); 
   //Adress
   _send_byte(package->data.data0);
   //data
   if (package->data.type==BLOCK_24 || package->data.type==BLOCK_16) _send_byte(package->data.data1);
   if (package->data.type==BLOCK_24) _send_byte(package->data.data2);
   //Stop bits
   _send_stop();
   //ets_delay_us(FULL_PERIOD*2);
   bool ok=true;
   
   uint8_t count = 0, gelen=0, old_count=0, tck=0;//, err=0;
   uint8_t old_level = gpio_get_level(_rx);
   
   //Start Düştü
   uint64_t tm = 0;
   uint64_t sr = 0;
   uint16_t cnt = 0;

   while(old_level) {
      if (gpio_get_level(_rx)==0) {tm = esp_timer_get_time();old_level=0;}
      ets_delay_us(PER_BEKLE);//600 94
      if(cnt++>90) {if (Led!=-1) gpio_set_level(Led,0);//printf("CNT=%d\n",cnt);
      return BACK_TIMEOUT;}
   }
 //printf("CNT=%d\n",cnt);
   old_level=1;cnt=0;
   while(old_level) {
      if (gpio_get_level(_rx)==1) {sr= esp_timer_get_time() - tm; old_level=0;}
      ets_delay_us(1);//150
      if(cnt++>2500) {if (Led!=-1) gpio_set_level(Led,0);//printf("CNT1=%d\n",cnt);
      return BACK_TIMEOUT;}
   }
 //printf("CNT1=%d %lld\n",cnt,sr);
   
   //printf("Start %llu\n",sr);
   old_level = gpio_get_level(_rx);
   tm = esp_timer_get_time();

   while(ok)
     {      
        uint8_t level = gpio_get_level(_rx);
        if (level==1)
          {
             old_count++;
          } else old_count = 0;
        if(old_count>176) ok=false;  

        if (old_level!=level)
        {
           sr = esp_timer_get_time() - tm ;
           tm = esp_timer_get_time();
           old_level = level;
          // if (sr>(FULL_PERIOD+FP_SAPMA)) err=1;
           if (sr<750) tck=1; else tck=2;
           //if (tck==2 && count==15) tck=1;
           count+=tck;
           if ((count % 2)==0 && count<18) gelen=(gelen<<1) | level; 

          // printf("%2d %d %llu\n",count,level,sr);
        }
         ets_delay_us(10);
     } 
    
    backword_tt ret=BACK_NONE;
    switch (gelen) {
      case 0xFF : {ret=BACK_YES;break;}
      case 0x00 : {ret=BACK_NO;break;}
      default: ret=(backword_tt)gelen;
    }
    
    
   // if (ret==BACK_NONE) printf("HARD Gelen %02X ERR=%d\n",gelen,err); 
    //ret=BACK_YES;
    if (Led!=-1) gpio_set_level(Led,0);
    vTaskDelay(2 / portTICK_PERIOD_MS);
    return ret;
}

