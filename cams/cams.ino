#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <config.h>

#define CAMERA_MODEL_AI_THINKER
#define WAITING_TIMEOUT 120000
#define OUTPUT 1
#define INPUT 0

#include "camera_pins.h"

const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;
const char *websocket_server_host = WEBSOCKET_SERVER_HOST;
const uint16_t websocket_server_port = WEBSOCKET_SERVER_PORT;

using namespace websockets;
WebsocketsClient client;
bool isWebSocketConnected;

void onEventsCallback(WebsocketsEvent event, String data)
{
  if (event == WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connection Opened");
    isWebSocketConnected = true;
    delay(100);
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connection Closed");
    isWebSocketConnected = false;
    delay(100);
  }
}

// Notification LED
void flashLED(int flashtime)
{
#if defined(LED_BUILTIN) // If we have it; flash it.
  delay(flashtime);
  digitalWrite(LED_BUILTIN, 0); // On at full power.
  delay(flashtime);             // delay
  digitalWrite(LED_BUILTIN, 1); // turn Off
#else
  return; // No notifcation LED, do nothing, no delay
#endif
}

void connectToNetwork()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long startWaiting = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWaiting <= WAITING_TIMEOUT)
  {
    flashLED(900);
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    Serial.println("Connect failed. Try again...");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Timeout. Restarting...");
    ESP.restart();
  }
}

void setup()
{
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);

  isWebSocketConnected = false;
  Serial.begin(115200);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 40;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
    return;
  }

  connectToNetwork();
  client.onEvent(onEventsCallback);
  webSocketConnect();
}

void webSocketConnect()
{
  while (!client.connect(websocket_server_host, websocket_server_port, "/"))
  {
    delay(500);
    Serial.print(".");
  }
  delay(500);
  Serial.println("Websocket Connected!");
}

void loop()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    connectToNetwork();
  }

  if (!isWebSocketConnected)
  {
    webSocketConnect();
  }

  if (client.available())
  {
    client.poll();
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    esp_camera_fb_return(fb);
    return;
  }

  if (fb->format != PIXFORMAT_JPEG)
  {
    Serial.println("Non-JPEG data not implemented");
    return;
  }

  fb->buf[12] = 0x01; // FIRST CAM
  // fb->buf[12] = 0x02; // SECOND CAM

  client.sendBinary((const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}
