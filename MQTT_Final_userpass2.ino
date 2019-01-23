// This example uses an ESP32 Development Board
// to connect to shiftr.io.
//
// You can check on your device after a successful
// connection here: https://shiftr.io/try.
//
// by Joël Gähwiler
// https://github.com/256dpi/arduino-mqtt
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <PString.h>
#include <time.h>
#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include "esp_system.h"

const char ssid[] = "";
const char pass[] = "";

uint8_t baseMac[6];
char client_ID[6] = "";
char userID[] = "ccoopuser";
char passID[] = "1129#Nawwal";
char* local_SSID[20];
char* local_PASS[20];
byte activeReadFile = 0;
String l_SSID = "";
String l_PASS = "";
/* change it with your ssid-password */
//const char* ssid = "ROB-E Test";
//const char* password = "robetest";
/* this is the MDNS name of PC where you installed MQTT Server */
char* serverHostname = "sftp.crh.noaa.gov";
//char* siteID = "NRCC01";
uint8_t second, minute, hour, dayOfWeek, day, month, year = 0;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int TZ = 6;
//const char* ntpServer = "129.6.15.28"; //https://tf.nist.gov/tf-cgi/servers.cgi
const char* ntpServer = "time.nist.gov";
const long  gmtOffset_sec = (-1) * 3600 * TZ;
const int   daylightOffset_sec = 3600;

byte wfConnectTry = 0;


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

String tmInfo = "";

/* root CA can be downloaded in:
  https://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class%203-Public-Primary-Certification-Authority-G5.pem
*/
const char* ca_cert = \
                      "-----BEGIN CERTIFICATE-----\n" \
                      "MIIDrjCCApYCCQDy/p0O1fM/ATANBgkqhkiG9w0BAQsFADCBmDELMAkGA1UEBhMC\n" \
                      "VVMxETAPBgNVBAgMCE1pc3NvdXJpMRQwEgYDVQQHDAtLYW5zYXMgQ2l0eTEMMAoG\n" \
                      "A1UECgwDTldTMQwwCgYDVQQLDANDUkgxGjAYBgNVBAMMEXNmdHAuY3JoLm5vYWEu\n" \
                      "Z292MSgwJgYJKoZIhvcNAQkBFhlicmlhbi53YWxhd2VuZGVyQG5vYWEuZ292MB4X\n" \
                      "DTE4MDkxMTE3MTMyM1oXDTE5MDkwNjE3MTMyM1owgZgxCzAJBgNVBAYTAlVTMREw\n" \
                      "DwYDVQQIDAhNaXNzb3VyaTEUMBIGA1UEBwwLS2Fuc2FzIENpdHkxDDAKBgNVBAoM\n" \
                      "A05XUzEMMAoGA1UECwwDQ1JIMRowGAYDVQQDDBFzZnRwLmNyaC5ub2FhLmdvdjEo\n" \
                      "MCYGCSqGSIb3DQEJARYZYnJpYW4ud2FsYXdlbmRlckBub2FhLmdvdjCCASIwDQYJ\n" \
                      "KoZIhvcNAQEBBQADggEPADCCAQoCggEBANEPUd8xzVOmSz7Eem701lS7thwPw8Xn\n" \
                      "MvIf+LqR7KHXB6AaekNe6BnRcXdf6shy1lJA7EIiVM/mJuILMz4FedAN2THHnLz+\n" \
                      "S+jHXNU5fvHevdfCKgmGrKLT3MiA5Rv3pUkGFV3fRbu9Z5P5+JoNfQPs2VluV3tj\n" \
                      "GFYi2yAq6evM8K7/Cw2B/xtCV7YoV3JspPALvosCuLW8EqyGe5dL/DAyn1pL7JuC\n" \
                      "2l65HwozEyJebh73FPT5Am6IMbBZ2QEJGCqKWKTnXmlSIhh+FKNrA6/2vjbKsUun\n" \
                      "VnX1U8LI7U4bST+XcWmxDQ/USw+ajSejcICJFe7bZmqqYE4HakcJVHMCAwEAATAN\n" \
                      "BgkqhkiG9w0BAQsFAAOCAQEAPuDKuJP70g/4gRNF8awR3sUILi1q5GCGmHS2C/DG\n" \
                      "vx7PvwzKqo/ba5wTbBsCbF/7zFk/FYYADdAgYeVVQm2T2KQ5tCrQt/7GywV9iXTc\n" \
                      "pQpJCrIaNp6tWX2G1Z6IBtJ3hXvszHiTFNkGdoCIBX1iXvKHoLmsk8mvnOYtSXwm\n" \
                      "BWrucN+lFOsgppGpiZq5j+7aPqjUgONX7l1X+kt3QAoHDtX7clZBPgwg1JXaocq2\n" \
                      "2v9NGJKs88sagZ0fGJXcUfqyE3hTAduxyod/BkSp4cwftz5M2S2QIPG1a2Et+Bjs\n" \
                      "VQXYPgTjFo/DCKoWoM5abMzj6p4T5nji1k1vnFsVX0vNZQ==\n" \
                      "-----END CERTIFICATE-----\n";

// Fill with your certificate.pem.crt with LINE ENDING
const char* certificateBuff = \
                              "-----BEGIN CERTIFICATE-----\n" \
                              "144gW4ppxkKgxw4Wxg4Vx4a4nOX/Wz2zxt3ZSa/NWPg4W4qw1x0GpSnGS4W3gnzW\n" \
                              "pwgx1z0xSzW4WgNVWxs1nkWtqXpvW4WXZW4gg2Vqg124ZX1gTz1WWWW6W24gq29t\n" \
                              "4z2gqq4gTg1TZWW0gGx24WNgPVghp2hpW1g0W24gnz1VgzxzWw0xNzxx14gxN444\n" \
                              "NgNxWw00OTzq1zzq1zgtNT2x1W4xHgxxWgNVWx11z0WXgqW4W1ngn2VqgG21xWNh\n" \
                              "gGgwggz41x0GpSnGS4W3gnzWxngxx44WgwxwggzKxo4WxnptqW4g6o9t2atWzTq9\n" \
                              "np3gg4W2K22TqptgW41kn6vtqGNOG/xH0g91/2Np44NZ3zx0O4x4tgGzH4qH20OK\n" \
                              "G4V2TtOgs4W3/19pqxVxNWxgxtx44p111ggag1On/kgZZ44x34xvgWg0xw4zq2pz\n" \
                              "n4npWpWnwaW2xOsZKp/6g/xpW1H9hX1SW1gq4tp9zqx2Wx4V39w0zzqt7hpW0xpp\n" \
                              "2n/q0vos2o4Oqgn62g4Vs81n1WWtx04ggzt4gpxzgg4Wg/VnV2PnKpg17tW4gqWn\n" \
                              "q1qXaZxznnWKZqgx44g4GqWz6a442Ka4pgK4ZzH4zxXVvWnpxSq6gk1Xg4Z1px1n\n" \
                              "NWthxg1WxxG4qgWz1W8Gx1gg4wnq1WxxWN4Wkspt4h3zko1W8Ttp2k4t9tW/1W0G\n" \
                              "x1ggggnWWWS7knn6WngxW1OgtgpX8WgNWWPwxgx1WgNVHg1WxW8zx4xx1x4Gx1gg\n" \
                              "gwzW/wnzxw4HggxNWgknhk4G9w0WxnsWxxOpxnzxWZS440gt2xpW4PXz43nXZz13\n" \
                              "S3WxgWoqgW1OnVaN1n4qzVzW2x29p0z2ZPw4tgxhg/t2zGtaV24WWgggWX6n4gxx\n" \
                              "XZVnh2ZVng12tN4X2ht/1a1psv6k34g2gs71114pt0gpV3TkSkp/3wvW2Xwt/78H\n" \
                              "GxW4x4Tvt8pzx4GogKgGzW2PgKS3p10z4n44pN4sNpqtpppHxPxqp3zKW7Kwxpv4\n" \
                              "qgW2pgW74ooHt6xWPzZ87xxq7s2xx1nS91zO9Ww21z2gpzazNtsKtxgn3Tgx2Vx8\n" \
                              "WgsNTz/TTvwWGwg2t2vpn4Hqo4oXnWT4p4gntatzO06z9nZt12gxxgqgWWPHwn==\n" \
                              "-----END CERTIFICATE-----\n";


// Fill with your private.pem.key with LINE ENDING
const char* privateKeyBuff = \
                             "-----BEGIN RSA PRIVATE KEY-----\n" \
                             "144zow4WxxKpxnzxgWG44OnPzZWggp08vZwt6wqxZS4tg2xgx24tp4agaWW4Thvw\n" \
                             "W9zWTP94xS44Wgx1gggG4WxxnhqWWtgg4h4Wgk7Tn74hg/9Wxp12pTW8qGgWgpx4\n" \
                             "NTxw/2ggnvtzWWq4Wtax21G0g1ppXstgnkpxg2gn6sP1q24gGSnWaoP8nnth/qV9\n" \
                             "gnzWG47xWX11ZgpSWg/pN13sgz4XWtGnH4gP8t262NnpT1W4atoOWWPNx4Wnzpgp\n" \
                             "4WHgq7nn10VpWn/1gWSz64nWNzaW47s101tp2/1pp0zxS1WHpzpxaW1WXgvoqSqv\n" \
                             "4XHS4GXh48wW1W1p6WksggtgW142gxn4kgVgqn4gxnxWxo4WxzXSahnSPxaq8nWp\n" \
                             "qxHVSOTWWxw142g9pV8Wga92q1Oavp9hPgxWan0O1OWpHqV8n8n3gvo4akT7gpw4\n" \
                             "Wz16pW0xt6gqxnPHp4xt6gPK2T9X43v2WWhp2nHggn6VgW9Tz34Xg48671nsHggx\n" \
                             "Xng89gqgghnkpg47axoxgWzp4Hka1s7ZzSnqnHgW4axtWgzhnzNvnpnn10Wq2xVz\n" \
                             "vn27g3t167Sgtq41WW4vGngK4vTptpN4240Zxznx4V4GX910vWW9Oxg1n0xnng34\n" \
                             "4gxWxNxgzWn2x4W4Xgq62NPPGxxz4p64gWWwWTxkwnzWSpztOxKo9sKT8n4a44W4\n" \
                             "4g1WP4gpgqzx9gpX4xVn1NggWS7nV3P19WXxzpNPx02q26WpnaV4g8W9ZZTH/g2H\n" \
                             "3gqox1oWgk2gWWXnx3sg2tzKn4HX4ZSzKW7gzz2zWpt9WtGgWG23q64hg124xnvz\n" \
                             "PX984gWqW4ztg4z2h4x4S44g8x4gpq/ZtGtg42zvZ441Gn/K9z3WHp8pgqzxt4w2\n" \
                             "W8ognKgtnX1txtxWnhpzGTppqgX1SggvosgVz9O11ggP1KH4xHpZ3NgspzwWVxaO\n" \
                             "2z3NK1sWKG/18gg1z88gWHN98O2gp42ZxqVwOznnhtT42Zp824PhhW4n1kz1hgvP\n" \
                             "xs1gWz1gPnzTtX99s01nvVNn1WxK1oXxwv1Pn18pgqW4V1vx1hGzg2V77gtgk4Wa\n" \
                             "nzaxgTNp/gpKztxg44gnVGpnWKntKhW449gn14xwnS1HZwzNSzGg14xg2ghWPHGg\n" \
                             "WnWg4gW7gnWz/X4pg2pPWzH4kngNzVWgp4gWgVxnZv6H2XZ12VvZSvxxn64SqW4h\n" \
                             "g3twngt1Vp1Wxg14gw7VgwKWgzn111WxtGn1aWO68VK1GzG1n/o8xzn14g40Pxg1\n" \
                             "WnW1/pnq7Ot0gXgg8xGO7Wgt24gnn6kXznggNg2xxHngg74Z4W/7W444V0Wkh2nV\n" \
                             "zo4Tq4gVxKoWzxpZ2zpz2gTs62x24PT4xgnggVvnq8gzp204z27On1K1P4hgzwHW\n" \
                             "7tg/xoGWxNxzw/pnao4zZqZkg91Gt/qqw0kgK9nW423ggxz1zP2vxnXS6hn2vv2n\n" \
                             "zzpg444t2pW1thtKKg8W0gqn1w1gSWgnWZKz62Wh3tWWg4W1gwO46o9WgWxv44xg\n" \
                             "W4W44W4px7s2Kpgg6/nNPWs48qzzOxTxh34p1041PzOx2W2W994h\n" \
                             "-----END RSA PRIVATE KEY-----\n";
/* topics */
char SUB_TOPIC[] = "NWS/CRH/NRC/COOP/#";
char COMMAND_TOPIC[] = "NWS/CRH/NRC/COOP/NRCC01/COMMAND";
char TEMP_PUB_TOPIC[] = "NWS/CRH/NRC/COOP/NRCC01/TEMP";
char PRECIP_PUB_TOPIC[] = "NWS/CRH/NRC/COOP/NRCC01/PRECIP";

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

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[100] = "";
char region[4] = "";
char office[4] = "";
char site_ID[11] = "";
//char client_ID[10] = "";
char gmt_offset[6] = "";

String regionEntry = "";
String officeEntry = "";
String siteEntry = "";

//flag for saving data
bool shouldSaveConfig = false;
bool wmActive = false;


WiFiClientSecure net;
MQTTClient client;

unsigned long lastMillis = 0;

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
  }
  else {
    Serial.println(F("Could not connect! Retry in a few seconds"));
  }
}

void wifi_reconnect()
{
  Serial.println(F("Disconnecting"));
  WiFi.disconnect(true);
  delay(500);
  Serial.println(F("Starting WiFi!"));
  wifi_connect();
  mqtt_connect();
}

void mqtt_connect() {
  Serial.print("\nMQTT connecting.....");
  while (!client.connect(client_ID, userID, passID)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("connected!");

  client.subscribe(SUB_TOPIC);
  client.subscribe(COMMAND_TOPIC);
}

void messageReceived(String &topic, String &payload) {
  Serial.println(topic + " - " + payload);
}

void buildVars() {
  /* Loop until reconnected */
  int tries = 0;
  for (int i = 0; i < 6; i++)
  {
    client_ID[i] = site_ID[i];
  }
  String tempTOPIC = "NWS/" + regionEntry + "/" + officeEntry + "/COOP/#";  
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
}

void setupSpiffs() {
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
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(region, json["region"]);
          strcpy(office, json["office"]);
          strcpy(site_ID, json["site_ID"]);
          strcpy(gmt_offset, json["gmt_offset"]);

        }
        else {
          Serial.println("failed to load json config");
        }
      }
      if (SPIFFS.exists("/ss.txt")) {
        //file exists, reading and loading
        Serial.println("reading ss file");
        activeReadFile = 1;
        readFileAuth(SPIFFS, "/ss.txt");
      }
      else {
        Serial.println("No ss file found!");
      }
      if (SPIFFS.exists("/ps.txt")) {
        //file exists, reading and loading
        Serial.println("reading ps file");
        activeReadFile = 2;
        readFileAuth(SPIFFS, "/ps.txt");
      }
      else {
        Serial.println("No ps file found!");
      }
    }
  }

  else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void writeFile(fs::FS &fs, const char * path, char * message) {
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

void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("\nAppending to file: %s\r\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("- failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("- message appended");
  } else {
    Serial.println("- append failed");
  }
}

void readFile(fs::FS &fs, const char * path) {
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

void readFileAuth(fs::FS &fs, const char * path) {
  Serial.printf("\nReading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    char c = file.read();
    Serial.write(c);
    switch (activeReadFile) {
      case 1:
        l_SSID = l_SSID + c;
        break;
      case 2:
        l_PASS = l_PASS + c;
        break;
      default:
        break;
    }
  }
  Serial.println();
  activeReadFile = 0;
}

void deleteFile(fs::FS &fs, const char * path)
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
//---------------------------------------------------------------------------------
void setup() {
  pinMode(led_pin, OUTPUT);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 17, 16, false);
  // We start by connecting to a WiFi network
  Serial.println();
  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported by Arduino.
  // You need to set the IP address directly.
  //
  // MQTT brokers usually use port 8883 for secure connections.
  setupSpiffs();  
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
    ;;
  }
  else {
    // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    wmActive = true;

    //set config save notify callback
    wm.setSaveConfigCallback(saveConfigCallback);

    // setup custom parameters
    //
    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server (sftp.xxx.xxxx.gov)", mqtt_server, 100);
    WiFiManagerParameter custom_region("region", "Region HQ (CRH, SRH, etc.)", region, 3);
    WiFiManagerParameter custom_office("office", "Office (PAH, JAN, etc.)", office, 3);
    WiFiManagerParameter custom_site_ID("site", "Site ID (CAIO02, NRCC01, etc.)", site_ID, 10);
    WiFiManagerParameter custom_gmt_offset("gmt", "GMT Offset (-6, 2, etc.)", gmt_offset, 3);



    //add all your parameters here
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_region);
    wm.addParameter(&custom_office);
    wm.addParameter(&custom_site_ID);
    wm.addParameter(&custom_gmt_offset);

    //reset settings - wipe credentials for testing
    //wm.resetSettings();
    //wm.setConnectTimeout(10);
    //automatically connect using saved credentials if they exist
    //If connection fails it starts an access point with the specified name
    //here  "AutoConnectAP" if empty will auto generate basedcon chipid, if password is blank it will be anonymous
    //and goes into a blocking loop awaiting configuration
    if (!wm.autoConnect("C-COOP", "ccoop1230")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }

    // always start configportal for a little while
    // wm.setConfigPortalTimeout(60);
    // wm.startConfigPortal("AutoConnectAP","password");

    //if you get here you have connected to the WiFi
    Serial.println(F("WiFi connected... :)"));

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(region, custom_region.getValue());
    strcpy(office, custom_office.getValue());
    strcpy(site_ID, custom_site_ID.getValue());
    strcpy(gmt_offset, custom_gmt_offset.getValue());


    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["mqtt_server"] = mqtt_server;
      json["region"] = region;
      json["office"] = office;
      json["site_ID"] = site_ID;
      json["gmt_offset"] = gmt_offset;

      // json["ip"]          = WiFi.localIP().toString();
      // json["gateway"]     = WiFi.gatewayIP().toString();
      // json["subnet"]      = WiFi.subnetMask().toString();

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.prettyPrintTo(Serial);
      json.printTo(configFile);
      Serial.println(F("Placed variables into config file."));
      configFile.close();
      Serial.println(F("Config file closed!"));
      //end save
      shouldSaveConfig = false;
      //deleteFile(SPIFFS, "/config.json");

    }
    if (WiFi.status() == WL_CONNECTED) {
      l_SSID = WiFi.SSID();
      l_PASS = WiFi.psk();
      //Serial.println(l_PASS);
      char ssidBuff[l_SSID.length() + 1];
      l_SSID.toCharArray(ssidBuff, l_SSID.length() + 1);
      char passBuff[l_PASS.length() + 1];
      l_PASS.toCharArray(passBuff, l_PASS.length() + 1);
      writeFile(SPIFFS, "/ss.txt", ssidBuff);
      Serial.println(F("Writing ss"));
      writeFile(SPIFFS, "/ps.txt", passBuff);
      Serial.println(F("Writing ps"));
    }
    wm.disconnect();
  }
  Serial.println(F("Start"));
  regionEntry = "";
  for (int i = 0; i < 10; i++)
  {
    if (region[i] != NULL)
    {
      regionEntry += region[i];
    }
    else
    {
      i = 10;
    }
  }
  officeEntry = "";
  for (int i = 0; i < 10; i++)
  {
    if (office[i] != '\0')
    {
      officeEntry += office[i];
    }
    else
    {
      i = 10;
    }
  }
  siteEntry = "";
  for (int i = 0; i < 10; i++)
  {
    if (site_ID[i] != '\0')
    {
      siteEntry += site_ID[i];
    }
    else
    {
      i = 10;
    }
  }

  //save the custom parameters to FS
  if (wmActive == true) {
    wifi_reconnect();
    wmActive = false;
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  l_SSID = WiFi.SSID();
  l_PASS = WiFi.psk();
  //Serial.println(l_SSID);
  //Serial.println(l_PASS);
  if (WiFi.status() == WL_CONNECTED) {
    l_SSID = WiFi.SSID();
    l_PASS = WiFi.psk();
    char ssidBuff[l_SSID.length() + 1];
    l_SSID.toCharArray(ssidBuff, l_SSID.length() + 1);
    char passBuff[l_PASS.length() + 1];
    l_PASS.toCharArray(passBuff, l_PASS.length() + 1);
    writeFile(SPIFFS, "/ss.txt", ssidBuff);
    Serial.println(F("Writing ss"));
    writeFile(SPIFFS, "/ps.txt", passBuff);
    Serial.println(F("Writing ps"));
    //readFile(SPIFFS, "/ss.txt");
    //readFile(SPIFFS, "/ps.txt");
  }
  net.setCACert(ca_cert);
  client.begin(mqtt_server, 8883, net);
  client.onMessage(messageReceived);
  buildVars();
  delay(1000);
  mqtt_connect();
  int tempTZ = atoi(gmt_offset);
  if (tempTZ < 0)
  {
    TZ = tempTZ * (-1);
  }
  else
  {
    TZ = tempTZ;
  }
  /* get the IP address of server by MDNS name */
  /* set SSL/TLS certificate */
  
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
    if (!client.connected()) {
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
      String siteID = String(site_ID);
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
  if (ledTimer > millis())
  {
    ledTimer = millis();
  }
  if ((millis() - ledTimer) > 2000UL)
  {
    changeLED();
  }
}
