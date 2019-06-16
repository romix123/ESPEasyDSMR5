//#################################### Plugin 110: P1WifiGateway ########################################
// 
//  based on P020 Ser2Net, extended by Ronald Leenes romix/-at-/macuser.nl
//   code cleanup Eric Wijngaards
//
//  designed for combo 
//    Wemos D1 mini (see http://wemos.cc) and 
//    P1 wifi gateway shield (see https://circuits.io/circuits/2460082)
//    see http://romix.macuser.nl for kits
//#######################################################################################################

#define PLUGIN_110
#define PLUGIN_ID_110         110
#define PLUGIN_NAME_110       "P1 Wifi Gateway"
#define PLUGIN_VALUENAME1_110 "P1WifiGateway"
#define LOG_LEVEL_RAW_DATA 5

boolean Plugin_110_init = false;

#define STATUS_LED 12
#define STATUS_LED1 D1
#define RTS D0
boolean serialdebug = false;

int serialTimeOut = 1; // The maximum timeout until the serial is available again.
int reportInterval = 1;  // rapporteer ieder xe frame
int frame = 0; //aantal telegrammen sinds vorige report

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define BUFFER_SIZE 1200
#define NETBUF_SIZE 1200
char serial_buf[BUFFER_SIZE];
unsigned int bytes_read = 0; 
String inputString = "";


#define DISABLED 0
#define WAITING 1
#define READING 2
#define CHECKSUM 3
#define DONE 4
#define FAILURE 5
int state = DISABLED;


boolean CRCcheck = false;
boolean DSMR4 = false;

unsigned int currCRC = 0;
int checkI = 0;

int pos181;
int pos182;
int pos170;
int pos270;
int pos281;
int pos282;
int pos2421; // gas 0-1:24.2.1
int tempPos;
int tempPos2;
String T170;
String T270;
String T181;
String T182;
String T281;
String T282;
String G2421;
  String data;
  

WiFiServer *P1GatewayServer;
WiFiClient P1GatewayClient;




boolean Plugin_110(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;
  static byte connectionState=0;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
 
        Device[++deviceCount].Number = PLUGIN_ID_110;
        Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
        Device[deviceCount].Custom = true;
        Device[deviceCount].TimerOption = false;
        
//        String taskdevicename = "P1 poort";
//        char tmpString[41];
//              taskdevicename.toCharArray(tmpString, 41);
//              strcpy(ExtraTaskSettings.TaskDeviceName, tmpString);

        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_110);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_110));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        if (ExtraTaskSettings.TaskDevicePluginConfigLong[0] ==0) { // set defaults


                ExtraTaskSettings.TaskDevicePluginConfigLong[0] = 8088;   // port
                ExtraTaskSettings.TaskDevicePluginConfigLong[1] = 115200; // baud
                ExtraTaskSettings.TaskDevicePluginConfigLong[2] = 8; // data
                ExtraTaskSettings.TaskDevicePluginConfigLong[3] = 0; // no parity
                ExtraTaskSettings.TaskDevicePluginConfigLong[4] = 1; // stop bits
                Settings.TaskDevicePluginConfig[event->TaskIndex][0] = 50; //timeout
                Settings.TaskDevicePluginConfig[event->TaskIndex][1] = 10; //sample freq every xth
                Settings.TaskDevicePluginConfig[event->TaskIndex][2] = false; //
        }
        char tmpString[128];
        sprintf_P(tmpString, PSTR("<TR><TD>TCP Port:<TD><input type='text' name='plugin_110_port' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[0]);
        string += tmpString;
        sprintf_P(tmpString, PSTR("<TR><TD>Baud Rate:<TD><input type='text' name='plugin_110_baud' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[1]);
        string += tmpString;
        sprintf_P(tmpString, PSTR("<TR><TD>Data bits:<TD><input type='text' name='plugin_110_data' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[2]);
        string += tmpString;

        byte choice = ExtraTaskSettings.TaskDevicePluginConfigLong[3];
        String options[3];
        options[0] = F("No parity");
        options[1] = F("Even");
        options[2] = F("Odd");
        int optionValues[3];
        optionValues[0] = 0;
        optionValues[1] = 2;
        optionValues[2] = 3;
        string += F("<TR><TD>Parity:<TD><select name='plugin_110_parity'>");
        for (byte x = 0; x < 3; x++)
        {
          string += F("<option value='");
          string += optionValues[x];
          string += "'";
          if (choice == optionValues[x])
            string += F(" selected");
          string += ">";
          string += options[x];
          string += F("</option>");
        }
        string += F("</select>");

        sprintf_P(tmpString, PSTR("<TR><TD>Stop bits:<TD><input type='text' name='plugin_110_stop' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[4]);
        string += tmpString;

        string += F("<TR><TD>Reset target after boot:<TD>");
        addPinSelect(false, string, "taskdevicepin1", Settings.TaskDevicePin1[event->TaskIndex]);

        sprintf_P(tmpString, PSTR("<TR><TD>RX Receive Timeout (mSec):<TD><input type='text' name='plugin_110_rxwait' value='%u'>"), Settings.TaskDevicePluginConfig[event->TaskIndex][0]);
        string += tmpString;
        
        sprintf_P(tmpString, PSTR("<TR><TD>Report every Xth sample (advice: DSMR4 - 1, DSMR5 - 10):<TD><input type='text' name='plugin_110_report' value='%u'>"), Settings.TaskDevicePluginConfig[event->TaskIndex][1]);
        string += tmpString;

         string += F("<TR><TD>Data checks:<TD>");
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][2])
          string += F("<input type=checkbox name=plugin_110_check checked>");
        else
          string += F("<input type=checkbox name=plugin_110_check>");
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        String plugin1 = WebServer.arg("plugin_110_port");
        ExtraTaskSettings.TaskDevicePluginConfigLong[0] = plugin1.toInt();
        String plugin2 = WebServer.arg("plugin_110_baud");
        ExtraTaskSettings.TaskDevicePluginConfigLong[1] = plugin2.toInt();
        String plugin3 = WebServer.arg("plugin_110_data");
        ExtraTaskSettings.TaskDevicePluginConfigLong[2] = plugin3.toInt();
        String plugin4 = WebServer.arg("plugin_110_parity");
        ExtraTaskSettings.TaskDevicePluginConfigLong[3] = plugin4.toInt();
        String plugin5 = WebServer.arg("plugin_110_stop");
        ExtraTaskSettings.TaskDevicePluginConfigLong[4] = plugin5.toInt();
        String plugin6 = WebServer.arg("plugin_110_rxwait");
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = plugin6.toInt();
        String plugin7 = WebServer.arg("plugin_110_report");
        Settings.TaskDevicePluginConfig[event->TaskIndex][1] = plugin7.toInt();
        String plugin8 = WebServer.arg("plugin_110_check");
        if (plugin8=="on"){
          Settings.TaskDevicePluginConfig[event->TaskIndex][2] = true;
        } else
        {          
          Settings.TaskDevicePluginConfig[event->TaskIndex][2] = false;
        }

        
        
        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        pinMode(STATUS_LED, OUTPUT);
        digitalWrite(STATUS_LED,0);
        pinMode(STATUS_LED1, OUTPUT);
        digitalWrite(STATUS_LED1,0);

        pinMode(RTS, OUTPUT);
        digitalWrite(RTS,0);

        LoadTaskSettings(event->TaskIndex);
        if ((ExtraTaskSettings.TaskDevicePluginConfigLong[0] != 0) && (ExtraTaskSettings.TaskDevicePluginConfigLong[1] != 0))
        {
          byte serialconfig = 0x10;
          serialconfig += ExtraTaskSettings.TaskDevicePluginConfigLong[3];
          serialconfig += (ExtraTaskSettings.TaskDevicePluginConfigLong[2] - 5) << 2;
          if (ExtraTaskSettings.TaskDevicePluginConfigLong[4] == 2)
            serialconfig += 0x20;
          Serial.begin(ExtraTaskSettings.TaskDevicePluginConfigLong[1], (SerialConfig)serialconfig);
          P1GatewayServer = new WiFiServer(ExtraTaskSettings.TaskDevicePluginConfigLong[0]);
          P1GatewayServer->begin();

          if (Settings.TaskDevicePin1[event->TaskIndex] != -1)
          {
            pinMode(Settings.TaskDevicePin1[event->TaskIndex], OUTPUT);
            digitalWrite(Settings.TaskDevicePin1[event->TaskIndex], LOW);
            delay(500);
            digitalWrite(Settings.TaskDevicePin1[event->TaskIndex], HIGH);
            pinMode(Settings.TaskDevicePin1[event->TaskIndex], INPUT_PULLUP);
          }

          serialTimeOut = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
          reportInterval = Settings.TaskDevicePluginConfig[event->TaskIndex][1];

        data = "Reporting every ";
        data += String(reportInterval);
        data += " samples";
          addLog(LOG_LEVEL_DEBUG, data);
          data ="";
          

          Plugin_110_init = true;
        }

        blinkLED();

                 if (ExtraTaskSettings.TaskDevicePluginConfigLong[1] == 115200) {
                     addLog(LOG_LEVEL_INFO, "DSMR version 4 or 5 meter");
                     DSMR4 = true;
                 } else {
                     addLog(LOG_LEVEL_INFO, "DSMR version 2.x meter");
                 }

         if (Settings.TaskDevicePluginConfig[event->TaskIndex][2] == true){
                    addLog(LOG_LEVEL_INFO, "Datagram checks on");
                    CRCcheck = true;
         } else {
                    addLog(LOG_LEVEL_INFO, "Datagram checks OFF");
                    CRCcheck = false;
         }
                
  // let's get going              
        digitalWrite(RTS,1);   // Activate Opto-coupler in meter
        blinkLED();
                 
        state = WAITING;
        success = true;
        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        if (Plugin_110_init)
        { 



          size_t bytes_read;
          if (P1GatewayServer->hasClient())
          {
            if (P1GatewayClient) P1GatewayClient.stop();
            P1GatewayClient = P1GatewayServer->available();
            char log[40];
            strcpy_P(log, PSTR("P1 msg: Client connected!"));
            addLog(LOG_LEVEL_INFO, log);
          } 

         
          if (P1GatewayClient.connected())
          {
            connectionState=1;
            uint8_t net_buf[BUFFER_SIZE];
            int count = P1GatewayClient.available();
            if (count > 0)
            {
              if (count > BUFFER_SIZE)
                count = BUFFER_SIZE;
              bytes_read = P1GatewayClient.read(net_buf, count);
              Serial.write(net_buf, bytes_read);
              Serial.flush(); // Waits for the transmission of outgoing serial data to complete

              if (count == BUFFER_SIZE) // if we have a full buffer, drop the last position to stuff with string end marker
              {
                count--;
                char log[40];
                strcpy_P(log, PSTR("P1 error: network buffer full!"));   // and log buffer full situation
                addLog(LOG_LEVEL_ERROR, log);
              }
              net_buf[count]=0; // before logging as a char array, zero terminate the last position to be safe.
              char log[BUFFER_SIZE+40];
              sprintf_P(log, PSTR("P1 error: N>: %s"), (char*)net_buf);
              addLog(LOG_LEVEL_DEBUG,log);
            }
          }
          else
          {
            if(connectionState == 1) // there was a client connected before...
            {
              connectionState=0;
              char log[40];
              strcpy_P(log, PSTR("P1 msg: Client disconnected!"));
              addLog(LOG_LEVEL_ERROR, log);
            }
            
            while (Serial.available())
              Serial.read();
          }

          success = true;
        }
        break;
      }

    case PLUGIN_SERIAL_IN:
      {
        if (!Plugin_110_init) {
            break;
        }

        clearInputStream();
        state = WAITING;              

        while (state != DONE && state != FAILURE) { 
            while (Serial.available()) {
                char ch = Serial.read();

                // Check if we need to change the state to READING or CHECKSUM
                if (state == WAITING && ch == '/') {
                   state = READING;
                   digitalWrite(STATUS_LED,1);
                } else if (state == READING && ch == '!') { 
                   state = CHECKSUM;
                } 

                // If the state is READING or CHECKSUM we have to add the character to the inputstream
                if (state == READING || state == CHECKSUM) {
                    if (!addToInputStream(ch)) {
                        // Something went wrong, probably the buffer is full or the character is not allowed.
                        state = FAILURE;
                        break;
                    }
                }

                // Check if the state must be changed to DONE
                if (state == CHECKSUM && ch == '\n') { // LF-character
                   // The full telegram has been read, finish it by adding a blank line ('CR LF') to the inputstream
                   if (!addToInputStream('\r') || !addToInputStream('\n')) { // CR and LF-character
                        // Something went wrong, probably the buffer is full.
                        state = FAILURE;
                        break;
                   }

                   // Check the telegram if CRC-check has been enabled.
                   if (CRCcheck && !checkDatagram(bytes_read)) {
                       addLog(LOG_LEVEL_ERROR,"P1 error: Invalid CRC, dropped data");
                       state = FAILURE;
                       break;
                   }
                 
                    state = DONE;
                    break;
                }
            }

            // If the state is DONE or FAILURE we don't want to wait for new data.
            if (state == DONE || state == FAILURE) {
                break;
            }

//            // Wait until new data is available.
//            int timeOut = serialTimeOut;
//            while (!Serial.available() && timeOut >= 0) {
//                delay(1);
//                timeOut--;
//
//                if (timeOut == 0) {
//                    addLog(LOG_LEVEL_DEBUG, "P1 debug: timeout reached");
//                    state = FAILURE;
//                    break;
//                }
//            }
        }

        digitalWrite(STATUS_LED,0);

        if (state == DONE) {
           addLog(LOG_LEVEL_DEBUG, inputString);
                   
            parse_P1(); // Parse the complete telegram
 
             // Push it to Domoticz if the reportInterval has been reached
             frame++;
            if (frame >= reportInterval ) {
                if (P1GatewayClient.connected()){
                  P1GatewayClient.write((const uint8_t*)serial_buf, bytes_read);
                  P1GatewayClient.flush();
                  addLog(LOG_LEVEL_INFO,"P1 msg: pushed data to Domoticz");
                } else {
                  addLog(LOG_LEVEL_DEBUG,"P1 msg: data could not be pushed since there is no connection to Domoticz");
                }
              frame = 0;
            }
            blinkLED();
        } else {           // The state is not DONE, clear the input stream and flush the serial connection.
             clearInputStream();
             Serial.flush();
             break;
        }
        success = true;
        break;
     }

  }
  return success;
}

boolean addToInputStream(char ch) {
    if (bytes_read >= BUFFER_SIZE) {
        addLog(LOG_LEVEL_ERROR,"P1 error: buffer full");
        addLog(LOG_LEVEL_DEBUG, inputString);
        return false;
    }

    if (!validP1char(ch)){
        addLog(LOG_LEVEL_ERROR,"P1 error: DATA corrupt, discarded input.");
        addLog(LOG_LEVEL_DEBUG, inputString);
        return false;
    }
    
    inputString += ch;
    serial_buf[bytes_read] = ch;
    bytes_read++;
    return true;
}

void clearInputStream() {
    inputString = "";
    bytes_read = 0;
}



void handle_P1monitor(){
  int i=0;

 // if (!isLoggedIn()) return;


 String str = "";
  addHeader(true, str);
  str += F("<script language='JavaScript'>function RefreshMe(){window.location = window.location}setTimeout('RefreshMe()', 3000);</script>");
  str += F("<table><tbody><tr><th>P1 monitor</th><th></th></tr> ");

  str += "<TR><TD>Totaal verbruik tarief 1: </TD><TD><span id=\"totaal1\">";
    str += T181;
//         str += F("<BR>");
  str += "</span> kWh</TD></TR> <TR><TD>Totaal verbruik tarief 2: </TD><TD><span id=\"totaal2\">";
    str += T182;
//         str += F("<BR>");
  str += "</span> kWh</TD></TR><TR><TD>Teruggeleverd tarief 1: </TD><TD><span id=\"retour1\">";
    str += T281;
 //        str += F("<BR>");
   str += "</span> kWh</TD></TR><TR><TD>Teruggeleverd tarief 2: </TD><TD><span id=\"retour2\">";
    str += T282;
//         str += F("<BR>");
  str += "</span> kWh</TD></TR><TR><TD>Actueel verbruik         : </TD><TD><span id=\"huidig\">";
    str += T170;
  //       str += F("<BR>");
   str += "</span> kW</TD></TR> <TR><TD>Huidige teruglevering   : </TD><TD><span id=\"retour\">";
    str += T270;
    str += "</span> kW</TD></TR> <TR><TD>Totaal gasverbuik   : </TD><TD><span id=\"gas\">";
   str += G2421;
 //        str += F(" <BR><BR>");
   //  str += "Input   : ";
   //       str += inputString;

 str += F("</span> m3</TD></TR> </tbody></table>");
  addFooter(str);
  WebServer.send(200, "text/html", str);
 // free(TempString);
  
  }

String trim_zero(String data){
  int i = 0;
  while (data.charAt(i) =='0' && data.charAt(1+i) !='.') {
    data.setCharAt(i,' ');
    i++;
  }
  data.trim();
  return data;
}

void parse_P1(){
  data = "T1: ";
                        pos181 = inputString.indexOf("1-0:1.8.1", 0);
                        tempPos = inputString.indexOf("*kWh)", pos181);
                        if (tempPos != -1)
                            T181 = trim_zero(inputString.substring(pos181 + 10, tempPos)); // + 4));

                            data += T181;
                            data += " - ";

                        pos182 = inputString.indexOf("1-0:1.8.2", 0);
                        tempPos = inputString.indexOf("*kWh)", pos182);
                        if (tempPos != -1)
                            T182 = trim_zero(inputString.substring(pos182 + 10, tempPos)); // + 4));

                            data += "T2: ";
                            data += T182;
                            data += " - ";
                            
                         pos281 = inputString.indexOf("1-0:2.8.1", 0);
                        tempPos = inputString.indexOf("*kWh)", pos281);
                        if (tempPos != -1)
                            T281 = trim_zero(inputString.substring(pos281 + 10, tempPos));//  + 4));

                         pos282 = inputString.indexOf("1-0:2.8.2", 0);
                        tempPos = inputString.indexOf("*kWh)", pos282);
                        if (tempPos != -1) T282 = trim_zero(inputString.substring(pos282 + 10, tempPos)); // + 4));
                        
                        pos170 = inputString.indexOf("1-0:1.7.0", 0);
                        tempPos = inputString.indexOf("*kW)", pos170);
                        if (tempPos != -1) T170 = trim_zero(inputString.substring(pos170 + 10, tempPos)); // +3));

                            data += "Huidig: ";
                            data += T170;
                            data += " - ";
                            
                        pos270 = inputString.indexOf("1-0:2.7.0", 0);
                        tempPos = inputString.indexOf("*kW)", pos270);
                        if (tempPos != -1) T270 = trim_zero(inputString.substring(pos270 + 10, tempPos)); //+3));

                        
                 pos2421 = inputString.indexOf(":24.2.1", 0); // 0-1:24.2.1  of  0-2:24.2.1
                    if (DSMR4){
                       // 0-1:24.2.1(160516110000S)(06303.228*m3)
                        tempPos = inputString.indexOf(")(", pos2421);
                        tempPos2 = inputString.indexOf("*m3", tempPos);
                        if (tempPos2 != -1) G2421 = trim_zero(inputString.substring(tempPos + 2, tempPos2)); //+3));
                    } else {
                    // 0-1:24.2.1(160516110000S)(*m3)(06303.228)!
                     // 0-1:24.2.1(160516110000S)(*m3)(06303.228)!
                       tempPos = inputString.indexOf("(m3)", pos2421);
                       tempPos = inputString.indexOf("(", tempPos+1);
                       tempPos2 = inputString.indexOf(")", tempPos);
                                               
                        if (tempPos2 != -1) G2421 = trim_zero(inputString.substring(tempPos+1, tempPos2 - 1));
                        //G2421 += "*m3";
                    }

                            data += "G: ";
                            data += G2421;
                            
                        addLog(LOG_LEVEL_INFO,data);  

                        data = "";

          //   }
}

void blinkLED(){        
        digitalWrite(STATUS_LED,1);
        digitalWrite(STATUS_LED1,1);

        delay(50);
        digitalWrite(STATUS_LED,0);
        digitalWrite(STATUS_LED1,0);

        }
/*
 * validP1char
 *     checks whether the incoming character is a valid one for a P1 datagram. Returns false if not, which signals corrupt datagram
 */
bool validP1char(char ch){  

  if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >='A' && ch <= 'Z') || ch =='.' || ch == ' ' || ch == '!' || 
      ch == '\\' || ch == '/' || ch == '\r' || ch == '\n' || ch == '(' || ch ==')' || ch == '-' || ch=='*' || ch ==':') 
  {
  return true;
  } else {
    data = "P1 error: invalid char read from P1 :";
    data += ch;
        addLog(LOG_LEVEL_DEBUG,data);

         if(serialdebug){
            Serial.print("faulty char>");
            Serial.print(ch);
            Serial.println("<"); 
         }
    return false;
  }
 }

int FindCharInArrayRev(char array[], char c, int len) {
  for (int i = len - 1; i >= 0; i--) {
    if (array[i] == c) {
      return i;
    }
  }
  return -1;
}

/*
 * CRC16 
 *    based on code written by Jan ten Hove
 *   https://github.com/jantenhove/P1-Meter-ESP8266
 */
unsigned int CRC16(unsigned int crc, unsigned char *buf, int len)
{

 
  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (unsigned int)buf[pos];    // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }

  return crc;
}

/*  checkDatagram 
 *    checks whether the checksum of the data received from P1 matches the checksum attached to the
 *    telegram 
 *   based on code written by Jan ten Hove
 *   https://github.com/jantenhove/P1-Meter-ESP8266
 */
bool checkDatagram(int len){
  int startChar = FindCharInArrayRev(serial_buf, '/', len);
  int endChar = FindCharInArrayRev(serial_buf, '!', len);
  bool validCRCFound = false;

if (!CRCcheck) return true;

 if(serialdebug){
  Serial.print("input length: ");
  Serial.println(len); 
    Serial.print("Start char \ : ");
  Serial.println(startChar); 
      Serial.print("End char ! : ");
  Serial.println(endChar); 
 }

  if (endChar >= 0)
  {
    currCRC=CRC16(0x0000,(unsigned char *) serial_buf, endChar-startChar+1);

    char messageCRC[4];
    strncpy(messageCRC, serial_buf + endChar + 1, 4);

if(serialdebug) {
      for(int cnt=0; cnt<len;cnt++)
        Serial.print(serial_buf[cnt]);
}    

    validCRCFound = (strtol(messageCRC, NULL, 16) == currCRC);
    if(!validCRCFound) {
      addLog(LOG_LEVEL_ERROR,"P1 error: invalid CRC found");
    }
    currCRC = 0;
  }
  return validCRCFound;
}
