#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>
#include <StepperMotor.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

/*--------Déclaration NeoPixels--------*/
#define PIN 4
#define NUMPIXELS 1
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

/*--------Vyhlásenie Krokový motor--------*/
StepperMotor stepper(15,13,12,14);        // IN1 --> D5, IN2 --> D6, IN3 --> D8, IN4 --> D7
int motorSpeed = 2;                       // So znižovaním tohto čísla sa motor spomaľuje
int motorSteps_360_degres_int = 4180;     // 4150 je približne 360 stupňov (normálne 4076)
float motorSteps_360_degres = 4.180;      // 4150 delené 100, pretože je príliš veľké na výpočet, v prípade potreby sa v kóde vynásobí 100
int Steps_from_switch_to_longitute_0 = motorSteps_360_degres_int/4;
int motorSteps_Correction = 70;           // Korekcia kvôli polohe spínača nie presne na -179°

int Position_Longitude_Laser;             // Skutočná zemepisná dĺžka lasera

/*--------Déclaration Servo--------*/
Servo Servo_Latitude;                     // Funkcia serva pre rozsah uhla {0°;120°}: MicroSecond = 10 849 * Uhol + 810 . Príklady              
                                          // MicroSecond/Angle:1461us=60° (stredný rozsah); 2150us=120° (max); 810us=0° (min)
int MicroSecond;             
int Min_MicroSecond = 897;                // Mikrosekunda na odoslanie do serva, aby zasiahlo Min_Latitude_ISS (-52°)
int Max_MicroSecond = 2025;               // Mikrosekunda, ktorá sa odošle do serva, aby zasiahlo Max_Latitude_ISS (52°)
int Correctif_Latitude = 11;              // Na opravu kalibrácie zemepisnej šírky lasera

int Position_Latitude_Laser;              // Aktuálna zemepisná šírka lasera

/*--------Déclaration Butée--------*/
const int Switch_Buttee = 0;              // Pozor, D3 je vstup --> 3,3V max --> D3 má vnútorný odpor proti vytiahnutiu (spojenie D3 so zemou)
int Etat_Buttee;

/*--------Déclaration Paramètre Connexion du D1 Mini--------*/
const char* ssid     = "UPC";             // SSID
const char* password = "kurieOko_+5";     // Password
const char* host = "192.168.43.1";        // IP serveur - Server IP
const int   port = 8080;                  // Port serveur - Server Port

/*--------Vyhlásenie ISS Nízka a Vysoká zemepisná šírka a dĺžka--------*/
int Min_Latitude_ISS = -52;               // Minimálna zemepisná šírka ISS je -52°
int Max_Latitude_ISS = 52;                // Maximálna zemepisná šírka ISS je 52°

int Latitude_ISS;
int Longitude_ISS;
int nb_move = 0;
  
/*--------Déclaration Unix Time on Greenwhich Meridian--------*/
int UnixTime_int;
int Hour_UTC;
int Minute_UTC;
int Second_UTC;

/*--------Déclaration Request for router location-------*/
String Request;
int latitude_Lamp;
int longitude_Lamp;

void setup() {
  strip.begin();
  strip.show();

/*turn off all neopixels*/ 
  for(int i=0; i<NUMPIXELS; i++){
  strip.setPixelColor(i, strip.Color(0,0,0)); //white color
  strip.show();                               // Toto odošle aktualizovanú farbu pixelov do hardvéru.
  }
                 
  stepper.setStepDuration(motorSpeed);
  Servo_Latitude.attach(5);
  pinMode(Switch_Buttee, INPUT_PULLUP);       //spoj D3 so zemou
  
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("IP address_router: ");
  Serial.println(WiFi.gatewayIP());

/*Init. Servo to latitude 0°)*/ 
  pinMode(5, OUTPUT);                       // Pripojte servo
  Servo_Latitude.writeMicroseconds(1461);   // 1461 mikrosekundový impulz zodpovedajúci 60° pre servo, čo zodpovedá 0° zemepisnej šírky
  delay(500);
  pinMode(5, INPUT);                        // Odpojte servo (aby ste sa vyhli chveniu serva)
  Position_Latitude_Laser = 0;

/*turn on all neopixels*/ 
  for(int i=0; i<NUMPIXELS; i++){
   strip.setPixelColor(i, strip.Color(205,0,205)); //white color
   strip.show();                                   // Toto odošle aktualizovanú farbu pixelov do hardvéru.
   }
   
/*Init. Stepper to longitude -180°)*/
  stepper.step(motorSteps_Correction);             //krokový krok je posunutý, ak je stacionárny medzi -179° a -180
  
  for(int s=0; s<motorSteps_360_degres_int; s++){
    Etat_Buttee = digitalRead(Switch_Buttee);
    if (Etat_Buttee == LOW){
      break;
    }
     stepper.step(-10);
  }
  stepper.step(-motorSteps_Correction); 
  Position_Longitude_Laser = -180;
   
/*Presuňte sa na miesto Lampa, aby ste informovali, kde sa nachádzame*/
  Get_Public_IP();                           //Getting Public IP of the router
  Get_Lamp_location();                       //Getting the Latitude and Longitude of the lamp 
  move_Longitude_Laser_Angle(longitude_Lamp + 6);
  move_Latitude_Laser_Angle(latitude_Lamp); 
  delay(1100);
  
  /*turn off all neopixels*/ 
  for(int i=0; i<NUMPIXELS; i++){
  strip.setPixelColor(i, strip.Color(0,0,0)); //white color
  strip.show();                               // Toto odošle aktualizovanú farbu pixelov do hardvéru.
  }

}

void loop() {

Get_Longitude_Latitude_Hour_Minute();       // Vyžiadanie času, zemepisnej šírky a dĺžky na server
Sun_Position_NeoPixels(Hour_UTC);           // Zobrazovanie Neopixelov, ktoré predstavujú polohu slnka na povrchu Zeme         
move_Longitude_Laser_Angle(Longitude_ISS);  // Presun krokového motora na zemepisnú dĺžku ISS
move_Latitude_Laser_Angle(Latitude_ISS);    // Presun servomotora na ISS Latitude

delay(2500);                                // Počkajte 2,5 sekundy, aby sa skrátil čas čakania, ak sa LED spínača práve rozsvieti
Sun_Position_NeoPixels(Hour_UTC);           // Opätovné zobrazenie neopixelov, ak sa prepínač LED práve zapne
delay(2500);                                // Počkajte ďalších 2,5 sekundy, aby medzi každou požiadavkou na server API bolo konečné oneskorenie 5 sekúnd

}

/*------------------------------------------------------------------------------------------------------------*/
void Get_Longitude_Latitude_Hour_Minute() {
  
  /* Odoslanie požiadavky do API, ktoré lokalizuje ISS */
  if (WiFi.status() == WL_CONNECTED) {      //Check WiFi connection status
    HTTPClient http;                        //Deklarujte objekt triedy HTTPClient
    http.begin("http://api.open-notify.org/iss-now.json");  //Zadajte cieľ žiadosti so žiadosťou o údaje ISS (používajte iba stránky http, nie stránky https)
    int httpCode = http.GET();              //Send the request
    
  /* Získavanie údajov JSON z údajov API a analýzy*/
    if (httpCode > 0) {                     //Skontrolujte návratový kód
      String payload = http.getString();    //Získajte užitočné zaťaženie odpovede na žiadosť
      Serial.println(payload);

  /* Analyzujte užitočné zaťaženie a získajte čas, zemepisnú šírku a dĺžku */
      const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 200;
      DynamicJsonDocument doc(capacity);
      
      deserializeJson(doc, http.getString());
      
      const char* message = doc["message"];                             // v príklade to bude „úspech"
      long timestamp = doc["timestamp"];                                // v príklade to bude "1553413828"
      
      float iss_position_latitude = doc["iss_position"]["latitude"];    // in the example it will be "-51.0835"
      float iss_position_longitude = doc["iss_position"]["longitude"];  // in the example it will be "-157.1986"

  /* Izolujte údaje zemepisnej dĺžky, zemepisnej šírky a času Unix */
                                             //Prevod pohyblivej zemepisnej šírky na vnútornú zemepisnú šírku
      Latitude_ISS = (int) iss_position_latitude;
      Serial.print("Latitude ISS : ");
      Serial.println(Latitude_ISS);

  //Prevod plávajúcej zemepisnej dĺžky na Int zemepisnú dĺžku
      Longitude_ISS = (int) iss_position_longitude;
      Serial.print("Longitude ISS : ");
      Serial.println(Longitude_ISS);
         
  //Converting the Unix Time data into Int Unix Time      
      UnixTime_int = (int) timestamp;                                 //Transformácia reťazca Unix Time na dáta Int
      Serial.print("Unix Time : ");
      Serial.println(UnixTime_int);
      
 //Modifying the Unix into a HH:MM:SS
      Hour_UTC = (UnixTime_int % 86400) / 3600;
      Minute_UTC = (UnixTime_int % 3600) / 60;
      Second_UTC = UnixTime_int % 60;
      
    }
    http.end();                                                       //Close connection
  }
}

/*------------------------------------------------------------------------------------------------------------*/
void Get_Public_IP() {
 
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient httpIP;                       
    String ip;
    
    httpIP.begin("http://api.ipify.org/?format=json");
    int httpCodeIP = httpIP.GET();                        //Send the request to get the Public IP of the router
  /* Getting the JSON data from the API and Parse data*/
    if (httpCodeIP > 0) {                                 //Check the returning code
      String payloadIP = httpIP.getString();              //Získajte užitočné zaťaženie odpovede na žiadosť

  /* Parse the payload to get Puáblic IP of the router */
      const size_t capacityIP = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 200;
      DynamicJsonDocument docIP(capacityIP);
      deserializeJson(docIP, httpIP.getString());
      String ip = docIP["ip"];
      Serial.println("Public IP address_router:");
      Serial.println(ip);
      Request = ("http://www.iplocate.io/api/lookup/");   //Pozor, „začiatok“ je pre nezabezpečenú http stránku, používajte iba http stránku a nie https.
      Request = Request+ip;                               //zadanie požiadavky pridaním IP adresy do API webovej stránky
    }
      httpIP.end();
  }
}


/*------------------------------------------------------------------------------------------------------------*/
void Get_Lamp_location(){
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient httpLamp;
    httpLamp.begin(Request);                      //Zadajte cieľ požiadavky a opýtajte sa na umiestnenie smerovača
    int httpCodeLamp = httpLamp.GET();            //Send the request
  /* Získavanie údajov JSON z údajov API a analýzy*/
    if (httpCodeLamp > 0) {                       //Check the returning code
      String payloadLamp = httpLamp.getString();  //Získajte užitočné zaťaženie odpovede na žiadosť
      Serial.println("Payload Lamp Location : ");
      Serial.println(payloadLamp);
      
   /* Analyzujte užitočné zaťaženie, aby ste získali verejnú IP adresu smerovača */
      const size_t capacityLamp = JSON_OBJECT_SIZE(13) + 400;
      DynamicJsonDocument docLamp(capacityLamp);
      deserializeJson(docLamp, httpLamp.getString());
      latitude_Lamp = docLamp["latitude"];         // 48.8931
      longitude_Lamp = docLamp["longitude"];       // 2.3465
      Serial.println("Latitude Lamp : ");
      Serial.println(latitude_Lamp);
      Serial.println("Longitude Lamp : ");
      Serial.println(longitude_Lamp);
    
    }
    httpLamp.end(); 
  }
}


/*------------------------------------------------------------------------------------------------------------*/
void move_Latitude_Laser_Angle(int Angle_Latitude) {
  int Angle_diff = Angle_Latitude - Position_Latitude_Laser;
  
  if(Angle_diff == 0){
    Position_Latitude_Laser = Angle_Latitude;
    
    }else{
          MicroSecond = map(Angle_Latitude, Min_Latitude_ISS, Max_Latitude_ISS, Min_MicroSecond, Max_MicroSecond);
          pinMode(5, OUTPUT);
          Servo_Latitude.writeMicroseconds(MicroSecond + Correctif_Latitude);
          delay(500);
          pinMode(5, INPUT);
          Position_Latitude_Laser = Angle_Latitude;
 }
}


/*------------------------------------------------------------------------------------------------------------*/
void move_Longitude_Laser_Angle(int Angle_Longitude) {
                                                   // NÁPAD FUNKCIE: Nápad s neopixelmi, obnoviť údaje o polohe, kde sa lampa nachádza, a ak je ISS v okruhu 50 km?   neopixely sa animujú, aby naznačili, že na oblohe vidíme ISS

  if (Angle_Longitude == -179){                    // Ak je uhol ISS rovný -179, návrat na -179° kalibráciou pomocou spínača (rovnako v nastavení)
    stepper.step(-50);                             // Posunutie -100 (takmer -10°) krokov na prvé miesto, ak je laserový modul za prepínačom
    for(int s=0; s<5000; s++){
    Etat_Buttee = digitalRead(Switch_Buttee);
    if (Etat_Buttee == LOW){
      break;
    }
     stepper.step(-10);
  }
  stepper.step(-motorSteps_Correction); 
  Position_Longitude_Laser = -180;
  
  //Umiestnenie na -179°
  float x_pas_calibration = (motorSteps_360_degres * 1 * 1000 / 360);
  int x_pas_int_calibration = ceil(x_pas_calibration);
  stepper.step(x_pas_int_calibration);
  Position_Longitude_Laser = -179;
  delay(20000);
  }
  
//  if (Angle_Longitude == 178 || Angle_Longitude == 179){ // Ak je uhol ISS 178 alebo 179, vynútime uhol 177, aby sme predišli situácii, keď zatvoríme lampu, kde je laser v pozícii na reštartovanie priamo kliknutím swit a odbočiť nesprávnym spôsobom
// Angle_Longitude = 177;
//  }                               
  
  int Angle_diff = Angle_Longitude - Position_Longitude_Laser;
  if(Angle_diff == 0){
        Position_Longitude_Laser = Angle_Longitude;
        
    }else{
          int x_pas_int;
          float x_pas = (motorSteps_360_degres * Angle_diff * 1000 / 360);  // Prevod hodnoty zadanej v Uhle° na počet krokov

          if(Latitude_ISS >= 41 || Latitude_ISS <= -41){                    // Ak je zemepisná šírka ISS vyššia ako 40° a nižšia ako -40°
            nb_move = nb_move+1;
                                                                            // Striedavá korekcia počtu krokov, pretože +4 je príliš rýchle a +3 príliš nízke
             if((nb_move/2)*2 != nb_move){                                  // Testovanie, či je číslo požiadavky párové alebo poškodené, pridaním inej opravy v x_pas_int (napr.: (5/2)=2 (a nie 2,5, pretože typ typu celé čísloe)
                x_pas_int = ((int) x_pas) + 5;                              // Konverzia plaváka na horný int, aby sa zabránilo necelému číslu
             }else{
                x_pas_int = ((int) x_pas) + 4;
             }                                                              // Prevod plaváka na horný int, aby sa zabránilo necelé číslo
            
          }else if(Latitude_ISS >= 24 && Latitude_ISS < 41){  //27 et 42    // Ak je zemepisná šírka ISS vyššia ako 25° a nižšia ako 40°
            nb_move = 0;
            x_pas_int = ((int) x_pas) + 4;
            
          }else if(Latitude_ISS <= -24 && Latitude_ISS > -41){              // Ak je zemepisná šírka ISS nižšia ako -25° a nižšia ako -40°
            nb_move = 0;
            x_pas_int = ((int) x_pas) + 4;
  
          }else if(Latitude_ISS > -24 && Latitude_ISS < 24){                // Ak je zemepisná šírka ISS vyššia ako -25° a nižšia ako 25°
            nb_move = nb_move+1;
                                                                            // Striedavá korekcia počtu krokov, pretože +4 je príliš rýchle a +3 príliš nízke
             if((nb_move/2)*2 != nb_move){                                  // Testovanie, či je číslo požiadavky párové alebo poškodené, pridaním inej opravy do x_pas_int (napr.: (5/2)=2 (a nie 2,5, pretože typ typu celé číslo)
                x_pas_int = ((int) x_pas) + 4;                              // Prevod plaváka na horný int, aby sa zabránilo necelé číslo
             }else{
                x_pas_int = ((int) x_pas) + 3;
             }
          
          }else{
            nb_move = 0;
            x_pas_int = ((int) x_pas) + 4;       
          }

          stepper.step(x_pas_int);
          Position_Longitude_Laser = Angle_Longitude;
   }
}


/*------------------------------------------------------------------------------------------------------------*/
void Sun_Position_NeoPixels(int Hour){
  
  if((latitude_Lamp >= (Latitude_ISS - 2)) && (latitude_Lamp <= (Latitude_ISS + 2)) && (longitude_Lamp >= (Longitude_ISS - 2)) && (longitude_Lamp <= (Longitude_ISS + 2))){                                                                    // Ak je ISS nad umiestnením lampy, neopixely budú zobrazovať zelenú farbu
    /*turn on all neopixels*/ 
    for(int z=0; z<NUMPIXELS; z++){
     strip.setPixelColor(z, strip.Color(205,0,205));                         // Biela farba
     strip.show();                                                           // Toto odošle aktualizovanú farbu pixelov do hardvéru.
     }
     delay(300);
    /*turn off all neopixels*/ 
    for(int z=0; z<NUMPIXELS; z++){
      strip.setPixelColor(z, strip.Color(0,0,0));                            // Biela farba
      strip.show();                                                          // Toto odošle aktualizovanú farbu pixelov do hardvéru.
    }
  }
  
  switch (Hour) {
  case 0:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
  case 1:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
  case 2:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
  case 3:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
  case 4:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 5:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 6:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(255, 90, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 7:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 8:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(255, 90, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 9:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 10:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 11:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(255, 90, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 12:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 13:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(255, 90, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 14:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 15:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(0, 0, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(255, 90, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 16:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 17:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 18:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(255, 90, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 19:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 20:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(255, 90, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(0, 0, 0));
   break;
  case 21:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
  case 22:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
  case 23:
   strip.setPixelColor(0, strip.Color(0, 0, 0));
   strip.setPixelColor(1, strip.Color(255, 90, 0));
   strip.setPixelColor(2, strip.Color(0, 0, 0));
   strip.setPixelColor(3, strip.Color(0, 0, 0));
   strip.setPixelColor(4, strip.Color(0, 0, 0));
   strip.setPixelColor(5, strip.Color(0, 0, 0));
   strip.setPixelColor(6, strip.Color(0, 0, 0));
   strip.setPixelColor(7, strip.Color(0, 0, 0));
   strip.setPixelColor(8, strip.Color(0, 0, 0));
   strip.setPixelColor(9, strip.Color(255, 90, 0));
   break;
 }
  strip.show();                                        // Toto odošle aktualizovanú farbu pixelov do hardvéru.
  delay(50);                                           // Oneskorenie o určité časové obdobie (v milisekundách).
}
  