#include "config.h"
#include "espFunctionality.h"

// Determines functionality of esp
// could instantiate in setup based on ESP_FUNCTION value
EspFunctionality* espFunction = new RelayEspFunction(5);

void setup() {
  espFunction->setUp();

  Serial.begin (115200);
  Serial.println();
  // Load values from memory
  EEPROM.begin(512);
  EEPROM.get(WIFI_CRED_ADDR, wifiCreds);

  attemptWifiConnection();
  // Set analogWrite range for ESP8266
#ifdef ESP8266
  analogWriteRange(255);
#endif

}

void loop() {
  if (connectedToWiFi) {
    io.run();
    webota.handle();
  } else {
    server.handleClient();
  }
}

void attemptWifiConnection() {
  Serial.println("Attempting to connect to AdafruitIO via: ");
  Serial.println(wifiCreds.WIFI_SSID);

  connectedToWiFi = timedConnectionAttempt();
  if (connectedToWiFi) {
    handleWifiConnected();
  } else {
    Serial.print("Connection as STA failed. Starting AP: ");
    Serial.println(ESP_AP_SSID);
    configureAndStartServer();
  }
}

void configureAndStartServer() {
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAP(ESP_AP_SSID, ESP_AP_PASS) ? "AP Started" : "AP Failed");
  Serial.println(WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/wifi_credentials", handleWifiCredentials);
  server.begin();
}

bool timedConnectionAttempt() {
  WiFi.mode(WIFI_STA);
  io.connect();
  home_iot->onMessage(handleMessage);
  unsigned long timeStartedConnectionAttempt = millis();
  // Wait for a connection
  while (io.status() < AIO_CONNECTED && millis() < timeStartedConnectionAttempt + CONN_ATTEMPT_DUR) {
    delay(10);
  }
  Serial.println(io.statusText());
  return (io.status() == AIO_CONNECTED);
}

void handleWifiConnected() {
  webota.init(UPDATE_PORT, UPDATE_LINK);
  String webotaStatusString =
    WiFi.localIP().toString() + ":"
    + UPDATE_PORT + UPDATE_LINK + " for "
    + (String)ESP_ID;
  Serial.println(webotaStatusString);
  home_iot->save(webotaStatusString);
}

const char** tokenizeStringOnDelimiter(char inputStr[], char* delim, int &numTokens) {
  const char* tokens[] = {};
  char* token = strtok(inputStr, delim);
  while (token != NULL) {
    tokens[numTokens] = token;
    token = strtok(NULL, delim);
    numTokens += 1;
  }
  return tokens;
}

void handleMessage(AdafruitIO_Data *data) {
  int numTokens = 0;
  const char** tokens = tokenizeStringOnDelimiter(data->toChar(), ":", numTokens);
  if (numTokens >= 2) {
    const char* targetEsp = tokens[0];
    const char* targetFunction = tokens[1];
    tokens = (numTokens > 2) ? tokens + 2 : NULL;
    if (!strcmp(targetEsp, ESP_ID)) {
      if (!strcmp(targetFunction, ESP_FUNCTION)) {
        espFunction->performTask(tokens);
      }
    }
  }
}

void handleRoot() {
  String payload = HOME_PAGE_HTML;
  server.send(200, "text/html", payload);
}

void handleWifiCredentials() {
  String payload = CONN_ATMPT_HTML;
  server.send(200, "text/html", payload);
  Serial.println(payload);
  saveAndUpdateWifiCreds(server.arg("ssid"), server.arg("password"));
  attemptWifiConnection();
}

//TODO Security should be considered when passing credentials arround...
void saveAndUpdateWifiCreds(String newWifiSsid, String newWifiPass) {
  // Update creds locally
  char credCopyBuff[WIFI_CHAR_ARRAY_SIZE];
  newWifiSsid.toCharArray(credCopyBuff, WIFI_CHAR_ARRAY_SIZE);
  strcpy(wifiCreds.WIFI_SSID, credCopyBuff);
  newWifiPass.toCharArray(credCopyBuff, WIFI_CHAR_ARRAY_SIZE);
  strcpy(wifiCreds.WIFI_PASS, credCopyBuff);
  // Save updated creds to memory
  EEPROM.put(WIFI_CRED_ADDR, wifiCreds);
  EEPROM.commit();
}