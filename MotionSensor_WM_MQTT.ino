#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "DHT.h"

#ifdef ESP32
  #include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define DHTPIN D5         // pin of the arduino where the sensor is connected to
#define DHTTYPE DHT11     // define the type of sensor (DHT11 or DHT22)

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char api_token[34] = "api_token";
char mqttSubTopic[40] = "/home/motionSensor/";
char mqttPubTopicMotion[40] = "/home/MotionSensor/motion/";
char mqttPubTopicTemp[40] = "/home/MotionSensor/temperature/";
char mqttPubTopicHum[40] = "/home/MotionSensor/humidity/";
char clientName[20] = "MotionSensor";
char strPayload[20] = "";

const char* nameAP = "AP-MotionSensor";
const char* passAP = "12345678";
//define PubSubClient values here
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
unsigned long lastMsgMotion = 0;
unsigned long lastMsgDht = 0;

//define DHT stuff here
DHT dht(DHTPIN, DHTTYPE, 6);
float temperature;
float humidity;
float tempCorrection = 1.5;
float dhtDelay = 300*1000; //Set dht value sending interval in seconds*1000

//define motionsensor stuff here
int pirPin = D7;
int motion;
int count = 0;
unsigned long now = millis();

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
//PubSubClient client(wifiClient);

void getDhtValues() { //get dht sensor values
  temperature = dht.readTemperature() - tempCorrection;
  humidity = dht.readHumidity();
  Serial.println(String(temperature) +" °C");
  Serial.println(String(humidity) +" %");
}

void callback(char* mqttSubTopic, byte* payload, unsigned int length) {
  Serial.print("\nMessage arrived [");
  Serial.print(mqttSubTopic);
  Serial.print("] \n");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    strPayload[i] = (char)payload[i];
  }
  Serial.println("\n");

  if(strcmp(strPayload, "status")==0){
    getDhtValues();
    client.publish(mqttPubTopicTemp, String(temperature).c_str());
    delay(10);
    client.publish(mqttPubTopicHum, String(humidity).c_str());
    
    //client.publish(mqttPubTopic, "test completed");
    //char mqttSubTopic[40] = "/home/motionSensor/sub/";
    //char mqttPubTopicMotion[40] = "/home/motionSensor/motion/";
    //char mqttPubTopicTemp[40] = "/home/motionSensor/temperature/";
    //char mqttPubTopicHum[40] = "/home/motionSensor/humidity/";
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = clientName;
    clientId += "-"+(random(0xffff), HEX);
    if (client.connect((char*)clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqttSubTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
//SETUP GPIO-config DHT and Motionsensor---------------------------------------------------------
  pinMode(DHTPIN, INPUT);
  dht.begin();
  
//SETUP WiFi Manager with custom json parameters-------------------------------------------------
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(api_token, json["api_token"]);
          
          //strcpy(mqttSubTopic, json["mqttSubTopic"]);
          //strcpy(mqttPubTopic, json["mqttPubTopic"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_api_token("apikey", "API token", api_token, 32);
  WiFiManagerParameter custom_mqttSubTopic("subTopic", "subTopic", mqttSubTopic, 40);
  WiFiManagerParameter custom_mqttPubTopicMotion("pubTopicMotion", "pubTopicMotion", mqttPubTopicMotion, 40);
  WiFiManagerParameter custom_mqttPubTopicTemp("pubTopicTemp", "pubTopicTemp", mqttPubTopicTemp, 40);
  WiFiManagerParameter custom_mqttPubTopicHum("pubTopicMotion", "pubTopicHum", mqttPubTopicHum, 40);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_api_token);
  wifiManager.addParameter(&custom_mqttSubTopic);
  wifiManager.addParameter(&custom_mqttPubTopicMotion);
  wifiManager.addParameter(&custom_mqttPubTopicTemp);
  wifiManager.addParameter(&custom_mqttPubTopicHum);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(nameAP, passAP)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }
  else {
      WiFi.mode(WIFI_STA);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(api_token, custom_api_token.getValue());
  strcpy(mqttSubTopic, custom_mqttSubTopic.getValue());
  strcpy(mqttPubTopicMotion, custom_mqttPubTopicMotion.getValue());
  strcpy(mqttPubTopicTemp, custom_mqttPubTopicTemp.getValue());
  strcpy(mqttPubTopicHum, custom_mqttPubTopicHum.getValue());
  
  Serial.println("The values in the file are: ");
  Serial.println("\tmqtt_server : " + String(mqtt_server));
  Serial.println("\tmqtt_port : " + String(mqtt_port));
  Serial.println("\tapi_token : " + String(api_token));
  Serial.println("\tmqttSubTopic: " + String(mqttSubTopic));
  Serial.println("\tmqttPubTopicMotion: " + String(mqttPubTopicMotion));
  Serial.println("\tmqttPubTopicTemp: " + String(mqttPubTopicTemp));
  Serial.println("\tmqttPubTopicHum: " + String(mqttPubTopicHum));


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["api_token"] = api_token;
    json["mqttSubTopic"] = mqttSubTopic;
    json["mqttPubTopicMotion"] = mqttPubTopicMotion;
    json["mqttPubTopicTemp"] = mqttPubTopicTemp;
    json["mqttPubTopicHum"] = mqttPubTopicHum;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }
//WiFi initialized --------------------------------------------------------------
  Serial.println(WiFi.localIP());

//SETUP PubSubClient ---------------------------------------------------------------------
  //client.setServer(mqtt_server, int(mqtt_port));
  //client.setCallback(callback);
  /*Serial.println("Connection to:");
  Serial.println(mqtt_server);
  if (client.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
  } */
  reconnect();
  client.subscribe(mqttSubTopic);
  //Serial.println("Subscribe to sub_topic");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(5);

  now = millis();
  //DHT loop-------------
  if((now - lastMsgDht) > dhtDelay) {
    Serial.println("Sending DHT values");
    lastMsgDht = now;
    getDhtValues();
    client.publish(mqttPubTopicTemp, String(temperature).c_str());
    delay(10);
    client.publish(mqttPubTopicHum, String(humidity).c_str());
  }
  //Motionsensor
  motion = digitalRead(pirPin);
  if(now - lastMsgMotion > 10000){
    if(motion == HIGH){
      Serial.println("Motion detected! Publishing message");
      lastMsgMotion = now;
      //count = 0;
      client.publish(mqttPubTopicMotion, String("TRUE").c_str());
    }
    /*else{
      count++;
      if(count >= 5){
        Serial.println("No motion detected five times");
        client.publish(mqttPubTopicMotion, String("FALSE").c_str());
        count = 0;
      }
    }*/
  }
}
