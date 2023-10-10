// send_img_aliyun.ino

#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// #include "aliyun_mqtt.h"
#include "aliyunmqtt.h"

#include "esp_camera.h"

#include <SPIFFS.h>
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>

// 内存存储相关配置
#define EEPROM_SIZE 1
int pictureNumber = 0;
String msg;
int buttonState = 0;
int btnHold = 0;


// #define SENSOR_PIN 10
//以下信息需要自己修改
#define WIFI_SSID "WIFI-NAME"  //替换自己的WIFI
#define WIFI_PASSWD "xxxxxxxx"   //替换自己的WIFI密码

// 阿里云物联网 三元组
#define PRODUCT_KEY "xxxxxxxxxxxxx"                         //替换自己的PRODUCT_KEY
#define DEVICE_NAME "xxxxxxxxxxxxx"                       //替换自己的DEVICE_NAME
#define DEVICE_SECRET "xxxxxxxxxxxxx"                     //替换自己的DEVICE_SECRET 
                                                          

//以下不需修改
#define ALINK_BODY_FORMAT "{\"id\":\"123\",\"version\":\"1.0\",\"method\":\"%s\",\"params\":%s}"
#define ALINK_TOPIC_PROP_POST "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"
#define ALINK_TOPIC_PROP_POSTRSP "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post_reply"
#define ALINK_TOPIC_PROP_SET "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/service/property/set"
#define ALINK_METHOD_PROP_POST "thing.event.property.post"
#define ALINK_TOPIC_DEV_INFO "/ota/device/inform/" PRODUCT_KEY "/" DEVICE_NAME ""
#define ALINK_VERSION_FROMA "{\"id\": 123,\"params\": {\"version\": \"%s\"}}"
unsigned long lastMs = 0;
// 测试 初始温度
int i = 15;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

//CAMERA_MODEL_AI_THINKER类型摄像头的引脚定义
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static camera_config_t camera_config = {
  .pin_pwdn = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk = XCLK_GPIO_NUM,
  .pin_sscb_sda = SIOD_GPIO_NUM,
  .pin_sscb_scl = SIOC_GPIO_NUM,

  .pin_d7 = Y9_GPIO_NUM,
  .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM,
  .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM,
  .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM,
  .pin_d0 = Y2_GPIO_NUM,
  .pin_vsync = VSYNC_GPIO_NUM,
  .pin_href = HREF_GPIO_NUM,
  .pin_pclk = PCLK_GPIO_NUM,

  .xclk_freq_hz = 20000000,
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,

  .pixel_format = PIXFORMAT_JPEG,
  // .frame_size = FRAMESIZE_VGA,
  // FRAMESIZE_UXGA (1600 x 1200)
  // FRAMESIZE_QVGA (320 x 240)
  // FRAMESIZE_CIF (352 x 288)
  // FRAMESIZE_VGA (640 x 480)
  // FRAMESIZE_SVGA (800 x 600)
  // FRAMESIZE_XGA (1024 x 768)
  // FRAMESIZE_SXGA (1280 x 1024)
  .frame_size = FRAMESIZE_QVGA,
  .jpeg_quality = 10,
  // 图像质量（jpeg_quality) 可以是 0 到 63 之间的数字。数字越小意味着质量越高
  .fb_count = 1,
};

void init_wifi(const char *ssid, const char *password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi does not connect, try again ...");
    delay(500);
  }

  Serial.println("Wifi is connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  Serial.println((char *)payload);

  if (strstr(topic, ALINK_TOPIC_PROP_SET)) {
    StaticJsonBuffer<100> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }
  }
}

void mqtt_check_connect() {
  while (!mqttClient.connected())  //
  {
    while (connect_aliyun_mqtt(mqttClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET)) {
      Serial.println("MQTT connect succeed!");
      //client.subscribe(ALINK_TOPIC_PROP_POSTRSP);
      mqttClient.subscribe(ALINK_TOPIC_PROP_SET);

      Serial.println("subscribe done");
    }
  }
}

void mqtt_interval_post() {
  //  static int i=0;
  char param[512];
  char jsonBuf[1024];
  sprintf(jsonBuf, "{\"id\":\"1189401707\",\"version\":\"1.0.0\",\"method\":\"%s\",\"params\":{\"img\":\"END\"}}");
  Serial.println(jsonBuf);
  mqttClient.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
  Serial.println("发送结束符");
  delay(1000);
}



// 摄像头、SD卡与 SPIFFS 初始化
esp_err_t camera_init() {
  //initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.println("Camera Init Failed");
    return err;
  }
  sensor_t *s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV2640_PID) {
    //        s->set_vflip(s, 1);//flip it back
    //        s->set_brightness(s, 1);//up the blightness just a bit
    //        s->set_contrast(s, 1);
  }
  Serial.println("Camera Init OK!");
  return ESP_OK;
}


void sd_init(void) {
  //SD card init
  if (!SD_MMC.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
  }
}

void SPIFFS_init() {
  //初始化SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
  } else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  //Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}
// 摄像头、SD卡与 SPIFFS 初始化 end



void setup() {
  Serial.begin(115200);
  Serial.println("程序 Start");
  init_wifi(WIFI_SSID, WIFI_PASSWD);

  camera_init();
  sd_init();
  SPIFFS_init();

  mqttClient.setCallback(mqtt_callback);
}

// the loop function runs over and over again forever
void loop() {

  // 程序开始拍照并保存
  Serial.print("进行拍照\n");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.print("Camera capture failed");
    return;
  } else {
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0) + 1;
    // Path where new picture will be saved in SD Card
    String path = "/picture" + String(pictureNumber) + ".jpg";

    fs::FS &fs = SD_MMC;
    Serial.printf("文件名字: %s\n", path.c_str());
    File file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    } else {
      file.write(fb->buf, fb->len);  // payload (image), payload length
      Serial.println(fb->len);
      Serial.print("抓拍成功并保存\n");
      Serial.printf("保存路径: %s\n\n", path.c_str());
      EEPROM.write(0, pictureNumber);
      EEPROM.commit();
    }


    String a1 = "{\"id\":\"1189401707\",\"version\":\"1.0.0\",\"method\":\"123\",\"params\":{\"img\":\"";
    String a2;
    String a3 = "\"}}";
    char data[4104];
    // 将图片分为不超过 800 通过 MQTT 发送出去
    for (int i = 0; i < fb->len; i++) {
      sprintf(data, "%02X", *(fb->buf + i));
      a2 += data;
      if (a2.length() == 800) {
        String a4 = a1 + a2;
        String a = a4 + a3;
        char jsonBuf[a.length() + 1];
        for (int i = 0; i < a.length(); i++)
          jsonBuf[i] = a[i];
        jsonBuf[a.length()] = '\0';
        Serial.println(jsonBuf);
        mqttClient.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
        a2 = "", a = "", a4 = "";
        // ms
        delay(200);
      }
    }
    if (a2.length() > 0) {
      String a4 = a1 + a2;
      String a = a4 + a3;
      char jsonBuf[a.length() + 1];
      for (int i = 0; i < a.length(); i++)
        jsonBuf[i] = a[i];
      jsonBuf[a.length()] = '\0';
      Serial.println(jsonBuf);
      mqttClient.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
      a2 = "", a = "", a4 = "";
    }
     // 将图片分为不超过 800 通过 MQTT 发送出去  end
    //  
    char endBuf[100];
    sprintf(endBuf, "{\"id\":\"1189401707\",\"version\":\"1.0.0\",\"method\":\"123\",\"params\":{\"img\":\"END\"}}");
    Serial.println(endBuf);
    mqttClient.publish(ALINK_TOPIC_PROP_POST, endBuf);
    Serial.println("发送结束符");
   
  }
  Serial.println("图片发送完成了......");
  delay(1000);
  // 图片发送结束后发送 END


  if (millis() - lastMs >= 10000) {
    lastMs = millis();
    mqtt_check_connect();
    // Post  interval 间隔
    // mqtt_interval_post();
  }

  mqttClient.loop();

  unsigned int WAIT_MS = 2000;
  delay(WAIT_MS);  // ms
  Serial.println(millis() / WAIT_MS);
}