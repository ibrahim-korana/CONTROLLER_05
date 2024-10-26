#ifndef _TCP_CLIENT_H
#define _TCP_CLIENT_H

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_timer.h"

#include "../core.h"

ESP_EVENT_DECLARE_BASE(TCP_CLIENT_EVENTS);

class TcpClient {
    public:
      TcpClient() {};
      ~TcpClient(){};
      bool start(const char*ip, uint16_t prt);

      uint16_t port;
      const char *host_ip;
      int sock;

      bool send_and_close(const char*ip, uint16_t prt, const char *data, char *recv, int *reclen);

    protected:  
      SemaphoreHandle_t pong_sem = NULL;
      SemaphoreHandle_t connect_sem = NULL;
      esp_timer_handle_t thandle;

      void log_socket_error(const char *tag, const int sock, const int err, const char *message);
      int socket_send(const char *tag, const int sock, const char * data, const size_t len);
      int try_receive(const char *tag, const int sock, char * data, size_t max_len);

    private:
      static void client_task(void *param); 
      static void ping_callback(void* arg);
      bool ping(void);
};

#endif