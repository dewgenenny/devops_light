// Required by wifimanager

#include <FS.h>      
#include <String.h>
// Libraries to control WS2812B lights

#include <NeoPixelBus.h>          // https://github.com/Makuna/NeoPixelBus
#include <NeoPixelAnimator.h>     // https://github.com/Makuna/NeoPixelBus

// Wifi Manager

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <WiFiClientSecure.h>     // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClientSecure.h
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>         // https://github.com/knolleary/pubsubclient

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define DEBUG                     // Define DEBUG to enable serial output

const int number_retries = 5;   // Number of connection retries before declaring host unavailable. Careful here, too high a number and you can mask real problems....

// required for OTA updates

const char* otaHost = "OTA-DevOps-Light";


// Define variables for MQTT connection 

char host[32] ;
char url[512] ;

char mqttServer[40];
char mqttPort[6] = "1883";
char devopsLightLocation[40];
bool urlUpdated = false;

String clientName;
String errorTopic;
String clientHello;
String urlTopic;
String statusTopic;

long lastTime = millis();

int refreshRate = 5000; 
int brightness = 100;

//int result = 0;         // variable that keeps the last results from the availability test

/* Define variables for host & url. URL is defined as a relatively large character array as it needs to be able to hold a long
json with all the URLs in. TODO - make it variable length and deal with the complexity
*/


const int httpsPort = 443;  // Server HTTPS port - if your server is listening on a different port, change it here



// Initiate WifiClient, required for MQTT client

WiFiClient wifiClient;

// Initiate MQTT Client

PubSubClient client;


//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  #ifdef DEBUG
     Serial.println("Should save config");
  #endif
  shouldSaveConfig = true;
}

// Callback to deal with subscribed MQTT message (URL) arriving 

void callback(char* topic, byte* payload, unsigned int length) {
    
    char buffer[512];
    
    // delete existing contents of url
    
    memset(url, 0, 512);
    
    // set buffer contents to nothing
    
    memset (buffer,0,512);
    
    // copy contents of incoming payload to url & buffer
    
    for (int i=0;i<length;i++) {
      
      buffer[i] = payload[i];
      url[i] = payload[i];
      
    }
    
    String received = buffer;
    
    // Print out for debugging purposes
    #ifdef DEBUG
    Serial.println(received);
    #endif

    urlUpdated = true;
}




const uint16_t PixelCount = 12; // make sure to set this to the number of pixels in your strip
const uint16_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266


NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma

//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
// For Esp8266, the Pin is omitted and it uses GPIO3 due to DMA hardware use.  
// There are other Esp8266 alternative methods that provide more pin options, but also have
// other side effects.
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount);

uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;



void write_to_mqtt (String topic, String message)
{
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  char buf[16];
  int freemem = ESP.getFreeHeap();
  
//  itoa (freemem, buf, 10);

  snprintf(buf, 15, "%d", freemem);
  
  root["content"] = message;
  root["freemem"] = buf;
  
String payload;

root.printTo(payload);


if(!client.loop()) client.connect("reconnecting");


  if (client.connected()){

    
    if (client.publish((char*) topic.c_str(), (char*) payload.c_str())) {

    }
    else {

    }
  }

}



int validateHost(char* hostname, char* urltoCheck)
{

int counter = 0;

while(counter <= number_retries){


WiFiClientSecure client;

 if (!client.connect(hostname, httpsPort)) {
  
    #ifdef DEBUG
      Serial.println("connection failed");
    #endif
    
    write_to_mqtt(errorTopic,"connection failed");
    write_to_mqtt(statusTopic,"DOWN");
    return 0;
  }
  
  #ifdef DEBUG
    Serial.print("requesting HOST: ");
    Serial.println(hostname);
    Serial.print("requesting URL: ");
    Serial.println(urltoCheck);
  #endif
  
  client.print(String("GET ") + urltoCheck + " HTTP/1.1\r\n" +
               "Host: " + hostname + "\r\n" +
               "User-Agent: CloudStatusDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");
               
  #ifdef DEBUG
    Serial.println("request sent");
  #endif
    
  while (client.connected()) {
    String line = client.readStringUntil('\n');

    #ifdef DEBUG
      Serial.println(line);
      Serial.print("Line length: ");
      Serial.println(line.length());
    #endif   
    
         if (line.length() == 0) {
           #ifdef DEBUG 
            Serial.println("empty line... retrying");
           #endif  
         }
    if(line.indexOf("200") != -1)
    {
      write_to_mqtt(statusTopic,"UP");
      #ifdef DEBUG 
        Serial.println("Bingo!");
      #endif   
      return 1;
    }
    else
    {
      #ifdef DEBUG 
        Serial.println("Bongo :(");
      #endif   
      
      write_to_mqtt(errorTopic,"No 200 received...retrying");
      write_to_mqtt(errorTopic,line);
      counter++;
      delay(250);
      //return 0;
    }
    
    if (line == "\r") {
      #ifdef DEBUG
        Serial.println("headers received");
      #endif 
      break;
    }


  }

}
write_to_mqtt(errorTopic,"URL unavailable after retries");
return 0;

  /*
   * Old code that looked at actual content JSON
   * 
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"status\":\"UP\"")) {
    Serial.println("Cloud status request successful");
  } else {
    Serial.println("Cloud status request unsuccessful :(");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");

  
String payload = line;

client.stop();
                

 StaticJsonBuffer<250> jsonBuffer;

  
  
  
  JsonObject& root = jsonBuffer.parseObject(payload,10);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return 0;
  }
  else{
     Serial.println("Payload is: ");
    Serial.println(payload);
    Serial.println("Length is");
    Serial.println(sizeof(payload));

    String status_code = root["status"];
    String up = "UP";
    String down = "DOWN";

    if (up.equals(status_code))
    {
       return 1;
    }
    else
    {
       return 0;
    }

  */

}

void circleColour(RgbColor colour){
  
 for(int i=0;i<PixelCount;i++){
    strip.SetPixelColor(i, colour);
    strip.Show(); // This sends the updated pixel color to the hardware.
    delay(250); // Delay for a period of time (in milliseconds).
  }
  
}


void setStripColour(int red, int green, int blue){

  RgbColor colour(red, green, blue);
  colour.Darken((255 - brightness));
  
  for(int i=0;i<PixelCount;i++){
    strip.SetPixelColor(i, colour);
    strip.Show(); // This sends the updated pixel color to the hardware.
  }
  
}


void saveConfig(void){

    #ifdef DEBUG
      Serial.println("saving config");
    #endif   
    
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["url"] = url;
    json["host"] = host;
    json["brightness"] = brightness;
    json["refreshRate"] = refreshRate;
    json["mqttServer"] = mqttServer;
    json["devopsLightLocation"] = devopsLightLocation;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      #ifdef DEBUG
        Serial.println("failed to open config file for writing");
      #endif  
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    
}


void setup() {
  
#ifdef DEBUG

  // Start serial output
  Serial.begin(115200);

#endif 

// Initiate strip - without colours provided it will be black / off

strip.Begin();
strip.Show();

// Show blue to indicate waiting on setup

RgbColor blue(0, 0, 255);
circleColour(blue);

// Check if SPIFFS file system exists and read config.json if available

if (SPIFFS.begin()) {
  #ifdef DEBUG 
    Serial.println("mounted file system");
  #endif 
  if (SPIFFS.exists("/config.json")) {
    
    //file exists, reading and loading
    #ifdef DEBUG 
      Serial.println("reading config file");
    #endif 
    File configFile = SPIFFS.open("/config.json", "r");
    
    if (configFile) {
      
      #ifdef DEBUG
        Serial.println("opened config file");
      #endif 
      
      size_t size = configFile.size();
      
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);

      // Parse JSON config file
      
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      
      #ifdef DEBUG
        json.printTo(Serial);
      #endif 
      
      if (json.success()) {
        
        #ifdef DEBUG
          Serial.println("\nparsed json");
        #endif 
        // Copy contents of stored json parameters into variables.

        strcpy(url, json["url"]);
        strcpy(host, json["host"]);
        strcpy(mqttServer, json["mqttServer"]);
        brightness = json["brightness"];
        refreshRate = json["refreshRate"];

      } else {
        #ifdef DEBUG
          Serial.println("failed to load json config");
        #endif 
      }
    }
  }
} else {
  #ifdef DEBUG
     Serial.println("failed to mount FS");
  #endif 
}

// Define custom input parameters for WifiManager

WiFiManagerParameter custom_status_url("url", "url", url, 120);
WiFiManagerParameter custom_status_host("host", "host", host, 32);
WiFiManagerParameter custom_brightness("brightness", "brightness", "100", 32);
WiFiManagerParameter custom_refresh("refreshRate", "refreshRate", "5000", 32);
WiFiManagerParameter custom_mqtt_server("mqttServer", "mqttServer", mqttServer, 32);
WiFiManagerParameter custom_light_location("devopsLightLocation", "devopsLightLocation", devopsLightLocation, 40);

// Initiate Wifimanager

WiFiManager wifiManager;

// Add custom parameters

wifiManager.addParameter(&custom_status_url);
wifiManager.addParameter(&custom_status_host);
wifiManager.addParameter(&custom_brightness);
wifiManager.addParameter(&custom_refresh);
wifiManager.addParameter(&custom_mqtt_server);
wifiManager.addParameter(&custom_light_location);

/* Here we can choose startconfigportal or autoconnect - autoconnect will take previous values and try and connect, falling back to 
an AP if it cant connect. Startconfigportal will create an AP each and every time you start up
*/

//wifiManager.startConfigPortal("CloudStatusLightConfig");
wifiManager.autoConnect("CloudStatusLightConfig");

// Once we get this far in setup, we're connected to WiFi :)
#ifdef DEBUG
  Serial.println("connected...yeey :)");
#endif 
// Cycle through purple and then black to indicate we're connected
  
RgbColor purple(154, 0, 255);
circleColour(purple);
RgbColor blackout(0, 0, 0);
circleColour(blackout);


//read updated parameters input from the WifiManager

strcpy(url, custom_status_url.getValue());
strcpy(host, custom_status_host.getValue());
strcpy(mqttServer, custom_mqtt_server.getValue());
strcpy(devopsLightLocation, custom_light_location.getValue());

String brightness_string = custom_brightness.getValue();
brightness = brightness_string.toInt();
String refresh_string = custom_refresh.getValue();
refreshRate = refresh_string.toInt();

// Make sure brightness can't exceed 255

if(brightness > 255)
{
  brightness = 255;
}

#ifdef DEBUG

Serial.println("New Values:");
Serial.println(url);
Serial.println(host);
Serial.println(brightness);
Serial.println(refreshRate);
Serial.println(mqttServer);
Serial.println(devopsLightLocation);

#endif 


 if (shouldSaveConfig) {
  
    saveConfig();
    shouldSaveConfig = false;
    
  }

// Define Client name & make unique with timestamp

clientName += "devops_light-";
clientName += devopsLightLocation;
clientName += "-";
clientName += String(micros() & 0xff, 16);

// Define error logging topic to publish to

errorTopic += "/devops_light/";
errorTopic += clientName;
errorTopic += "/error";

// Define status logging topic to publish to

statusTopic += "/devops_light/";
statusTopic += clientName;
statusTopic += "/status";

urlTopic += "/devops_light/";
urlTopic += clientName;
urlTopic += "/url";

// Define the hello to publish from our client 


clientHello += clientName;
clientHello +=" joined ";
clientHello += errorTopic;





char* mqttServer_pointer = strdup(mqttServer);    // MQTT client requires a char* instead of a char array, probably I am doing something stupid & unnecessary here 

// Set required MQTT infos in client

client.setClient(wifiClient);
client.setServer(mqttServer_pointer,1883);
client.setCallback(callback);

// Connect to MQTT server 

if (client.connect((char*) clientName.c_str())) {
  
    #ifdef DEBUG
    Serial.println("Connected to MQTT broker");
    #endif
    
    const char* helloPointer = clientHello.c_str();

    // Publish hello note on joining MQTT server
    
    if (client.publish((char*) errorTopic.c_str(), helloPointer)) {
      
         #ifdef DEBUG 
         Serial.println("Publish ok");
         #endif
    }
    else {
         #ifdef DEBUG 
          Serial.println("Publish failed");
         #endif
    }

   // Subscribe to URL topic - used for OTA URL updates

   if(client.subscribe((char*) urlTopic.c_str()))
   {
    #ifdef DEBUG
      Serial.println("subscribe OK");
    #endif
   }
   else
   {
    #ifdef DEBUG
      Serial.println("subscribe failed");
    #endif
   }

}
else {
  #ifdef DEBUG
    Serial.println("Oops... dying :/ can't connect to MQTT");
    Serial.println(mqttServer_pointer);
    Serial.println(clientHello.c_str());
  #endif
  // If cant connect, abort & reboot the device // try again
  abort();
}

ArduinoOTA.setHostname(otaHost);
ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
                 // startup_sequence();
                   write_to_mqtt(errorTopic, "Updating Firmware");
                    //tft.println("Updating Firmware");
                    });

  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
                       //startup_sequence();
                         write_to_mqtt(errorTopic, "Firmware update complete, restarting");
                       ESP.restart();

                       //some code
                        });

   ArduinoOTA.onError([](ota_error_t error) { ESP.restart(); });

  
}

int checkURLs(void) {
          int result = 1;
          #ifdef DEBUG
          Serial.print("url contents: ");
          Serial.println(url);
        #endif
        
        char *throwaway = (char *)malloc(strlen(url)+1);
       //char* throwaway[512];

        strcpy(throwaway, url);
        
        #ifdef DEBUG
          Serial.print("throwaway = ");
          Serial.println(throwaway);
        #endif
        
        DynamicJsonBuffer jsonBuffer;
        
        JsonObject& object = jsonBuffer.parseObject(throwaway);
        
        String input = object["url"];
        
        object.prettyPrintTo(input);
        
        JsonArray& array = jsonBuffer.parseArray(input);
        
        for(JsonArray::iterator it=array.begin(); it!=array.end(); ++it) 
        {
            // read out each url
            const char* value = *it;
            #ifdef DEBUG
              Serial.println(value);
            #endif
            char* url_to_check = (char*)value;
            result = result * validateHost(host,url_to_check);
        }
    
        #ifdef DEBUG
          Serial.print("Result is: ");
          Serial.println(result);
        #endif
        
        return result;
}



void loop() {
  // put your main code here, to run repeatedly:
   int result = 1;
    strip.Show();

    ArduinoOTA.begin();
    ArduinoOTA.handle();
    
    if (client.connected()) {
      client.loop();
    } 

    if(urlUpdated)
    {
      write_to_mqtt(errorTopic, "URL Updated");
      shouldSaveConfig = true;
      urlUpdated = false;
    }

    if(shouldSaveConfig)
    {
      saveConfig();
      write_to_mqtt(errorTopic, "Config Saved");
      shouldSaveConfig = false;
    }

    if(millis() - lastTime > refreshRate)
    {
        result = checkURLs();
        
        if(result == 1)
        {
          red = 0;
          green = 255;
          blue = 0;
        }
        else
        {
          red = 255;
          green = 0;    
          blue = 0;
        }
    
    /*
        RgbColor black(0, 0, 0);
        setStripColour(black);
        delay(25);
       */ 
   //    std::auto_ptr<RgbColor> current(new RgbColor);
       
        setStripColour(red,green,blue);
        
        lastTime = millis();
       
    }
        
}
