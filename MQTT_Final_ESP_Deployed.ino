#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <PString.h>
#include <time.h>
#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include "esp_system.h"


uint8_t second, minute, hour, dayOfWeek, day, month, year, dayCheck = 0;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int TZ;
//const char* ntpServer = "129.6.15.28"; //https://tf.nist.gov/tf-cgi/servers.cgi
const char* ntpServer = "time.nist.gov";
long gmtOffset_sec = 0L;
int   daylightOffset_sec = 3600;
int DST;

byte wfConnectTry = 0;
int updateConfig = 0;

int spiffsWipe = 0;


String dateInfo = "";
String timeInfo = "";
String timeTemp = "";
String tempStr = "";
String readingString = "";
String ds, maxTempT, maxTemp, minTempT, minTemp, tempT, temp = "";

String currentDate = "";

unsigned long timeCheck = 0UL;
unsigned long readingTimer = 0UL;

boolean reading = false;
boolean update = false;
boolean synced = false;
byte timeUpdated = 0;
byte wifiReset = 0;
byte mqttReset = 0;

String tmInfo = "";

/* root CA can be downloaded in:
  https://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class%203-Public-Primary-Certification-Authority-G5.pem
*/
const char* ca_cert = \
                      "-----BEGIN CERTIFICATE-----\n" \
                      
                      "-----END CERTIFICATE-----\n";

// Fill with your certificate.pem.crt with LINE ENDING
/*const char* certificateBuff = \
                              "-----BEGIN CERTIFICATE-----\n" \
                             
                              "-----END CERTIFICATE-----\n";


  // Fill with your private.pem.key with LINE ENDING
  const char* privateKeyBuff = \
                             "-----BEGIN RSA PRIVATE KEY-----\n" \
                            
                             "-----END RSA PRIVATE KEY-----\n";*/
//-----topics
char SUB_TOPIC[50] = "NWS/CRH/NRC/COOP/#";
char SUB_ALL[50] = "NWS/#";
char COMMAND_TOPIC[50] = "NWS/CRH/NRC/COOP/NRC01/COMMAND";
char TEMP_PUB_TOPIC[50] = "NWS/CRH/NRC/COOP/NRC01/TEMP";
char PRECIP_PUB_TOPIC[50] = "NWS/CRH/NRC/COOP/NRC01/PRECIP";
char MESSAGE_TOPIC[50] = "NWS/CRH/NRC/COOP/NRC01/MSG";
char UPDATE_TOPIC[50] = "NWS/CRH/NRC/COOP/NRC01/UPDATE";

float temperature = 0.00;
boolean tempUpdate = false;
String fprString = "";
boolean fprUpdate = false;

long lastMsg = 0;
char msg[50];
int counter = 0;

int led_pin = 4;
unsigned long ledTimer = 0UL;

#ifdef ESP32
#include <SPIFFS.h>
#endif

char serverHostname[30] = "";

String RIR = "";
String regionEntry = "";
String officeEntry = "";
String siteEntry = "";
uint8_t baseMac[6];
String site_ID = "";
String userIDstring = "";//MQTT userid
String passIDstring = "";//MQTT password
String l_SSID = "";
String l_PASS = "";
String message = "";
String versionInfo = "0.2";

WiFiClientSecure net;
MQTTClient client;

unsigned long lastMillis = 0;

//---------------------------------------------------------------------------------
void setup() {
  pinMode(led_pin, OUTPUT);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 17, 16, false);
  // We start by connecting to a WiFi network
  delay(2000);
  Serial.println(F("CCOOP MQTT"));
  Serial.print(F("ver: "));
  Serial.println(versionInfo);
  while (Serial.available() > 0)
  {
    byte c = Serial.read();
  }
  delay(2000);
  unsigned long techTimer = millis();
  techInputRequest();
  if (updateConfig == 1)
  {
    techUpdate();
    deleteConfigFiles();
    updateConfigFiles();
    updateWiFiFiles();
    updateMQTTFiles();
    for (int x = 0; x < sizeof(serverHostname); x++)
    {
      serverHostname[x] = char('\0');
    }
    regionEntry = "";
    officeEntry = "";
    siteEntry = "";
    userIDstring = "";//MQTT userid
    passIDstring = "";//MQTT password
    l_SSID = "";
    l_PASS = "";
    setupSpiffs();
    /*String updateString = String(TZ) + "," + String(DST) + "," + regionEntry + ","  + officeEntry + "," + siteEntry + "," + l_SSID + "," + l_PASS + "," + server + "," + userIDstring + "," + passIDstring;
      char updateBuff[updateString.length() + 1];
      updateString.toCharArray(updateBuff, updateString.length() + 1);
      writeFile(SPIFFS, "/config.txt", updateBuff);
      Serial.println(F("Writing Config"));*/
  }
  else
  {
    setupSpiffs();
  }
  String server = "";
  int indexNull = 0;
  for (int i = 0; i < 30; i++)
  {
    if (serverHostname[i] == '\0')
    {
      Serial.println(i);
      indexNull = i;
      i = 30;
    }
  }
  for (int x = 0; x < indexNull; x++)
  {
    server = server + serverHostname[x];
  }
  Serial.println(F("Setup: "));
  Serial.print(F("1. "));
  Serial.println(TZ);
  Serial.print(F("2. "));
  Serial.println(DST);
  Serial.print(F("3. "));
  Serial.println(regionEntry);
  Serial.print(F("4. "));
  Serial.println(officeEntry);
  Serial.print(F("5. "));
  Serial.println(siteEntry);
  Serial.print(F("6. "));
  Serial.println(l_SSID);
  Serial.print(F("7. "));
  Serial.println(l_PASS);
  Serial.print(F("8. "));
  Serial.println(server);
  Serial.print(F("9. "));
  Serial.println(userIDstring);
  Serial.print(F("10. "));
  Serial.println(passIDstring);

  if (DST == 1)
  {
    daylightOffset_sec = 3600;
  }
  else
  {
    daylightOffset_sec = 0;
  }
  net.setCACert(ca_cert);
  //net.setCertificate(certificateBuff); // for client verification
  //net.setPrivateKey(privateKeyBuff);  // for client verification
  client.begin(serverHostname, 8883, net);
  client.onMessage(messageReceived);
  if (l_SSID != "" && l_PASS != "") {
    int connAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && connAttempts < 3) {
      wifi_connect();
      connAttempts++;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    l_SSID = WiFi.SSID();
    l_PASS = WiFi.psk();
  }
  net.setCACert(ca_cert);
  client.begin(serverHostname, 8883, net);
  client.onMessage(messageReceived);
  buildVars();
  delay(1000);
  //mqtt_connect();
  /* get the IP address of server by MDNS name */
  /* set SSL/TLS certificate */
  gmtOffset_sec = (-1) * 3600 * TZ;
  //Serial.println(gmtOffset_sec);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  timeCheck = millis();
  //Serial.print("ESP Mac Address: ");
  //Serial.println(WiFi.macAddress());
  printLocalTime();
  Serial.println(F("Running....."));
  pinMode(led_pin, OUTPUT);
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(led_pin, HIGH);
    delay(250);
    digitalWrite(led_pin, LOW);
    delay(250);
  }
  digitalWrite(led_pin, HIGH);
  ledTimer = millis();
  delay(1000);
}

void loop() {
  checkSerial2();
  /* if client was disconnected then try to reconnect again */
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected() || mqttReset == 1) {
      mqtt_connect();
    }
    /* this function will listen for incomming
      subscribed topic-process-invoke receivedCallback */
    client.loop();
    /* we increase counter every 3 secs
      we count until 3 secs reached to avoid blocking program if using delay()*/
    long now = millis();
    if (synced)
    {
      if (now - lastMsg > 300000) {
        printLocalTime();
        lastMsg = now;
      }
    }
    else
    {
      if (now - lastMsg > 10000) {
        printLocalTime();
        lastMsg = now;
      }
    }

    if (tempUpdate == true)
    {
      String comma = ",";
      String tee = "T";
      String reading = String(temperature, 1);
      String siteID = siteEntry;
      String pubString = siteID + comma + ds + comma + maxTempT + comma + maxTemp + comma + minTempT + comma + minTemp + comma + tempT + comma + temp;
      Serial.println(pubString);
      Serial2.println(pubString);
      char message_buff[pubString.length() + 1];
      pubString.toCharArray(message_buff, pubString.length() + 1);
      client.publish(TEMP_PUB_TOPIC, message_buff);
      ds, maxTempT, maxTemp, minTempT, minTemp, tempT, temp = "";
      tempUpdate = false;
    }

    if (fprUpdate == true)
    {
      String pubString2 = "";
      pubString2 = fprString;
      Serial.println(pubString2);
      Serial2.println(pubString2);
      char message_buff2[pubString2.length() + 1];
      pubString2.toCharArray(message_buff2, pubString2.length() + 1);
      client.publish(PRECIP_PUB_TOPIC, message_buff2);
      fprUpdate = false;
    }
  }
  else {
    wifi_reconnect();
  }
  if (wifiReset == 1)
  {
    message = "";
    message = "Manual Wifi/MQTT Reset";
    sendMessage(message);
    wifi_reconnect();
  }
  if (ledTimer > millis())
  {
    ledTimer = millis();
  }
  if ((millis() - ledTimer) > 2000UL)
  {
    changeLED();
  }
}
//----------------------------------------------------------------

void wifi_connect() {
  unsigned long timer1 = millis();
  WiFi.mode(WIFI_STA);
  Serial.print("Attempting to connect to SSID: ");
  char ssidBuff[l_SSID.length() + 1];
  l_SSID.toCharArray(ssidBuff, l_SSID.length() + 1);
  char passBuff[l_PASS.length() + 1];
  l_PASS.toCharArray(passBuff, l_PASS.length() + 1);
  Serial.println(l_SSID);
  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  WiFi.begin(ssidBuff, passBuff);
  while (WiFi.status() != WL_CONNECTED && millis() - timer1 < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F(" CONNECTED"));
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("ESP Mac Address: ");
    Serial.println(WiFi.macAddress());
    delay(2000);
    wifiReset = 0;
  }
  else {
    Serial.println(F("Could not connect! Retry in a few seconds"));
  }
}

void wifi_reconnect()
{
  wifiReset = 1;
  Serial.println(F("Disconnecting"));
  WiFi.disconnect(true);
  delay(500);
  Serial.println(F("Starting WiFi!"));
  wifi_connect();
  mqtt_connect();
}

void mqtt_connect() {
  mqttReset = 1;
  unsigned long timer1 = millis();
  char client_ID[siteEntry.length() + 1];
  siteEntry.toCharArray(client_ID, siteEntry.length() + 1);
  char userID[userIDstring.length() + 1];
  userIDstring.toCharArray(userID, userIDstring.length() + 1);
  char passID[passIDstring.length() + 1];
  passIDstring.toCharArray(passID, passIDstring.length() + 1);
  Serial.print("\nMQTT connecting.....");
  while (!client.connect(client_ID, userID, passID) && (millis() - timer1 < 20000UL)) {
    Serial.print(".");
    delay(1000);
  }
  if (client.connected()) {
    Serial.println("connected!");
    mqttReset = 0;
    client.subscribe(SUB_ALL);
    client.subscribe(COMMAND_TOPIC);
    client.subscribe(UPDATE_TOPIC);
    message = "";
    message = "Wifi and MQTT Connected, ver: " + versionInfo;
    sendMessage(message);
  }
  else
  {
    Serial.println(F("Could not connect to MQTT Broker!"));
  }
}

void sendMessage(String msg)
{
  String pubmsg = "";
  pubmsg = msg;
  Serial.println(msg);
  //Serial2.println(pubString2);
  char msg_buff[pubmsg.length() + 1];
  pubmsg.toCharArray(msg_buff, pubmsg.length() + 1);
  client.publish(MESSAGE_TOPIC, msg_buff);
}

void messageReceived(String &topic, String &payload) {
  Serial.println(topic);
  Serial.println(payload);
  if (topic == COMMAND_TOPIC)
  {
    Serial.println(F("Command Received!"));
  }
  if (topic == UPDATE_TOPIC)
  {
    Serial.println(F("Update Received!"));
  }
  if (topic == COMMAND_TOPIC && payload == "reset_ccoop1230!")
  {
    message = "";
    message = dateInfo + ", " + timeInfo + ": Reset initiated!";
    Serial.println(message);
    sendMessage(message);
    ESP.restart();
  }
  if (topic == COMMAND_TOPIC && payload == "test")
  {
    message = "";
    message = "Test Received!";
    Serial.println(message);
    sendMessage(message);
  }

  if (topic == UPDATE_TOPIC)
  {
    String temp = payload;
    String newVarString = "";
    int index = temp.indexOf(",");
    char varSelect = char(payload[0]);
    newVarString = temp.substring(index+1);
    Serial.println(varSelect);
    Serial.println(newVarString);
    switch (varSelect) {
      case '0':
        TZ = int(newVarString[0]) - 48;
        Serial.print(F("TZ Update Received: "));
        Serial.println(TZ);
        if (DST == 1)
        {
          daylightOffset_sec = 3600;
        }
        else
        {
          daylightOffset_sec = 0;
        }
        gmtOffset_sec = (-1) * 3600 * TZ;
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        printLocalTime();
        break;
      case '1':
        DST = int(newVarString[0]) - 48;
        Serial.print(F("DST Update Received: "));
        Serial.println(DST);
        if (DST == 1)
        {
          daylightOffset_sec = 3600;
        }
        else
        {
          daylightOffset_sec = 0;
        }
        gmtOffset_sec = (-1) * 3600 * TZ;
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        printLocalTime();
        break;
      case '2':
        for (int x = 0; x < sizeof(serverHostname); x++)
        {
          serverHostname[x] = char('\0');
        }
        for (int y = 0; y < newVarString.length(); y++)
        {
          serverHostname[y] = newVarString[y];
        }
        Serial.print(F("MQTT Server Update Received: "));
        for (int i = 0; i < newVarString.length(); i++)
        {
          Serial.println(serverHostname[i]);
        }
        break;
      case '3':
        regionEntry = newVarString;
        Serial.print(F("Region Update Received: "));
        Serial.println(regionEntry);
        break;
      case '4':
        officeEntry = newVarString;
        Serial.print(F("Office Update Received: "));
        Serial.println(officeEntry);
        break;
      case '5':
        siteEntry = newVarString;
        Serial.print(F("Site ID Update Received: "));
        Serial.println(siteEntry);
        break;
      case '6':
        l_SSID = newVarString;
        Serial.print(F("SSID Update Received: "));
        Serial.println(l_SSID);
        break;
      case '7':
        l_PASS = newVarString;
        Serial.print(F("WiFi Password Update Received: "));
        for (int d = 0; d < l_PASS.length(); d++)
        {
          Serial.print(F("x"));
        }
        Serial.println(F(""));
        break;
      case '8':
        userIDstring = newVarString; //MQTT
        Serial.print(F("MQTT UserID Update Received: "));
        Serial.println(userIDstring);
        break;
      case '9':
        passIDstring = newVarString; //MQTT
        Serial.print(F("MQTT UserPass Update Received: "));
        for (int e = 0; e < passIDstring.length(); e++)
        {
          Serial.print(F("x"));
        }
        Serial.println(F(""));
        break;
      case 'a':
        deleteConfigFiles();
        updateConfigFiles();
        updateWiFiFiles();
        updateMQTTFiles();
        for (int x = 0; x < sizeof(serverHostname); x++)
        {
          serverHostname[x] = char('\0');
        }
        regionEntry = "";
        officeEntry = "";
        siteEntry = "";
        userIDstring = "";//MQTT userid
        passIDstring = "";//MQTT password
        l_SSID = "";
        l_PASS = "";
        setupSpiffs();
        buildVars();
        wifi_reconnect();
        break;
      default:
        break;
    }
    message = "";
    message = String(TZ) + "," + String(DST) + "," + serverHostname + "," + regionEntry + ","  + officeEntry + "," + siteEntry;
    sendMessage(message);
  }
}

void buildVars() {
  /* Loop until reconnected */
  String tempTOPIC = "";
  for (int x = 0; x < sizeof(SUB_TOPIC); x++)
  {
    SUB_TOPIC[x] = char('\0');
  }
  tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/#";
  for (int x = 0; x < tempTOPIC.length(); x++)
  {
    SUB_TOPIC[x] = char(tempTOPIC[x]);
  }
  Serial.print(F("Subscribe Topic: "));
  Serial.println(SUB_TOPIC);
  tempTOPIC = "";
  for (int x = 0; x < sizeof(COMMAND_TOPIC); x++)
  {
    COMMAND_TOPIC[x] = char('\0');
  }
  tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/" + siteEntry + "/COMMAND";
  for (int x = 0; x < tempTOPIC.length(); x++)
  {
    COMMAND_TOPIC[x] = char(tempTOPIC[x]);
  }
  Serial.print(F("Command Topic: "));
  Serial.println(COMMAND_TOPIC);
  tempTOPIC = "";
  for (int x = 0; x < sizeof(TEMP_PUB_TOPIC); x++)
  {
    TEMP_PUB_TOPIC[x] = char('\0');
  }
  tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/" + siteEntry + "/TEMP";
  for (int x = 0; x < tempTOPIC.length(); x++)
  {
    TEMP_PUB_TOPIC[x] = char(tempTOPIC[x]);
  }
  Serial.print(F("Temp_Pub Topic: "));
  Serial.println(TEMP_PUB_TOPIC);
  tempTOPIC = "";
  for (int x = 0; x < sizeof(PRECIP_PUB_TOPIC); x++)
  {
    PRECIP_PUB_TOPIC[x] = char('\0');
  }
  //Serial.println(tempTOPIC);
  tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/" + siteEntry + "/PRECIP";
  for (int x = 0; x < tempTOPIC.length(); x++)
  {
    PRECIP_PUB_TOPIC[x] = char(tempTOPIC[x]);
  }
  Serial.print(F("Precip_Pub Topic: "));
  Serial.println(PRECIP_PUB_TOPIC);
  tempTOPIC = "";
  for (int x = 0; x < sizeof(MESSAGE_TOPIC); x++)
  {
    MESSAGE_TOPIC[x] = char('\0');
  }
  //Serial.println(tempTOPIC);
  tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/" + siteEntry + "/MSG";
  for (int x = 0; x < tempTOPIC.length(); x++)
  {
    MESSAGE_TOPIC[x] = char(tempTOPIC[x]);
  }
  Serial.print(F("Message_Topic: "));
  Serial.println(MESSAGE_TOPIC);
  tempTOPIC = "";
  for (int x = 0; x < sizeof(UPDATE_TOPIC); x++)
  {
    UPDATE_TOPIC[x] = char('\0');
  }
  //Serial.println(tempTOPIC);
  tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/" + siteEntry + "/UPDATE";
  for (int x = 0; x < tempTOPIC.length(); x++)
  {
    UPDATE_TOPIC[x] = char(tempTOPIC[x]);
  }
  /*Serial.print(F("Message Topic: "));
    Serial.println(MESSAGE_TOPIC);
    for (int x = 0; x < sizeof(SUB_TOPIC); x++)
    {
    Serial.print(byte(SUB_TOPIC[x]));
    Serial.print(F(","));
    }
    Serial.println();
    for (int x = 0; x < sizeof(COMMAND_TOPIC); x++)
    {
    Serial.print(byte(COMMAND_TOPIC[x]));
    Serial.print(F(","));
    }
    Serial.println();
    for (int x = 0; x < sizeof(TEMP_PUB_TOPIC); x++)
    {
    Serial.print(byte(TEMP_PUB_TOPIC[x]));
    Serial.print(F(","));
    }
    Serial.println();
    for (int x = 0; x < sizeof(PRECIP_PUB_TOPIC); x++)
    {
    Serial.print(byte(PRECIP_PUB_TOPIC[x]));
    Serial.print(F(","));
    }
    Serial.println();
    for (int x = 0; x < sizeof(MESSAGE_TOPIC); x++)
    {
    Serial.print(byte(MESSAGE_TOPIC[x]));
    Serial.print(F(","));
    }
    Serial.println();*/
}

void setupSpiffs() {
  //clean FS, for testing
  //SPIFFS.format();
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.txt")) {
      //file exists, reading and loading
      Serial.println(F("Reading Config"));
      readFileConfig(SPIFFS, "/config.txt");
      Serial.println(F(""));
    }
    else {
      Serial.println("No config file found!");
    }
    if (SPIFFS.exists("/wifi.txt")) {
      //file exists, reading and loading
      Serial.println(F("Reading wifi config"));
      readFileWiFi(SPIFFS, "/wifi.txt");
      Serial.println(F(""));
    }
    else {
      Serial.println("No wifi file found!");
    }
    if (SPIFFS.exists("/mqtt.txt")) {
      //file exists, reading and loading
      Serial.println(F("Reading MQTT Config"));
      readFileMQTT(SPIFFS, "/mqtt.txt");
      Serial.println(F(""));
    }
    else {
      Serial.println("No MQTT file found!");
    }
  }
  else {
    Serial.println(F("failed to mount FS"));
    
  }
  //end read
}

void deleteConfigFiles()
{
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.txt")) {
      //file exists, reading and loading
      Serial.println(F("deleting Config"));
      deleteFile(SPIFFS, "/config.txt");
      Serial.println(F(""));
    }
    else {
      Serial.println("No config file found!");
    }
    if (SPIFFS.exists("/wifi.txt")) {
      //file exists, reading and loading
      Serial.println(F("deleting wifi config"));
      deleteFile(SPIFFS, "/wifi.txt");
      Serial.println(F(""));
    }
    else {
      Serial.println("No wifi config file found!");
    }
    if (SPIFFS.exists("/mqtt.txt")) {
      //file exists, reading and loading
      Serial.println(F("deleting MQTT Config"));
      deleteFile(SPIFFS, "/mqtt.txt");
      Serial.println(F(""));
    }
    else {
      Serial.println("No mqtt config file found!");
    }
  }
}

void updateConfigFiles()
{
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    String updateString = String(TZ) + "," + String(DST) + "," + regionEntry + ","  + officeEntry + "," + siteEntry;
    char updateBuff[updateString.length() + 1];
    updateString.toCharArray(updateBuff, updateString.length() + 1);
    Serial.println(updateString);
    for (int z = 0; z < sizeof(updateBuff); z++)
    {
      Serial.print(updateBuff[z]);
    }
    Serial.println(F(""));
    writeFile(SPIFFS, "/config.txt", updateBuff);
    Serial.println(F("Writing Config"));
  }
  else {
    Serial.println("failed to mount FS");
  }
  //end read
}
void updateWiFiFiles()
{
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    String updateString2 = "";
    updateString2 = l_SSID + "," + l_PASS;
    char updateBuff2[updateString2.length() + 1];
    updateString2.toCharArray(updateBuff2, updateString2.length() + 1);
    Serial.println(updateString2);
    for (int y = 0; y < sizeof(updateBuff2); y++)
    {
      Serial.print(updateBuff2[y]);
    }
    Serial.println(F(""));
    writeFile(SPIFFS, "/wifi.txt", updateBuff2);
    Serial.println(F("Writing wifi config"));
  }
  else {
    Serial.println("failed to mount FS");
  }
  //end read
}
void updateMQTTFiles()
{
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    String server = "";
    int indexNull = 0;
    for (int i = 0; i < 30; i++)
    {
      if (serverHostname[i] == '\0')
      {
        Serial.println(i);
        indexNull = i;
        i = 30;
      }
    }
    for (int x = 0; x < indexNull; x++)
    {
      server = server + serverHostname[x];
    }
    String updateString3 = "";
    updateString3 = updateString3 + server + "," + userIDstring + "," + passIDstring;
    Serial.println(updateString3);
    char updateBuff3[updateString3.length() + 1];
    updateString3.toCharArray(updateBuff3, updateString3.length() + 1);
    Serial.println(updateString3);
    for (int i = 0; i < sizeof(updateBuff3); i++)
    {
      Serial.print(updateBuff3[i]);
    }
    Serial.println(F(""));
    writeFile(SPIFFS, "/mqtt.txt", updateBuff3);
    Serial.println(F("Writing mqtt config"));
  }
  else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void writeFile(fs::FS & fs, const char * path, char * message) {
  Serial.printf("\nWriting file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

void readFile(fs::FS & fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    Serial.write(file.read());
  }
}
void readFileConfig(fs::FS & fs, const char * path) {
  Serial.printf("\nReading file: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }
  Serial.println("- read from file:");
  int sepNum = 0;
  while (file.available()) {
    char c = file.read();
    if (c == ',')
    {
      sepNum++;
      Serial.println(F(""));
    }
    else
    {
      Serial.print(c);
      switch (sepNum) {
        case 0:
          TZ = int(c) - 48;
          break;
        case 1:
          DST = int(c) - 48;
          break;
        case 2:
          regionEntry = regionEntry + c;
          break;
        case 3:
          officeEntry = officeEntry + c;
          break;
        case 4:
          siteEntry = siteEntry + c;
          break;
        default:
          break;
      }
    }
  }
  Serial.println();
}

void readFileWiFi(fs::FS & fs, const char * path) {
  Serial.printf("\nReading file: %s\r\n", path);
  int cnt = 0;
  for (int x = 0; x < sizeof(serverHostname); x++)
  {
    serverHostname[x] = char('\0');
  }
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  int sepNum = 0;
  while (file.available()) {
    char c = file.read();
    if (c == ',')
    {
      sepNum++;
      Serial.println(F(""));
    }
    else
    {
      Serial.print(c);
      switch (sepNum) {
        case 0:
          l_SSID = l_SSID + c;
          break;
        case 1:
          l_PASS = l_PASS + c;
          break;
        default:
          break;
      }
    }
  }
  Serial.println();
}
void readFileMQTT(fs::FS & fs, const char * path) {
  Serial.printf("\nReading file: %s\r\n", path);
  int cnt = 0;
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }
  Serial.println("- read from file:");
  int sepNum = 0;
  while (file.available()) {
    char c = file.read();
    if (c == ',')
    {
      sepNum++;
      Serial.println(F(""));
    }
    else
    {
      Serial.print(c);
      switch (sepNum) {
        case 0:
          serverHostname[cnt] = c;
          cnt++;
          break;
        case 1:
          userIDstring = userIDstring + c; //MQTT
          break;
        case 2:
          passIDstring = passIDstring + c; //MQTT
          break;
        default:
          break;
      }
    }
  }
  Serial.println();
}

void deleteFile(fs::FS & fs, const char * path)
{
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) {
    Serial.println("- file deleted");
  } else {
    Serial.println("- delete failed");
  }
}


void checkSerial2()
{
  //Serial.println(F("Checking"));
  //delay(1000);
  if (Serial2.available() > 0)
  {
    //Serial.println(F("Serial2 Info Received!"));
    while (Serial2.available() > 0)
    {
      char inChar = char(Serial2.read());
      if (inChar == '#');
      {
        doSerial2Command();
      }
    }
  }
}
void doSerial2Command()
{
  char commandChar = char(Serial2.read());
  switch (commandChar)
  {
    case 'T':
      processCurrentTemp();
      break;
    case 'F':
      processFPR();
    default:
      break;
  }

}

void processCurrentTemp()
{
  String msgArray;
  char *strings[8];
  char *ptr = NULL;
  //String temp = "";
  boolean done = false;
  int index = 0;
  while (Serial2.available() > 0 && done == false)
  {
    char inChar = char(Serial2.read());
    if (inChar == '\n' || inChar == '\r')
    {
      done = true;
    }
    else
    {
      msgArray += inChar;
      delay(5);
    }
  }
  char arrays[msgArray.length() + 1];
  msgArray.toCharArray(arrays, msgArray.length() + 1);
  ptr = strtok(arrays, ",");  // takes a list of delimiters
  while (ptr != NULL)
  {
    strings[index] = ptr;
    index++;
    ptr = strtok(NULL, ",");  // takes a list of delimiters
  }
  //Serial.println(index);
  // print the tokens
  for (int n = 0; n < index; n++)
  {
    Serial.print(strings[n]);
    Serial.print(F("  "));
  }
  Serial.println(F(""));
  if (index == 7)
  {
    ds = String(strings[0]);
    maxTempT = String(strings[1]);
    maxTemp = String(strings[2]);
    minTempT = String(strings[3]);
    minTemp = String(strings[4]);
    tempT = String(strings[5]);
    temp = String(strings[6]);
  }
  else
  {
    ds = String(strings[0]);
    maxTempT = "";
    maxTemp = "";
    minTempT = "";
    minTemp = "";
    tempT = String(strings[1]);
    temp = String(strings[2]);
  }
  temperature = temp.toFloat();
  Serial.print(F("Received Temp: "));
  Serial.println(temperature, 1);
  Serial2.println(F("ESP Received MMTS"));
  tempUpdate = true;
}

void processFPR()
{
  String temp = "";
  boolean done = false;
  while (Serial2.available() > 0 && done == false)
  {
    char inChar = char(Serial2.read());
    if (inChar == '\n' || inChar == '\r')
    {
      done = true;
    }
    else
    {
      temp += inChar;
      delay(2);
    }
  }
  fprString = "";
  fprString = temp;
  Serial.print(F("Received FPR: "));
  Serial.println(fprString);
  Serial2.println(F("ESP Received FPR"));
  fprUpdate = true;
}

void changeLED()
{
  digitalWrite(led_pin, !digitalRead(led_pin));
  ledTimer = millis();
}

void printLocalTime()
{
  char buff[9];
  char buff1[20];
  char buff2[9];
  PString dateString(buff, sizeof(buff));
  PString timeString(buff2, sizeof(buff2));
  PString tempTimeStr(buff1, sizeof(buff1));
  struct tm timeinfo;
  Serial.println(F("\nChecking connection to gather time!"));
  if (WiFi.status() == WL_CONNECTED)
  {
    while (!getLocalTime(&timeinfo))
    {
      Serial.println(F("Failed to obtain time"));
      delay(1000);
    }
    Serial.println(&timeinfo);

    dateString.print(&timeinfo, "%y/%m/%d");
    timeString.print(&timeinfo, "%H:%M:%S");
    //if (minInfo.toInt() == 5)
    //{
    //  Serial.println(F("5 minute interval"));
    //  synced = true;
    //}
    tempTimeStr.print("#T");
    tempTimeStr.print(&timeinfo, "%y,%m,%d,%H,%M,%S,");
    tempTimeStr.print(String(TZ));
    //Serial.print(buff);
    //Serial.print(F("\r\n"));
    dateInfo = String(buff);
    timeInfo = String(buff2);
    timeTemp = String(buff1);
    /*for (int x = 0; x < sizeof(buff); x++)
      {
      Serial.print(x);
      Serial.print(F("\t"));
      Serial.println(buff[x]);
      }
      for (int x = 0; x < sizeof(buff2); x++)
      {
      Serial.print(x);
      Serial.print(F("\t"));
      Serial.println(buff2[x]);
      }
      for (int x = 0; x < sizeof(buff1); x++)
      {
      Serial.print(x);
      Serial.print(F("\t"));
      Serial.println(buff1[x]);
      }*/


    year = 2000 + (String(buff[6]).toInt() * 10) + String(buff[7]).toInt();
    month = (String(buff[0]).toInt() * 10) + String(buff[1]).toInt();
    day = (String(buff[3]).toInt() * 10) + String(buff[4]).toInt();
    hour = (String(buff2[0]).toInt() * 10) + String(buff2[1]).toInt();
    minute = (String(buff2[3]).toInt() * 10) + String(buff2[4]).toInt();
    second = (String(buff2[6]).toInt() * 10) + String(buff2[7]).toInt();
    if (!synced && (minute % 5 == 0))
    {
      Serial.println(F("5 minute interval"));
      synced = true;
      message = "";
      message = "System Time Sync Complete!";
      sendMessage(message);
    }
    if (dayCheck != day)
    {
      dayCheck = day;
      wifi_reconnect();
      if (wifiReset == 0 && mqttReset == 0)
      {
        delay(100);
      }
      else {
        Serial.println(F(" 1"));
        ESP.restart();
      }
    }

    /*Serial.print(F("Year: "));
      Serial.println(year);
      Serial.print(F("Month: "));
      Serial.println(month);
      Serial.print(F("Day: "));
      Serial.println(day);
      Serial.print(F("Hour: "));
      Serial.println(hour);
      Serial.print(F("Minute: "));
      Serial.println(minute);
      Serial.print(F("Second: "));
      Serial.println(second);    */

    //Serial.println(F("Updating RTC with network time!"));
    //rtc.adjust(DateTime(year, month, day, hour, minute, second));
    //delay(250);
    //getTime();
    //Serial.println(F(""));

    currentDate = dateInfo;
    Serial2.println(timeTemp);
  }
  else
  {
    Serial.println(F("No Wifi coonection available to get network time update!!"));
  }
}

void techInputRequest()
{
  RIR = WaitForInput("Update Configuration?\n", 30);
  //Serial.println(RIR);
  if (RIR[0] == 'Y' || RIR[0] == 'y')
  {
    updateConfig = 1;
    //Serial.println(F("True!!"));
  }
  else
  {
    updateConfig = 0;
    //Serial.println(F("False!!"));
  }
}

void techUpdate()
{
  Serial.println(F("\nClearing Old Config......"));
  deleteConfigFiles();
  RIR = WaitForInput("\nTime Zone?\n1. Pacific\n2. Alaska\n3. Western\n4. Mountain\n5. Central\n6. Eastern\n\nInput Numeric Selection: ", 30);
  Serial.print(F("\nEntry: "));
  Serial.println(RIR.toInt());
  TZ = (RIR.toInt()) - 11;
  RIR = "";
  RIR = WaitForInput("\nDaylight Savings Time Observed?(Y/N): ", 30);
  Serial.print(F("\nEntry: "));
  if (RIR[0] == 'Y' || RIR[0] == 'y')
  {
    DST = 1;
    Serial.println(F("Yes"));
  }
  else
  {
    DST = 0;
    Serial.println(F("No"));
  }
  RIR = "";
  RIR = WaitForInput("\nRegion HQ?\n1. PRH\n2. ARH\n3. WRH\n4. SRH\n5. CRH\n6. ERH\n\nInput Numeric Selection: ", 30);
  Serial.print(F("\nEntry: "));
  int tempSel = RIR.toInt();
  Serial.print(tempSel);
  switch (tempSel)
  {
    case 1:
      regionEntry = "PRH";
      break;
    case 2:
      regionEntry = "ARH";
      break;
    case 3:
      regionEntry = "WRH";
      break;
    case 4:
      regionEntry = "SRH";
      break;
    case 5:
      regionEntry = "CRH";
      break;
    case 6:
      regionEntry = "ERH";
      break;
    default:
      break;
  }
  RIR = "";
  RIR = WaitForInput("\n\n3 Letter WFO/Office? (JAN, PAH, EAX, etc..: ", 30);
  Serial.print(F("\nEntry: "));
  officeEntry = RIR;
  Serial.println(officeEntry);
  RIR = "";
  RIR = WaitForInput("\nAlphanumeric Site ID? (GILC1, CAOI2, NRCC01, etc.: ", 30);
  Serial.print(F("\nEntry: "));
  siteEntry = RIR;
  Serial.println(siteEntry);
  RIR = "";
  RIR = WaitForInput("\nWiFi SSID: ", 30);
  Serial.print(F("\nEntry: "));
  l_SSID = RIR;
  Serial.println(l_SSID);
  RIR = "";
  RIR = WaitForInput("\nWiFi Password: ", 30);
  Serial.print(F("\nEntry: "));
  l_PASS = RIR;
  Serial.println(l_PASS);
  RIR = "";
  RIR = WaitForInput("\nMQTT Server Host Address? (ex: sftp.xxx.xxxx.xxx): ", 30);
  Serial.print(F("\nEntry: "));
  for (int x = 0; x < sizeof(serverHostname); x++)
  {
    serverHostname[x] = char('\0');
  }
  for (int y = 0; y < RIR.length(); y++)
  {
    serverHostname[y] = RIR[y];
  }
  Serial.println(serverHostname);
  RIR = "";
  RIR = WaitForInput("\nMQTT UserID: ", 30);
  Serial.print(F("\nEntry: "));
  userIDstring = RIR;
  Serial.println(userIDstring);
  RIR = "";
  RIR = WaitForInput("\nMQTT Password: ", 30);
  Serial.print(F("\nEntry: "));
  passIDstring = RIR;
  Serial.println(passIDstring);
  Serial.println(F("Var Assignment: "));
  Serial.print(F("1. "));
  Serial.println(TZ);
  Serial.print(F("2. "));
  Serial.println(DST);
  Serial.print(F("3. "));
  Serial.println(regionEntry);
  Serial.print(F("4. "));
  Serial.println(officeEntry);
  Serial.print(F("5. "));
  Serial.println(siteEntry);
  Serial.print(F("6. "));
  Serial.println(l_SSID);
  Serial.print(F("7. "));
  Serial.println(l_PASS);
  Serial.print(F("8. "));
  Serial.println(serverHostname);
  Serial.print(F("9. "));
  Serial.println(userIDstring);
  Serial.print(F("10. "));
  Serial.println(passIDstring);
}

String WaitForInput(String question, int num)
{
  int numLength = 0;
  while (Serial.available() > 0)
  {
    byte c = Serial.read();
  }
  Serial.print(question);
  unsigned long qTimer = millis();
  while (!Serial.available() && (millis() - qTimer < 30000UL))
  {
    ;;
  }
  if (millis() - qTimer >= 30000UL) {
    String blank = "0";
    Serial.println(F("No Answer!"));
    return blank;
  }
  else {
    //Serial.println(F("Waiting for input!"));
    String tempString = "";
    while (numLength < num)
    {
      delay(10);
      if (Serial.available() > 0)
      {
        char c = char(Serial.read());

        //Serial.println(c);
        if (c == '\n' || c == '\r')
        {
          numLength = num;
          while (Serial.available() > 0)
          {
            delay(10);
            byte c = Serial.read();
          }
        }
        else
        {
          numLength++;
          tempString = tempString + String(c);
        }
      }
    }
    return tempString;
  }
}
