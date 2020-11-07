#include <Wire.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <OneWire.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define SEALEVELPRESSURE_HPA (1013.25)

// 20 másodperc van csatlakozni a WIFI-hez
#define WIFI_TIMEOUT (20 * 1000)

// 10 perc deep sleep
#define DEEPSLEEP_TIMEOUT (10 * 60 * 1000000)

boolean error = false;
boolean serialInit = false;

Adafruit_BME280 bme280;
OneWire oneWire(D1);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

#include "settings.h"

float temperature = 0;
float pressure = 0;
float altitude = 0;
float humidity = 0;

void setupSerial()
{
        if (!error && !serialInit)
        {
                serialInit = true;
                Serial.begin(9600);
        }
}

void setupBME280()
{
        if (!error)
        {
                Wire.begin(D1, D2);
                if (!bme280.begin(0x76))
                {
                        setupSerial();
                        Serial.println(F("A BME280 szenzor nem található."));
                        error = true;
                }
        }
}

void setupWifi()
{
        if (!error)
        {
                WiFi.forceSleepWake();
                delay(1);

                WiFi.persistent(false);
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid, password);

                uint64 time = millis();
                while (WiFi.status() != WL_CONNECTED || time + WIFI_TIMEOUT < millis())
                {
                        delay(100);
                }
                if (WiFi.status() != WL_CONNECTED)
                {
                        setupSerial();
                        Serial.print(F("WiFi kapcsolat létrehozása nem sikerült."));
                        error = true;
                }
        }
}

void setupMqtt()
{
        if (!error)
        {
                mqttClient.setServer(mqtt_server, mqtt_port);
                if (!mqttClient.connected())
                {
                        if (!mqttClient.connect(mqtt_client, mqtt_user, mqtt_password))
                        {
                                setupSerial();
                                Serial.print(F("A csatlakozás az MQTT szerverhez nem sikerült: "));
                                Serial.println(mqttClient.state());
                                error = true;
                        }
                }
        }
}

void publish(char *payload, int length)
{
        if (mqttClient.connected())
        {
                if (!mqttClient.publish(mqtt_topic, payload))
                {
                        setupSerial();
                        Serial.println(F("MQTT: az adatok küldése nem sikerült."));
                }
        }
        else
        {
                setupSerial();
                Serial.println(F("Az MQTT kliens nincs csatlakozva, az adatok küldése ezért nem sikerült."));
        }
}

void readSensorValues()
{
        temperature = bme280.readTemperature();
        pressure = bme280.readPressure() / 100.0F;
        altitude = bme280.readAltitude(SEALEVELPRESSURE_HPA);
        humidity = bme280.readHumidity();
}

void sendValues()
{
        StaticJsonDocument<1000> doc;
        JsonObject root = doc.to<JsonObject>();
        root["temperature"] = temperature;
        root["pressure"] = pressure;
        root["altitude"] = altitude;
        root["humidity"] = humidity;

        char json[1000];
        int length = measureJson(doc) + 1;
        serializeJson(doc, json, length);

        publish(json, length);
}

void deepSleep()
{
        mqttClient.disconnect();
        WiFi.forceSleepBegin();
        delay(1);
        WiFi.disconnect(true);
        delay(1);
        ESP.deepSleep(DEEPSLEEP_TIMEOUT, WAKE_RF_DISABLED);
}

void setup()
{
        WiFi.mode(WIFI_OFF);
        WiFi.forceSleepBegin();
        delay(1);

        setupBME280();

        if (!error)
        {
                setupWifi();
                setupMqtt();
                readSensorValues();
                if (!error)
                {
                        sendValues();
                }
        }

        deepSleep();
}

void loop()
{
        delay(1000);
}
