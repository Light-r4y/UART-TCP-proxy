#ifndef CONFIG_H
#define CONFIG_H

// WiFi AP Configuration
#define SSID     "ssid"      // SSID AP
#define PASSWD   "passw0rd"  // wiFi password
#define STATIC_IP IPAddress(192, 168,  80, 1)  // static IP 
#define NETMASK   IPAddress(255, 255, 255, 0)  // netmask 
#define PORT   2000

// UART Configuration
#define UART_TX_PIN 17
#define UART_RX_PIN 16
#define UART_BAUD_RATE 115200
#define UART_BUFFER_SIZE 2048

//LED
#define LED_PIN 15

#endif  // CONFIG_H
