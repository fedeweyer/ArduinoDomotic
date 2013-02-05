#define DEBUG true

#include <SPI.h>
#include <Ethernet.h>
#include <Bounce.h>
#include <EEPROMEx.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192,168,1,128};
#if defined(ARDUINO) && ARDUINO >= 100
EthernetServer server(80);
#else
Server server(80);
#endif

int address = 10;
const int numberOfInputs = 2 ;
typedef struct
  {
      int input;
      int output;
      int type;
      int state;
      //Bounce *bounce;
  }  input_type;

input_type config[numberOfInputs];

Bounce *bouncer;

void loadConfig (){
  Serial.println("Leyendo eprom."); 
  EEPROM.readBlock(address, config);
  for (int i = 0; i < numberOfInputs; i++) {
    Serial.println(config[i].input); 
    Serial.println(config[i].output);
    Serial.println(config[i].type);
    Serial.println(config[i].state);  
  }

  
}

void createConfig () {
  config[0].input = 4;
  config[0].output = 8;
  config[0].type = 0;
  config[0].state = 0;
  
  config[1].input = 5;
  config[1].output = 9;
  config[1].type = 0;
  config[1].state = 1;
  
  Serial.println("Escribiendo eprom."); 
  EEPROM.writeBlock(address, config);
}

String configToJson() {
  String configStr = String();
  configStr += "{\"";
  configStr += "config\" : [ \n";
  for (int i = 0; i < numberOfInputs; i++) {
    configStr += "{\"input\":";
    configStr += config[i].input;
    configStr += ",";
    configStr += "\"output\":";
    configStr += config[i].output;
    configStr += ",";
    configStr += "\"state\":";
    configStr += config[i].state;
    configStr += ",";
    configStr += "\"type\":";
    configStr += config[i].type;
    configStr += "}";
}
  configStr += "]}";
  
#if DEBUG
    Serial.println("Config output: "); 
    Serial.println(configStr);
#endif
  return configStr;
}

void updateOutput(int pin, int state){

  for (int i = 0; i < numberOfInputs; i++) {
    if (config[i].output==pin){
      if (state){
        digitalWrite(pin, HIGH);
        config[i].state=1;
      }else{
        digitalWrite(pin, LOW);
        config[i].state=0;
      }      
    }
  }
  EEPROM.updateBlock(address, config);
}

void setup()
{
#if DEBUG
  Serial.begin(9600);
  Serial.println("---"); 
#endif
   
  loadConfig ();
  
  bouncer = (Bounce *) malloc(sizeof(Bounce) * numberOfInputs);
  
  for (int i = 0; i < numberOfInputs; i++) {

    pinMode(config[i].input , INPUT); 
    pinMode(config[i].output, OUTPUT);  
    //config[i].bounce = (Bounce *) malloc(sizeof(Bounce));
    //*config[i].bounce = Bounce(config[i].input ,5); 
    
    if (config[i].type==0){
      if (config[i].state){
        digitalWrite(config[i].output, HIGH); 
        
      } else {
        digitalWrite(config[i].output, LOW); 
          
      }
     bouncer[i] = Bounce( config[i].input,5 ); 
    }  
    
    
  }
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  
  Serial.println("---");
}

//  url buffer size
#define BUFSIZE 255

// Toggle case sensitivity
#define CASESENSE true

void loop()
{
  //·····································Comienzo codigo input handling·····································//  
  for (int i = 0; i < numberOfInputs; i++) {
    
    if (bouncer[i].update()){
      Serial.println("cambio");
      if (bouncer[i].read()){
        updateOutput(config[i].output, 1);     
      }else{
        updateOutput(config[i].output, 0);
      }
      //EEPROM.updateBlock(address, config);
    }
  }
  
  
  //·····································Comienzo codigo restDuino·····································//
  char clientline[BUFSIZE];
  int index = 0;
  // listen for incoming clients
#if defined(ARDUINO) && ARDUINO >= 100
  EthernetClient client = server.available();
#else
  Client client = server.available();
#endif
  if (client) {

    //  reset input buffer
    index = 0;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        //  fill url the buffer
        if(c != '\n' && c != '\r' && index < BUFSIZE){ // Reads until either an eol character is reached or the buffer is full
          clientline[index++] = c;
          continue;
        }  

#if DEBUG
        Serial.print("client available bytes before flush: "); Serial.println(client.available());
        Serial.print("request = "); Serial.println(clientline);
#endif

        // Flush any remaining bytes from the client buffer
        client.flush();

#if DEBUG
        // Should be 0
        Serial.print("client available bytes after flush: "); Serial.println(client.available());
#endif

        //  convert clientline into a proper
        //  string for further processing
        String urlString = String(clientline);

        //  extract the operation
        String op = urlString.substring(0,urlString.indexOf(' '));

        //  we're only interested in the first part...
        urlString = urlString.substring(urlString.indexOf('/'), urlString.indexOf(' ', urlString.indexOf('/')));

        //  put what's left of the URL back in client line
#if CASESENSE
        urlString.toUpperCase();
#endif
        urlString.toCharArray(clientline, BUFSIZE);

        //  get the first two parameters
        char *pin = strtok(clientline,"/");
        char *value = strtok(NULL,"/");

        //  this is where we actually *do something*!
        char outValue[10] = "MU";
        String jsonOut = String();
        if (urlString!="/CONFIG"){
        if(pin != NULL){
          if(value != NULL){

#if DEBUG
            //  set the pin value
            Serial.println("setting pin");
#endif

            //  select the pin
            int selectedPin = atoi (pin);
#if DEBUG
            Serial.println(selectedPin);
#endif
            //Revisar si esta bien que cambie los tipos de output input
            //  set the pin for output
            pinMode(selectedPin, OUTPUT);

            //  determine digital or analog (PWM)
            if(strncmp(value, "HIGH", 4) == 0 || strncmp(value, "LOW", 3) == 0){

#if DEBUG
              //  digital
              Serial.println("digital");
#endif

              if(strncmp(value, "HIGH", 4) == 0){
#if DEBUG
                Serial.println("HIGH");
#endif
                //digitalWrite(selectedPin, HIGH);
                updateOutput(selectedPin, 1);
              }

              if(strncmp(value, "LOW", 3) == 0){
#if DEBUG
                Serial.println("LOW");
#endif
                //digitalWrite(selectedPin, LOW);
                updateOutput(selectedPin, 0);
              }

            } 
            else {

#if DEBUG
              //  analog
              Serial.println("analog");
#endif
              //  get numeric value
              int selectedValue = atoi(value);              
#if DEBUG
              Serial.println(selectedValue);
#endif
              analogWrite(selectedPin, selectedValue);

            }

            //  return status
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println();

          } 
          else {
#if DEBUG
            //  read the pin value
            Serial.println("reading pin");
#endif

            //  determine analog or digital
            if(pin[0] == 'a' || pin[0] == 'A'){

              //  analog
              int selectedPin = pin[1] - '0';

#if DEBUG
              Serial.println(selectedPin);
              Serial.println("analog");
#endif

              sprintf(outValue,"%d",analogRead(selectedPin));

#if DEBUG
              Serial.println(outValue);
#endif

            } 
            else if(pin[0] != NULL) {

              //  digital
              int selectedPin = pin[0] - '0';

#if DEBUG
              Serial.println(selectedPin);
              Serial.println("digital");
#endif

              pinMode(selectedPin, INPUT);

              int inValue = digitalRead(selectedPin);

              if(inValue == 0){
                sprintf(outValue,"%s","LOW");
                //sprintf(outValue,"%d",digitalRead(selectedPin));
              }

              if(inValue == 1){
                sprintf(outValue,"%s","HIGH");
              }

#if DEBUG
              Serial.println(outValue);
#endif
            }

            //  assemble the json output
            jsonOut += "{\"";
            jsonOut += pin;
            jsonOut += "\":\"";
            jsonOut += outValue;
            jsonOut += "\"}";

            //  return value with wildcarded Cross-origin policy
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Access-Control-Allow-Origin: *");
            client.println();
            client.println(jsonOut);
          }
        } 
        else {

          //  error
#if DEBUG
          Serial.println("erroring");
#endif
          client.println("HTTP/1.1 404 Not Found");
          client.println("Content-Type: text/html");
          client.println();

        }
      }else{
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Access-Control-Allow-Origin: *");
        client.println();
        client.println(configToJson());
      }
        break;
      }
    }

    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    //client.stop();
    client.stop();
      while(client.status() != 0){
      delay(5);
    }
  }
}
