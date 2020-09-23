#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiUdp.h>

#include "der2.h"
#include "izq2.h"

#include "AppleMIDI.h"
USING_NAMESPACE_APPLEMIDI

#include <AudioFileSourcePROGMEM.h>
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputMixer.h"

APPLEMIDI_CREATE_INSTANCE(WiFiUDP, MIDI, "ESP8266", DEFAULT_CONTROL_PORT);

// To run, set your ESP8266 build to 160MHz, and include a SPIFFS of 512KB or greater.

unsigned long t0 = millis();


AudioOutputI2SNoDAC *out;
AudioOutputMixerStub *stub[2];
AudioOutputMixer *mixer;
AudioGeneratorWAV *wav[2];
AudioFileSourcePROGMEM *file[2];
boolean Running[2];
long start[2];
long Play = 0;

//Configuramos el wifi en modo AP, puerto 80, dirección 192.168.1.2, contrasenya petisus, nombre BATERIA
ESP8266WebServer server(80);
IPAddress ip(192, 168, 1, 2);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
String contrasenya = "petisuis";

int eepromInicializada = 0; //eeprom pos 1. Está a 1 si la placa está inicializada



void setup()
{

  Serial.begin(115200);
  EEPROM.begin(512);
  delay(1000);
  leerEeprom(); //Lee las variables configurables de la Eeprom y si tienen valores inválidos, las inicializa



  audioLogger = &Serial;

  out = new AudioOutputI2SNoDAC();

  mixer = new AudioOutputMixer(32, out);
  stub[0] = mixer->NewInput();
  stub[1] = mixer->NewInput();
  delay(100);
  Beginplay(1, der2,der2_len, 1.0);
  Beginplay(0, izq2,izq2_len, 1.0);
  Play = 1;




  initWifi();
}

void loop()
{
  if (wav[0]->isRunning()) {
    if (!wav[0]->loop()) {
      wav[0]->stop(); stub[0]->stop();
      Running[0] = false; Serial.printf("stopping 0\n");
    }
  }

  if (wav[1]->isRunning()) {
    if (!wav[1]->loop()) {
      wav[1]->stop();
      stub[1]->stop();
      Running[1] = false;
      Serial.printf("stopping 1\n");
    }
  }


  if (((millis() - start[1]) > 500) && (!wav[1]->isRunning())) {
    if (!Running[1]) {
      Play = Play + 1;
      Serial.println(Play);
      Beginplay(1, der2, der2_len, 1.0);
    }
  }

  if (((millis() - start[0]) > 100) && (!wav[0]->isRunning())) {
    if (!Running[0]) {
      Play = Play + 1;
      Serial.println(Play);
      //Beginplay(0,"/piz11.wav", vol);
      Beginplay(0, izq2, izq2_len, 1.0);
    }
  }
}




//inicializa el wifi como AP
void initWifi()
{
  Serial.println("Iniciando wifi...");

  WiFi.mode(WIFI_AP);
  while (!WiFi.softAP("bateria", contrasenya))
  {
    Serial.println(".");
    delay(100);
  }
  WiFi.softAPConfig(ip, gateway, subnet);

  Serial.print("Iniciado AP ");
  Serial.println("bateria");
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());
  server.on("/", handleRoot);      //Which routine to handle at root location

  server.begin();                  //Start server

    Serial.println();
  Serial.print(F("IP address is "));
  Serial.println(WiFi.localIP());

  Serial.println(F("OK, now make sure you an rtpMIDI session that is Enabled"));
  Serial.print(F("Add device named Arduino with Host/Port "));
  Serial.print(WiFi.localIP());
  Serial.println(F(":5004"));
  Serial.println(F("Then press the Connect button"));
  Serial.println(F("Then open a MIDI listener (eg MIDI-OX) and monitor incoming notes"));

  MIDI.begin(1);

}



//muestra la web de configuración y recoge los parámetros de configuración cuando se envían
void handleRoot()
{
    String strRoot = "<!DOCTYPE HTML><html>\
  <body>\
  por hacer\
  </body>\
  </html>";
  /*if (server.hasArg("dia") && server.hasArg("hora")) {
    hora = getValue(server.arg("hora"), ':', 0).toInt(); // sino probar con %3A
    minuto = getValue(server.arg("hora"), ':', 1).toInt();
    dia = getValue(server.arg("dia"), '-', 2).toInt();
    mes = getValue(server.arg("dia"), '-', 1).toInt();
    anyo = getValue(server.arg("dia"), '-', 0).toInt();

    setTime(hora, minuto, 0, dia, mes, anyo); // alternative to above, yr is 2 or 4 digit yr
    digitalClockDisplay();
  }
  if (server.hasArg("MINUTOS_MUESTRA"))
  {
    MINUTOS_MUESTRA = server.arg("MINUTOS_MUESTRA").toInt();
    EEPROM.put(50, MINUTOS_MUESTRA);
    EEPROM.commit();
    Serial.println("Nuevo parametro MINUTOS_MUESTRA: " + String(MINUTOS_MUESTRA));
  }
  if (server.hasArg("NUM_MUESTRAS")) {
    NUM_MUESTRAS = server.arg("NUM_MUESTRAS").toInt();
    EEPROM.put(100, NUM_MUESTRAS);
    EEPROM.commit();
    Serial.println("Nuevo parametro NUM_MUESTRAS: " + String(NUM_MUESTRAS));
  }
  if (server.hasArg("HUMEDAD_MIN")) {
    HUMEDAD_MIN = server.arg("HUMEDAD_MIN").toInt();
    EEPROM.put(60, HUMEDAD_MIN);
    EEPROM.commit();
    Serial.println("Nuevo parametro HUMEDAD_MIN: " + String(HUMEDAD_MIN));
  }

  if (server.hasArg("SEGUNDOS_RIEGO")) {
    SEGUNDOS_RIEGO = server.arg("SEGUNDOS_RIEGO").toInt();
    EEPROM.put(70, SEGUNDOS_RIEGO);
    EEPROM.commit();
    Serial.println("Nuevo parametro SEGUNDOS_RIEGO: " + String(SEGUNDOS_RIEGO));
  }
  if (server.hasArg("DIAS_MAX")) {
    DIAS_MAX = server.arg("DIAS_MAX").toInt();
    EEPROM.put(80, DIAS_MAX);
    EEPROM.commit();
    Serial.println("Nuevo parametro DIAS_MAX: " + String(DIAS_MAX));
  }
  if (server.hasArg("HORAS_ENTRE_RIEGO_MIN")) {
    HORAS_ENTRE_RIEGO_MIN = server.arg("HORAS_ENTRE_RIEGO_MIN").toInt();
    EEPROM.put(90, HORAS_ENTRE_RIEGO_MIN);
    EEPROM.commit();
    Serial.println("Nuevo parametro HORAS_ENTRE_RIEGO_MIN: " + String(HORAS_ENTRE_RIEGO_MIN));
  }

  if (server.hasArg("accion") && server.arg("accion") == "regar")
    regar();

  if (server.hasArg("accion") && server.arg("accion") == "compruebaHumedad")
    compruebaHumedad();

  String ultimosRiegos = getUltimosRiegos();
  String ultimasHumedades = getUltimasHumedades();
  char str[15] = "";
  timeToString(str, sizeof(str));
  String strRoot = "<!DOCTYPE HTML><html>\
  <body>\
  <h1>Programa de riego</h1>\
  Cada MINUTOS_MUESTRA, miramos la humedad. Tomamos NUM_MUESTRAS y hacemos la media para mayor precision. Si es inferior a HUMEDAD_MIN, regamos durante SEGUNDOS_RIEGO. Si han pasado DIAS_MAX desde el ultimo riego, regamos igualmente.\
  Nos aseguramos que entre riego y riego pasan un minimo de HORAS_ENTRE_RIEGO_MIN.<br><br>\
  Tiempo desde el ultimo encendido: " + String(str) + "<br>";

  strRoot += "              <form>\
                   <label for = \"fname\">Fecha y hora:</label>\
    <input type=\"date\" id=\"dia\" name=\"dia\" value=\"" + String(year()) + "-" + transformaAdosDigitos(month()) + "-" + transformaAdosDigitos(day()) + "\">\
    <input type=\"time\" id=\"hora\" name=\"hora\" value=\"" + transformaAdosDigitos(hour()) + ":" + transformaAdosDigitos(minute()) + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form> \
  \
  <form>\
    <label for=\"fname\">MINUTOS_MUESTRA:</label>\
    <input type=\"number\" style=\"width: 3em;\" min=\"1\" max=\"59\" id=\"MINUTOS_MUESTRA\" name=\"MINUTOS_MUESTRA\" value=\"" +  MINUTOS_MUESTRA + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form> \
  <form>\
    <label for=\"fname\">NUM_MUESTRAS:</label>\
    <input type=\"number\" style=\"width: 3em;\" min=\"1\" max=\"100\" id=\"NUM_MUESTRAS\" name=\"NUM_MUESTRAS\" value=\"" + NUM_MUESTRAS + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form>\
  <form>\
    <label for=\"fname\">HUMEDAD_MIN:</label>\
    <input type=\"number\" style=\"width: 4em;\" min=\"1\" max=\"1000\" id=\"HUMEDAD_MIN\" name=\"HUMEDAD_MIN\" value=\"" + HUMEDAD_MIN + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form>\
  <form>\
    <label for=\"fname\">SEGUNDOS_RIEGO:</label>\
    <input type=\"number\" style=\"width: 3em;\" min=\"1\" max=\"200\" id=\"SEGUNDOS_RIEGO\" name=\"SEGUNDOS_RIEGO\" value=\"" + SEGUNDOS_RIEGO + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form>\
  <form>\
    <label for=\"fname\">DIAS_MAX:</label>\
    <input type=\"number\" style=\"width: 3em;\" min=\"1\" max=\"30\" id=\"DIAS_MAX\" name=\"DIAS_MAX\" value=\"" + DIAS_MAX + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form>\
  <form>\
    <label for=\"fname\">HORAS_ENTRE_RIEGO_MIN:</label>\
    <input type=\"number\" style=\"width: 3em;\" min=\"1\" max=\"300\" id=\"HORAS_ENTRE_RIEGO_MIN\" name=\"HORAS_ENTRE_RIEGO_MIN\" value=\"" + HORAS_ENTRE_RIEGO_MIN + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form>\
  Ultimas mediciones humedad: " + ultimasHumedades + "<br>\
  Ultimos riegos: " + ultimosRiegos + "<br>\
  <form>\
    <button name=\"accion\" type=\"submit\" value=\"regar\">Regar ahora</button><br>\
  </form> \
  <form>\
    <button name=\"accion\" type=\"submit\" value=\"compruebaHumedad\">Comprobar humedad</button><br>\
  </form> \
  </body>\
  </html>";
*/
  server.send(200, "text/html", strRoot);

}

//si es la primera vez que conectamos la placa, se inicializa la eeprom
void inicializarEeprom()
{
  Serial.println("Inicializando Eeprom, tardaremos 10 segundos...");

  for (int i = 0; i < 10; i++)
  {
    EEPROM.put(10+(i * 10), i);
    EEPROM.commit();

  }
}



//Lee las variables configurables de la Eeprom y si tienen valores inválidos, las inicializa
void leerEeprom()
{
 /* EEPROM.get(50, MINUTOS_MUESTRA);
  if (MINUTOS_MUESTRA < 1 || MINUTOS_MUESTRA > 59)
  {
    MINUTOS_MUESTRA = 5;
    EEPROM.put(50, MINUTOS_MUESTRA);
    EEPROM.commit();
  }
  EEPROM.get(60, HUMEDAD_MIN);
  if (HUMEDAD_MIN < 1 || HUMEDAD_MIN > 1000)
  {
    HUMEDAD_MIN = 730;
    EEPROM.put(60, HUMEDAD_MIN);
    EEPROM.commit();
  }
  EEPROM.get(70, SEGUNDOS_RIEGO);
  if (SEGUNDOS_RIEGO < 1 || SEGUNDOS_RIEGO > 200)
  {
    SEGUNDOS_RIEGO = 60;
    EEPROM.put(70, SEGUNDOS_RIEGO);
    EEPROM.commit();
  }
  EEPROM.get(80, DIAS_MAX);
  if (DIAS_MAX < 1 || DIAS_MAX > 30)
  {
    DIAS_MAX = 4;
    EEPROM.put(80, DIAS_MAX);
    EEPROM.commit();
  }
  EEPROM.get(90, HORAS_ENTRE_RIEGO_MIN);
  if (HORAS_ENTRE_RIEGO_MIN < 1 || HORAS_ENTRE_RIEGO_MIN > 300)
  {
    HORAS_ENTRE_RIEGO_MIN = 24;
    EEPROM.put(90, HORAS_ENTRE_RIEGO_MIN);
    EEPROM.commit();
  }
  EEPROM.get(100, NUM_MUESTRAS);
  if (NUM_MUESTRAS < 1 || NUM_MUESTRAS > 100)
  {
    NUM_MUESTRAS = 10;
    EEPROM.put(100, NUM_MUESTRAS);
    EEPROM.commit();
  }
  EEPROM.get(110, contador_muestras_humedad);
  if (contador_muestras_humedad < 0 || contador_muestras_humedad > 9)
  {
    contador_muestras_humedad = 0;
    EEPROM.put(110, contador_muestras_humedad);
    EEPROM.commit();
  }
  EEPROM.get(120, contador_riegos);
  if (contador_riegos < 0 || contador_riegos > 9)
  {
    contador_riegos = 0;
    EEPROM.put(120, contador_riegos);
    EEPROM.commit();
  }
*/
  EEPROM.get(1, eepromInicializada);
  if (eepromInicializada != 1)
  {
    inicializarEeprom();
    eepromInicializada = 1;
    EEPROM.put(1, eepromInicializada);
    EEPROM.commit();
  }
}


void Beginplay(int Channel, const void *wavfilename, int sizewav,float Volume) {
  // String Filename;
  // Filename=wavfilename;
  Serial.printf("CH:");
  Serial.print(Channel);
  stub[Channel]->SetGain(Volume);

  wav[Channel] = new AudioGeneratorWAV();

  delete file[Channel]; // housekeeping ?

  file[Channel] = new AudioFileSourcePROGMEM( wavfilename, sizewav ); 
 
  wav[Channel]->begin(file[Channel], stub[Channel]);

  Running[Channel] = true;
  Serial.printf("> at volume :");
  Serial.println(Volume);
  start[Channel] = millis();

      byte note = 45;
    byte velocity = 55;
    byte channel = 1;

    MIDI.sendNoteOn(note, velocity, channel);
    MIDI.sendNoteOff(note, velocity, channel);
}
