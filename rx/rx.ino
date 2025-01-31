#include <LittleFS.h>
#include <ArduinoJson.h>

//  Yeet for ESP32 later.
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ElegantOTA.h>

#define FS LittleFS

void i_am_error() {
	Serial.flush();
	ESP.deepSleep(0);
}

unsigned long relay_val = 0;
unsigned long relay_pin = 5;

boolean relay_cfg_valid(const JsonDocument &doc) {
	const char *reqKeys[] = {
		"relay_pin"
	};

	for (auto key : reqKeys) {
		if (!doc.containsKey(key)) {
			Serial.print(key);
			Serial.println(" not found in config");
			return(false);
		}
	}

	return(true);
}

void setup_relay(const char *cfgfile) {
	File file = FS.open(cfgfile,"r");

	StaticJsonDocument<2048> doc;

	DeserializationError error = deserializeJson(doc, file);
	file.close();

	if (error || !relay_cfg_valid(doc)) {
		Serial.print(cfgfile);
		Serial.println(F(" error"));
		return;
	}

	relay_pin = doc["relay_pin"];

	pinMode(relay_pin, OUTPUT);
	digitalWrite(relay_pin, relay_val);
}

AsyncWebServer webserver(80);

String html_processor(const String& var) {
	if (var == "relay_val") {
		return String(relay_val);
	}

        return String();
}

//  how can haz checksum and a buttload of error checking?
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
	if (!index) {
		request->_tempFile = FS.open("/www/_tempFile","w");
	}
	if (len) {
		request->_tempFile.write(data, len);
	}
	if (final) {
		request->_tempFile.close();
		//  move to dest file once successful
		if (filename == "config.json") {
			FS.rename("/www/_tempFile", "/www/config.json");
		}
		request->redirect("/");
	}
}

boolean wifi_cfg_valid(const JsonDocument &doc) {
	//  probalo want ESP-NOW peers too
	const char *reqKeys[] = {
		"ssid",
		"psk",
		"hostname",
	};

	for (auto key : reqKeys) {
		if (!doc.containsKey(key)) {
			Serial.print(key);
			Serial.println(" not found in config");
			return(false);
		}
	}

	return(true);
}

void setup_wifi(const char *cfgfile) {
	File file = FS.open(cfgfile,"r");

	StaticJsonDocument<2048> doc;

	DeserializationError error = deserializeJson(doc, file);
	file.close();

	if (error || !wifi_cfg_valid(doc)) {
		Serial.print(cfgfile);
		Serial.println(F(" error"));
		i_am_error();
	}

	WiFi.persistent(false);  //  starting to have flashbacks regarding hostnames, mdns, and light sleep.
	WiFi.hostname(String(doc["hostname"]));
	WiFi.mode(WIFI_STA);

	WiFi.begin(String(doc["ssid"]), String(doc["psk"]));
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
	}

	Serial.println(WiFi.localIP());

	webserver.serveStatic("/", FS, "/www/").setDefaultFile("index.html").setTemplateProcessor(html_processor);

	webserver.on("/action_page.php", HTTP_GET, [](AsyncWebServerRequest *request) {
		AsyncWebParameter *p = NULL;
		if (request->hasParam("cmd")) {
			p = request->getParam("cmd");

			if (p->value().compareTo("set_val") == 0) {
				if (request->hasParam("val")) {
					//  todo:  need error checking
					relay_val = strtoul(request->getParam("val")->value().c_str(), NULL, 10);  //  lolc++
					digitalWrite(relay_pin, relay_val);
				}
				request->redirect("/");
			}
		}
		request->redirect("/");
	});

	webserver.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
		request->send(200);
	}, handleUpload);

	webserver.begin();

	ElegantOTA.begin(&webserver);
}

void setup() {
	Serial.begin(115200);
	Serial.println("\n");
	delay(1000);

	if (!FS.begin()) {
		Serial.println(F("ur fs is borken mang"));
		i_am_error();
	}

	setup_wifi("/wifi.json");
	setup_relay("/www/config.json");
}

void loop() {
	ElegantOTA.loop();
	delay(100);
}
