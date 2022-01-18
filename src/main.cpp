/* Tweliteは3台 */

#include <Arduino.h>
#include <WiFi.h>
//#include <ESP8266WiFi.h>
#include "ThingSpeak.h"


#define HEX_CH   6  //16進文字列の要素数

// Tweliteからのデータの形式
static bool use_binary = true;  // Binary -> true, Ascii -> false

// WiFi
const char* WiFi_SSID = "##WIFI_SSID_HERE##";    // WiFiのSSID
const char* WiFi_PS = "##WIFI_PASSWORD_HERE##";      // WiFiのパスワード 

// ThingSpeak
WiFiClient client;  // Create a Wi-Fi client to connect to ThingSpeak
char thingSpeakAddress[] = "api.thingspeak.com";
unsigned long channelID = 1503732;      // チャネルID
const char* writeAPIKey = "##THINGSPEAK_WRITE_API_KEY_HERE##";   // APIキー

unsigned int dataField;


// Serial
char inChar;
String inStr;
char buff[255];
int counter = 0;
unsigned long int past = 0;
int data_count;

unsigned long time_last_data_recieved = 0;
bool first_time = true;

typedef struct {
  int layer;
  float sensor;
} st_t;

static void WiFi_start();
static int serial_read_by_line(String &str);
static int split_bainary(String src, int step, String *dst);
static int split_ascii(String src, char delimiter, String *dst);
static void convert_str_data(String *src, st_t *dst, int size);
static int ThingSpeak_write(long TSChannel, st_t *data, int size);
static void twelite_command();
static void serial2_read_by_line();


void setup(){
  // Serial
  Serial.begin(115200);
  Serial2.begin(115200);

  // WiFi
  WiFi_start();
}

void loop(){
  // 受信したシリアルデータを読み込む
  String serial_data = {"\0"};    // 読み込んだデータを格納する配列
  int serial_result = serial_read_by_line(serial_data);

  if(serial_result == 1){               // シリアルデータがある場合
    // データを分割
    int data_count = 0;
    String split_data[255] = {"\0"};    // 分割したデータを格納する配列
    serial_data.trim();                 // 前後の空白を除去
    if(serial_data != "")               // データが空ではないとき
    {
      if(use_binary == true)            // バイナリ形式を使用する場合 
      {
          data_count = split_bainary(serial_data, 6, split_data);
      }
      else if(use_binary == false)      // アスキー形式を使用する場合
      {
          data_count = split_ascii(serial_data, ':', split_data);
      }
    }

    // 時間を記録
    time_last_data_recieved = millis();

    // データを数値に変換
    st_t twelite_data[data_count] = {0};
    convert_str_data(split_data, twelite_data, data_count);

    // ThingSpeakに送信
    int ThingSpeak_result = ThingSpeak_write(channelID, twelite_data, data_count);
    

    // For debug
    if(serial_result){
      Serial.println("RAW: " + serial_data);
      Serial.print("Split: ");
      for(int i=0; i<data_count; i++){
        Serial.print(split_data[i] + " ");
      }
      Serial.println();
      Serial.println("ToInt:");
      for(int i=0; i<data_count; i++){
        Serial.print("Layer:");
        Serial.print(twelite_data[i].layer);
        Serial.print(", Sensor:");
        Serial.println(twelite_data[i].sensor);
      }
      Serial.print("ThingSpeak: ");
      Serial.println(ThingSpeak_result);
    }
  }
  
  twelite_command();
  serial2_read_by_line();
}


/*
関数名: WiFi_start

機能: WiFiの初期化。ついでにThingSpeakも初期化。

引数: なし

戻り値: なし
*/
void WiFi_start(){
  static int connect_timeout = 10000;
  static int connect_retry = 3;

  Serial.print("WiFi Connecting");

  WiFi.mode(WIFI_STA);      // ステーションモードに設定

  for(int i=0; i<connect_retry; i++){
    long time_wifi_start = millis();
    WiFi.begin(WiFi_SSID, WiFi_PS);

    while( millis()-time_wifi_start < connect_timeout ) {
      Serial.print('.');
      delay(500);
      if(WiFi.status() == WL_CONNECTED){
          break;
      }
    }

    if(WiFi.status() == WL_CONNECTED){
      Serial.println("Connected!");
      ThingSpeak.begin(client);       // Start ThingSpeak
      break;
    }
    else {
      Serial.println();
      Serial.print("Retrying(");
      Serial.print(i+1);
      Serial.print(")");
    }

  }
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Timeout!");
  }
}


/* 
関数名: serial_read_by_line

機能: シリアル受信データを1行（改行コードまで）読み出す

引数: str - 読み込んだデータを書き込む変数

戻り値: シリアル受信データがある場合 -> 1,  シリアル受信データがない場合 -> 0
*/
int serial_read_by_line(String &str){
    if(Serial.available()){
        str = Serial.readStringUntil(0x0a);   // 改行コードまで読み込み
        return 1;
    }
    else{
        return 0;
    }
}


/*
関数名: split_bainary

機能: 文字列を指定の文字数ごとに分割する。

引数: 
  src - 分割したい文字列
  step - 分割ステップ数（step数ごとに分割する）
  dst - 分割後の文字列を書き込む配列

戻り値: 分割数（分割後の配列の要素数）
*/
int split_bainary(String src, int step, String *dst){
  int index = 0;
  int data_length = src.length();
  int data_count = floor(data_length / step);

  for(int i = 0; i < data_length; i++) {
    char tmp = src.charAt(i);
    dst[index] += tmp;
    if( (i+1)%step == 0 ){
      index++;
    }
  }

  return (data_count);
}


/*
関数名: split_ascii

機能: 文字列を指定の文字ごとに分割する。

引数: 
  src - 分割したい文字列
  delimiter - 分割文字（delimiterの文字ごとに分割する）
  dst - 分割後の文字列を書き込む配列

戻り値: 分割数（分割後の配列の要素数）
*/
int split_ascii(String src, char delimiter, String *dst){
  int index = 0;
  int arraySize = (sizeof(src))/sizeof((src[0]));
  int datalength = src.length();
  
  for(int i = 0; i < datalength; i++){
    char tmp = src.charAt(i);
    if( tmp == delimiter ){
      index++;
      if( index > (arraySize - 1)) return -1;
    }
    else dst[index] += tmp;
  }
  return (index + 1);
}


/*
関数名: convert_str_data

機能: 文字列を数値に変換し、構造体に代入する。

引数: 
  src - 変換したい文字列の配列（16進数表記）
  dst - 変換後の数値を書き込む構造体配列
  size - 変換する文字列の数

戻り値: なし
*/
void convert_str_data(String *src, st_t *dst, int size){
  for(int i=0; i<size; i++){
    String str_layer = src[i].substring(0,2);
    String str_sensor = src[i].substring(2);

    long lo_layer = strtol(&str_layer[0], NULL, 16);
    long lo_sensor = strtol(&str_sensor[0], NULL, 16);

    dst[i].layer = lo_layer;
    dst[i].sensor = (lo_sensor - 600) / 10.0;     // ついでに温度(℃)に変換しちゃう
  }
}


/*
関数名: ThingSpeak_write

機能: ThingSpeakにデータを送信。

引数: 
  TSChannel - ThingSpeakのチャンネルID
  data - 送信するデータの構造体配列
  size - 送信するデータの数

戻り値: 成功時 -> HTTP status code 200
*/
int ThingSpeak_write(long TSChannel, st_t *data, int size){
  for(int i=0; i<size; i++){
    unsigned int TSField = data[i].layer;
    float fieldData = data[i].sensor;
    ThingSpeak.setField( TSField, fieldData );
  }
  int writeSuccess = ThingSpeak.writeFields( TSChannel, writeAPIKey );
  return writeSuccess;
}


/*
関数名: twelite_command

機能: Tweliteにコマンドを送信。

引数: なし

戻り値: なし
*/
void twelite_command(){
  // tを送る
  if(first_time == true){
    Serial.println("Send commands: t");
    Serial2.print('t');
    first_time = false;
  }
  if((millis()-time_last_data_recieved) > 150000){
    Serial.println("Send commands: t");
    Serial2.print('t');
    time_last_data_recieved = millis();
  }
}


/*
関数名: serial2_read_by_line

機能: UART2ポートのシリアル通信を読む。テスト用に作った関数なので用無し。

引数: なし

戻り値: なし
*/
void serial2_read_by_line(){
    if(Serial2.available()){
        String str = Serial2.readStringUntil(0x0a);   // 改行コードまで読み込み
        Serial.println(str);
    }
}