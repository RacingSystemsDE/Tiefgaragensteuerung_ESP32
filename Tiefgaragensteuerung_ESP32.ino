/*Tiefgaragensteuerung
 * Copyright by Stephan Krause (RacingSystems)
 */

//Bibliotheken
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>


//Allgemeine Info
const char* Version = "0.0.1 Alpha";                   					//Versionsnummer
const char* Datum = "31.10.2020";                   					//Datum
const char* Wasserzeichen = "Powered by Stephan Krause";         		//Wasserzeichen

//Konfiguration
int Login_Timeout_Minuten = 5;											//Minuten bis die Weboberfläche wieder gesperrt wird

//Webserver Info
const char* Title = "Tiefgaragensteuerung";											//Title und Überschrift des Websevers

//Weboberfläche Login
const char* Login_Weboberflaeche_Passwort = "test";						//Passwort für Weboberflaeche
bool Login_Weboberflaeche_Erfolgreich = false;							//Wenn das Passwort korrekt eingegeben wurde, wird die Loginmaske nicht angezeigt
const char* Login_Weboberflaeche_TempPasswort;							//Variable für Passwortfeld für abgleich bei anmeldung
//Timer für automatisches Ausloggen
int Login_Timeout = Login_Timeout_Minuten * 60 * 1000;					//Nach dieser Zeit wird die Weboberfläche wieder gesperrt.
int Login_Timeout_Helper_Aktuell;
int Login_Timeout_Helper_Rest;

//Webserver
// SSID & Password
const char* ssid = "Prototyp";											//WLAN SSID
const char* password = "01234567";										//WLAN Passwort
const int channel = 6;													//WLAN Kanal
const bool hidden = false;												//SSID verstecken
 
// IP Address details
IPAddress local_ip(192, 168, 1, 1);										//Lokale IP
IPAddress gateway(192, 168, 1, 1);										//Gateway
IPAddress subnet(255, 255, 255, 0);										//Subnetmask
 
WebServer server(80);													//WebServer HTTP Port (80 ist Standard)

int Webserver_Seite;													//Variable für Inhalt der Weboberfläche
int Webserver_Anzeige_Seiten = 0;										//Einzelene Seiten des Webserver: Startseite = 0, Einstellungen = 98, Informationen = 99


//Updatefunktion
const char* FirmwareUpdate = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

void setup() {
	
Serial.begin(115200);
	
//WEBSERVER (START)
	WiFi.softAP(ssid, password, channel, hidden);
	delay(1000);
	WiFi.softAPConfig(local_ip, gateway, subnet);
	delay(500);
	
	//Seite: Startseite
	server.on("/", HTTP_GET, []() {
	int Webserver_Anzeige_Seiten = 0;
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", SendHTML(Webserver_Seite));
	});																	//Geräte Verbindung zum Webserver per HTML
	
		server.on("/FirmwareUpdate", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			if(Login_Weboberflaeche_Erfolgreich == true){
				server.send(200, "text/html", FirmwareUpdate);
			}else{
				server.send(200, "text/html", SendHTML(Webserver_Seite)); 
			}
		});
		/*handling uploading firmware file */
		server.on("/update", HTTP_POST, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
			ESP.restart();
		}, []() {
			HTTPUpload& upload = server.upload();
			if (upload.status == UPLOAD_FILE_START) {
				Serial.printf("Update: %s\n", upload.filename.c_str());
				if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
					Update.printError(Serial);
				}
			} else if (upload.status == UPLOAD_FILE_WRITE) {
				/* flashing firmware to ESP*/
				if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
					Update.printError(Serial);
				}
			} else if (upload.status == UPLOAD_FILE_END) {
				if (Update.end(true)) { //true to set the size to the current progress
				Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
				} else {
					Update.printError(Serial);
				}
			}
		});
	
	
	server.on("/anmelden", handle_anmelden);													//An Weboberflaeche anmelden
	server.on("/abmelden", handle_abmelden);													//Von Weboberflaeche abmelden

	
	server.onNotFound(handle_NotFound);
	
	Serial.print("Access Point gestartet: ");
	Serial.println(ssid);
	server.begin();
	Serial.println("HTTP Server gestartet");
	delay(100);
}

void loop() {
	server.handleClient();																		//Webserver
	
//Timer für automatisches Abmelden

	if(Login_Weboberflaeche_Erfolgreich == true){
		Login_Timeout_Helper_Rest = millis() - Login_Timeout_Helper_Aktuell;
		if(Login_Timeout_Helper_Rest > Login_Timeout){
			Login_Weboberflaeche_Erfolgreich = false;
			//Serial.println("Timeout Abmeldung erfolgreich");
		}
	}
	
}




//Webserver Handler (dienen als Schnittstelle zwischen Webserver und Hardware)

void handle_anmelden() {
	if (server.hasArg("SUBMIT")){
		String Login_Weboberflaeche_AusgabePasswort = server.arg("pass");												
		Login_Weboberflaeche_TempPasswort = Login_Weboberflaeche_AusgabePasswort.c_str();																	//Mit ".c_str()" kann man String in "Const Char*" wandeln
		if(strcmp(Login_Weboberflaeche_TempPasswort,Login_Weboberflaeche_Passwort) == 0){																	//String Vergleich mit "Const Char* || 0 die Strings sind gleich || >0 das erste ungleiche Zeichen in str1 ist größer als in str2 || <0 das erste ungleiche Zeichen in str1 ist kleiner als in str2
			Login_Weboberflaeche_Erfolgreich = true;
			Login_Timeout_Helper_Aktuell = millis();
			//Serial.println("Anmeldung erfolgreich");
		}else{
			//Serial.println("Anmeldung nicht erfolgreich");
		}
	}
	server.send(200, "text/html", SendHTML(Webserver_Seite)); 
}

void handle_abmelden() {
    Login_Weboberflaeche_Erfolgreich = false;
	server.send(200, "text/html", SendHTML(Webserver_Seite)); 
}

void handle_NotFound(){
	server.send(404, "text/plain", "Seite nicht vorhanden");
}

//Webserver HTML

String SendHTML(uint8_t Webserver_Seite){
	String ptr = "<!DOCTYPE html> <html>";
	ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">";
	ptr +="<title>";ptr +=Title;ptr +="</title>";
	ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
	ptr +="body {margin-top: 20px;background-color: #000000;} h1 {color: #ffffff;margin: 1px auto 20px;} h2 {color: #ffffff;margin: 1px auto 10px;} h3 {color: #ffffff;margin: 1px 1px 5px;}";
	ptr +=".buttongross {display: inline-block;width: 110px;background-color: #ffffff;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;color: #000000;margin: 0px auto 8px;cursor: pointer;border-radius: 4px;}";
	ptr +=".buttongross {background-color: #ffffff;}";
	ptr +=".buttongross:active {background-color: #b69b00;}";
	ptr +=".buttonklein {display: inline-block;width: 8px;background-color: #ffffff;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;color: #000000;margin: 0px auto 8px;cursor: pointer;border-radius: 4px;}";
	ptr +=".buttonklein {background-color: #ffffff;}";
	ptr +=".buttonklein:active {background-color: #b69b00;}";
	ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}";
	ptr +="</style>";
	ptr +="</head>";
	ptr +="<body>";

	
	if(Login_Weboberflaeche_Erfolgreich == true){																		//Wird angezeigt wenn das Passwort richtig eingegeben wurde
	{ptr +="<p>\n";}
	{ptr +="<h1>";ptr +=Title;ptr +="</h1><p>\n";}
	{ptr +="<a class=\"button buttongross\" style=\"font-size: 18px;\" href=\"/abmelden\">Abmelden</a><p>\n";}
	//if(Seite = 1) hier kommen die einzelnen Seite mit Ihren Funktionen hin
	}else{																												//Wird angezeigt wenn das Passwort falsch eingegeben wurden
	{ptr +="<p>\n";}
	{ptr +="<h1>";ptr +=Title;ptr +="</h1><p>\n";}
	
	//Passwort Eingabe und Übermittlung an Handler
	{ptr +="<form action= '/anmelden'  method='POST'>";}
	{ptr +="<input type='password' placeholder='Passwort' name='pass' value=''>";}
	{ptr +="<br>br>";}
	{ptr +="<input type='submit' name='SUBMIT' class='button buttongross' value='Anmelden'>";}
	{ptr +="</form><p>\n";}
	
	{ptr +="Version: ";}{ptr +=Version;}{ptr +="<p>\n";}
	{ptr +="Datum: ";}{ptr +=Datum;}{ptr +="<p>\n";}
	{ptr +=Wasserzeichen;}{ptr +="<p>\n";}
	}
	ptr +="</body>";
	ptr +="</html>";
	return ptr;
}
