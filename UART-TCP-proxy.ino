//FQBN: esp32:esp32:lolin_s2_mini:PartitionScheme=huge_app

#include <WiFi.h>
#include <AsyncTCP.h>  // need install v 3.4.4, by ESP32Async [https://github.com/ESP32Async/AsyncTCP]
#include "config.h"

HardwareSerial SerialUART(1);

const char* ssid = SSID;
const char* password = PASSWD;

// TCP Server
AsyncServer tcpServer(PORT);
AsyncClient* tcpClient = NULL;

uint8_t uartRxBuffer[UART_BUFFER_SIZE];
volatile size_t uartRxHead = 0;
volatile size_t uartRxTail = 0;

portMUX_TYPE uartBufferMux = portMUX_INITIALIZER_UNLOCKED;

// UART receive interrupt handler
void IRAM_ATTR handleUARTInterrupt() {
  portENTER_CRITICAL(&uartBufferMux);

  while (SerialUART.available()) {
    size_t nextHead = (uartRxHead + 1) % UART_BUFFER_SIZE;

    if (nextHead != uartRxTail) {
      uartRxBuffer[uartRxHead] = SerialUART.read();
      uartRxHead = nextHead;
    } else {
      uartRxTail = (uartRxTail + 1) % UART_BUFFER_SIZE;
      uartRxBuffer[uartRxHead] = SerialUART.read();
      uartRxHead = nextHead;
    }
  }

  portEXIT_CRITICAL(&uartBufferMux);
}

void sendUARTDataToTCP() {
  static uint8_t tempBuffer[UART_BUFFER_SIZE];
  size_t bytesToSend = 0;

  portENTER_CRITICAL(&uartBufferMux);

  while (uartRxTail != uartRxHead && bytesToSend < UART_BUFFER_SIZE) {
    tempBuffer[bytesToSend++] = uartRxBuffer[uartRxTail];
    uartRxTail = (uartRxTail + 1) % UART_BUFFER_SIZE;
  }

  portEXIT_CRITICAL(&uartBufferMux);

  if (bytesToSend > 0 && tcpClient && tcpClient->connected()) {
    tcpClient->write((const char*)tempBuffer, bytesToSend);
  }
}

void setup() {
  Serial.begin(UART_BAUD_RATE); // Debug serial
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.softAP(ssid, password);
  delay(1000);
  WiFi.softAPConfig(STATIC_IP, STATIC_IP, NETMASK);

  Serial.println("UART-TCP Proxy");
  Serial.print("AP SSID: ");
  Serial.println(ssid);
  Serial.print("AP IP: ");
  Serial.println(STATIC_IP);
  Serial.printf("TCP server on port %d\n", PORT);

  SerialUART.begin(UART_BAUD_RATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  SerialUART.onReceive(handleUARTInterrupt);
  Serial.printf("UART on pins %d(RX), %d(TX) @ %d baud\n", UART_RX_PIN, UART_TX_PIN, UART_BAUD_RATE);

  // TCP server callbacks
  tcpServer.onClient([](void* arg, AsyncClient* client) {
    if (tcpClient == NULL || !tcpClient->connected()) {
      tcpClient = client;
      Serial.println("TCP client connected");

      client->onData([](void* arg, AsyncClient* client, void* data, size_t len) {
        SerialUART.write((uint8_t*)data, len);
      }, NULL);

      client->onDisconnect([](void* arg, AsyncClient* client) {
        Serial.println("TCP client disconnected");
        if (tcpClient == client) {
          tcpClient = NULL;
        }
        delete client;
      }, NULL);

      client->onError([](void* arg, AsyncClient* client, int8_t error) {
        Serial.printf("TCP error: %d\n", error);
        if (tcpClient == client) {
          tcpClient = NULL;
        }
        client->close();
      }, NULL);

      client->onTimeout([](void* arg, AsyncClient* client, uint32_t time) {
        Serial.println("TCP timeout");
        client->close();
      }, NULL);

      client->onPoll([](void* arg, AsyncClient* client) {
      }, NULL);

    } else {
      Serial.println("TCP connection refused (only one client allowed)");
      client->close();
      delete client;
    }
  }, NULL);

  tcpServer.begin();
}

void loop() {
  sendUARTDataToTCP();

  delay(1);
}