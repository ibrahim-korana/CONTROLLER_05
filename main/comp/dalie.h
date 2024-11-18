#ifndef _DALI_H
#define _DALI_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "dali_globale.h"
#include "tcpclient.h"

#define BUS_IDLE 1



union flag_t {
    struct {
        uint8_t send_active:1;
        uint8_t receive_active:1;
        uint8_t busy:1;
    } data;
    uint8_t package;
};

typedef void (*dali_callback_t)(package_t *data);

class WDali  {
    public:
        WDali(){};
        WDali(TcpClient *cln, dali_callback_t cb, gpio_num_t led)
        {
            initialize(cln,cb,led);
        }
        ~WDali() {};
        void initialize(TcpClient *cln, dali_callback_t cb, gpio_num_t led);
        backword_t send(package_t *package);
        backword_t send_receive(package_t *package);
           
    private:
        TcpClient *client;
        gpio_num_t _led;
        dali_callback_t callback=NULL; 
    protected: 
};

class Dali  {
    public:
        Dali(){};
        Dali(gpio_num_t tx, gpio_num_t rx, dali_callback_t cb, gpio_num_t led, TcpClient *cln)
        {
            initialize(tx,rx,cb,led, cln);
        }
        ~Dali() {};
        void initialize(gpio_num_t tx, gpio_num_t rx, dali_callback_t cb, gpio_num_t led, TcpClient *cln);
        backword_t send(package_t *package);
        backword_t send_receive(package_t *package);
        backword_t _send_receive(package_t *package);
        bool busy_is_wait(void) {
            uint8_t kk=0;
            while (flag.data.busy==1)
              {
                vTaskDelay(2/portTICK_PERIOD_MS);
                if(++kk>30) {
                    flag.package = 0;
                }
              };
            return false;  
        }

    
    private:
        gpio_num_t _tx;
        gpio_num_t _rx;
        gpio_num_t _led;
        TcpClient *client;
        void _send_one(void);
        void _send_zero(void);
        void _send_stop(void);
        uint64_t period_us = 0;
        volatile bool start_bit = false;
        package_t gelen;
        flag_t flag;

        static void IRAM_ATTR int_handler(void *args);
        
        static void sem_task(void* arg);
        static void end_clock(void* arg);
        void _send_byte(uint8_t data);

        SemaphoreHandle_t semaphore;
        SemaphoreHandle_t block_semaphore = NULL;
        esp_timer_handle_t clock_timer;
        bool cont = false;
        dali_callback_t callback=NULL;
                
    protected: 
       

};

#endif