#include <FirebaseArduino.h>
#include <SPI.h>
#include "FS.h"
#include "mngMem.h"
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "conver_hex.h"
#include "oled_0_96.h"
#include "eeprom_man.h"
#include "command_st95.h"
#include "ChuongThongBao.h"
#include <WiFiClientSecure.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include "images.h"

ESP8266WiFiMulti WiFiMulti;
WiFiClient client;
HTTPClient http;  
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
ESP8266WebServer server(80);
/*Bien Thoi Gian*/
String timeStamp;
String dayStamp;
String currentDay = "";
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
int setHours = 0;
int setSec = 0;
int setMin = 0;
int setDay = 0;
int setDat = 0;
int setMon = 0;
int setYear = 0;
String Date = "";
String formattedDate = "";
/*********************************/
String ver = "Ver 2.0";
String macAdd = "";
String textUID = "";
String tempUID = "";
int PORT = 0;
/* CR95HF HEADER command definition ---------------------------------------------- */
uint8_t TagID[8];
uint8_t UID[16];
int Type[1];
/*Bien kiem tra nut bam*/
int checkbutton = 0;
/*======================Dia chi de luu gia tri trong bo nho EEPROM========================*/
const int EEaddress = 0;
const int EEaddress_2 = 8;
const int EEaddress_3 = 12;
const int EEaddress_4 = 16;
/*Bien kiem tra tin hieu mang Wifi*/
long rssi = 0;
int bars = 0;
/*Variable Send API*/
String linkPingInternet = "http://testcodeesp8266.000webhostapp.com/"; //Link kiem tra xem co ket noi Internet hay khong
/*================================================================================*/
/*Function de update code qua Mang Internet OTA*/
void UpdateCode()
{
  if ((WiFiMulti.run() == WL_CONNECTED)) 
  {
    Oled_print(60,20,"Updating");  
    ESP.getFreeSketchSpace();
    if(ESPhttpUpdate.update(client,"http://testcodeesp8266.000webhostapp.com/test3.bin") != HTTP_UPDATE_OK){
    }else{
      Oled_print(65,20,"Failure");
    }
  }
}
int ESP_POST(String phoneNumber, String message)
{
  int httpResponseCode = 0;
  http.begin("http://api.conek.vn/SendSms");  
  http.addHeader("Content-Type","application/json");
  http.addHeader("Accept","application/json");
  String httpRequestData = "{\"id\": \"1\",\"username\": \"autocaretest\",\"password\": \"autocaretest\",\"brandname\": \"CONEK\",\"phone\": \""+phoneNumber+"\",\"message\": \""+message+"\"}";
  httpResponseCode = http.POST(httpRequestData);
  String payLoad = http.getString();
  if(payLoad.indexOf("Success") > 0 && httpResponseCode == 200)
  {
    Serial.println("Gui thanh cong");   
    Oled_print(65,20,"Success");
  }
  else
  {
    Serial.println("Gui khong thanh cong");  
    Oled_print(62,20,"Sorry,Failed");
  }
  http.end();
  return httpResponseCode;
}
/*===================Khoi tao module doc de no hoat dong=================*/
void Init_Module_Reader(){
    SPI.begin();
    SPI.setDataMode (SPI_MODE0);
    SPI.setBitOrder (MSBFIRST);
    SPI.setFrequency (4000000);
    pinMode (SS_Pin, OUTPUT);
    digitalWrite (SS_Pin, HIGH);     
    WakeUp_TinZ();
}
/*===================Function de doc ma UID cua Tag NFC========================== */
String ReadTagNFC(){
  uint8_t count;
  String IdTag = "";
  if(CR95HF_ping()){
    if (setprotocol_tagtype5()){
      if (getID_Tag(TagID) == true ){
        TagID[0] = 0x00;
        if (encode8byte_big_edian (TagID, UID) == 0){
          for (count = 0; count < 8 * 2; count++){
            UID[count] += 0x30;
            IdTag += char(UID[count]);
          }
        }
      }
    }
  }
  return IdTag;
}
/*===============Function kiem tra gia tri tra ve Sau khi gui len clound===============*/
int CheckDataResponse(String InputDataResponse){
  if(InputDataResponse.indexOf('1') > 0 && InputDataResponse.length() < 10){
    return 1;
  }else if(InputDataResponse.indexOf('0') > 0 && InputDataResponse.length() < 10){
    return 2;
  }
  else{
    return 3;
  }
}
/*Function nhan chuoi gom SSID, PASS va PORT tu dien thoai thong qua Bluetooth*/
String GetSsidPassWifi(){
  String textData;
  while (1) {
    if(Serial.available() > 0){
      char ch = Serial.read(); //doc 
      if(ch >= 33 && ch <= 126){
        textData += ch; 
        if(ch == ')'){
          break;
        }
      }
      /*Ket thuc cua chuoi la ky tu ')', neu thiet bi nhan duoc ky tu nay thi Break Loop*/
      delay(5); // Thoi gian tre de lay chuoi la 3s
    }
 }  
 return textData; // Tra lai chuoi nhan duoc
}
/*Function gui Du lieu len may tinh thong qua giao thuc UDP*/
static void UDP_send_data(String DataNeedSend, int gatePort)
{
  /*Gui goi tin di*/
  ntpUDP.beginPacket("255.255.255.255",gatePort);
  ntpUDP.write(DataNeedSend.c_str(), DataNeedSend.length());
  //ntpUDP.print(DataNeedSend);
  ntpUDP.endPacket();
}

/*Function Lay Wifi, phan tich SSID, Pass and PORT de truyen du lieu voi may tinh */
void GetWifi(){
  String ssid = "";
  String pass = "";
  int toData = 0;
  int fromData = 0;
  String textWifi;
  String textPORT = "";
  drawImageDemo(34,14,60,36,WiFi_Logo_bits);
  Serial.begin (9600);
  WiFi.disconnect();
  delay(1000);
  Serial.flush();
  /*Lay chuoi SSID, PASS and PORT from Dien thoai thong qua Bluetooth*/
  textWifi = GetSsidPassWifi();
  /*Phan tich chuoi vua nhan xong thanh SSID, PASS and PORT*/
  fromData = textWifi.indexOf(",",toData);
  pass = textWifi.substring(toData,fromData);
  toData = textWifi.indexOf(",",fromData + 1);
  ssid = textWifi.substring(fromData + 1,toData);     
  int a = textWifi.indexOf(")",toData + 1);
  textPORT = textWifi.substring(toData + 1,a);
  PORT = textPORT.toInt();
  /*Luu PORT vao trong bo nho EEPROM cua Chip ESP8266*/
  EEPROMWrite(EEaddress,PORT);
  /*Start ket noi voi Wifi moi*/
  //WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  /*Sau khi bat dau ket noi voi mot Wifi moi, Thong bao va Restart sau 2s*/
  Oled_print(65,20,"Restart");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  /*Reset thiet bi*/
  ESP.reset();
}

/*Function button*/
void FunctionButton()
{
  int checkbutton = 0;
  if(digitalRead(3) == HIGH){
  }else{
    do{
      checkbutton++;
      digitalWrite(15, LOW);
      delay(100);
      digitalWrite(15, HIGH);  
      delay(400);    
    }while(digitalRead(3) == LOW);        
  }
  if(checkbutton >= 1 && checkbutton <= 3){
    checkbutton = 0;
    //Oled_print(62,20,WiFi.localIP().toString());
    Oled_print(65,20,ver);
    delay(1000);
  }  
  if(checkbutton >= 3 && checkbutton <= 6){
    checkbutton = 0;
    GetWifi();
    delay(1000);
  }
  if(checkbutton > 6){
    checkbutton = 0;
    UpdateCode();
    delay(1000);
  }    
}
void LayThoiGian(){
  while(!timeClient.update()) 
  {
     yield();
     timeClient.forceUpdate();
  }
  setHours = timeClient.getHours();
  setSec = timeClient.getSeconds();
  setMin = timeClient.getMinutes();   
  setDay = timeClient.getDay();
   
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  Date = formattedDate.substring(0, splitT);
  int splitPhay = Date.indexOf("-");
  setYear = Date.substring(0, splitPhay).toInt();
  int splitPhay2 = Date.indexOf("-",splitPhay + 1); 
  setMon = Date.substring(splitPhay + 1, splitPhay2).toInt();
  setDat = Date.substring(splitPhay2 + 1).toInt();  
  
  int abcT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, abcT);
  currentDay = daysOfTheWeek[timeClient.getDay()];
  currentDay += String(" ");
  currentDay += dayStamp;                
  String ipadd = WiFi.localIP().toString();                             
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
 
}
/***********************************/
void setup()
{
  /*Khoi tao bo nho EEPROM*/
  EEPROM.begin(32);
  /*Khoi tao Coi*/
  pinMode(15,OUTPUT);
  digitalWrite(15, HIGH); 
  /*Khoi tao giao tiep cong COM*/
  Serial.begin (9600);
  /*Khoi tao man hinh*/
  Init_Oled();
  /*Lay gia tri cua PORT*/
  //Neu gia tri < 0 hoac > 9999 thi Toi ham de Setup Wifi va PORT
  //Neu gia tri thoa man > 0 va <= 9999 thi se lay luon
  if(EEPROMRead(EEaddress) < 0 || EEPROMRead(EEaddress) > 9999){
    GetWifi();
  }else{
    PORT = EEPROMRead(EEaddress);
  }  
  /*Chao hoi*/
  Oled_print(33,20,"Welcome");
  delay(1000);
  /*Man hinh ket noi Wifi*/
  drawImageDemo(34,14,60,36,WiFi_Logo_bits);
  delay(500);
  /*Ket noi Wifi*/
  //WiFi.mode(WIFI_STA);
  //WiFi.setAutoConnect(true);
  //Serial.println(WiFi.SSID());
  //Serial.println(WiFi.psk());
  WiFi.begin();
  /*Man hinh hien thi toc do ket noi*/
  int counter = 1;
  int goalValue = 0;
  int currentLoad = 0;
  while (WiFi.status() != WL_CONNECTED) {
    goalValue += 8;
    if(goalValue < 85){
      drawProgressBarDemo(currentLoad,counter, goalValue);
    }else{
      GetWifi();
      break;
    }
    delay(2000);
  }
  /*Open Port de truyen du lieu len may tinh*/
  ntpUDP.begin(PORT);
  /*Lay dia chi Mac cua thiet bi*/
  macAdd = WiFi.macAddress();
  /*Khoi tao Module de doc Tag*/
  Init_Module_Reader();
  /*Hien thi tin hieu song Wifi*/
  rssi = WiFi.RSSI();
  bars = getBarsSignal(rssi);
  Main_Screen(Barca_Logo_bits,bars,currentDay,timeStamp);
  /*Khoi dong nut bam*/
  pinMode(3,INPUT);
  timeClient.setTimeOffset(3600 * 7);
  timeClient.begin();

  Firebase.begin("cloud-nfc-proj.firebaseio.com");
  while(Firebase.getString("Conek/DanhSachNhanVien/0601435065410621/SDT/") != "0356435101"){
    Firebase.begin("cloud-nfc-proj.firebaseio.com");
    delay(500);  
  }
}
void loop() {
  /*Lay trang thai cua nut bam*/
  //Neu bam nut thi Setup lai Wifi va PORT cho thiet bi
  FunctionButton();
  LayThoiGian();
  /*Lay tin hieu song Wifi de hien thi nen man hinh chinh*/
  rssi = WiFi.RSSI();
  bars = getBarsSignal(rssi);
  Main_Screen(Barca_Logo_bits,bars,currentDay,timeStamp);
  /*Doc gia tri Tag*/
  textUID = ReadTagNFC();
  //Co gia tri Tag thi xu ly
  if(textUID != ""/* && tempUID != textUID*/){
    tempUID = textUID;
    Oled_print(65,20,"Sending");
    String data = textUID +","+ dayStamp +","+ timeStamp +",";
    Serial.println(data);
    
    //Serial.println(data);
    //UDP_send_data(textUID,PORT);
    //Serial.println(ESP_POST());
    //Firebase.pushString("/annguyen",data);
    //String nodeNeedGet = "/getData/"+textUID;//"/getData"+"/"+textUID;
    //String nodeNeedGet = "/DanhSachNhanVien/01/BirthDay/";//"/getData"+"/"+textUID;
    //String nodePostData = "/DuLieuDiemDanh";//"/getData"+"/"+textUID;
    String nodeNeedGet = "Conek/DanhSachNhanVien/"+textUID+"/Name/";
    String nodePostData = "Conek/DuLieuDiemDanh/"+textUID+"/"+dayStamp;
    UDP_send_data(textUID,PORT);
GuiDuLieuDiemDanh:    
    delay(100);
    if(setHours < 12 && setHours >= 8){
      int soPhutMuon = setHours*60 + setMin - 8*60 - 30;
      Firebase.pushString(nodePostData,timeStamp+","+soPhutMuon);
    }else if(setHours >= 13 && setHours < 18){
      int soPhutMuon = setHours*60 + setMin - 13*60 - 30;
      Firebase.pushString(nodePostData,timeStamp+","+soPhutMuon);
    }else{
      Firebase.pushString(nodePostData,timeStamp+",0");
    }
    
    delay(500);
    if(Firebase.failed())
    {
      goto GuiDuLieuDiemDanh;
      //Oled_print(62,20,"Sorry,Failed");
    }
    else
    {
      Oled_print(62,20,"Success");
      //Serial.println(Firebase.getString(nodeNeedGet));
      ChuongBaoThanhCong();
        //Serial.println(Firebase.getString(nodeNeedGet));
//      String phoneNumberGet = Firebase.getString(nodeNeedGet);
//      Serial.println(phoneNumberGet);
//      
//      if(Firebase.failed())
//      {
//        Oled_print(62,20,"Sorry,Failed");  
//      }
//      else
//      {
//        ESP_POST(phoneNumberGet,timeStamp);
//      }
    }
    
    //Serial.println(ESP_POST(Firebase.getString(nodeNeedGet),timeStamp));
    
    //Serial.println(Firebase.getString(nodeNeedGet));
                                                                                                                                                                            
    textUID = "";
    delay(1500);
  }
//  if(textUID != "" && tempUID == textUID){
//    Oled_print(60,20,"Ban da cham the");
//    delay(1000);
//  }        
}
