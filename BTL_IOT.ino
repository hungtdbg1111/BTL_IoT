#include <DHT.h>
#include <WiFi.h>
#include "ThingSpeak.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Wifi
const char* ssid = "Galaxy A1202A3";
const char* password = "12345679";

// Discord
const char* discordWebhookUrl = "https://discord.com/api/webhooks/1308864842270248972/u0uOuaAYcgf7IMnn21ClTFX9glmH_tv4d2exL_6IPl1YEzVyDQ-bEhnlXh2aGGi2_7zP";

// ThingSpeak
#define SECRET_CH_ID 2755765      
#define SECRET_WRITE_APIKEY "4CKFUHXGOH2QHDAC" 
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;
WiFiClient  client;

// Pins
#define dhtPin 26
#define mqPin 39
#define ledPower 13
#define dustPin 36

#define DHTTYPE DHT11
DHT dht(dhtPin, DHTTYPE);

// Variables
unsigned long sendTime = 0;
unsigned long sysTime = 0;
int count = 0;
int notiTime = 1;
float pmValue, coValue, totalPm = 0, totalCo = 0, pmAvg, coAvg, aqi;
const float e = 2.71828;

// Functions declarations (good practice)
void initWiFi();
void measureDht();
void measurePm2_5();
void measureCo();
void calAqi();
void sendToDiscord(String message);
void sendIndex();

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(ledPower, OUTPUT);
  pinMode(dustPin, INPUT);
  analogReadResolution(10);

  initWiFi();
  ThingSpeak.begin(client);
}

void loop() {
  measureDht();
  measurePm2_5();
  measureCo();

  count++;
  if (count >= 4) {
    calAqi();
    sendDataToThingSpeak(aqi, 0.0, 0.0, 0.0, true); // Send AQI and clear status
    sendIndex();
    totalCo = 0;
    totalPm = 0;
    count = 0;
  }
  sendDataToThingSpeak(pmValue, coValue, dht.readTemperature(), dht.readHumidity(), false); // Send PM and CO, don't clear status
  delay(15000);
}

void sendDataToThingSpeak(float value1, float value2, float value3, float value4, bool sendAQI) {
  String myStatus = "";
  if (sendAQI) {
    ThingSpeak.setField(1, aqi);
  } else {
    ThingSpeak.setField(2, value1); // Gửi PM
    ThingSpeak.setField(3, value2); // Gửi CO
    ThingSpeak.setField(4, value3); // Gửi nhiệt độ
    ThingSpeak.setField(5, value4); // Gửi độ ẩm
    myStatus = String(value1) + "," + String(value2); // Kết hợp giá trị cho trạng thái
  }
  ThingSpeak.setStatus(myStatus);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("Channel update successful.");
  } else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void measureDht(){
  int temp = dht.readTemperature();
  int humi = dht.readHumidity();
  Serial.print(temp);
  Serial.println(" °C");
  Serial.print(humi);
  Serial.println(" %");
}

void measurePm2_5(){
  digitalWrite(ledPower,LOW); // power on the LED
  delayMicroseconds(280);
  int dustVal=analogRead(dustPin)+85; // read the dust value
  delayMicroseconds(40);
  digitalWrite(ledPower,HIGH); // turn the LED off
  delayMicroseconds(9680);
  
  float voltage = dustVal*0.00489;
  pmValue = (0.172*voltage-0.1)*1000.00;

  if (pmValue < 0 )
    pmValue = 0;
  if (pmValue > 500)
    pmValue = 500;

  Serial.print(pmValue);
  Serial.println(" ug/m3");
  totalPm +=pmValue;
  delay(10);
}

void measureCo(){
  float sensorValue = analogRead(mqPin);
  float sensor_volt = sensorValue/1024*5.0;
  coValue = 3.027*pow(e,(1.0698*sensor_volt));
  Serial.print(coValue);
  Serial.println(" ppm");
  totalCo += coValue;
  delay(10);
}

void calAqi(){
  pmAvg = totalPm/count;
  coAvg = totalCo/count;
  float pmAqi = pmAvg*2.103004+25.553648;
  float coAqi = coAvg*16.89655-59.51724;
  delay(10);
  if(pmAqi > coAqi) aqi = pmAqi;
  else aqi = coAqi;
}

void sendIndex() {
  String mess;
  mess += "AQI index: ";
  mess.concat(aqi);
  mess += "\n";
  mess += "PM2.5 index: ";
  mess.concat(pmValue);
  mess += "\n";
  mess += "Temperature: ";
  mess.concat(dht.readTemperature()); // Gửi nhiệt độ
  mess += " °C\n";
  mess += "Humidity: ";
  mess.concat(dht.readHumidity()); // Gửi độ ẩm
  mess += " %\n";
  sendToDiscord(mess); // Gửi tin nhắn đến Discord
}

void sendToDiscord(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Đang gửi yêu cầu đến Discord...");
    HTTPClient http;
    http.begin(discordWebhookUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000); // Thiết lập timeout là 5 giây

    // Sử dụng DynamicJsonDocument để tạo JSON đúng định dạng
    StaticJsonDocument<256> doc; // Kích thước 256, điều chỉnh nếu cần
    doc["content"] = message;

    String jsonMessage;
    serializeJson(doc, jsonMessage); // Chuyển đổi JSON thành chuỗi

    Serial.println("JSON đang được gửi:");
    Serial.println(jsonMessage);

    int httpResponseCode = http.POST(jsonMessage);
    Serial.println("Yêu cầu đã gửi.");
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}