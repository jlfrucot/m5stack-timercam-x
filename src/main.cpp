/****************************************************************************************************************************************************
 *  TITLE: HOW TO BUILD A $9 RSTP VIDEO STREAMER: Using The ESP-32 CAM Board || Arduino IDE - DIY #14
 *  DESCRIPTION: This sketch creates a video streamer than uses RTSP. You can configure it to either connect to an existing WiFi network or to create
 *  a new access point that you can connect to, in order to stream the video feed.
 *
 *  By Frenoy Osburn
 *  YouTube Video: https://youtu.be/1xZ-0UGiUsY
 *  BnBe Post: https://www.bitsnblobs.com/rtsp-video-streamer---esp32
 ****************************************************************************************************************************************************/

/********************************************************************************************************************
 *  Board Settings: that works for me
 *  platform = espressif32
 *  framework = arduino
 *  board = m5stack-timer-cam
 *  board_build.mcu = esp32
 *  board_build.f_cpu = 240000000L
 *  upload_speed = 1500000

 *  monitor_speed = 115200
 *  build_flags = 
	    -DBOARD_HAS_PSRAM
	    -mfix-esp32-psram-cache-issue

 *********************************************************************************************************************/

#include "src/OV2640.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <Update.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include "led.h"
#include "battery.h"

// Enable/Disable Serial
#define DEBUG true   // true pour avoir les debug sur le port série
Print &out = Serial; //Pour garder un lien vers le port serie
#define Serial \
    if (DEBUG) \
    Serial

#define ENABLE_WEBSERVER
// #define ENABLE_RTSPSERVER
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 15       /* Time ESP32 will go to sleep (in seconds) */

// #include <AsyncElegantOTA.h>

// Select camera model voir src/camera_pins.h
#define CAMERA_MODEL_M5STACK_TIMERCAM

#include "camera_pins.h"

OV2640 cam;

#ifdef ENABLE_WEBSERVER
#include "ota.h"

WebServer server(80);

#endif

#ifdef ENABLE_WEBSERVER
void handle_jpg_stream(void)
{
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    while (1)
    {
        cam.run();
        if (!client.connected())
            break;
        response = "--frame\r\n";
        response += "Content-Type: image/jpeg\r\n\r\n";
        server.sendContent(response);

        client.write((char *)cam.getfb(), cam.getSize());
        server.sendContent("\r\n");
        if (!client.connected())
            break;
    }
}

void handleNotFound()
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
}
#endif

//////////////////////////////////////////////       WiFi         ///////////////////////////////
void restartESP32Cam()
{
    esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR * TIME_TO_SLEEP);
    esp_deep_sleep_start();
}
void onWiFiEventFired(WiFiEvent_t event)
{
    // On traite les évènements émis par le wifi
    switch (event)
    {
    case SYSTEM_EVENT_STA_DISCONNECTED:
        // Si on est déconnecté, on redémarre l'esp
        Serial.println("On a perdu le wifi");
        restartESP32Cam();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.print("On a obtenu une adresse IP : ");
        Serial.println(WiFi.localIP());
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        Serial.println("WiFi connecté");
        break;
    default:
        break;
    }
}
//////////////////////////////////////////////       Setup         ///////////////////////////////
void setup()
{
    Serial.begin(115200);
    Serial.println("On (re)démarre");

// On tente de récupérer les paramètres enregistrés dans la mémoire permanente

    // Démarre le système de fichier SPIFFS
    if (!SPIFFS.begin())
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
    }
    File file = SPIFFS.open("/settings.json");

    StaticJsonDocument<500> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        // return;
    }

    JsonObject wifi = doc["wifi"];

    JsonArray wifi_localeIP = wifi["localeIP"];
    IPAddress mylocalIp(wifi_localeIP[0],
                        wifi_localeIP[1],
                        wifi_localeIP[2],
                        wifi_localeIP[3]);

    JsonArray wifi_subnet = wifi["subnet"];
    IPAddress subnet(wifi_subnet[0],
                     wifi_subnet[1],
                     wifi_subnet[2],
                     wifi_subnet[3]);

    JsonArray wifi_gateway = wifi["gateway"];
    IPAddress gateway(wifi_gateway[0],
                      wifi_gateway[1],
                      wifi_gateway[2],
                      wifi_gateway[3]);

    const char *wifikey_ssid = doc["wifikey"]["ssid"];
    const char *wifikey_password = doc["wifikey"]["password"];

    int webserverPort = doc["webserverPort"];

    // Fix WiFi static IP
    // IPAddress ip;
    Serial.println(wifikey_ssid);
    Serial.println(wifikey_password);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWiFiEventFired);
    WiFi.config(mylocalIp, gateway, subnet);
    WiFi.begin(wifikey_ssid, wifikey_password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }
    // ip = WiFi.localIP();
    Serial.println(F("WiFi connected"));
    Serial.println("");
    Serial.println(WiFi.localIP());

// Démarrage de la caméra
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
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_CIF; /*!< Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA  */
    config.jpeg_quality = 16;
    config.fb_count = 2;
    led_init(CAMERA_LED_GPIO);
    bat_init();
    if (cam.init(config) == ESP_OK)
    {
        led_brightness(512);
        sensor_t *s = esp_camera_sensor_get();
        // s->set_hmirror(s, 1);
        s->set_vflip(s, 1);
    }
    else
    {
        led_brightness(0);
    }


#ifdef ENABLE_WEBSERVER
    server.on("/", HTTP_GET, handle_jpg_stream);

    /*handling uploading firmware file */
    server.on("/serverIndex", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex);
    });

    server.on(
        "/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart(); }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
#if DEBUG
          Update.printError(out);
#endif
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
#if DEBUG
          Update.printError(out);
#endif
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
#if DEBUG
          Update.printError(out);
#endif

      }
    } });
    server.onNotFound(handleNotFound);
    server.begin(webserverPort);
#endif
}

void loop()
{
#ifdef ENABLE_WEBSERVER
    server.handleClient();
#endif
}
