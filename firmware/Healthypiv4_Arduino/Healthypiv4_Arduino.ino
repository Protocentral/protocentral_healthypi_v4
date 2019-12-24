//////////////////////////////////////////////////////////////////////////////////////////
//   Arduino Library for Healthypi-v4
//   
//   Copyright (c) 2019 ProtoCentral
//   Heartrate and respiration computation based on original code from Texas Instruments
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,  INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR   OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   Requires g4p_control graphing library for processing. 
//   Downloaded from Processing IDE Sketch->Import Library->Add Library->G4P Install
//////////////////////////////////////////////////////////////////////////////////////
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiClient.h>
#include "SPIFFS.h"
#include <FS.h>      //Include File System Headers
#include "Protocentral_ADS1292r.h"
#include "Protocentral_ecg_resp_signal_processing.h"
#include "Protocentral_AFE4490_Oximeter.h"
#include "Protocentral_MAX30205.h"
#include "Protocentral_spo2_algorithm.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define Heartrate_SERVICE_UUID (uint16_t(0x180D))
#define Heartrate_CHARACTERISTIC_UUID (uint16_t(0x2A37))
#define sp02_SERVICE_UUID (uint16_t(0x1822)) 
#define sp02_CHARACTERISTIC_UUID (uint16_t(0x2A5E))
#define DATASTREAM_SERVICE_UUID (uint16_t(0x1122)) 
#define DATASTREAM_CHARACTERISTIC_UUID (uint16_t(0x1424))
#define TEMP_SERVICE_UUID (uint16_t(0x1809)) 
#define TEMP_CHARACTERISTIC_UUID (uint16_t(0x2a6e))
#define BATTERY_SERVICE_UUID (uint16_t(0x180F)) 
#define BATTERY_CHARACTERISTIC_UUID (uint16_t(0x2a19))
#define HRV_SERVICE_UUID "cd5c7491-4448-7db8-ae4c-d1da8cba36d0"
#define HRV_CHARACTERISTIC_UUID "01bfa86f-970f-8d96-d44d-9023c47faddc"
#define HIST_CHARACTERISTIC_UUID "01bf1525-970f-8d96-d44d-9023c47faddc"

#define BLE_MODE                0X01
#define WEBSERVER_MODE          0X02
#define V3_MODE                 0X03
#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_DATA_LEN_LSB   20
#define CES_CMDIF_DATA_LEN_MSB    0
#define CES_CMDIF_TYPE_DATA     0x02
#define CES_CMDIF_PKT_STOP_1    0x00
#define CES_CMDIF_PKT_STOP_2    0x0B
#define PUSH_BUTTON              17
#define SLIDE_SWITCH             16
#define MAX30205_READ_INTERVAL 10000
#define LINELEN                 34 
#define HISTGRM_DATA_SIZE      12*4
#define HISTGRM_CALC_TH         10
#define MAX                     20

unsigned int array[MAX];

int rear = -1;
int sqsum;
int hist[] = {0};
int k=0;
int count = 0;
int min_f=0;
int max_f=0;
int max_t=0;
int min_t=0;
int index_cnt = 0;
int pass_size; 
int data_count;
int ssid_size;
int status_size;
int temperature;
int number_of_samples = 0;
int battery=0;
int bat_count=0;
int bt_rem = 0;
int wifi_count;
int flag=0;

float sdnn;
float sdnn_f;
float rmssd;
float mean_f;
float rmssd_f;
float per_pnn;
float pnn_f=0;
float tri =0;
float temp;

void HealthyPiV4_Webserver_Init();
void send_data_serial_port(void);

volatile uint8_t global_HeartRate = 0;
volatile uint8_t global_HeartRate_prev = 0;
volatile uint8_t global_RespirationRate=0;
volatile uint8_t global_RespirationRate_prev = 0;
volatile uint8_t npeakflag = 0;
volatile long time_count=0;
volatile long hist_time_count=0;
volatile bool histgrm_ready_flag = false;
volatile unsigned int RR;

uint8_t ecg_data_buff[20];
uint8_t resp_data_buff[2];
uint8_t ppg_data_buff[20];
uint8_t Healthypi_Mode = WEBSERVER_MODE;
uint8_t lead_flag = 0x04;
uint8_t data_len = 20;
uint8_t heartbeat,sp02,respirationrate;
uint8_t histgrm_percent_bin[HISTGRM_DATA_SIZE/4];
uint8_t hr_percent_count = 0;
uint8_t hrv_array[20];

uint16_t ecg_stream_cnt = 0;
uint16_t resp_stream_cnt = 0;
uint16_t ppg_stream_cnt = 0;
uint16_t ppg_wave_ir;

int16_t ecg_wave_sample,ecg_filterout;
int16_t res_wave_sample,resp_filterout;

uint32_t hr_histgrm[HISTGRM_DATA_SIZE];

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool temp_data_ready = false;
bool spo2_calc_done = false;
bool ecg_buf_ready = false;
bool resp_buf_ready = false;
bool ppg_buf_ready = false;
bool hrv_ready_flag = false;
bool mode_write_flag = false;
bool slide_switch_flag = false;
bool processing_intrpt = false;
bool credential_success_flag = false;
bool STA_mode_indication = false;
bool bat_data_ready = false;
bool leadoff_detected = true;
bool startup_flag = true;

char DataPacket[30];
char ssid[32];
char password[64];
char modestatus[32];
char tmp_ecgbuf[1200];

String ssid_to_connect;
String password_to_connect;
String tmp_ecgbu;
String strValue = "";

static int bat_prev=100;
static uint8_t bat_percent = 100;

const int ADS1292_DRDY_PIN = 26;
const int ADS1292_CS_PIN = 13;
const int ADS1292_START_PIN = 14;
const int ADS1292_PWDN_PIN = 27;
const int AFE4490_CS_PIN = 21; 
const int AFE4490_DRDY_PIN = 39; 
const int AFE4490_PWDN_PIN = 4; 
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
const char* host = "Healthypi_v4";
const char* host_password = "Open@1234";
const char DataPacketHeader[] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, CES_CMDIF_DATA_LEN_LSB, CES_CMDIF_DATA_LEN_MSB, CES_CMDIF_TYPE_DATA};
const char DataPacketFooter[] = {CES_CMDIF_PKT_STOP_1, CES_CMDIF_PKT_STOP_2};

BLEServer* pServer = NULL;
BLECharacteristic* Heartrate_Characteristic = NULL;
BLECharacteristic* sp02_Characteristic = NULL;
BLECharacteristic* datastream_Characteristic = NULL;
BLECharacteristic* battery_Characteristic = NULL;
BLECharacteristic* temperature_Characteristic = NULL;
BLECharacteristic* hist_Characteristic = NULL;
BLECharacteristic* hrv_Characteristic = NULL;

ads1292r ADS1292R;   // define class ads1292r
ads1292r_processing ECG_RESPIRATION_ALGORITHM; // define class ecg_algorithm
AFE4490 afe4490;
MAX30205 tempSensor;
spo2_algorithm spo2;
ads1292r_data ads1292r_raw_data;
afe44xx_data afe44xx_raw_data;

class MyServerCallbacks: public BLEServerCallbacks 
{
  void onConnect(BLEServer* pServer)
  {
    deviceConnected = true;
    Serial.println("connected");
  }

  void onDisconnect(BLEServer* pServer)
  {
    deviceConnected = false;
  }
};

class MyCallbackHandler: public BLECharacteristicCallbacks 
{
  void onWrite(BLECharacteristic *datastream_Characteristic)
  {
    std::string value = datastream_Characteristic->getValue();
    int len = value.length();
    strValue = "0";

    if (value.length() > 0) 
    {
      Serial.print("New value: ");

      for (int i = 0; i < value.length(); i++)
      {
        Serial.print(String(value[i]));
        strValue += value[i];
      }

      Serial.println();
    }

  }
};

String processor(const String& var)
{

  if(var == "heartrate")
  {
    return String(global_HeartRate);
  }

  return String();
}

WebServer server(80);

void handleWebRequests()
{
  if(loadFromSpiffs(server.uri())) return;

  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i=0; i<server.args(); i++)
  {
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  Serial.println(message);
}

void webpage() 
{    
  server.sendHeader ("Location","/set_network_credentials.html",true);
  server.send(303,"text/plane","");
} 

void response()
{

  if(server.hasArg("SSID") && server.hasArg("Password") && (server.arg("Password").length()>7))
  { 
    ssid_to_connect = server.arg("SSID");
    password_to_connect = server.arg("Password");
    server.send(200, "text/html", "<html><body><h3>Successfully Changed the Network Credentials. It is restarting.<br></h3><h3>After restart connect to the changed network. In case if it is not connected in 60sec it will go to the softAp mode.<br><br>Enter healthypi.local/ in the address bar. If it is not connecting, use the IP address of the connected network which is printed in the serial moniter<br><br></h3></body></html>");
    Serial.print("SSID: ");
    Serial.println(ssid_to_connect);
    Serial.print("Password: ");
    Serial.println(password_to_connect);
    credential_success_flag = true;
  } 
  else 
  {
    server.send(400, "text/html", "<html><body><h1>HTTP Error 400</h1><p>Bad request. Please enter a value.</p></body></html>");
    credential_success_flag = false;
  }
  
}

void handleRoot() 
{
  server.sendHeader("Location", "/main.html",true);   //Redirect to our html web page
  server.send(303, "text/plane","");
}

void handleheartrate() 
{
  int ln = tmp_ecgbu.length();
  server.setContentLength(ln);
  index_cnt = 0;
  number_of_samples = 0;
  server.send(200, "text/html", tmp_ecgbu);
}

void handletemperature() 
{
  char sensor_data[20]; 
  sprintf(sensor_data, "%d,%d,%d,%d", global_HeartRate, global_RespirationRate, afe44xx_raw_data.spo2, temperature);
  server.send(200, "text/html", sensor_data); //Send ADC value only to client ajax request
}

void push_button_intr_handler()
{

  if(Healthypi_Mode != WEBSERVER_MODE)
  {
    detachInterrupt(ADS1292_DRDY_PIN);
    mode_write_flag = true;
  }

}

void slideswitch_intr_handler()
{ 

  if(!processing_intrpt)
  {
    processing_intrpt = true;
    detachInterrupt(ADS1292_DRDY_PIN);
    slide_switch_flag = true;
  }

}

void delLine(fs::FS &fs, const char * path, uint32_t line,const int char_to_delete)
{ 
  File file = fs.open(path, FILE_WRITE);
  
  if(!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  
  uint32_t S= (line-1)*LINELEN;
  file.seek(S);  
  char ch[35]; 

  // build the 'delete line'
  for(uint8_t i=0;i<char_to_delete;i++) 
  {
    ch[i]=' ';
  }

  file.print(ch); // all marked as deleted! yea!
  file.close();
  Serial.println("file closed");
}

void deleteFile(fs::FS &fs, const char * path)
{
  Serial.printf("Deleting file: %s\r\n", path);
  
  if(fs.remove(path))
  {
    Serial.println("- file deleted");
  } 
  else 
  {
    Serial.println("- delete failed");
  }
 
}

bool readFile(fs::FS &fs, const char * path)
{
  Serial.printf("Reading file: %s\r\n", path);
  uint8_t rd_config = 0;
  File file = fs.open(path, FILE_READ);
  
  if(!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return false;
  }

  Serial.println("- read from file:");
  rd_config = file.read();
  Serial.println(rd_config);
 
  if(rd_config == 0x0f)
  {
    Healthypi_Mode = WEBSERVER_MODE;
    delLine(SPIFFS,"/v4_mode.txt",1,5);
  }
  else
  {
    return false;
  }

  file.close();
  Serial.println("file closed");
  return true;
}

bool fileread(fs::FS &fs, const char * path)
{
  Serial.printf("Reading file: %s\r\n", path);
  uint8_t md_config = 0;
  File file = fs.open(path, FILE_READ);
  
  if(!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return false;
  }

  Serial.println("- read from file:");
  md_config = file.read();
  Serial.println(md_config);
 
  if(md_config == 0x0a)
  {
    Healthypi_Mode = WEBSERVER_MODE;
    delLine(SPIFFS,"/web_mode.txt",1,5);
  }
  else if(md_config == 0x0b)
  {
    Healthypi_Mode = WEBSERVER_MODE;
    delLine(SPIFFS,"/web_mode.txt",1,5);
  }
  else if(md_config == 0x0c)
  {
    Healthypi_Mode = WEBSERVER_MODE;
    delLine(SPIFFS,"/web_mode.txt",1,5);
  }
  else
  {
    return false;
  }

  file.close();
  Serial.println("file closed");
  return true;
}

void writeFile(fs::FS &fs, const char * path, const char * message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);

  if(!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  
  if(file.print(message))
  {
    Serial.println("- file written");
  } 
  else 
  {
    Serial.println("- frite failed");
  }

  file.close();
  Serial.println("file closed");
}

void readFile(fs::FS &fs, const char * path, int* data_count, char* file_data)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, FILE_READ);
  
  if(!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");    
  }

  Serial.println("- read from file:"); 
  int i=0;
  
  while (file.available())
  {
    file_data[i++] = file.read();
    Serial.write(file_data[i-1]);
  }

  *data_count = file.size();
  file.close();
  Serial.println("file closed");
}

void HealthyPiV4_BLE_Init()
{
  BLEDevice::init("Healthypi v4"); // Create the BLE Device
  pServer = BLEDevice::createServer();   // Create the BLE Server
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *HeartrateService = pServer->createService(Heartrate_SERVICE_UUID);   // Create the BLE Service
  BLEService *sp02Service = pServer->createService(sp02_SERVICE_UUID);   // Create the BLE Service
  BLEService *TemperatureService = pServer->createService(TEMP_SERVICE_UUID);
  BLEService *batteryService = pServer->createService(BATTERY_SERVICE_UUID);
  BLEService *hrvService = pServer->createService(HRV_SERVICE_UUID);
  BLEService *datastreamService = pServer->createService(DATASTREAM_SERVICE_UUID);

  Heartrate_Characteristic = HeartrateService->createCharacteristic(
                              Heartrate_CHARACTERISTIC_UUID,
                              BLECharacteristic::PROPERTY_READ   |
                              BLECharacteristic::PROPERTY_WRITE  |
                              BLECharacteristic::PROPERTY_NOTIFY
                              );

  sp02_Characteristic = sp02Service->createCharacteristic(
                        sp02_CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY
                        );

  temperature_Characteristic = TemperatureService->createCharacteristic(
                                TEMP_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ   |
                                BLECharacteristic::PROPERTY_WRITE  |
                                BLECharacteristic::PROPERTY_NOTIFY
                                );
                                                                      
  battery_Characteristic = batteryService->createCharacteristic(
                            BATTERY_CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_WRITE  |
                            BLECharacteristic::PROPERTY_NOTIFY
                            );

  hrv_Characteristic = hrvService->createCharacteristic(
                        HRV_CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY
                        );

  hist_Characteristic = hrvService->createCharacteristic(
                          HIST_CHARACTERISTIC_UUID,
                          BLECharacteristic::PROPERTY_READ   |
                          BLECharacteristic::PROPERTY_WRITE  |
                          BLECharacteristic::PROPERTY_NOTIFY
                          );
 
  datastream_Characteristic = datastreamService->createCharacteristic(
                              DATASTREAM_CHARACTERISTIC_UUID,
                              BLECharacteristic::PROPERTY_READ   |
                              BLECharacteristic::PROPERTY_WRITE  |
                              BLECharacteristic::PROPERTY_NOTIFY 
                              );
                
  Heartrate_Characteristic->addDescriptor(new BLE2902());
  sp02_Characteristic->addDescriptor(new BLE2902());
  temperature_Characteristic->addDescriptor(new BLE2902());
  battery_Characteristic->addDescriptor(new BLE2902());
  hist_Characteristic->addDescriptor(new BLE2902());
  hrv_Characteristic->addDescriptor(new BLE2902());
  datastream_Characteristic->addDescriptor(new BLE2902());
  datastream_Characteristic->setCallbacks(new MyCallbackHandler()); 

  // Start the service
  HeartrateService->start();
  sp02Service->start();
  TemperatureService->start();
  batteryService->start();
  hrvService->start();
  datastreamService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(Heartrate_SERVICE_UUID);
  pAdvertising->addServiceUUID(sp02_SERVICE_UUID);
  pAdvertising->addServiceUUID(TEMP_SERVICE_UUID);
  pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
  pAdvertising->addServiceUUID(HRV_SERVICE_UUID);
  pAdvertising->addServiceUUID(DATASTREAM_SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x00);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  ble_advertising(); 
  Serial.println("Waiting a client connection to notify...");
}

void read_battery_value()
{
  static int adc_val = analogRead(A0);
  battery += adc_val;
  
  if(bat_count == 9)
  {         
    battery = (battery/10);
    battery=((battery*2)-400);
    
    if (battery > 4100)
    {
      battery = 4100;
    }
    else if(battery < 3600 )
    {
      battery = 3600;
    }

    if (startup_flag == true)
    {
      bat_prev = battery;
      startup_flag = false;
    }

    bt_rem = (battery % 100); 

    if(bt_rem>80 && bt_rem < 99 && (bat_prev != 0))
    {
      battery = bat_prev;
    }

    if((battery/100)>=41)
    {
      battery = 100;
    }
    else if((battery/100)==40)
    {
      battery = 80;
    }
    else if((battery/100)==39)
    {
      battery = 60;
    }
    else if((battery/100)==38)
    {
      battery=45;
    }
    else if((battery/100)==37)
    {
      battery=30;
    }
    else if((battery/100)<=36)
    {
      battery = 20;
    }

    bat_percent = (uint8_t) battery;
    bat_count=0;
    battery=0;
    bat_data_ready = true;
  }
  else
  {
    bat_count++;
  }

}
 
void add_hr_histgrm(uint8_t hr)
{
  uint8_t index = hr/10;
  hr_histgrm[index-4]++;
  uint32_t sum = 0;

  if(hr_percent_count++ > HISTGRM_CALC_TH)
  {
    hr_percent_count = 0;
    
    for(int i = 0; i < HISTGRM_DATA_SIZE; i++)
    {
      sum += hr_histgrm[i];
    }

    if(sum != 0)
    {

      for(int i = 0; i < HISTGRM_DATA_SIZE/4; i++)
      {
        uint32_t percent = ((hr_histgrm[i] * 100) / sum);
        histgrm_percent_bin[i] = percent;
      }

    }
    
    histgrm_ready_flag = true;
  }

}

uint8_t* read_send_data(uint8_t peakvalue,uint8_t respirationrate)
{  
  int meanval;
  uint16_t sdnn;
  uint16_t pnn;
  uint16_t rmsd;
  RR = peakvalue;
  k++;

  if(rear == MAX-1)
  {
    
    for(int i=0;i<(MAX-1);i++)
    {
      array[i]=array[i+1];
    }

    array[MAX-1] = RR;   
  }
  else
  {     
    rear++;
    array[rear] = RR;
  }

  if(k>=MAX)
  { 
    max_f = HRVMAX(array);
    min_f = HRVMIN(array);
    mean_f = mean(array);
    sdnn_f = sdnn_ff(array);
    pnn_f = pnn_ff(array);
    rmssd_f=rmssd_ff(array);
  
    meanval = mean_f*100;
    sdnn= sdnn_f*100;
    pnn= pnn_f*100;
    rmsd=rmssd_f*100;

    hrv_array[0]= meanval;
    hrv_array[1]= meanval>>8;
    hrv_array[2]= meanval>>16;
    hrv_array[3]= meanval>>24;
    hrv_array[4]= sdnn;
    hrv_array[5]= sdnn>>8;
    hrv_array[6]= pnn;
    hrv_array[7]= pnn>>8;
    hrv_array[10]=rmsd;
    hrv_array[11]=rmsd>>8;
    hrv_array[12]=respirationrate;
    hrv_ready_flag= true;
  }

}

int HRVMAX(unsigned int array[])
{  

  for(int i=0;i<MAX;i++)
  {

    if(array[i]>max_t)
    {
      max_t = array[i];
    }

  }

  return max_t;
}

int HRVMIN(unsigned int array[])
{   
  min_t = max_f;

  for(int i=0;i<MAX;i++)
  {

    if(array[i]< min_t)
    {
      min_t = array[i]; 
    }

  }

  return min_t;
}

float mean(unsigned int array[])
{ 
  int sum = 0;
  float mean_rr;
 
  for(int i=0;i<(MAX);i++)
  {
    sum = sum + array[i];
  }
  mean_rr = (((float)sum)/ MAX);  
  return mean_rr;
} 

float sdnn_ff(unsigned int array[])
{
  int sumsdnn = 0;
  int diff;
 
  for(int i=0;i<(MAX);i++)
  {
    diff = (array[i]-(mean_f));
    diff = diff*diff;
    sumsdnn = sumsdnn + diff;   
  }

  sdnn = (sqrt(sumsdnn/(MAX)));
  return   sdnn;
}

float pnn_ff(unsigned int array[])
{ 
  unsigned int pnn50[MAX];
  count = 0;
  sqsum = 0;

  for(int i=0;i<(MAX-2);i++)
  {
    pnn50[i]= abs(array[i+1] - array[i]);
    sqsum = sqsum + (pnn50[i]*pnn50[i]);

    if(pnn50[i]>50)
    {
      count = count + 1;    
    }

  }
  per_pnn = ((float)count/MAX)*100;
  return per_pnn;
}

float rmssd_ff(unsigned int array[])
{
  unsigned int pnn50[MAX];
  sqsum = 0;

  for(int i=0;i<(MAX-2);i++)
  {
    pnn50[i]= abs(array[i+1] - array[i]);
    sqsum = sqsum + (pnn50[i]*pnn50[i]);
  }

  rmssd = sqrt(sqsum/(MAX-1));
  return rmssd;
}

//Led_indications
void ble_advertising()
{

  while((deviceConnected==false)&&(slide_switch_flag==false)&&(mode_write_flag==false))
  {
    digitalWrite(A13, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(100);                       // wait for a 100ms
    digitalWrite(A13, HIGH);    // turn the LED off by making the voltage LOW
    delay(3000);
  }

}

void V3_mode_indication()
{
  digitalWrite(A13, HIGH);

  for(int dutyCycle = 0; dutyCycle <= 254; dutyCycle=dutyCycle+3)
  {   
    // changing the LED brightness with PWM
    ledcWrite(ledChannel, dutyCycle);
    delay(25);
  }
   // decrease the LED brightness
  for(int dutyCycle = 254; dutyCycle >= 0; dutyCycle=dutyCycle-3)
  {
    // changing the LED brightness with PWM
    ledcWrite(ledChannel, dutyCycle);   
    delay(25);
  }

}

void restart_indication()
{
  digitalWrite(A13, LOW);
  delay(2500);
  digitalWrite(A13, HIGH);
  delay(2500);
}

void webserver_mode_indication()
{
  // decrease the LED brightness
  for(int dutyCycle = 254; dutyCycle >= 0; dutyCycle=dutyCycle-3)
  {
    // changing the LED brightness with PWM
    ledcWrite(ledChannel, dutyCycle);   
    delay(25);
  }

  for(int dutyCycle = 0; dutyCycle <= 254; dutyCycle=dutyCycle+3)
  {   
    // changing the LED brightness with PWM
    ledcWrite(ledChannel, dutyCycle);
    delay(25);
  }

}

void soft_AP_mode_indication()
{

  for(int i=0; i<=5 ;i++)
  {
    digitalWrite(A15,HIGH);
    delay(50);
    digitalWrite(A15,LOW);
    delay(2000); 
  }

}

void OTA_update_indication()
{

  for (int i =0;i<5;i++)
  {
    digitalWrite(A13,HIGH);
    digitalWrite(A15,HIGH);
    delay(25);
    digitalWrite(A15,LOW);
    digitalWrite(A13,LOW);
    delay(25);
    digitalWrite(A13,HIGH);
  }

}

void send_data_serial_port(void)
{

  for (int i = 0; i < 5; i++)
  {
   Serial.write(DataPacketHeader[i]);     // transmit the data over USB
  }
  
  for (int i = 0; i < 20; i++)
  {
    Serial.write(DataPacket[i]);     // transmit the data over USB
  }
  
  for (int i = 0; i < 2; i++)
  {
    Serial.write(DataPacketFooter[i]);     // transmit the data over USB
  } 

}

void handle_ble_stack()
{

  if(strValue == "\0")
  {
    
    if(ecg_buf_ready)
    {
      ecg_buf_ready = false;
      datastream_Characteristic->setValue(ecg_data_buff, 18);    
      datastream_Characteristic->notify();
    }

  }
  else if(strValue =="0spo2")
  {
    
    if(ppg_buf_ready)
    {
      ppg_buf_ready = false;
      datastream_Characteristic->setValue(ppg_data_buff, 18);    
      datastream_Characteristic->notify();
    }

  }
 
  //send notifications if connected to a client
  if(global_HeartRate_prev != global_HeartRate)
  {
    global_HeartRate_prev = global_HeartRate;
    uint8_t hr_att_ble[2];
    hr_att_ble[0] = lead_flag;
    hr_att_ble[1] = (uint8_t)global_HeartRate;    
    Heartrate_Characteristic->setValue(hr_att_ble, 2);
    Heartrate_Characteristic->notify(); 
  }  
    
  if(spo2_calc_done)
  {
    // afe44xx_raw_data.buffer_count_overflow = false;
    uint8_t spo2_att_ble[5];
    spo2_att_ble[0] = 0x00;
    spo2_att_ble[1] = (uint8_t)sp02;
    spo2_att_ble[2] = (uint8_t)(sp02>>8);
    spo2_att_ble[3] = 0;
    spo2_att_ble[4] = 0;
    sp02_Characteristic->setValue(spo2_att_ble, 5);     
    sp02_Characteristic->notify();        
    spo2_calc_done = false;
  }

  if(hrv_ready_flag)
  {
    hrv_Characteristic->setValue(hrv_array, 13);
    hrv_Characteristic->notify(); 
    hrv_ready_flag = false;
  }
    
  if(temp_data_ready)
  {
    temperature_Characteristic->setValue((uint8_t *)&temperature, 2);
    temperature_Characteristic->notify();
    temp_data_ready = false;
  }
  
  if(histgrm_ready_flag )
  {
    histgrm_ready_flag = false;
    hist_Characteristic->setValue(histgrm_percent_bin, 13);
    hist_Characteristic->notify();
  }
 
  if(bat_data_ready)
  {
    battery_Characteristic->setValue((uint8_t *)&bat_percent, 1);
    battery_Characteristic->notify();
    bat_data_ready = false;
  }

  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    ble_advertising();
    oldDeviceConnected = deviceConnected;
  }
  
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
   // do stuff here on connecting
   oldDeviceConnected = deviceConnected;
  } 

}

bool loadFromSpiffs(String path)
{
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
 
  if (server.hasArg("download")) dataType = "application/octet-stream";
  
  if (server.streamFile(dataFile, dataType) != dataFile.size()) 
  {
  }
 
  dataFile.close();
  return true;
}

void HealthyPiV4_Webserver_Init()
{ 
  
  if((!SPIFFS.exists("/mode_status.txt")))
  {
    
    if (!SPIFFS.exists("/mode_status.txt"))
    {
      Serial.print("Creating "); 
      Serial.println("/mode_status.txt");
      File file = SPIFFS.open("/mode_status.txt", "w");
      file.close();
    }
    
    Serial.println("Enter Soft AP Mode");
    soft_AP_mode_indication();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(host);
    IPAddress myIP = WiFi.softAPIP();
    Serial.println(myIP);
    delay(1000); 
  }
  else
  {
    Serial.println("Required files does exist");
    readFile(SPIFFS,"/mode_status.txt",&status_size,modestatus); 
    Serial.println();
    readFile(SPIFFS,"/ssid_list.txt",&ssid_size,ssid); 
    Serial.println();
    readFile(SPIFFS,"/pass_list.txt",&pass_size,password);   
    Serial.println();
  
    if ((status_size == 0))
    {
      Serial.println("Enter Soft AP Mode");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(host);
      IPAddress myIP = WiFi.softAPIP();
      Serial.println(myIP);
      soft_AP_mode_indication();
      delay(1000);
    }
    else
    {  
      Serial.printf("Connecting to the network ");

      for (byte k=0;k<32;k=k+1)
      {
        Serial.print(ssid[k]); 
      }

      Serial.printf("\nWi-Fi mode set to WIFI_STA %s\n", WiFi.mode(WIFI_STA) ? "Success" : "Failed!"); 
      WiFi.begin(ssid,password);

      while ((WiFi.status() != WL_CONNECTED)) 
      {
        delay(1000);
        Serial.print(".");
        wifi_count++;

        if(wifi_count==61)
        {
          wifi_count=0;
          Serial.println(" ");
          Serial.println("connection failed");
          deleteFile(SPIFFS,"/mode_status.txt");
          delay(1000);
          const char w = 0x0c;
          writeFile(SPIFFS,"/web_mode.txt",&w);
          restart_indication();
          ESP.restart();  
        }

      }
      
      Serial.println("");
      Serial.println("WiFi connected.");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      STA_mode_indication = true;
    }

  }
 
  if (MDNS.begin("HealthyPi")) 
  {
    Serial.println("MDNS responder started");
  } 
  
  server.on("/",handleRoot);
  server.on("/readheartrate", handleheartrate);
  server.on("/readtemperature", handletemperature);
  server.on("/root",HTTP_GET, webpage);
  server.on("/data",HTTP_POST,response);
  server.onNotFound(handleWebRequests);
 
  server.on("/network_change", HTTP_GET, []()
  {
    server.sendHeader("Connection", "close");
    server.sendHeader("Location", "/set_network_credentials.html", true);  //Redirect to our html web page
    server.send(303, "text/plane", "");
  });
  
  server.on("/OTA_login", HTTP_GET, []()
  {
    detachInterrupt(ADS1292_DRDY_PIN);
    server.sendHeader("Connection", "close");
    server.sendHeader("Location", "/ota_login.html", true);  //Redirect to our html web page
    server.send(303, "text/plane", "");
  });

  server.on("/serverIndex", HTTP_GET, []()
  {
    server.sendHeader("Connection", "close");
    server.sendHeader("Location", "/ota_upload.html", true);  //Redirect to our html web page
    server.send(303, "text/plane", "");
  });
  
  server.on("/update", HTTP_POST, []()
  {
    OTA_update_indication();
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    restart_indication();
    ESP.restart();  }, []()
    {
    //OTA_update_indication();
    HTTPUpload& upload = server.upload();
   
    if (upload.status == UPLOAD_FILE_START)
    {
      Serial.printf("Update: %s\n", upload.filename.c_str());
    
      if (!Update.begin(UPDATE_SIZE_UNKNOWN))
      { //start with max available size
        Update.printError(Serial);
        Serial.println("unknown");
      }

    } 
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
      //flashing firmware to ESP
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      {
        Update.printError(Serial);
        Serial.println("size mismatch");
      }

    }
    else if (upload.status == UPLOAD_FILE_END)
    {
      
      if (Update.end(true))
      { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      }
      else
      {
        Update.printError(Serial);
        Serial.println("fail");
      }

    }
    else
    {
      Serial.println("Aborted");
    }

  });
  server.begin();
  Serial.println("HTTP server started");
}

void setup()
{
  delay(2000);
  Serial.begin(115200);  // Baudrate for serial communication
  Serial.println("Setting up Healthy pI V4...");
  
  // initalize the  data ready and chip select pins:
  pinMode(ADS1292_DRDY_PIN, INPUT);  
  pinMode(ADS1292_CS_PIN, OUTPUT);    
  pinMode(ADS1292_START_PIN, OUTPUT);
  pinMode(ADS1292_PWDN_PIN, OUTPUT);  
  pinMode (A15, OUTPUT);
  pinMode (A13, OUTPUT);
  pinMode (AFE4490_PWDN_PIN,OUTPUT);
  pinMode (AFE4490_CS_PIN,OUTPUT);//Slave Select
  pinMode (AFE4490_DRDY_PIN,INPUT);// data ready 
  //set up mode selection pins
  pinMode(SLIDE_SWITCH, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT);
  int buttonState = digitalRead(SLIDE_SWITCH);
  Serial.println(buttonState);

  if(!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  else
  {
    Serial.println("SPIFFS initialization completed");
  }

  if((!readFile(SPIFFS, "/v4_mode.txt"))&&(!fileread(SPIFFS, "/web_mode.txt")))
  {
    delLine(SPIFFS,"/web_mode.txt",1,5);

    if(buttonState)
    {
      restart_indication();
      Healthypi_Mode = V3_MODE;
    }
    else
    {
      restart_indication();
      Healthypi_Mode = BLE_MODE;
    }

  }
  
  pinMode(PUSH_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PUSH_BUTTON), push_button_intr_handler, FALLING);
  pinMode(SLIDE_SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SLIDE_SWITCH), slideswitch_intr_handler,CHANGE);
  
  if(Healthypi_Mode == WEBSERVER_MODE)
  {
    ledcSetup(ledChannel, freq, resolution);
    ledcAttachPin(A13, ledChannel);
    Serial.println("Starts in webserver mode");
    webserver_mode_indication();
    ledcDetachPin(A13);
    HealthyPiV4_Webserver_Init();

  }
  else if(Healthypi_Mode == V3_MODE)
  { 
    ledcSetup(ledChannel, freq, resolution);
    ledcAttachPin(A15, ledChannel);  
    V3_mode_indication();
    Serial.println("Starts in v3 mode");
    ledcDetachPin(A15);
  }
  else
  {
    Serial.println("Starts in ble mode");
    HealthyPiV4_BLE_Init();
  }

  SPI.begin();
  Wire.begin(25,22);
  SPI.setClockDivider (SPI_CLOCK_DIV16);
  SPI.setBitOrder (MSBFIRST);
  SPI.setDataMode (SPI_MODE0);
  delay(10);
  afe4490.afe44xxInit (AFE4490_CS_PIN,AFE4490_PWDN_PIN);
  delay(10); 
  SPI.setDataMode (SPI_MODE1);          //Set SPI mode as 1
  delay(10);
  ADS1292R.ads1292_Init(ADS1292_CS_PIN,ADS1292_PWDN_PIN,ADS1292_START_PIN);  //initalize ADS1292 slave
  delay(10); 
  attachInterrupt(digitalPinToInterrupt(ADS1292_DRDY_PIN),ads1292r_interrupt_handler, FALLING ); // Digital2 is attached to Data ready pin of AFE is interrupt0 in ARduino
  tempSensor.begin();
  Serial.println("Initialization is complete");
}

void loop()
{
  boolean ret = ADS1292R.getAds1292r_Data_if_Available(ADS1292_DRDY_PIN,ADS1292_CS_PIN,&ads1292r_raw_data);

  if (ret == true)
  {  
    ecg_wave_sample = (int16_t)(ads1292r_raw_data.raw_ecg >> 8) ;  // ignore the lower 8 bits out of 24bits 
    res_wave_sample = (int16_t)(ads1292r_raw_data.raw_resp>>8) ;
  
    if (!((ads1292r_raw_data.status_reg & 0x1f) == 0))
    {
      leadoff_detected  = true; 
      lead_flag = 0x04;
      ecg_filterout = 0;
      resp_filterout = 0;      
      DataPacket[14] = 0;
      DataPacket[16] = 0;
    }  
    else
    {
      leadoff_detected  = false;
      lead_flag = 0x06;
      ECG_RESPIRATION_ALGORITHM.Filter_CurrentECG_sample(&ecg_wave_sample, &ecg_filterout);   // filter out the line noise @40Hz cutoff 161 order
      ECG_RESPIRATION_ALGORITHM.Calculate_HeartRate(ecg_filterout,&global_HeartRate,&npeakflag); // calculate
      ECG_RESPIRATION_ALGORITHM.Filter_CurrentRESP_sample(res_wave_sample, &resp_filterout);
      ECG_RESPIRATION_ALGORITHM.Calculate_RespRate(resp_filterout,&global_RespirationRate);   
   
      if(npeakflag == 1)
      {
        read_send_data(global_HeartRate,global_RespirationRate);
        add_hr_histgrm(global_HeartRate);
        npeakflag = 0;
      }
   
      if(Healthypi_Mode == BLE_MODE)
      {
        ecg_data_buff[ecg_stream_cnt++] = (uint8_t)ecg_wave_sample;//ecg_filterout;
        ecg_data_buff[ecg_stream_cnt++] = (ecg_wave_sample>>8);//(ecg_filterout>>8);
      
       if(ecg_stream_cnt >=18)
       {
          ecg_buf_ready = true;
          ecg_stream_cnt = 0;
       }
       
      }
      else if(Healthypi_Mode == WEBSERVER_MODE)
      {
        
        if(index_cnt< 1200)
        {  
          static uint8_t ec = 0;
          tmp_ecgbu += String(ecg_wave_sample);
          tmp_ecgbu += String(",");
          number_of_samples++;
        }
        else
        {
          // Do Nothing
        }

      }
   
      DataPacket[14] = global_RespirationRate;
      DataPacket[16] = global_HeartRate;
    }
  
    memcpy(&DataPacket[0], &ecg_filterout, 2);
    memcpy(&DataPacket[2], &resp_filterout, 2);
    SPI.setDataMode (SPI_MODE0);
    afe4490.get_AFE4490_Data(&afe44xx_raw_data,AFE4490_CS_PIN,AFE4490_DRDY_PIN);
    ppg_wave_ir = (uint16_t)(afe44xx_raw_data.IR_data>>8);
    ppg_wave_ir = ppg_wave_ir;
    
    ppg_data_buff[ppg_stream_cnt++] = (uint8_t)ppg_wave_ir;
    ppg_data_buff[ppg_stream_cnt++] = (ppg_wave_ir>>8);
  
    if(ppg_stream_cnt >=18)
    {
      ppg_buf_ready = true;
      ppg_stream_cnt = 0;
    }
  
    memcpy(&DataPacket[4], &afe44xx_raw_data.IR_data, sizeof(signed long));
    memcpy(&DataPacket[8], &afe44xx_raw_data.RED_data, sizeof(signed long));
  
    if( afe44xx_raw_data.buffer_count_overflow)
    {
      
      if (afe44xx_raw_data.spo2 == -999)
      {
        DataPacket[15] = 0;
        sp02 = 0;
      }
      else
      { 
        DataPacket[15] =  afe44xx_raw_data.spo2;
        sp02 = (uint8_t)afe44xx_raw_data.spo2;       
      }

      spo2_calc_done = true;
      afe44xx_raw_data.buffer_count_overflow = false;
    }
   
    DataPacket[17] = 80;  //bpsys
    DataPacket[18] = 120; //bp dia
    DataPacket[19]=  ads1292r_raw_data.status_reg;  

    SPI.setDataMode (SPI_MODE1);
   
    if ((time_count++ * (1000/SAMPLING_RATE)) > MAX30205_READ_INTERVAL)
    {      
      temp = tempSensor.getTemperature()*100; // read temperature for every 100ms
      temperature =  (uint16_t) temp;
      time_count = 0;
      DataPacket[12] = (uint8_t) temperature; 
      DataPacket[13] = (uint8_t) (temperature >> 8);
      temp_data_ready = true;
      //reading the battery with same interval as temp sensor
      read_battery_value();
    }
  
    if(Healthypi_Mode == BLE_MODE)
    {
      handle_ble_stack();
    }
    else if(Healthypi_Mode == V3_MODE)
    {
      send_data_serial_port();
    }

  }

  if(mode_write_flag)
  {
    mode_write_flag = false;
    const char t = 0x0f;
    writeFile(SPIFFS, "/v4_mode.txt", &t);
    Serial.println("setting webserver mode..\n retsrts in 3 sec"); 
    delay(3000);
    restart_indication();
    ESP.restart();
  }
 
  if(slide_switch_flag)
  {
    slide_switch_flag = false;
    Serial.println("changing the mode..\n retsrts in 3 sec");
    delay(3000);
    ESP.restart();
  }
 
  if (credential_success_flag)
  {
    detachInterrupt(ADS1292_DRDY_PIN);
    writeFile(SPIFFS,"/ssid_list.txt",ssid_to_connect.c_str());
    writeFile(SPIFFS,"/pass_list.txt",password_to_connect.c_str());
    writeFile(SPIFFS,"/mode_status.txt","datawritten");
    credential_success_flag = false;
    const char u = 0x0a;
    writeFile(SPIFFS,"/web_mode.txt",&u);
    restart_indication();
    ESP.restart();   
  }
 
  if (STA_mode_indication)
  {
    
    for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle=dutyCycle-3)
    {
      // changing the LED brightness with PWM
      ledcWrite(ledChannel, dutyCycle);   
      delay(25);
    }

    STA_mode_indication = false;
  }
 
  server.handleClient();
}
