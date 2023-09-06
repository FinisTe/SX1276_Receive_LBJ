/*
   RadioLib Pager (POCSAG) Receive Example

   This example shows how to receive FSK packets without using
   SX127x packet engine.

   This example receives POCSAG messages using SX1278's
   FSK modem in direct mode.

   Other modules that can be used to receive POCSAG:
    - SX127x/RFM9x
    - RF69
    - SX1231
    - CC1101
    - Si443x/RFM2x

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/
#pragma execution_character_set("utf-8")
// include the library
#include "boards.h"
#include <RadioLib.h>
#include <WiFi.h>
#include "sntp.h"
#include "networks.h"
#include "sdlog.h"

// definitions


// variables
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
// receiving packets requires connection
// to the module direct output pin
const int pin = RADIO_BUSY_PIN;
float rssi = 0;
float fer = 0;
// create Pager client instance using the FSK module
PagerClient pager(&radio);
uint64_t timer1 = 0;
uint64_t timer2 = 0;
uint32_t ip_last = 0;
bool is_startline = true;

SD_LOG sd1(SD);

// functions

#ifdef HAS_DISPLAY
void pword(const char *msg, int xloc, int yloc) {
    int dspW = u8g2->getDisplayWidth();
    int strW = 0;
    char glyph[2];
    glyph[1] = 0;
    for (const char *ptr = msg; *ptr; *ptr++) {
        glyph[0] = *ptr;
        strW += u8g2->getStrWidth(glyph);
        ++strW;
        if (xloc + strW > dspW) {
            int sxloc = xloc;
            while (msg < ptr) {
                glyph[0] = *msg++;
                xloc += u8g2->drawStr(xloc, yloc, glyph);
            }
            strW -= xloc - sxloc;
            yloc += u8g2->getMaxCharHeight();
            xloc = 0;
        }
    }
    while (*msg) {
        glyph[0] = *msg++;
        xloc += u8g2->drawStr(xloc, yloc, glyph);
    }
}
#endif

void dualPrintf(bool time_stamp, const char* format, ...) { // Generated by ChatGPT.
    char buffer[256]; // 创建一个足够大的缓冲区来容纳格式化后的字符串
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);

    // 输出到 Serial
    Serial.print(buffer);

    // 输出到 Telnet
    if (telnet.online) { // code from Multimon-NG unixinput.c 还得是multimon-ng，chatgpt写了四五个版本都没解决。
        if (is_startline){
            telnet.print("\r> ");
                if (time_stamp && getLocalTime(&time_info,0))
                    telnet.printf("\r%d-%02d-%02d %02d:%02d:%02d > ", time_info.tm_year+1900, time_info.tm_mon+1,
                                  time_info.tm_mday,time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
            is_startline = false;
        }
        telnet.print(buffer);
        if (nullptr != strchr(buffer,'\n')) {
            is_startline = true;
            telnet.print("\r< ");
        }
    }

}

void dualPrint(const char* fmt){
    Serial.print(fmt);
    telnet.print(fmt);
}
void dualPrintln(const char* fmt){
    Serial.println(fmt);
    telnet.println(fmt);
}

void LBJTEST(){
    PagerClient::pocsag_data pocdat[8];
    pocdat[0].str = "37012  28  1503";pocdat[0].addr = 1234000;pocdat[0].func = 1;pocdat[0].is_empty = false;pocdat[0].len = 15;
    pocdat[1].str = "20202350025730U-(2 9U- (-(202011719157439058013000";pocdat[1].addr = 1234002;pocdat[1].func = 1;pocdat[1].is_empty = false;pocdat[1].len = 0;
    Serial.println("[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ ");
    dualPrintf(false,"[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ \n");
    struct lbj_data lbj;
    for (auto & i : pocdat){
        if (i.is_empty)
            continue;
        float ber = (float) i.errs_total / ( ((float)i.len/5)*32);
        dualPrintf(true,"[Pager] %d/%d: %s  [ERR %d/%d/%zu, BER %.2f%%]\n", i.addr, i.func,
                   i.str.c_str(), i.errs_uncorrected, i.errs_total,(i.len/5)*32,ber * 100);
        sd1.append("[Pager] %d/%d: %s  [ERR %d/%d/%zu, BER %.2f%%]\n", i.addr, i.func,
                   i.str.c_str(), i.errs_uncorrected, i.errs_total,(i.len/5)*32,ber * 100);
    }

    readDataLBJ(pocdat,&lbj);

    // print data in lbj format.
    if (lbj.type == 2)          // Time sync
        dualPrintf(true,"[LBJ] 当前时间 %s \n",lbj.time);
    else if (lbj.type == 1) {     // Additional
        if (lbj.direction == FUNCTION_UP)
            dualPrintf(true,"[LBJ] 方向：上行  ");
        else if (lbj.direction == FUNCTION_DOWN)
            dualPrintf(true,"[LBJ] 方向：下行  ");
        else
            dualPrintf(true,"[LBJ] 方向：%3d  ", lbj.direction);

        dualPrintf(true,"车次：%s%s  速度：%6s KM/H  公里标：%8s KM\n",lbj.lbj_class,
                   lbj.train,lbj.speed,lbj.position);

        dualPrintf(true,"[LBJ] 机车编号：%s  线路：%s  ",lbj.loco, lbj.route_utf8);

        if (lbj.pos_lat_deg[1] && lbj.pos_lat_min[1])
            dualPrintf(true,"位置：%s°%2s′ ", lbj.pos_lat_deg, lbj.pos_lat_min);
        else
            dualPrintf(true,"位置：%s ",lbj.pos_lat);
        if (lbj.pos_lon_deg[1] && lbj.pos_lon_min[1])
            dualPrintf(true,"%s°%2s′\n",lbj.pos_lon_deg,lbj.pos_lon_min);
        else
            dualPrintf(true,"%s\n",lbj.pos_lon);

    } else if (lbj.type == 0) {      // Standard
        if (lbj.direction == FUNCTION_UP)
            dualPrintf(true,"[LBJ] 方向：上行  ");
        else if (lbj.direction == FUNCTION_DOWN)
            dualPrintf(true,"[LBJ] 方向：下行  ");
        else
            dualPrintf(true,"[LBJ] 方向：%3d  ",lbj.direction);
        dualPrintf(true,"车次：%7s  速度：%6s KM/H  公里标：%8s KM\n",lbj.train,lbj.speed,lbj.position);
    }
}

// SETUP

void setup() {\
    timer2 = millis();
    initBoard();
    delay(1500);
    if(have_sd){
        sd1.begin("/LOGTEST");
    }
    // Sync time.

    sntp_set_time_sync_notification_cb( timeAvailable );
    sntp_servermode_dhcp(1);
    configTzTime(time_zone, ntpServer1, ntpServer2);

    // initialize wireless network.
    Serial.printf("Connecting to %s ",WIFI_SSID);
    connectWiFi(WIFI_SSID,WIFI_PASSWORD,1); // usually max_tries = 25.
    if (isConnected()) {
        ip = WiFi.localIP();
        Serial.println();
        Serial.print("[Telnet] "); Serial.print(ip); Serial.print(":"); Serial.println(port);
        setupTelnet(); // todo: find another library / modify the code to support multiple client connection.
    } else {
        Serial.println();
        Serial.println("Error connecting to WiFi, Telnet startup skipped.");
    }

    // initialize SX1276 with default settings
    dualPrint("[SX1276] Initializing ... ");
    int state = radio.beginFSK(434.0, 4.8, 5.0, 12.5);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success."));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    state = radio.setGain(1);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[SX1276] Gain set."));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    // initialize Pager client
    Serial.print(F("[Pager] Initializing ... "));
    // base (center) frequency:     821.2375 MHz + 0.005 FE
    // speed:                       1200 bps
    state = pager.begin(821.2375 + 0.005, 1200, false, 2500);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    // start receiving POCSAG messages
    Serial.print(F("[Pager] Starting to listen ... "));
    // address of this "pager":     12340XX
    state = pager.startReceive(pin, 1234000, 0xFFFF0);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success."));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

    digitalWrite(BOARD_LED, LOW);
    Serial.printf("Booting time %llu ms\n",millis()-timer2);
    sd1.append("启动用时 %lu ms\n",millis()-timer2);
    timer2 = 0;

    // test stuff
    // LBJTEST();
}

// LOOP

void loop() {
    if (ip_last != WiFi.localIP()){
        Serial.print("Local IP ");
        Serial.print(WiFi.localIP());
        Serial.print("\n");
    }
    ip_last = WiFi.localIP();

//    if (millis()%10000 == 0){
//        dualPrintf(true,"当前运行时间 %lu",micros());
//        dualPrintf(true," us\n");
//    }

    if (millis()-timer1 >= 100)
        digitalWrite(BOARD_LED, LOW);

    if (isConnected() && !telnet.online){
        ip = WiFi.localIP();
        Serial.printf("WIFI Connection to %s established.\n",WIFI_SSID);
        Serial.print("[Telnet] "); Serial.print(ip); Serial.print(":"); Serial.println(port);
        setupTelnet();
    }

    telnet.loop();

    if (pager.gotSyncState()) {
        if (rssi == 0)
            rssi = radio.getRSSI(false, true);
        // todo：implement packet RSSI indicator based on average RSSI value.
        if (fer == 0)
            fer = radio.getFrequencyError();
    }

    // the number of batches to wait for
    // 2 batches will usually be enough to fit short and medium messages
    if (pager.available() >= 2) {
        timer2 = millis();
        PagerClient::pocsag_data pocdat[POCDAT_SIZE];
        struct lbj_data lbj;

//        Serial.print(F("[Pager] Received pager data, decoding ... "));

        // you can read the data as an Arduino String
        String str = {};

        int state = pager.readDataMSA(pocdat,0);

        if (state == RADIOLIB_ERR_NONE) {
//            Serial.printf("success.\n");
            digitalWrite(BOARD_LED, HIGH);
            timer1 = millis();

            for (auto & i : pocdat){
                if (i.is_empty)
                    continue;
                float ber = (float) i.errs_total / ( ((float)i.len/5)*32);
                dualPrintf(true,"[Pager] %d/%d: %s  [ERR %d/%d/%zu, BER %.2f%%]\n", i.addr, i.func,
                       i.str.c_str(), i.errs_uncorrected, i.errs_total,(i.len/5)*32,ber * 100);
                sd1.append("[Pager] %d/%d: %s  [ERR %d/%d/%zu, BER %.2f%%]\n", i.addr, i.func,
                           i.str.c_str(), i.errs_uncorrected, i.errs_total,(i.len/5)*32,ber * 100);
                str = str + "  " + i.str;
            }

            readDataLBJ(pocdat,&lbj);
            // Serial.printf("type %d \n",lbj.type);

            // print data in lbj format.
            if (lbj.type == 2)          // Time sync
                dualPrintf(true,"[LBJ] 当前时间 %s \n",lbj.time);
            else if (lbj.type == 1) {     // Additional
                if (lbj.direction == FUNCTION_UP)
                    dualPrintf(true,"[LBJ] 方向：上行  ");
                else if (lbj.direction == FUNCTION_DOWN)
                    dualPrintf(true,"[LBJ] 方向：下行  ");
                else
                    dualPrintf(true,"[LBJ] 方向：%3d  ", lbj.direction);

                dualPrintf(true,"车次：%s%s  速度：%6s KM/H  公里标：%8s KM\n",lbj.lbj_class,
                           lbj.train,lbj.speed,lbj.position); // todo: 想办法显示等级的时候把车次前面的空格去了...

                dualPrintf(true,"[LBJ] 机车编号：%s  线路：%s  ",lbj.loco, lbj.route_utf8);

                if (lbj.pos_lat_deg[1] && lbj.pos_lat_min[1])
                    dualPrintf(true,"位置：%s°%2s′ ", lbj.pos_lat_deg, lbj.pos_lat_min);
                else
                    dualPrintf(true,"位置：%s ",lbj.pos_lat);
                if (lbj.pos_lon_deg[1] && lbj.pos_lon_min[1])
                    dualPrintf(true,"%s°%2s′\n",lbj.pos_lon_deg,lbj.pos_lon_min);
                else
                    dualPrintf(true,"%s\n",lbj.pos_lon);

            } else if (lbj.type == 0) {      // Standard
                if (lbj.direction == FUNCTION_UP)
                    dualPrintf(true,"[LBJ] 方向：上行  ");
                else if (lbj.direction == FUNCTION_DOWN)
                    dualPrintf(true,"[LBJ] 方向：下行  ");
                else
                    dualPrintf(true,"[LBJ] 方向：%3d  ",lbj.direction);
                dualPrintf(true,"车次：%7s  速度：%6s KM/H  公里标：%8s KM\n",lbj.train,lbj.speed,lbj.position);
            }

            switch(lbj.type){
                case 0:
                {
                    if (lbj.direction == FUNCTION_UP)
                        sd1.append("[LBJ] 方向：上行  ");
                    else if (lbj.direction == FUNCTION_DOWN)
                        sd1.append("[LBJ] 方向：下行  ");
                    else
                        sd1.append("[LBJ] 方向：%3d  ", lbj.direction);
                    sd1.append("车次：%7s  速度：%6s KM/H  公里标：%8s KM\n",lbj.train,lbj.speed,lbj.position);
                    break;
                }
                case 1:
                {
                    if (lbj.direction == FUNCTION_UP)
                        sd1.append("[LBJ] 方向：上行  ");
                    else if (lbj.direction == FUNCTION_DOWN)
                        sd1.append("[LBJ] 方向：下行  ");
                    else
                        sd1.append("[LBJ] 方向：%3d  ", lbj.direction);
                    sd1.append("车次：%s%s  速度：%6s KM/H  公里标：%8s KM\n",lbj.lbj_class,
                               lbj.train,lbj.speed,lbj.position);
                    sd1.append("[LBJ] 机车编号：%s  线路：%s  ",lbj.loco, lbj.route_utf8);
                    if (lbj.pos_lat_deg[1] && lbj.pos_lat_min[1])
                        sd1.append("位置：%s°%2s′ ", lbj.pos_lat_deg, lbj.pos_lat_min);
                    else
                        sd1.append("位置：%s ",lbj.pos_lat);
                    if (lbj.pos_lon_deg[1] && lbj.pos_lon_min[1])
                        sd1.append("%s°%2s′\n",lbj.pos_lon_deg,lbj.pos_lon_min);
                    else
                        sd1.append("%s\n",lbj.pos_lon);
                    break;
                }
                case 2:
                {
                    sd1.append("[LBJ] 当前时间 %s \n",lbj.time);
                    break;
                }
            }

            // print rssi
            dualPrintf(true,"[SX1276] RSSI:  ");
            dualPrintf(true,"%.2f",rssi);
            dualPrintf(true," dBm  ");
            sd1.append("[SX1276] RSSI: %.2f dBm    Frequency Error: %.2f Hz \n",rssi,fer);
            rssi = 0;

            // print Frequency Error
            dualPrintf(true,"Frequency Error:  ");
            dualPrintf(true,"%.2f",fer);
            dualPrintf(true," Hz\r\n");
            fer = 0;

#ifdef HAS_DISPLAY
            if (u8g2) {
                u8g2->clearBuffer();
                u8g2->drawStr(0, 12, "Received OK!");
                // u8g2->drawStr(0, 26, str.c_str());
                pword(str.c_str(), 0, 26);
                u8g2->sendBuffer();
            }
#endif
//            if (millis()-timer1 >= 100)
//                digitalWrite(BOARD_LED, LOW);

        } else if (state == RADIOLIB_ERR_MSG_CORRUPT) {
//            Serial.printf("failed.\n");
//            Serial.println("[Pager] Reception failed, too many errors.");
            dualPrintf(true,"[Pager] Reception failed, too many errors. \n");
            sd1.append("[Pager] Reception failed, too many errors. \n");
        }
        else {
            // some error occurred
            sd1.append("[Pager] Reception failed, code %d \n",state);
            dualPrintf(true,"[Pager] Reception failed, code %d \n",state);
        }
        Serial.printf("[Pager] Processing time %llu ms.\n",millis()-timer2);
        timer2 = 0;
    }
}