/*
 * Forked from: https://github.com/joeybab3/M5Stack-NTP-Clock by @PaulskPt
 * The README.md says: You need this library tho so get installing: https://github.com/taranais/NTPClient
 * I already had an NTPClient library installed. I had to delete that one because it did not have the function: getFormattedDate()
 * Modifications in this fork:
 * a) use of <M5Cardputer.h> and <M5Unified.h>;
 * b) moved date/time handling from loop() to dt_handler();
 * c) added functionality to retrieve WiFi SECRET_SSID, SECRET_PASSWORD, DEBUG_FLAG, LOCAL_TIME_FLAG, NTP_LOCAL_FLAG, NTP_LOCAL_URL from SD card, file: '/secrets.h'
 * d) added flag 'show_wifi_creds' to show or hide the WiFi credentials when retrieving this data from the file on SD card.
 * e) set/clear global flags: my_debug, local_time and ntp_local from retrieved flags
 * f) added functionality for button presses of BtnA (in the M5Cardputer this is the "GO"-button on the back right)
 * g) added function to update date/time of built-in RTC from NTP server on internet. 
 * h) added functionality to use BtnA to force re-synchronization of the built-in RTC from a NTP server.
 * i) in loop() added a elapsed time calculation.
 * j) in loop() added a call to dt_handler() every 5 minutes with flag lRefresh true to force re-synchronization of the built-in RTC from a NTP server.
 * k) in dt_handler() added functionality for a 24 hour clock instead of 12 hour with am/pm
 * l) in dt_handler() added functionality to display the day-of-the-week (using the added global char array daysOfTheWeek[7][12]) 
 * m) added function disp_msg() to display informative/warning or error messages
 * n) added a function disp_frame() which builds the static texts on the display
 * o) in dt_handler() added functionality to update only the most frequently changed data: the time. The date and day-of-the-week will only updated when necessary.
 * The measures in r) and s) make the appearance of the display more 'quiet'.
 *
 * Update 2024-09-07. Trial to make it work with a M5 Cardputer with external RV3028 (Pimoroni) 
 * M5 Cardputer display 240 x 135 px
 * This sketch has been tested on a M5Cardputer. It works OK (the sketch still has some minor bugs),
 */
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include "NTPClient.h"
#include <WiFiUdp.h>
// #include <M5Core2.h>
#include <M5Cardputer.h>
#include <Wire.h>
#include <M5GFX.h>

boolean my_debug = NULL;
boolean local_time = NULL;
boolean time_only = NULL;

FILE myFile;

#ifndef USE_PSK_SECRETS
#define USE_PSK_SECRETS (1)   // Comment-out when defining the credentials inside this sketch
#endif

#ifdef USE_PSK_SECRETS
#include "secrets.h"
#endif

#ifdef USE_PSK_SECRETS
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[]            = SECRET_SSID;     // Network SSID (name)
char password[]        = SECRET_PASS;     // Network password (use for WPA, or use as key for WEP)
char debug_flag[]      = DEBUG_FLAG;      // General debug print flag
char local_time_flag[] = LOCAL_TIME_FLAG; // Defines local_time if true, else gmt
char ntp_local_flag[]  = NTP_LOCAL_FLAG;  // Defines to use local NTP server or world pool server
char ntp_local_url[]   = NTP_LOCAL_URL;   // URL of local NTP server pool
char time_only_flag[]  = TIME_ONLY_FLAG;  // Defines if only time will be shown or all date time
char use_12hr_flag[]   = USE_12HR_FLAG;   // defines 12/24hr handling
#else
char ssid[]            = "SSID";         // your network SSID (name)
char password[]        = "password"; // your network password
char debug_flag[]      = "0";
char local_time_flag[] = "0";
char ntp_local_flag[]  = "0";
char ntp_local_url[]   = "pt.pool.ntp.org";
char time_only_flag[]  = "1"; 
char use_12hr_flag[]   = "1";
#endif

boolean use_12hr = false;  // default: 24hr time

boolean ntp_local = false; // default use worldwide ntp pool
char ntp_url[]= "";
//char* ntp_url = "";

#define Port_A_SCL_pin 1
#define Port_A_SDA_pin 2

#define TF_card_CS_pin 12
#define TF_card_MOSI_pin 14
#define TF_card_CLK_pin 40
#define TF_card_MISO_pin 39

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String fDate;
String fDateOld;
String sDay;
String sTime;
boolean btnA_state = false;
boolean lRefresh = false;
boolean show_wifi_creds = false;
int dw = 0;  // display width
int dh = 0;  // display height
int title_h = 0; // title horizontal space from left side display

#define NTP_OFFSET  +3600              // for Europe/Lisbon       was: -28798 // In seconds for los angeles/san francisco time zone
#define NTP_INTERVAL 60 * 1000         // In miliseconds
#define NTP_ADDRESS  "pool.ntp.org"

char ntp_global_url[] = "pool.ntp.org";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

#define TFT_GREY 0x5AEB
boolean lStart = true;
boolean lStop = false;

String TITLE =  "NTP synchronized clock";
unsigned int colour = 0;
int hori[] = {0, 80, 120, 200};
int vert[] = {20, 45, 70, 95, 120};
unsigned long t_start = millis();

String sDate8_old = "";

boolean vars_fm_sd = false; // flag: read variables from file secret.h on SD-card 
// ============================ SETUP =================================================
void setup(void) 
{
  boolean SD_present = false;

  auto cfg = M5.config();
  cfg.output_power = true; 
  cfg.external_rtc  = false;  // default=false.

  M5Cardputer.begin(cfg, true);
  M5Cardputer.Power.begin();
  dw = M5Cardputer.Display.width();
  dh = M5Cardputer.Display.height();

  int le = sizeof(TITLE)/sizeof(TITLE[0]);
  title_h = (dh - le) / 4;


  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
  M5Cardputer.Display.setTextColor(GREEN);
  M5Cardputer.Display.setTextDatum(middle_center);
  M5Cardputer.Display.setTextFont(&fonts::FreeSerifBoldItalic18pt7b);
  M5Cardputer.Display.setTextSize(1);
  //M5Cardputer.Display.drawString("Press a Key",
  //  dw / 2,
  //  dh / 2);

  Serial.begin(115200);
  Serial.println("\n");
  Serial.println(TITLE);
  Serial.print("display width: ");
  Serial.println(dw);
  Serial.print("display height: ");
  Serial.println(dh);

  Wire.begin();  // Just to get rid of an error caused in C:\Users\pauls2\AppData\Local\Temp\arduino\sketches\...\libraries\M5GFX\lgfx\v1\platforms\esp32\common.cpp.o
  
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(TF_card_CS_pin))
  {
    Serial.println(F("initialization failed")); 
    // Use WiFi-credentials linked-in at build process
    // while(true) 
    // { 
    //   delay(3000); // infinite loop
    //}
  }
  else
  {
    SD_present = true;
    Serial.println(F("initialization done."));
  }

  if (SD_present && vars_fm_sd)
  {
    Serial.println("SD card present.\nGoing to read variables.");
    // Read WiFi credentials from SD Card
    if (rdSecrets(SD,"/secrets.h"))
    {
      Serial.print(F("All needed values successfully read from file \""));
      Serial.print("/secrets.h");
      Serial.println("\"");
    }
    else
    {
      Serial.print("None or some values read from file \"");
      Serial.println("/secrets.h");
    }
  }
  else
  {
    Serial.println("Not using SD card or no SD card present.\nWe use variables from built-in secrets.h");
    my_debug = debug_flag[0] == 1 ? true : false;
    local_time = local_time_flag[0] == 1 ? true : false;
    ntp_local = ntp_local_flag[0] == 1 ? true : false;
  }
  
  if (my_debug)
  {
    Serial.print("Value of my_debug flag = ");
    Serial.println(my_debug == 1 ? "true" : "false");
    Serial.print("Value of time_only flag = ");
    Serial.println(time_only == 1 ? "true" : "false");
    Serial.print("Value of local time_flag = ");
    Serial.println(local_time == 1 ? "true" : "false");
    Serial.print("Value of ntp_local flag = ");
    Serial.println(ntp_local == 1 ? "true" : "false");
    Serial.print("ntp_local_url = ");
    Serial.println(ntp_local_url);
    delay(1000);
  }

  if (ntp_local)
  {
    int le = sizeof(ntp_local_url)/sizeof(ntp_local_url[0]);
    if (le > 0)
    {
      Serial.print("le= ");
      Serial.println(le);
      for (int i = 0; i < le; i++)
      {
        ntp_url[i] = ntp_local_url[i];
      }
    }
  }
  else
  {
    int le = sizeof(ntp_global_url)/sizeof(ntp_global_url[0]);
    if (le > 0)
    {
      if (my_debug)
      {
        Serial.print("le= ");
        Serial.println(le);
      }
      for (int i = 0; i < le; i++)
      {
        ntp_url[i] = ntp_global_url[i];
      }
    }
  }
  
  Serial.print("for NTPClient we will use url: \"");
  Serial.print(ntp_url);
  Serial.println("\"");

  timeClient.chg_url(ntp_url);  // Adjust the NTP pool URL
  
  if (!my_debug)
  {
    Serial.print("Using NTP Pool server: \"");
    Serial.print(ntp_url[0]);
    Serial.println("\"");
    Serial.print("NTP timezone offset from GMT: ");
    if (NTP_OFFSET > 0)
      Serial.print("+");
    Serial.print(NTP_OFFSET);
    Serial.println(" seconds\n");
  }
  //----------------------------------------------
  M5.begin();

  int textsize = 1; //dh / 60;
  if (textsize == 0) {
    textsize = 1;
  }    
  M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
  M5Cardputer.Display.setTextSize(textsize);
  M5Cardputer.Display.clear();
  M5Cardputer.Display.setTextColor(YELLOW);
  disp_title();
  //----------------------------------------------
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //delay(3000);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  char wcon[] = "WiFi connected";
  IPAddress ip; 
  ip = WiFi.localIP();
  Serial.println(wcon);
  Serial.print("IP address: ");
  Serial.println(ip);
  Serial.println();

  M5Cardputer.Display.fillRect(0, vert[0]+10, dw-1, dh-1, BLACK); // wipe part of display
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.setCursor(title_h, vert[1]);
  M5Cardputer.Display.print(wcon);
    M5Cardputer.Display.setCursor(title_h, vert[2]);
  M5Cardputer.Display.print(ip);
  delay(3000);
  timeClient.begin();
  // ========== TEST ===============
  if (my_debug)
  {
    Serial.println("NATO time-zone\'s test");
    String ofs, tz = "";
    for (int i = 0; i < 4; i++)
    {
      if (i == 0)
        ofs = "-10";
      else if (i == 1)
        ofs = "0";
      else if (i == 2)
        ofs = "+12";
      else if (i == 3)
        ofs = "-14"; // outside range, shout result in "?"
      tz = timeClient.tz_nato(ofs);
      Serial.print("offset = ");
      Serial.print(ofs);
      Serial.print(", NATO time-zone = ");
      Serial.println(tz);
    }
    Serial.println("End-of-test");
  }
  // ======= END-OF-TEST ===========
  if (!time_only)
    disp_frame();
}

// ====================================== END OF SETUP =================================================


boolean rdSecrets(fs::FS &fs, const char * path)
{
  Serial.print("rdSecrets() reading from file: \"");
  Serial.print(path);
  Serial.println("\"");
  String s2 = "";
  String sSrch = "";
  String line = "";
  int le = 0;
  int idx = -1;
  int n = -1;
  int fnd = 0;
  boolean lFnd, txt_shown = false;
  const char item_lst[][16] = {"SECRET_SSID", "SECRET_PASS", "DEBUG_FLAG", "LOCAL_TIME_FLAG", "NTP_LOCAL_FLAG", "NTP_LOCAL_URL", "TIME_ONLY_FLAG", "USE_12HR_FLAG"};
  le = sizeof(item_lst)/sizeof(item_lst[0]);
  
  File file = fs.open(path);
  if (!file) {
    Serial.print("rdSecrets() Failed to open file: ");
    Serial.print(path);
    Serial.println(" for reading");
    return false;
  }

  while (file.available())
  {
    try 
    {
      if (my_debug)
      {
        Serial.print("rdSecrets(): nr of items to retrieve: ");
        Serial.println(le);
      }
      for (uint8_t i = 0; i < le; i++)
      {
        sSrch = item_lst[i];
        if (file.available())
        { 
          txt_shown = false;
          //while (true)
          //{
            line = file.readStringUntil('\n'); // was: ('\r\n');
            delay(100);
            if (my_debug)
            {
              Serial.print(" rdSecrets() line= ");
              Serial.println(line);
            }
            if (!txt_shown && my_debug)  // shown text only once
            {
              //txt_shown = true;
              Serial.print("searching string: ");
              Serial.println(sSrch);
            }
            idx = line.indexOf(sSrch);
            if (my_debug)
            {
              Serial.print(" rdSecrets() idx= ");
              Serial.println(idx);
            }
            if (idx> 0)
            {
              fnd++;  // increase items found counter
              if (my_debug)
              {
                Serial.print("   items extracted from secrets.h: ");
                Serial.println(fnd);
              }

              s2 = extract_item(line, idx);
              if (my_debug)
              {
                Serial.print("s2 = ");
                Serial.println(s2);
              }
              if (s2.length() > 0)
              {
                if (i == 0)
                {
                  s2.toCharArray(ssid,s2.length()-1);
                  if (my_debug)
                  {
                    Serial.print("extracted ssid: ");
                    Serial.println(ssid);
                  }
                }
                else if (i == 1)
                {
                  s2.toCharArray(password,s2.length()-1);
                  if (my_debug)
                  {
                    Serial.print("extracted password ");
                    Serial.println(password);
                  }
                }
                else if (i == 2)
                {
                  s2.toCharArray(debug_flag,s2.length()-1);
                  my_debug = debug_flag[0] == 1 ? true : false;
                  if (my_debug)
                  {
                    Serial.print("extracted debug_flag: ");
                    Serial.println(debug_flag[0]);
                  }
                }
                else if (i == 3)
                {
                  s2.toCharArray(local_time_flag,s2.length()-1);
                  local_time = local_time_flag[0] == 1 ? true : false;
                  if (my_debug)
                  {
                    Serial.print("extracted local time flag ");
                    Serial.println(local_time_flag[0]);
                  }
                }
                else if (i == 4)
                {
                  s2.toCharArray(ntp_local_flag,s2.length()-1);
                  ntp_local = ntp_local_flag[0] == 1 ? true : false;
                  if (my_debug)
                  {
                    Serial.print("extracted ntp_local_flag: ");
                    Serial.println(ntp_local_flag[0]);
                  }
                }
                else if (i == 5)
                {
                  s2.toCharArray(ntp_local_url,s2.length()-1);
                  if (my_debug)
                  {
                    Serial.print("extracted ntp_local_url: ");
                    Serial.println(ntp_local_url);
                  }
                }
                else if (i == 6)
                {
                  s2.toCharArray(time_only_flag,s2.length()-1);
                  time_only = time_only_flag[0] == 1 ? true : false;
                  if (my_debug)
                  {
                    Serial.print("extracted time_only_flag: ");
                    Serial.println(time_only_flag[0]);
                    Serial.print("time_only = ");
                    Serial.println(time_only);
                  }
                }
                else if (i == 7)
                {
                  s2.toCharArray(use_12hr_flag,s2.length()-1);
                  use_12hr = use_12hr_flag[0] == 1 ? true : false;
                  if (my_debug)
                  {
                    Serial.print("extracted use_12hr_flag: ");
                    Serial.println(use_12hr_flag[0]);
                    Serial.print("use_12hr = ");
                    Serial.println(use_12hr);
                  }
                }
                txt_shown = false;
                //break; // break out of while loop
              }
            }
            else
            {
              Serial.print("rdSecrets() search item ");
              Serial.print(sSrch);
              Serial.print(" not found in line: ");
              Serial.println(line);
            }
          //}
        }
        else
        {
          if (my_debug)
            Serial.println("rdSecrets() file_available() = false. Exiting function.");
          throw 308;  // indicate linenr where file.available revealed a false
        }
      }
    }
    catch(int e)
    {
      Serial.print("End of file reached in line: ");
      Serial.println(e);
    }
  }
  file.close();

  if (fnd == le)
    return true;  // Indicate all items were found and handled
  return false;
}

String extract_item(String itm, int idx)
{
  String sRet = "";
  int le = itm.length();
  int n = 0;

  n = itm.indexOf('\"');  // search of a Double quote mark
  if (n == 0)
    n = itm.indexOf('\''); // search for a Single quote mark

  if (n > 0)
    sRet = itm.substring(n+1, le);

  return sRet;
}

void disp_frame()
{
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
    M5Cardputer.Display.fillScreen(BLACK);
    disp_title();
    M5Cardputer.Display.setCursor(0, vert[1]+5);
    M5Cardputer.Display.print("Day: ");
    M5Cardputer.Display.setCursor(0, vert[2]+5);
    M5Cardputer.Display.print("Date: ");
    M5Cardputer.Display.setCursor(0, vert[3]+5);
    M5Cardputer.Display.print("Time: ");
    M5Cardputer.Display.setCursor(0, vert[4]+5);
    M5Cardputer.Display.print("Time-Zone: ");
}

void disp_msg(String msg, int linenr)
{
  if (linenr == 1)
  {
      clr_disp_part();
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
      M5Cardputer.Display.setCursor(0, vert[2]-10);
      M5Cardputer.Display.print(msg);
  }
  if (linenr == 2)
  {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
    M5Cardputer.Display.setCursor(0, vert[3]-10);
    M5Cardputer.Display.print(msg);
  }
  Serial.println(msg);
  delay(2000);
}


void dt_handler(boolean lRefr)
{
  int hour, minute, second = 0;
  int year, month, day, weekDay = 0;
  int height_date = vert[3]-vert[1];
  int height_time = vert[4]-vert[3]+5;
  String TAG = "dt_handler(): ";
  String sDate, sDay, sMonth, sYear = "";
  String wd, sYearSmall, sTime, sHour, sMinute, sSecond = "";
  String hrs,mins,secs = "";
  String tz_ltr = "?";
  boolean use_local_time = local_time_flag;

  lRefr = true;  // force NTP update

  if (lRefr)  // Refresh (from NTP Server)
  {
    timeClient.update();
    fDate = timeClient.getFormattedDate(0, use_local_time);
    fDateOld = fDate;
  }
  else {
    fDate = fDateOld;
  }
  Serial.print("DateTime sync from NTP server: ");
  Serial.println(fDate);
  Serial.println();

  int splitT = fDate.indexOf("T");
  
  sDate      = fDate.substring(0, splitT);
  sYear      = fDate.substring(0, 4);
  sYearSmall = fDate.substring(2, 4);
  sMonth     = fDate.substring(splitT-5, splitT-3);      
  sDay       = fDate.substring(splitT-2, splitT);

  sTime      = fDate.substring(splitT+1, fDate.length()-1);
  sHour      = sTime.substring(0,2);
  sMinute    = sTime.substring(3,5);
  sSecond    = sTime.substring(6,8);
  weekDay    = timeClient.getDay();
  wd         = daysOfTheWeek[weekDay];
  tz_ltr     = timeClient.tz_nato(String(NTP_OFFSET/3600));

  if (!my_debug)
  {
    Serial.print(TAG);
    Serial.print("timeClient.getFormattedDate() = \"");
    Serial.print(fDate);
    Serial.println("\"\n");
  }
  
  year   = sYear.toInt();
  month  = sMonth.toInt();
  day    = sDay.toInt();
  hour   = sHour.toInt();
  minute = sMinute.toInt();
  second = sSecond.toInt();

  sDay       = day < 10 ? "0" + String(day) : String(day);
  sMonth     = month < 10 ? "0" + String(month) : String(month);
  sYear      = String(year);
  sDate      = String(year)+"-"+sMonth+"-"+sDay;
  sYearSmall = sYear.substring(2, 4);
  sHour      = hour   < 10 ? "0" + String(hour)   : String(hour);
  sMinute    = minute < 10 ? "0" + String(minute) : String(minute);
  sSecond    = second < 10 ? "0" + String(second) : String(second);
  
  boolean isPm = false;

  if (use_12hr)
  {
    if(hour > 12)
    {
      hour = hour - 12;
      isPm = true;
    }
    else
    {
      isPm = false;
    }
  }
  
  if(hour < 10)
  {
    sHour = "0"+(String)hour;
  }
  else
  {
    sHour = (String)hour;
  }

  int daysLeft = 0;
  int monthsLeft = 0;

  sTime = sHour + ":" + sMinute + ":" + sSecond;  // hh:mm:ss
  String sDate8 = sYearSmall + "-" + sMonth + "-" + sDay;  // yy-mo-dd

  if (my_debug)
  {
    Serial.print("sDate8: ");
    Serial.println(sDate8);
    Serial.print("sTime:  ");
    Serial.print(sTime);
    Serial.println("\n");
  }
  M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
  M5Cardputer.Display.setTextSize(1);
  
  if (use_12hr)
  {
    if (!time_only) 
    {
      if (sDate8_old != sDate8)
      {
        sDate8_old = sDate8;
        M5Cardputer.Display.fillRect(hori[1], vert[1], dw-5, height_date, BLACK);  // wipe out the variable date text 
        M5Cardputer.Display.setCursor(hori[1], vert[1]+5);
        M5Cardputer.Display.println(wd);
        M5Cardputer.Display.setCursor(hori[1], vert[2]+5);
        M5Cardputer.Display.println(sDate8);
      }
      M5Cardputer.Display.fillRect(hori[1], vert[3]-10, dw-5, height_time, BLACK); // wipe out the variable time text
      M5Cardputer.Display.setCursor(hori[1], vert[3]+5);
      M5Cardputer.Display.println(sTime);
      M5Cardputer.Display.setCursor(hori[1], vert[4]+3);
      M5Cardputer.Display.println(tz_ltr);

      if(isPm)
      {
        M5Cardputer.Display.setCursor(hori[3], 80);
        M5Cardputer.Display.println("PM");
      }
      else
      {
        M5Cardputer.Display.setCursor(hori[3], 60);
        M5Cardputer.Display.println("AM");
      }
    }
    else 
    {
      disp_time(sTime, isPm);
    }
  }
  else  // 24 hr clock
  {
    if (!time_only)
    {
      if (sDate8_old != sDate8)
      {
        sDate8_old = sDate8;
        M5Cardputer.Display.fillRect(hori[2], vert[1], dw-hori[2], height_date, BLACK);  // wipe out the variable date text
        M5Cardputer.Display.setCursor(hori[2], vert[1]+5);
        M5Cardputer.Display.println(wd);                  // day of the week
        M5Cardputer.Display.setCursor(hori[2], vert[2]+5);
        M5Cardputer.Display.print(sDate8);  // yy-mo-dd
      }
      M5Cardputer.Display.setCursor(hori[2], vert[3]+5);
      M5Cardputer.Display.fillRect(hori[2], vert[3]-10, dw-hori[2], height_time, BLACK); // wipe out the variable time text
      M5Cardputer.Display.setCursor(hori[2], vert[3]+5);
      M5Cardputer.Display.println(sTime);
      M5Cardputer.Display.setCursor(hori[2], vert[4]+3);
      M5Cardputer.Display.println(tz_ltr);
      M5Cardputer.Display.println();   
      M5Cardputer.Display.setTextSize(2);
    }
    else 
    {
      disp_time(sTime, isPm);
    }
  }
  delay(950);
}

void disp_title(void)
{

  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(title_h, vert[0]-5);
  M5Cardputer.Display.print(TITLE);
}

void disp_time(String sTs, boolean isPm)
{
  int le = sizeof(sTs)/sizeof(sTs[0]);
  int h = (dw - le) / 4;
  M5Cardputer.Display.fillRect(0, vert[0]+10, dw-1, dh-1, BLACK); // wipe out the variable time text
  M5Cardputer.Display.setTextColor(YELLOW);
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setCursor(h, vert[2]);

  if (use_12hr)
  {
    M5Cardputer.Display.println(sTs);
    M5Cardputer.Display.setCursor(h, vert[3]+5);
      M5Cardputer.Display.setTextSize(1);
    if (isPm == true)
      M5Cardputer.Display.println("PM");
    else
      M5Cardputer.Display.println("AM");
  }
  else
  {
    M5Cardputer.Display.println(sTs);
  }
}

void clr_disp_part(void)
{
  M5Cardputer.Display.fillRect(0, vert[0]+10, dw-1, dh-1, BLACK); // clear display except title
}

void buttonA_wasPressed()
{
  btnA_state = true;
  disp_msg("Button A was pressed.", 1);
  disp_msg("Refresh RTC from NTP", 2);
  lRefresh = true;
 } 


void loop() 
{
  unsigned long t_current = millis();
  unsigned long t_elapsed = 0;

  M5Cardputer.Display.clear();
  disp_title();

  while (true)
  {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      if (M5Cardputer.Keyboard.isPressed())
      {
        clr_disp_part();
        M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
        M5Cardputer.Display.drawString("a key was Pressed",
          dw / 2,
          dh / 2);
        delay(3000);
        clr_disp_part();
      } 
      else 
      {
        clr_disp_part();
        M5Cardputer.Display.setTextFont(&fonts::FreeSerif9pt7b);
        disp_title();
        M5Cardputer.Display.drawString("Press a Key",
          dw / 2,
          dh / 2);
        delay(3000);
        clr_disp_part();
      }
    }
    
    if(M5Cardputer.BtnA.wasPressed()) 
    {
      M5Cardputer.Speaker.tone(8000, 20);
      buttonA_wasPressed();
    }
   
    t_current = millis();
    t_elapsed = t_current - t_start;


    if (btnA_state)
    {
      if (btnA_state)
        btnA_state = false;
        
      clr_disp_part();
      sDate8_old = ""; // trick to force re-displaying of: a) day-of-the-week; b) yy-mo-dd (the time is alway displayed!)
      if (!time_only)
        disp_frame();
    }

    if (t_elapsed % 1000 == 0)  // 1000 milliseconds = 1 minute
    {
      t_start = t_current;
      lRefresh = true;
    }
    
    if (lStart || lRefresh)
    {
      dt_handler(true);
      lStart = false;
      lRefresh = false;
    }
    else
      dt_handler(false);
  }
  // Infinite loop
  while(true){
    delay(3000);
  }
}
