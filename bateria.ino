#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <analogmuxdemux.h>
#include <ESP8266WebServer.h>

#include "der1cl.h"
#include "der1op.h"
#include "der2.h"
#include "der3.h"
#include "izq1.h"
#include "izq2.h"
#include "izq3.h"
#include "pie_der.h"
#include "pie_izq_cer.h"

unsigned long t0 = millis();
int j = 0;

#include "AppleMIDI.h"
USING_NAMESPACE_APPLEMIDI

#include <AudioFileSourcePROGMEM.h>
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputMixer.h"

std::unique_ptr<ESP8266WebServer> server;

APPLEMIDI_CREATE_INSTANCE(WiFiUDP, MIDI, "ESP8266", DEFAULT_CONTROL_PORT);

// To run, set your ESP8266 build to 160MHz, and include a SPIFFS of 512KB or greater.



AudioOutputI2SNoDAC *out;
AudioOutputMixerStub *stub[8];
AudioOutputMixer *mixer;
AudioGeneratorWAV *wav[8];
AudioFileSourcePROGMEM *file[8];
boolean Running[8];
long start[8];

AnalogMux amux(4, 3, 2, 0); //specifies the values for S0=4, S1=3, S2=2 on the 4051
int valoresMaximos[8];

int configMax[8];
int configMin[8];
double configVol[8];

boolean pedalHit = false;
boolean pararSplash = false;


int eepromInicializada = 0; //eeprom pos 1. Está a 1 si la placa está inicializada



void setup()
{

  Serial.begin(115200);
  EEPROM.begin(512);
  delay(1000);
  leerEeprom(); //Lee las variables configurables de la Eeprom y si tienen valores inválidos, las inicializa

  WiFiManager wifiManager;
  wifiManager.autoConnect("bateria");

  audioLogger = &Serial;

  out = new AudioOutputI2SNoDAC();

  mixer = new AudioOutputMixer(32, out);
  for (int i = 0; i < 8; i++)
    stub[i] = mixer->NewInput();
  delay(100);





  initWifi();
  resetValoresMax();
}

void loop()
{
  MIDI.read();
  leerPiezos();
  for (int i = 0; i < 8; i++)
  {
    if (i == 1 && pararSplash && Running[i])
    {


      pararSplash = false;
      wav[i]->stop(); stub[i]->stop();
      Running[i] = false; Serial.printf("stopping %d \n", i);
         

    } else if (Running[i] && wav[i]->isRunning()) {
        

      if (!wav[i]->loop()) {
        wav[i]->stop(); stub[i]->stop();
        Running[i] = false; Serial.printf("stopping %d \n", i);
      }
    }
  }


server->handleClient();

}




//inicializa el wifi
void initWifi()
{
  Serial.println("connected...yeey :)");

  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

  server->on("/", handleRoot);

  server->begin();                //Start server


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
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

  for (int i = 0; i < 8; i++)
  {
    if (server->hasArg("piezo_" + (String) i + "_max"))
    {
      int valor = server->arg("piezo_" + (String) i + "_max").toInt();
      EEPROM.put(10 + (i * 10), valor);
      EEPROM.commit();
      Serial.println("Nuevo parametro piezo_" + (String) i + "_max: " + String(valor));
      configMax[i] = valor;

    }

    if (server->hasArg("piezo_" + (String) i + "_min"))
    {
      int valor = server->arg("piezo_" + (String) i + "_min").toInt();
      EEPROM.put(90 + (i * 10), valor);
      EEPROM.commit();
      Serial.println("Nuevo parametro piezo_" + (String) i + "_min: " + String(valor));
      configMin[i] = valor;
    }
    if (server->hasArg("vol_" + (String) i ))
    {
      float valor = server->arg("vol_" + (String) i ).toFloat();
      EEPROM.put(170 + (i * 10), valor);
      EEPROM.commit();
      Serial.println("Nuevo parametro vol_" + (String) i + String(valor));
      configVol[i] = valor;
    }

  }


  if (server->hasArg("accion") && server->arg("accion") == "ResetMax")
    resetValoresMax();



  String strRoot = "<!DOCTYPE HTML><html>\
    <body>\
    <h1>ESP8288 BATERÍA</h1>";
  for (int i = 0; i < 8; i++)
  {
    strRoot = strRoot + "<br><form>\
    <label for=\"fname\">PIEZO " + (String) i + " (Máx actual: " + (String)valoresMaximos[i] + ") </label>\
    Umbral Min: <input type=\"number\" style=\"width: 4em;\" min=\"1\" max=\"1024\" id=\"piezo_" + (String) i + "_min\" name=\"piezo_" + (String) i + "_min\" value=\"" +  (String)configMin[i] + "\">\
    Umbral Max: <input type=\"number\" style=\"width: 4em;\" min=\"1\" max=\"1024\" id=\"piezo_" + (String) i + "_max\" name=\"piezo_" + (String) i + "_max\" value=\"" +  (String)configMax[i] + "\">\
    Volumen: <input type=\"number\" step=\"0.01\" style=\"width: 4em;\" min=\"0\" max=\"10\" id=\"vol_" + (String) i + "\" name=\"vol_" + (String) i + "\" value=\"" +  (String)configVol[i] + "\">\
    <input type=\"submit\" value=\"Envia\"><br>\
  </form><br>";
  }

  strRoot = strRoot + "<form>\
    <button name=\"accion\" type=\"submit\" value=\"ResetMax\">Resetea máximos</button><br>\
    </form> \
    </body>\
    </html>";


  server->send(200, "text/html", strRoot);

}

//si es la primera vez que conectamos la placa, se inicializa la eeprom
void inicializarEeprom()
{
  Serial.println("Inicializando Eeprom, tardaremos 10 segundos...");

  for (int i = 0; i < 16; i++)
  {
    EEPROM.put(10 + (i * 10), i);
    EEPROM.commit();

  }
}



//Lee las variables configurables de la Eeprom y si tienen valores inválidos, las inicializa
void leerEeprom()
{
  for (int i = 0; i < 16; i++)
  {
    int valor = 0;
    EEPROM.get(10 + (i * 10), valor);
    if (valor < 1 || valor > 1024)
    {
      valor = 512;
      EEPROM.put(10 + (i * 10), valor);
      EEPROM.commit();
    }
    if (i < 8)
    {
      configMax[i] = valor;
    }
    else
    {
      configMin[i - 8] = valor;

    }



  }

  for (int i = 16; i < 24; i++)
  {
    float valor = 1.0;
    EEPROM.get(10 + (i * 10), valor);
    if (valor < 0.0 || valor > 10.0)
    {
      valor = 1.0;
      EEPROM.put(10 + (i * 10), valor);
      EEPROM.commit();
    }

    configVol[i - 16] = valor;




  }



  EEPROM.get(1, eepromInicializada);
  if (eepromInicializada != 1)
  {
    inicializarEeprom();
    eepromInicializada = 1;
    EEPROM.put(1, eepromInicializada);
    EEPROM.commit();
  }
}


void Beginplay(int Channel, const void *wavfilename, int sizewav, float Volume, byte note) {
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

  byte velocity = Volume * 127;
  byte channel = 1;

  MIDI.sendNoteOn(note, velocity, channel);
  MIDI.sendNoteOff(note, velocity, channel);
}

void leerPiezos() {
  for (int i = 0; i < 8; i++)
  {
    int val=0;
    if (millis() - t0 > 1000)
    {
      t0 = millis();
      if (j == i)
      {
          Serial.printf("asignando valor 1000");

        val = 512;

      }
      j++;
      if (j > 7)
        j = 0;
    }
    

    //int val = amux.AnalogRead(i);
    if (valoresMaximos[i] < val)
      valoresMaximos[i] = val;

 

    if (i == 0 && pedalHit && configMin[i] >= val)
    {
      pedalHit = false;


      Beginplay(7, pie_izq_cer, pie_izq_cer_len, configVol[i], 70);
      pararSplash = true;

    }
    if (configMin[i] < val)
    {

      if (i == 0)
        pedalHit = true;
      else if (i == 1 && pedalHit)
        Beginplay(0, der1op, der1op_len, ((float)val / (float)configMax[i])*configVol[i], 1);
      else if (i == 1 && !pedalHit)
        Beginplay(0, der1cl, der1cl_len, ((float)val / (float)configMax[i])*configVol[i], 1);

      else if (i == 2)
        Beginplay(1, der2, der2_len, ((float)val / (float)configMax[i])*configVol[i], 20);

      else if (i == 3)
        Beginplay(2, der3, der3_len, ((float)val / (float)configMax[i])*configVol[i], 20);
      else if (i == 4)
        Beginplay(3, izq1, izq1_len, ((float)val / (float)configMax[i])*configVol[i], 30);
      else if (i == 5)
        Beginplay(4, izq2, izq2_len, ((float)val / (float)configMax[i])*configVol[i], 40);
      else if (i == 6)
        Beginplay(5, izq3, izq3_len, ((float)val / (float)configMax[i])*configVol[i], 50);
      else if (i == 7)
        Beginplay(6, pie_der, pie_der_len, ((float)val / (float)configMax[i])*configVol[i], 60);








    }


  }
}

void resetValoresMax() {
  for (int i = 0; i < 8; i++)
  {
    valoresMaximos[i] = 0;


  }
}
