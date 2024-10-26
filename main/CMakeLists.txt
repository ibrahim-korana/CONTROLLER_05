idf_component_register(
    SRCS "main.cpp" "comp/storage.cpp" "comp/rs485.cpp" "comp/ww5500.cpp" "comp/dali.cpp" 
    "comp/enc28j60/ethernet.cpp" 
    "comp/enc28j60/esp_eth_mac_enc28j60.c"
    "comp/enc28j60/esp_eth_phy_enc28j60.c"
    "comp/tcpserver.cpp" "comp/jsontool.cpp" "comp/network.cpp" 
    "comp/cron/jobs.cpp" "comp/cron/ccronexpr.cpp" "comp/cron/cron.cpp" 
    "comp/tcpclient.cpp"
    "comp/udpserver.cpp"
    INCLUDE_DIRS ""
)
