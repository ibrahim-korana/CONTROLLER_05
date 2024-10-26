#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"


#include "esp_system.h"
#include "esp_event.h"


#include "tcpclient.h"

const char *CLNTAG = "TcpClient";

#define INVALID_SOCK (-1)


void TcpClient::client_task(void *param)
{
    TcpClient *ths = (TcpClient *)param;

    int addr_family = 0;
    int ip_protocol = 0;
    char *rx_buffer = (char *)calloc(1,512);

    while (1) {
        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, ths->host_ip, &dest_addr.sin_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(ths->port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        ths->sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (ths->sock < 0) {
            ESP_LOGE(CLNTAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(CLNTAG, "Socket created, connecting to %s:%d", ths->host_ip, ths->port);

        int err = connect(ths->sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(CLNTAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(CLNTAG, "Successfully connected");
        xSemaphoreGive( ths->connect_sem );

        while(1) {
            int len = recv(ths->sock, rx_buffer, 510, 0);
            if (len < 0) {
                ESP_LOGE(CLNTAG, "recv failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; 
                char s1[] = "pong";
                if (strstr(rx_buffer,s1)) xSemaphoreGive( ths->pong_sem );
                tcp_events_data_t ms = {};
                ms.data = rx_buffer;
                ms.data_len = len;
                ms.socket = ths->sock;
                ms.port = ths->port;
                ESP_ERROR_CHECK(esp_event_post(TCP_CLIENT_EVENTS, LED_EVENTS_MQTT, &ms, sizeof(tcp_events_data_t), portMAX_DELAY));
                vTaskDelay(50/portTICK_PERIOD_MS);
            }
        }

        if (ths->sock != -1) {
            ESP_LOGE(CLNTAG, "Shutting down socket and restarting...");
            shutdown(ths->sock, 0);
            close(ths->sock);
        }
    }
}

bool TcpClient::start(const char*ip, uint16_t prt)
{
    port = prt;
    host_ip = ip;
    pong_sem = xSemaphoreCreateBinary();
    connect_sem = xSemaphoreCreateBinary();
    xTaskCreate(client_task, "tcp_client", 4096, (void*)this, 5, NULL);
    if (xSemaphoreTake(connect_sem,5000/portTICK_PERIOD_MS)) {
            esp_timer_create_args_t arg = {};
            arg.callback = &ping_callback;
            arg.name = "periodic";
            arg.arg = (void *)this;
            ESP_ERROR_CHECK(esp_timer_create(&arg, &thandle)); 
            ESP_ERROR_CHECK(esp_timer_start_periodic(thandle, 2000000));
        return true;
    }
    return false;
}

void TcpClient::ping_callback(void* arg)
{
    TcpClient *ths = (TcpClient *)arg;
    if (ths->ping()==0) ESP_LOGE(CLNTAG,"Baglantı Kopuk");
}
bool TcpClient::ping(void)
{
    char s1[] = "{\"com\":\"ping\"}";
    send(sock, s1, strlen(s1), 0);
    if (xSemaphoreTake(pong_sem,2000/portTICK_PERIOD_MS)) return true;
    return false;
}


void TcpClient::log_socket_error(const char *tag, const int sock, const int err, const char *message)
{
    ESP_LOGE(tag, "[sock=%d]: %s\n"
                  "error=%d: %s", sock, message, err, strerror(err));
}

int TcpClient::socket_send(const char *tag, const int sock, const char * data, const size_t len)
{
    int to_write = len;
    while (to_write > 0) {
        int written = send(sock, data + (len - to_write), to_write, 0);
        if (written < 0 && errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) {
            log_socket_error(tag, sock, errno, "Error occurred during sending");
            return -1;
        }
        to_write -= written;
    }
    return len;
}


#define YIELD_TO_ALL_MS 50

int TcpClient::try_receive(const char *tag, const int sock, char * data, size_t max_len)
{
    int len = recv(sock, data, max_len, 0);
    if (len < 0) {
        if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;   // Not an error
        }
        if (errno == ENOTCONN) {
            ESP_LOGW(tag, "[sock=%d]: Connection closed", sock);
            return -2;  // Socket has been disconnected
        }
        log_socket_error(tag, sock, errno, "Error occurred during receiving");
        return -1;
    }

    return len;
}


bool TcpClient::send_and_close(const char*ip, uint16_t prt, const char *data, char *recv, int *reclen)
{
    struct addrinfo hints = { .ai_socktype = SOCK_STREAM };
    struct addrinfo *address_info;
    int sock = INVALID_SOCK;
    char *ppp = (char*)calloc(1,20);
    sprintf(ppp,"%d",prt);

    int res = getaddrinfo(ip, ppp, &hints, &address_info);
    if (res != 0 || address_info == NULL) {
        ESP_LOGE(CLNTAG, "couldn't get hostname for `%s` "
                      "getaddrinfo() returns %d, addrinfo=%p", ip, res, address_info);
        free(address_info);
        free(ppp);              
        return false;
    }
    sock = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    if (sock < 0) {
        log_socket_error(CLNTAG, sock, errno, "Unable to create socket");
        free(address_info);
        free(ppp);
        return false;
    }
 //   ESP_LOGI(CLNTAG, "Socket created, connecting to %s:%d", ip, prt);

    // Marking the socket as non-blocking
    int flags = fcntl(sock, F_GETFL);
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_socket_error(CLNTAG, sock, errno, "Unable to set socket non blocking");
        return false;
    }


    if (connect(sock, address_info->ai_addr, address_info->ai_addrlen) != 0) {
        if (errno == EINPROGRESS) {
            ESP_LOGD(CLNTAG, "connection in progress");
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(sock, &fdset);

            struct timeval tmv = {};
            tmv.tv_sec = 2;

            // Connection in progress -> have to wait until the connecting socket is marked as writable, i.e. connection completes
            res = select(sock+1, NULL, &fdset, NULL, &tmv);
            if (res < 0) {
                log_socket_error(CLNTAG, sock, errno, "Error during connection: select for socket to be writable");
                free(address_info);
                free(ppp);
                return false;
            } else if (res == 0) {
                log_socket_error(CLNTAG, sock, errno, "Connection timeout: select for socket to be writable");
                free(address_info);
                free(ppp);
                return false;
            } else {
                int sockerr;
                socklen_t len = (socklen_t)sizeof(int);

                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0) {
                    log_socket_error(CLNTAG, sock, errno, "Error when getting socket error using getsockopt()");
                    free(address_info);
                    free(ppp);
                    return false;
                }
                if (sockerr) {
                    log_socket_error(CLNTAG, sock, sockerr, "Connection error");
                    free(address_info);
                    free(ppp);
                    return false;
                }
            }
        } else {
            log_socket_error(CLNTAG, sock, errno, "Socket is unable to connect");
            free(address_info);
            free(ppp);
            return false;
        }
    }

    bool ret = true;
    int len = socket_send(CLNTAG, sock, data, strlen(data));
    if (len < 0) {
        ESP_LOGE(CLNTAG, "Error occurred during socket_send");
        ret=false;
    }

    uint8_t count=0;
    if (recv!=NULL && *reclen>0)
      {
        // Keep receiving until we have a reply
            do {
                len = try_receive(CLNTAG, sock, recv, *reclen);
                if (len < 0) {
                    ESP_LOGE(CLNTAG, "Error occurred during try_receive");
                    len=999;
                }
                if (len!=999) vTaskDelay(pdMS_TO_TICKS(YIELD_TO_ALL_MS));
                if(++count>50) len=999;
                //printf("len=%d\n",len);
            } while (len == 0);
            if(len!=999) {
                *reclen=len;
                //ESP_LOGI(CLNTAG, "Received: %.*s", len, recv);
            } else *reclen=0;
      }


    if (sock != INVALID_SOCK) {
        close(sock);
    }
    free(address_info);
    free(ppp);


    return ret;
}