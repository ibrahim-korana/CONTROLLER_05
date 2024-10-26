

#include "udpserver.h"

static const char *UDP_TAG = "UDP_SERVER";
#define MAX_DATA_LEN 64
#define RECEIVE_TIMEOUT 400000

void UdpServer::server_task(void *pvParameters)
{
    UdpServer *ths =( UdpServer *)pvParameters;
    uint8_t *rx_buffer;
    uint8_t *back;
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int port = ths->port;
    struct sockaddr_in dest_addr;

    while (1) 
    {
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        ths->MainSock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (ths->MainSock < 0) {
            ESP_LOGE(UDP_TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(UDP_TAG, "Socket created");
        int broadcast = 1;
        if (setsockopt(ths->MainSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))<0)
        {
            ESP_LOGE(UDP_TAG,"Soket Broadcast olarak işaretlenemedi");
            break;
        }

        // Set timeout
        /*
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 800000;
        setsockopt (ths->MainSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        */
       
        int err = bind(ths->MainSock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(UDP_TAG, "Socket unable to bind: errno %d", errno);
            break;
        }
        ESP_LOGI(UDP_TAG, "Socket bound, port %d", port);
        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        rx_buffer = (uint8_t *)calloc(1,MAX_DATA_LEN);
        
        while (1) 
        {
            memset(rx_buffer,0,MAX_DATA_LEN);
            int len = recvfrom(ths->MainSock, rx_buffer, MAX_DATA_LEN - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len < 0) {
                ESP_LOGE(UDP_TAG, "Main recvfrom failed: errno %d", errno);
                break;
            } else {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                rx_buffer[len] = 0;
                back = (uint8_t *)calloc(1,MAX_DATA_LEN);
                uint8_t bcklen=0;
                ths->callback(rx_buffer, len, back, &bcklen);
                if (bcklen>0)
                  {
                        int err = sendto(ths->MainSock, back, bcklen, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                        if (err < 0) {
                            ESP_LOGE(UDP_TAG, "Error occurred during sending: errno %d", errno);
                            break;
                        }
                  }
                  free(back);
            }
        } //while
        free(rx_buffer);
        if (ths->MainSock != -1) {
            ESP_LOGE(UDP_TAG, "Shutting down socket and restarting...");
            shutdown(ths->MainSock, 0);
            close(ths->MainSock);
        }
       
    } //while
    vTaskDelete(NULL);
};


bool UdpServer::_send(const char* hostip, uint8_t *data, int datalen)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(hostip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(UDP_TAG, "_send : Soket oluşturulamadı: errno %d", errno);
        return false;
    }
    
    bool bck = false;
    int err = sendto(sock, data, datalen, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(UDP_TAG, "_send : Data Gonderilemedi errno %d", errno);
    } else bck=true;
    shutdown(sock, 0);
    close(sock);
    return bck;       
}


/*
bool UdpServer::send_and_receive(const char* hostip, uint8_t *data, int datalen, uint8_t* recv, uint8_t *reclen)
{
         
         struct sockaddr_in dest_addr;
         dest_addr.sin_addr.s_addr = inet_addr(hostip);
         dest_addr.sin_family = AF_INET;
         dest_addr.sin_port = htons(5000);

        int err = sendto(MainSock, data, datalen, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(UDP_TAG, "Error occurred during sending: errno %d", errno);
            return false;
                    } 

        if (recv!=NULL)
        {
            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(MainSock, recv, *reclen, 0, (struct sockaddr *)&source_addr, &socklen);
            if (len < 0) {
                    ESP_LOGE(UDP_TAG, "recvfrom failed: errno %d", errno);
                    *reclen=0;;
                    return false;
                } else {
                    char addr_str[60];
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                    recv[len] = 0; // Null-terminate whatever we received and treat like a string
                    *reclen=len;
                    ESP_LOGE(UDP_TAG, "recvlen %s %d %s", addr_str, len, (char*)recv);
                    vTaskDelay(10/portTICK_PERIOD_MS);
                }   
        }            
                  
        return true;            
}
*/

bool UdpServer::send_and_receive(const char* hostip, uint8_t *data, int datalen, uint8_t* recv, uint8_t *reclen)
{
    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(hostip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(UDP_TAG, "Unable to create socket: errno %d", errno);
        return false;
    }
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = RECEIVE_TIMEOUT;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    int err = sendto(sock, data, datalen, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(UDP_TAG, "Error occurred during sending: errno %d", errno);
        shutdown(sock, 0);
        close(sock);
        return false;
    }
    if (recv!=NULL)
    {
        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, recv, *reclen, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
                //ESP_LOGE(UDP_TAG, "recvfrom failed: errno %d", errno);
                *reclen=0;
                shutdown(sock, 0);
                close(sock);
                return false;
            } else {
                //char addr_str[60];
                //inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                recv[len] = 0; // Null-terminate whatever we received and treat like a string
                *reclen=len;
                //ESP_LOGI(UDP_TAG, "recvlen %s %d %s", addr_str, len, (char*)recv);
                vTaskDelay(10/portTICK_PERIOD_MS);
            }   
    }
    
    shutdown(sock, 0);
    close(sock);
    return true;
}


bool UdpServer::send_dali(const char* hostip, package_t *pk)
{
    sprintf(mm,"%ld",pk->package);  
    return _send(hostip,(uint8_t *)mm,strlen(mm));
}

uint8_t UdpServer::send_and_receive_dali(const char* hostip, package_t *pk, bool fast)
{
    sprintf(mm,"%ld",pk->package);  
    memset(ret,0,20);
    bool ok=true;
    uint8_t rr=0, ret0=0x71, Max=2;
    if (fast) Max=0;
    while(ok)
    {
        rcvlen = 200;        
        if (send_and_receive(hostip,(uint8_t *)mm,strlen(mm),(uint8_t*)ret,&rcvlen))
        {
            ok=false;
            if (strcmp((char*)ret,"OK")==0) return 0x71;            
            ret0=atoi((char*)ret);
            //printf("Donen %d %s %d\n",rcvlen,(char*)ret,ret0);
            return ret0;
        }  
        if (++rr>Max) {
            ret0=0x71;
            ok=false;
        }
        vTaskDelay(10/portTICK_PERIOD_MS);      
    }   
    return ret0;
}

bool UdpServer::start(uint16_t prt, udpserver_callback_t callb)
{
    port = prt;
    callback = callb;
    mm = (char*)calloc(1,20);
    ret = (char*)calloc(1,200);
    xTaskCreate(server_task, "udp_server", 4096, (void*)this, 5, NULL);
    return true;
}