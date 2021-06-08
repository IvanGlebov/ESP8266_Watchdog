#include <Arduino.h>
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include <EEPROM.h>

#define USE_LOCAL_SERVER false

#define master_pin D8
#define master_reboot_pin D1

#define check_delay 20 // Задержка проверки мастера в секундах

// Локальный ключ.
// char auth[] = "qD_9-u6CLaII9eHjmrR_JJw4OZimugzQ";
// Боевой ключ
char auth[] = "9-KQWyfz0hoNqhtdCl7_o4ukSJyE6Byr";

char ssid_prod[] = "Farm_router";    // prod
char ssid_local[] = "Keenetic-4926"; // home

char pass_prod[] = "zqecxwrv123"; // prod
char pass_local[] = "Q4WmFQTa";   // home

int reboot_counter;
long master_check;
bool down_flag = false;
bool watch_after_master = false;

// bool first_start_flag = true;
WidgetTerminal terminal(V0);
WidgetRTC rtc;

// Пин для терминала - V0
// Пин дл таблички - V86
// Пин для сброса таблички - V87

enum timeShowModes
{
  timestamp = 1,
  hms
};
class logger
{
private:
  char workMode = 'M';
  char messageType = 'S';
  int messageNumber = 0;
  bool sendToTerminal = true;
  bool sendToTable = true;
  bool showLogs = true;
  long time = 0;
  int timeShowMode = hms;

public:
  logger(char workmode, char messagetype, bool sendtoterminal, bool showlogs) : workMode(workmode), messageType(messagetype), messageNumber(0), sendToTerminal(sendtoterminal), showLogs(showlogs){};
  void setLogsState(bool state, int logType);
  void setMode(char mode) { workMode = mode; }
  void setType(char type) { messageType = type; }
  void print(String text);
  void println(String text);
  void setTimestamp(long timestamp) { time = timestamp; }
  void setTimeShowMode(int mode) { timeShowMode = (mode == timestamp) ? 1 : 2; }
};

void logger::println(String text)
{
  String output;
  String timeStr = (timeShowMode != hms) ? String(time) : String(time / 3600) + ":" + String(time % 3600 / 60) + ":" + String(time % 60);
  output += "<" + String(timeStr) + "> " + "[" + String(workMode) + String(messageType) + "_" + String(messageNumber) + "] ";
  output += text;
  if (showLogs)
  {
    if (sendToTerminal == true)
    {
      terminal.println(output);
      terminal.flush();
    }
    if (sendToTable == true)
    {
      Blynk.virtualWrite(V86, "add", messageNumber, text, timeStr);
    }
    Serial.println(output);
  }
  messageNumber++;
}

void logger::print(String text)
{
  String output;
  String timeStr = (timeShowMode != hms) ? String(time) : String(time / 3600) + ":" + String(time % 3600 / 60) + ":" + String(time % 60);
  output += "<" + String(timeStr) + "> " + "[" + String(workMode) + String(messageType) + "_" + String(messageNumber) + "] ";
  output += text;
  if (showLogs)
  {
    if (sendToTerminal == true)
    {
      terminal.print(output);
      terminal.flush();
    }
    Serial.print(output);
  }
  messageNumber++;
}

// Сброс таблицы логов
BLYNK_WRITE(V87)
{
  int a = param.asInt();
  if (a == 1)
  {
    Blynk.virtualWrite(V86, "clr");
  }
}

logger logg('M', 'S', true, true);

void reboot_master();

BLYNK_CONNECTED()
{
  rtc.begin();
}
// Количество перезагрузок
BLYNK_WRITE(V1)
{
  int a = param.asInt();
  reboot_counter = a;
}
// Перезапустить мастера
BLYNK_WRITE(V2)
{
  int a = param.asInt();
  if (a == 1)
  {
    long curr_time = hour() * 3600 + minute() * 60 + second();
    logg.setTimestamp(curr_time);
    logg.setMode('M');
    logg.setType('S');
    logg.println("Rebooting master");
    reboot_master();
  }
}
// Сбросить перезапуски
BLYNK_WRITE(V3)
{
  int a = param.asInt();
  if (a == 1)
  {
    long curr_time = hour() * 3600 + minute() * 60 + second();
    logg.setTimestamp(curr_time);
    logg.setMode('M');
    logg.println("Master reboots dropped");
  }
}
// Флаг слежения за мастером
BLYNK_WRITE(V4)
{
  int a = param.asInt();
  if (a == 1)
  {
    long curr_time = hour() * 3600 + minute() * 60 + second();
    logg.setTimestamp(curr_time);
    logg.setMode('M');
    logg.println("Now I'm watching after master");
    watch_after_master = true;
  }
  else
  {
    long curr_time = hour() * 3600 + minute() * 60 + second();
    logg.setTimestamp(curr_time);
    logg.setMode('M');
    logg.println("Master is free now");
    watch_after_master = false;
  }
}

void setup()
{
  EEPROM.begin(5);
  pinMode(master_pin, INPUT);
  pinMode(master_reboot_pin, OUTPUT);
  digitalWrite(master_reboot_pin, LOW);
  reboot_counter = EEPROM.read(1);
  // Показывать как обычное вермя. Если нужна метка, то режим timestamp
  logg.setTimeShowMode(hms);
  if (USE_LOCAL_SERVER)
  {
    Blynk.begin(auth, ssid_local, pass_local, IPAddress(192, 168, 1, 106), 8080);
  }
  else
  {
    Blynk.begin(auth, ssid_prod, pass_prod, IPAddress(10, 1, 92, 35), 8080);
  }
}

void loop()
{
  Blynk.run();
  if ((20 < (hour()*3600 + minute()*60 + second())) && ((hour()*3600 + minute()*60 + second()) < 30))
  {
    reboot_master();
  }
  // Если надо следить за мастером, то бдим
  // if (watch_after_master)
  // {
  //   long curr_time = hour() * 3600 + minute() * 60 + second();
  //   // Проверить жив ли мастер
  //   if (digitalRead(master_pin) == HIGH)
  //   {
  //     down_flag = false;
  //   }
  //   // Если мастер упал и это произошло только сейчас то добавим ребут и ждём оживления n секунд,
  //   // если мастер всё ещё мёртв, то добавляем в счётчик ребута ещё 1 и посылаем сигнал ребута
  //   if (digitalRead(master_pin) == LOW && down_flag == false)
  //   {
  //     master_check = curr_time + check_delay;
  //     reboot_counter++;
  //     down_flag = true;
  //     logg.setTimestamp(curr_time);
  //     logg.setMode('A');
  //     logg.setType('E');
  //     logg.println("Master is down!");
  //   }

  //   if (curr_time - 2 < master_check && master_check < curr_time + 2)
  //   {
  //     reboot_counter++;
  //     logg.setTimestamp(curr_time);
  //     logg.setMode('A');
  //     logg.setType('E');
  //     logg.println("Master forced reboot!");
  //     reboot_master();
  //     delay(500);
  //     if (digitalRead(master_pin) == HIGH)
  //     {
  //       down_flag = false;
  //     }
  //   }
  // }
}

void reboot_master()
{
  digitalWrite(master_reboot_pin, HIGH);
  delay(100);
  // digitalWrite(master_reboot_pin, );
  digitalWrite(master_reboot_pin, LOW);
  reboot_counter++;
  Blynk.virtualWrite(V1, reboot_counter);
  EEPROM.write(1, reboot_counter);
  EEPROM.commit();
}