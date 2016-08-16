#include <OneWire.h> 
#include <DallasTemperature.h>
#include <DHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

//----------------------- mqtt -----------------------------
char msg[50];

// Update these with values suitable for your network.
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
IPAddress ip(192, 168, 51, 71);
IPAddress server(104, 236, 18, 140);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

EthernetClient ethClient;
PubSubClient client(ethClient);

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic","hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
//----------------------------------------------------------

//----------------------- pH -------------------------------
#define pHSensor A6            //pH meter Analog output to Arduino Analog Input 0
unsigned long int avgValue;  //Store the average value of the sensor feedback
float b;
int buf[10],temp;
//----------------------------------------------------------

//----------------------- DS18B20 --------------------------
#define pin 5 //Termocupla DS18B20
OneWire ourWire(pin); 
DallasTemperature sensors(&ourWire);
//----------------------------------------------------------

//----------------------- DHT11 ----------------------------
#define spin1 19
#define spin2 18
#define dhttype DHT11
DHT dht111(spin1, dhttype);
DHT dht112(spin2, dhttype);
//----------------------------------------------------------

//----------------------- AIR PUMP -------------------------
#define airPumpRelay 7
int flagAP = 0;
//----------------------------------------------------------

//----------------------- WATER PUMP -----------------------
#define waterPumpRelay 6
int flagWP = 0;
//----------------------------------------------------------

//----------------------- LDR ------------------------------
#define ldrs21 A0
#define ldrs22 A1
#define ldrs11 A2
#define ldrs12 A3
//----------------------------------------------------------

//----------------------- YF S201 --------------------------
unsigned int waterFlow = 0;
volatile int pulses; // Cantidad de pulsos del sensor. Como se usa dentro de una interrupcion debe ser volatile
unsigned int l_hour; // Calculated litres/hour
unsigned char flowSensor = 2; // Pin al que esta conectado el sensor. PWM e interrupcion!
unsigned long previous_t; // Para calcular el tiempo
unsigned long accumulated_pulses; // Pulsos acumulados
float l_total; // Litros acumulados
//----------------------------------------------------------
void flow(){
  pulses++; // Simplemente sumar el numero de pulsos
}

void setup() {
  Serial.begin(9600);
  
  client.setServer(server, 1883);
  client.setCallback(callback);
  Ethernet.begin(mac, ip);
 
  sensors.begin(); //Termocupla DS18B20
  dht111.begin();  //DHT11-1
  dht112.begin();  //DHT11-2
  pinMode(airPumpRelay,OUTPUT);
  pinMode(waterPumpRelay,OUTPUT);
  
  pinMode(flowSensor, INPUT); //initializes digital pin 4 as an input
  attachInterrupt(digitalPinToInterrupt(flowSensor), flow, RISING); //and the interrupt is attached
  interrupts(); // Habilitar interrupciones
  previous_t = millis();

  // Allow the hardware to sort itself out
  delay(1500);
}

float getpH(){
  for(int i=0;i<10;i++)       //Get 10 sample value from the sensor for smooth the value
  { 
    buf[i]=analogRead(pHSensor);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  avgValue=0;
  for(int i=2;i<8;i++)                      //take the average value of 6 center sample
    avgValue+=buf[i];
  float phValue=(float)avgValue*5.0/1024/6; //convert the analog into millivolt
  phValue=3.5*phValue;  
  return phValue;
}

void airWaterPumpONOFF (){
  char airWaterPump = '0';
  if (Serial.available() > 0) { 
    airWaterPump = Serial.read();
    if (airWaterPump == 'a'){
      if (flagAP == 0){
        digitalWrite(airPumpRelay,HIGH);
        flagAP = 1;
      }else{
        digitalWrite(airPumpRelay,LOW);
        flagAP = 0;
      }
    }else if (airWaterPump == 'b'){
      if (flagWP == 0){
        digitalWrite(waterPumpRelay,HIGH);
        flagWP = 1;
      }else{
        digitalWrite(waterPumpRelay,LOW);
        flagWP = 0;
      }
    }
    airWaterPump = '0';
  } 
  Serial.print("Air pump state: ");
  if (flagAP == 1){
    Serial.println("ON");
    client.publish("001/actuator/airpump", "on");
  }else{
    Serial.println("OFF");
    client.publish("001/actuator/airpump", "off");
  }
  Serial.print("Water pump state: ");
  if (flagWP == 1){
    Serial.println("ON");
    client.publish("001/actuator/waterpump", "on");
  }else{
    Serial.println("OFF");
    client.publish("001/actuator/waterpump", "off");
  }
}

int getCapsuleHum(DHT sensor){
  int hum = sensor.readHumidity();
  return hum;
}

int getCapsuleTemp(DHT sensor){
  int temp = sensor.readTemperature();
  return temp;
}

float getWaterTemp(){
  sensors.requestTemperatures(); 
  float temp = sensors.getTempCByIndex(0);
  return temp;
}

double light (int ldr1, int ldr2){
  double Vout1=analogRead(ldr1)*0.0048828125;
  double Vout2=analogRead(ldr2)*0.0048828125;
  //int lux=500/(10*((5-Vout)/Vout));//use this equation if the LDR is in the upper part of the divider
  int lux1=(2500/Vout1-500)/10;
  int lux2=(2500/Vout2-500)/10;
  int lux = (lux1 + lux2)/2;
  return lux;
}

void measureWaterFlow (){
// Cada segundo calcular e imprimir Litros/seg
  if( millis() - previous_t > 1000){
    previous_t = millis(); // Updates cloopTime
    // Pulse frequency (Hz) = 6.67 Q, Q is flow rate in L/min. (Results in +/- 3% range)
    // Q = frecuencia / 6.67 (L/min)
    // Q = (frecuencia * 60) / 6.67 (L/hora)
    accumulated_pulses += pulses;
    l_hour = (pulses * 60 / 6.67); // (Pulse frequency x 60 min) / 7.5Q = flow rate in L/hour
    pulses = 0; // Reset Counter
    }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  double lightIntensityS1 = light(ldrs11, ldrs12);
  char str_temp[9];
  /* 4 is mininum width, 2 is precision; float value is copied onto str_temp*/
  dtostrf(lightIntensityS1, 4, 2, str_temp);
  client.publish("001/sensor/environment/s1/lux", str_temp);
  
  double  lightIntensityS2 = light(ldrs21, ldrs22);
  dtostrf(lightIntensityS2, 4, 2, str_temp);
  client.publish("001/sensor/environment/s2/lux", str_temp);
  
  float   waterTemp = getWaterTemp();
  dtostrf(waterTemp, 4, 2, str_temp);
  client.publish("001/sensor/water/temperature", str_temp);
  
  float   waterpH = getpH();
  dtostrf(waterpH, 4, 2, str_temp);
  client.publish("001/sensor/water/ph", str_temp);
  
  int     humCapsuleS1 = getCapsuleHum(dht111); 
  snprintf (str_temp, 75, "%.2d", humCapsuleS1);
  client.publish("001/sensor/environment/s1/humidity", str_temp);
  
  int     tempCapsuleS1 = getCapsuleTemp(dht111);
  snprintf (str_temp, 75, "%.2d", tempCapsuleS1);
  client.publish("001/sensor/environment/s1/temperature", str_temp);

  int     humCapsuleS2 = getCapsuleHum(dht112); 
  snprintf (str_temp, 75, "%.2d", humCapsuleS2);
  client.publish("001/sensor/environment/s2/humidity", str_temp);
  
  int     tempCapsuleS2 = getCapsuleTemp(dht112);
  snprintf (str_temp, 75, "%.2d", tempCapsuleS2);
  client.publish("001/sensor/environment/s2/temperature", str_temp);

  Serial.println("_______________________e-Grow_______________________");
  airWaterPumpONOFF();
  measureWaterFlow();
  // to print in serial monitor
  Serial.print("Water temperature: ");
  Serial.print(waterTemp);
  Serial.println("C");
  Serial.print("Water pH: ");
  Serial.print(waterpH);
  Serial.println(" pH");
  Serial.print("Capsule's light intensity sector 1: ");
  Serial.print(lightIntensityS1);
  Serial.println(" Lux");
  Serial.print("Capsule's light intensity sector 2: ");
  Serial.print(lightIntensityS2);
  Serial.println(" Lux");
  Serial.print("Capsule's humidity sector 1: ");
  Serial.print(humCapsuleS1);
  Serial.println("%");
  Serial.print("Capsule's temperature sector 1: ");
  Serial.print(tempCapsuleS1);
  Serial.println("C");
  Serial.print("Capsule's humidity sector 2: ");
  Serial.print(humCapsuleS2);
  Serial.println("%");
  Serial.print("Capsule's temperature sector 2: ");
  Serial.print(tempCapsuleS2);
  Serial.println("C");
  Serial.println("____________________________________________________");
  Serial.println();
  Serial.println();
  Serial.println();
  
  delay(5000);
}

