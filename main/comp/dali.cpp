#include "dali.h"
#include "math.h"

#include <rom/ets_sys.h>
#include "esp_timer.h"
#include "math.h"

#define HALF_PERIOD 416  //Te
#define FULL_PERIOD 832  //2Te
#define HP_SAPMA 42
#define FP_SAPMA 84
#define PER_BEKLE 104

#define HALF_PERIOD_UP HALF_PERIOD+HP_SAPMA
#define HALF_PERIOD_DOWN HALF_PERIOD-HP_SAPMA
#define FULL_PERIOD_UP FULL_PERIOD+FP_SAPMA
#define FULL_PERIOD_DOWN FULL_PERIOD-FP_SAPMA

#define SEND_TIMEOUT 5000


void Dali::initialize(gpio_num_t tx, gpio_num_t rx,dali_callback_t cb, gpio_num_t led, TcpClient *cln)
{
            _tx = tx;
            _rx = rx;
            callback = cb;
            _led = led;
            client = cln;
            gpio_config_t io_conf = {};
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL<<_tx) ;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE; 
            gpio_config(&io_conf);
            gpio_set_level(_tx, BUS_IDLE);
            gpio_set_drive_capability(_tx,GPIO_DRIVE_CAP_3);

            gpio_config_t intConfig;
            intConfig.pin_bit_mask = (1ULL<<_rx);
            intConfig.mode         = GPIO_MODE_INPUT;
            intConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
            intConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
            intConfig.intr_type    = GPIO_INTR_ANYEDGE;
            gpio_config(&intConfig);
            
            
            gpio_set_intr_type(_rx, GPIO_INTR_ANYEDGE);
	         gpio_isr_handler_add(_rx, int_handler, (void *)this);
            //gpio_intr_disable(_rx);

            esp_timer_create_args_t tim_arg0 = {};
                    tim_arg0.callback = &end_clock;
                    tim_arg0.arg = (void*) this;
                    tim_arg0.name = "clkend";
            flag.package = 0x00;                        
            ESP_ERROR_CHECK(esp_timer_create(&tim_arg0, &clock_timer));
            semaphore = xSemaphoreCreateBinary();
            xTaskCreate(sem_task, "task_02", 2048, (void *)this, 10, NULL);
}

portMUX_TYPE mux;

void Dali::_send_one(void)
{
 // portDISABLE_INTERRUPTS();
     gpio_set_level(_tx, 0);
     ets_delay_us(HALF_PERIOD);
     gpio_set_level(_tx, 1);
     ets_delay_us(HALF_PERIOD);
 // portENABLE_INTERRUPTS();  
}

void Dali::_send_zero(void)
{
 // portDISABLE_INTERRUPTS();
     gpio_set_level(_tx, 1);
     ets_delay_us(HALF_PERIOD);
     gpio_set_level(_tx, 0);
     ets_delay_us(HALF_PERIOD);
 // portENABLE_INTERRUPTS();    
}

void Dali::_send_stop(void)
{
 // portDISABLE_INTERRUPTS();
     gpio_set_level(_tx, 1);
     ets_delay_us(FULL_PERIOD*2); 
 // portENABLE_INTERRUPTS();     
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

backword_t Dali::send_receive(package_t *package)
{
    backword_t kk = _send_receive(package);
    if (kk==BACK_TIMEOUT) kk = _send_receive(package);
    if (kk==BACK_TIMEOUT) kk = _send_receive(package);
    if (kk==BACK_TIMEOUT) kk = _send_receive(package);
    return kk;
}

backword_t Dali::_send_receive(package_t *package)
{
   gpio_intr_disable(_rx);
  // printf("Dali send_rec %02X %02X\n", package->data.data0, package->data.data1);
   if (_led!=-1) gpio_set_level(_led,1);

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
      if(cnt++>90) {if (_led!=-1) gpio_set_level(_led,0);//printf("CNT=%d\n",cnt);
      return BACK_TIMEOUT;}
   }
 //printf("CNT=%d\n",cnt);
   old_level=1;cnt=0;
   while(old_level) {
      if (gpio_get_level(_rx)==1) {sr= esp_timer_get_time() - tm; old_level=0;}
      ets_delay_us(1);//150
      if(cnt++>2500) {if (_led!=-1) gpio_set_level(_led,0);//printf("CNT1=%d\n",cnt);
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
    
    backword_t ret=BACK_NONE;
    switch (gelen) {
      case 0xFF : {ret=BACK_YES;break;}
      case 0x00 : {ret=BACK_NO;break;}
      default: ret=(backword_t)gelen;
    }
    
    
   // if (ret==BACK_NONE) printf("HARD Gelen %02X ERR=%d\n",gelen,err); 
    //ret=BACK_YES;
    if (_led!=-1) gpio_set_level(_led,0);
    vTaskDelay(2 / portTICK_PERIOD_MS);
    return ret;
}

backword_t Dali::send(package_t *package)
{
   if (flag.data.busy) return BACK_BUSY;
   backword_t ret = BACK_NONE;
   if (_led!=-1) gpio_set_level(_led,1);

   flag.data.send_active =1 ;
   flag.data.busy = 1;
   gpio_intr_disable(_rx);
   //printf("Dali send %02X %02X %02X\n", package->data.data0, package->data.data1, package->data.data2);

   //Start Bit
   _send_one(); 
   //Adress
   _send_byte(package->data.data0);
   //data
   if (package->data.type==BLOCK_24 || package->data.type==BLOCK_16) _send_byte(package->data.data1);
   if (package->data.type==BLOCK_24) _send_byte(package->data.data2);
   //Stop bits
   _send_stop();
   ets_delay_us(FULL_PERIOD*7);
   gpio_intr_enable(_rx);

   flag.data.send_active = 0;
   if (flag.data.receive_active==0) flag.data.busy = 0;
   if (_led!=-1) gpio_set_level(_led,0);
   return ret;
}

void IRAM_ATTR  Dali::int_handler(void *args)
{
    Dali *self = (Dali *)args;
    uint8_t level = gpio_get_level(self->_rx);
    static bool start_bit_begin = false; 
    if (!self->start_bit)
    { 
      if (start_bit_begin && level==1) 
        {          
           self->start_bit = true;
           start_bit_begin=false;    
           gpio_intr_disable(self->_rx);  
          // gpio_set_level(GPIO_NUM_14,1);
           self->flag.data.receive_active = 1;
           self->flag.data.busy = 1;
           //if (self->block_semaphore!=NULL) xSemaphoreGive(self->block_semaphore);
           if (self->clock_timer!=NULL) (esp_timer_start_periodic(self->clock_timer, HALF_PERIOD));    
        } else {
             start_bit_begin=true;
             
               }  
    } else self->cont = true;    
}


void Dali::sem_task(void *args)
{
    Dali *self = (Dali *)args;   
    for (;;) {
        if (xSemaphoreTake(self->semaphore, portMAX_DELAY)) {
           //if (self->block_semaphore!=NULL) xSemaphoreGive(self->block_semaphore);
           if (self->callback!=NULL) self->callback(&self->gelen); 
            self->gelen.package = 0;  
            self->flag.data.receive_active = 0;
            if (self->flag.data.send_active==0) self->flag.data.busy = 0;                                                
          }           
        }
}


void Dali::end_clock(void *args)
{
    Dali *self = (Dali *)args;
    static int bb = 0;  
    static uint32_t gelen = 0x000000;
    static uint8_t max=22;
    static uint8_t count=0;
    count++;
    if(++bb>1)
    {
      bb=0;
      ets_delay_us(15);
      uint8_t level = gpio_get_level(self->_rx);
      gelen = (gelen<<1) | level;  
    }

    if (count==18 || count==35 ) {
      gpio_intr_enable(self->_rx);
    }
    if (count==21 || count==38) {
      gpio_intr_disable(self->_rx);
      if (self->cont) {
         self->cont=false; 
         max=38;
         if (count==38) max=54;
         } 
    }

    if (count>max) {
      self->gelen.package =(gelen>>3);
      self->gelen.data.type = BLOCK_24;
      //gpio_set_level(GPIO_NUM_14,0);
      
      if(count==23) {self->gelen.package = self->gelen.package<<16;self->gelen.data.type = BLOCK_8;}
      if(count==39) {self->gelen.package = self->gelen.package<<8;self->gelen.data.type = BLOCK_16;}
      if ((gelen&0x7)==0x7) self->gelen.data.error=0; else self->gelen.data.error=1;
      //printf("Count %d %d %08X %08X %d %d\n",count,max,(int)self->gelen.package,(int)gelen,self->gelen.data.type,self->gelen.data.error);
      //printf("Count %d  %08X\n",count,(int)self->gelen.package);
      if (self->block_semaphore!=NULL) xSemaphoreGive(self->block_semaphore);
      xSemaphoreGive(self->semaphore);
      gelen = 0;
      esp_timer_stop(self->clock_timer); 
      bb=0; 
      count=0;
      max = 22;
      self->start_bit = false;  
      self->cont=false;
      gpio_intr_enable(self->_rx);      
      
    }

}

