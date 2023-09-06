//
// Created by FLN1021 on 2023/9/2.
//

#include "networks.h"

/* ------------------------------------------------ */
ESPTelnet telnet;
IPAddress ip;
uint16_t  port = 23;

const char* time_zone = "CST-8";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

struct tm time_info{};

bool isConnected() {
    return (WiFiClass::status() == WL_CONNECTED);
}
bool connectWiFi(const char* ssid, const char* password, int max_tries, int pause) {
    int i = 0;
    WiFiClass::mode(WIFI_STA);
    WiFi.begin(ssid, password);
    do {
        delay(pause);
        Serial.print(".");
    } while (!isConnected() && i++ < max_tries);
    if(isConnected())
        Serial.print("SUCCESS.");
    else
        Serial.print("FAILED.");
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    return isConnected();
}

/* ------------------------------------------------ */

void timeAvailable(struct timeval *t){
    Serial.println("[SNTP] Got time adjustment from NTP!");
    getLocalTime(&time_info);
    Serial.println(&time_info, "[SNTP] %Y-%m-%d %H:%M:%S");
}

//void printLocalTime()
//{
//    struct tm timeinfo;
//    if(!getLocalTime(&timeinfo)){
//        Serial.println("No time available (yet)");
//        return;
//    }
//    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
//}

/* ------------------------------------------------ */

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" connected");

    telnet.println("===[ESP32 DEV MODULE TELNET SERVICE]===");
    getLocalTime(&time_info,10);
    char timeStr[20];
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d", time_info.tm_year+1900, time_info.tm_mon+1, time_info.tm_mday,
            time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    telnet.print("System time is ");
    telnet.print(timeStr);
    telnet.println("\n\rWelcome " + telnet.getIP());
    telnet.println("(Use ^] + q  to disconnect.)");
    telnet.println("=======================================");
    telnet.print("< ");
}

void onTelnetDisconnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" disconnected");
}

void onTelnetReconnect(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip) {
    Serial.print("[Telnet] ");
    Serial.print(ip);
    Serial.println(" tried to connected");
}

void onTelnetInput(String str) {
    // checks for a certain command
    if (str == "ping") {
        telnet.println("> pong");
        Serial.println("[Telnet] > pong");
        // disconnect the client
    } else if (str == "bye") {
        telnet.println("> disconnecting you...");
        telnet.disconnectClient();
    }
    telnet.print("< ");
}


/* ------------------------------------------------- */

void setupTelnet() {
    // passing on functions for various telnet events
    telnet.onConnect(onTelnetConnect);
    telnet.onConnectionAttempt(onTelnetConnectionAttempt);
    telnet.onReconnect(onTelnetReconnect);
    telnet.onDisconnect(onTelnetDisconnect);
    telnet.onInputReceived(onTelnetInput);

    Serial.print("[Telnet] ");
    if (telnet.begin(port)) {
        Serial.println("running");
    } else {
        Serial.println("error.");
    }
}

/* ------------------------------------------------- */

//bool ipChanged(uint16_t interval, uint64_t *timer, uint32_t *last_ip) {
//    uint32_t ip1 = WiFi.localIP();
//    if (millis()-timer >= interval){
//        if (WiFi.localIP() != ip1){
//            return true;
//        }
//    }
//    return false;
//}

/* ------------------------------------------------- */

int16_t readDataLBJ(struct PagerClient::pocsag_data *p, struct lbj_data *l){
    for (size_t i=0;i<POCDAT_SIZE;i++){
        if (p[i].is_empty)
            continue;
        switch (p[i].addr) {
            case LBJ_INFO_ADDR:
            {
                if (!p[i].func && ( p[i].str[0] == '-' || p[i].str[0] == '*' ) &&
                    (p[i].str[1]&&p[i].str[2]&&p[i].str[3]&&p[i].str[4] != '-')){

                    p[i].addr = LBJ_SYNC_ADDR; // don't know if addr = 1234008 can perform this.
                    Serial.println("Transformed type.");

                } else {
                    l->type = 0;
                    l->direction = (int8_t)p[i].func;

                    /*
                     * The standard LBJ Proximity Alarm info is formed in:
                     * ----- --- -----
                     * TRAIN SPD POS
                     * 0-4   6-8 10-14 in total of 15 nibbles / 60 bits.
                     * 'X' indicates BCH correction failed code words.
                     */
                    if (p[i].str.length() >= 5 && p[i].str[0] != 'X') {
                        for (size_t c = 0; c < 5; c++) {
                            l->train[c] = p[i].str[c];
                        }
                    }
                    if (p[i].str.length() >= 10 && p[i].str[6] != 'X') {
                        for (size_t c = 6, v = 0; c < 9; c++, v++) {
                            l->speed[v] = p[i].str[c];
                        }
                        l->speed[3] = 0;
                    }
                    if (p[i].str.length() >= 15 && p[i].str[10] != 'X') {
                        size_t v = 0;
                        for (size_t c = 10; c < 15; c++, v++) {
                            l->position[v] = p[i].str[c];
                            if(c == 13 && p[i].str[c]!=' '){
                                l->position[++v]='.';
                            } else if (c == 13) {
                                // 0.x km is better than .x km, isn't it?
                                l->position[v] = '0';
                                l->position[++v]='.';
                            }
                        }
                        l->position[v] = 0;
                    }
                    /* there are times we got this kind of message:
                     * XXXXX --- -----XXXXX20201100600430U-(2 9U- (-(2020----------XXXXX--000
                     * we have to process them as type 1.
                     */
                    if (!(p[i].str.length() == 65 || p[i].str.length() >= 70 )){
                        break;
                    }
                }
            }
            case LBJ_INFO2_ADDR:
            {
                l->type = 1; // type = 1 when implemented.
                if (p[i].addr == LBJ_INFO_ADDR)
                {
                    if (p[i].str.length() == 65)
                        p[i].str = p[i].str.substring(15);
                    else if (p[i].str.length() == 70 || (p[i].str.length() >= 70 && p[i].str[67] == '0'
                            && p[i].str[68] == '0' && p[i].str[69] == '0'))
                        p[i].str = p[i].str.substring(20);
                    else
                        break;
                }
                /*
                 * The LBJ Additional info message does not appear on any standards,
                 * decoding based purely on guess and formerly received type 1 messages.
                 * A typical type 1 message is:
                 * 204U2390093130U-(2 9U- (-(202011720927939053465000
                 * |204U2|39009|3130U|-(2 9|U- (-|(2020|11720|92793|90534|65000|
                 *   0-4   5-9  10-14 15-19 20-24 25-29 30-34 35-39 40-44 45-49
                 * in which we phrase it to:
                 * |204U|23900931|30|U-(2 9U- (-(2020|117209279|39053465|000
                 *    I     II   III       IV            V         VI    VII
                 * I.   00-03   Two ASCII bytes for class, in this case 4U = 4B = K.
                 * II.  04-11   8 nibbles/32 bits for locomotive register number.
                 * III. 12-13   Unknown, usually 30, maybe some sort of sync word.
                 * IV.  14-29   4 GBK characters/8 bytes/16 nibbles/32 bits for route.
                 * V.   30-38   9 nibbles for longitude in format XXX°XX.XXXX′ E.
                 * VI.  39-46   8 nibbles for latitude in format XX°XX.XXXX′ N.
                 * VII. 47-49   Unknown, usually 000, sometimes FFF("(((")/CCC("   ")?, maybe some sort of idle word.
                 * in total of 50 nibbles / 200 bits.
                 */

                // locomotive registry number.
                if (p[i].str.length() >= 12 && p[i].str[4] != 'X' && p[i].str[5] != 'X' && p[i].str[10] != 'X') {
                    for (size_t c = 4, v = 0; c < 12; c++, v++) {
                        l->loco[v] = p[i].str[c];
                    }
                }

                // positions lon
                if (p[i].str.length() >= 39 && p[i].str[30] != 'X' && p[i].str[35] != 'X'){
                    for (size_t c = 30,v = 0;c<39;c++,v++){
                        l->pos_lon[v] = p[i].str[c];
                    }
                    for (size_t c = 30,v = 0;c<33;c++,v++){
                        l->pos_lon_deg[v] = p[i].str[c];
                    }
                    size_t v = 0;
                    for (size_t c = 33;c<39;c++,v++){
                        l->pos_lon_min[v] = p[i].str[c];
                        if (c == 34)
                            l->pos_lon_min[++v] = '.';
                    }
                }
                // position lat
                if (p[i].str.length() >= 47 && p[i].str[39] != 'X' && p[i].str[40] != 'X' && p[i].str[45] != 'X'){
                    for (size_t c = 39,v = 0;c<47;c++,v++){
                        l->pos_lat[v] = p[i].str[c];
                    }
                    for (size_t c = 39,v = 0;c<41;c++,v++){
                        l->pos_lat_deg[v] = p[i].str[c];
                    }
                    size_t v = 0;
                    for (size_t c = 41;c<47;c++,v++){
                        l->pos_lat_min[v] = p[i].str[c];
                        if (c == 42)
                            l->pos_lat_min[++v] = '.';
                    }
                }

                // reformat to hexadecimal string.
                for (char & c : p[i].str){
                    recodeBCD(&c,&l->info2_hex);
                }

                // BCD to HEX and to ASCII for class
                if (l->info2_hex.length() >= 4 && l->info2_hex[0] != 'X') {
                    // this is very likely the most ugly code I've ever write, I apologize for that.
                    size_t c = 0;
                    for (size_t v = 0; v < 3; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        l->lbj_class[c] = (int8_t) ((a << 4) | b);
                    }
                }
                // to GBK for route.
                if (l->info2_hex.length() >= 20 && l->info2_hex[14] != 'X' && l->info2_hex[15] != 'X') { // Character 1
                    size_t c = 0;
                    for (size_t v = 14; v < 17; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        l->route[c] = (int8_t) ((a << 4) | b);
                    }
                }
                if (l->info2_hex.length() >= 25 && l->info2_hex[18] != 'X' && l->info2_hex[20] != 'X') {// Character 2
                    size_t c = 2;
                    for (size_t v = 18; v < 21; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        l->route[c] = (int8_t) ((a << 4) | b);
                    }
                }
                if (l->info2_hex.length() >= 30 && l->info2_hex[22] != 'X' && l->info2_hex[25] != 'X') {// Character 3,4
                    size_t c = 4;
                    for (size_t v = 22; v < 29; v++, c++) {
                        int a = isdigit(l->info2_hex[v]) ? (l->info2_hex[v] - '0') : (l->info2_hex[v] - '0' - 7),
                                b = isdigit(l->info2_hex[v + 1]) ? (l->info2_hex[++v] - '0') : (l->info2_hex[++v] - '0' - 7);
                        l->route[c] = (int8_t) ((a << 4) | b);
                    }
                }
                gbk2utf8(l->route,l->route_utf8,17);
                break;
            }
            case LBJ_SYNC_ADDR:
            {
                l->type = 2;
                if (p[i].str.length() == 5 && p[i].str[0] != 'X') {
                    for (size_t c = 1, v = 0; c < 5; c++, v++){
                        l->time[v] = p[i].str[c];
                        if(c == 2)
                            l->time[++v]=':';
                    }
                }
                break;
            }
        }
    }

    return 0;
}

void recodeBCD( const char* c, String* v){
    switch (*c) {
        case '*':
        {
            *v += 'A';
            break;
        }
        case 'U':
        {
            *v += 'B';
            break;
        }
        case ' ':
        {
            *v += 'C';
            break;
        }
        case '-':
        {
            *v += 'D';
            break;
        }
        case ')':
        {
            *v += 'E';
            break;
        }
        case '(':
        {
            *v += 'F';
            break;
        }
        default:
        {
            *v += *c;
            break;
        }
            // 最好能改成单次转换...这样未免有点太浪费性能了
    }
}

/*---------------------------------------------------------*/

int enc_unicode_to_utf8_one(unsigned long unic,unsigned char *pOutput)
{
    assert(pOutput != nullptr);

    if (unic <= 0x0000007F)
    {
        // * U-00000000 - U-0000007F:  0xxxxxxx
        *pOutput     = (unic & 0x7F);
        return 1;
    }
    else if (unic >= 0x00000080 && unic <= 0x000007FF)
    {
        // * U-00000080 - U-000007FF:  110xxxxx 10xxxxxx
        *(pOutput + 1) = (unic & 0x3F) | 0x80;
        *pOutput     = ((unic >> 6) & 0x1F) | 0xC0;
        return 2;
    }
    else if (unic >= 0x00000800 && unic <= 0x0000FFFF)
    {
        // * U-00000800 - U-0000FFFF:  1110xxxx 10xxxxxx 10xxxxxx
        *(pOutput + 2) = (unic & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >>  6) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 12) & 0x0F) | 0xE0;
        return 3;
    }
    else if (unic >= 0x00010000 && unic <= 0x001FFFFF)
    {
        // * U-00010000 - U-001FFFFF:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 3) = (unic & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >>  6) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 12) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 18) & 0x07) | 0xF0;
        return 4;
    }
    else if (unic >= 0x00200000 && unic <= 0x03FFFFFF)
    {
        // * U-00200000 - U-03FFFFFF:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 4) = (unic & 0x3F) | 0x80;
        *(pOutput + 3) = ((unic >>  6) & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 18) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 24) & 0x03) | 0xF8;
        return 5;
    }
    else if (unic >= 0x04000000 && unic <= 0x7FFFFFFF)
    {
        // * U-04000000 - U-7FFFFFFF:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        *(pOutput + 5) = (unic & 0x3F) | 0x80;
        *(pOutput + 4) = ((unic >>  6) & 0x3F) | 0x80;
        *(pOutput + 3) = ((unic >> 12) & 0x3F) | 0x80;
        *(pOutput + 2) = ((unic >> 18) & 0x3F) | 0x80;
        *(pOutput + 1) = ((unic >> 24) & 0x3F) | 0x80;
        *pOutput     = ((unic >> 30) & 0x01) | 0xFC;
        return 6;
    }

    return 0;
}

void gbk2utf8(const uint8_t *gbk,uint8_t *utf8,size_t gbk_len){
    uint16_t unic[gbk_len];
    size_t c=0;
    for (size_t i=0;i<gbk_len;i++,c++){
        if (gbk[i] < 0x80){
            unic[c] = gbk[i];
        } else {
            unic[c] = ff_oem2uni((uint16_t) (gbk[i] << 8 | gbk[i + 1]), 936);
            i++;
        }
    }
    printf("%zu:%x/", c,unic[c-1]);
    c=0;
    size_t i=0;
    for (;i<gbk_len*2;i++,c++){
        uint8_t ut8[4];
        int r = enc_unicode_to_utf8_one(unic[c],ut8);
        if (i+4<gbk_len*2) {
            if (r == 1) utf8[i] = ut8[0];
            else if (r == 2) utf8[i] = ut8[0], utf8[++i] = ut8[1];
            else if (r == 3) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2];
            else if (r == 4) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2], utf8[++i] = ut8[3];
        }
    }
}

void gbk2utf8(const char *gbk1,char *utf8s,size_t gbk_len){
    uint16_t unic[gbk_len];
    uint8_t gbk[gbk_len];

    size_t c=0;
    for (size_t i=0;i<gbk_len;i++){
        gbk[i]=(uint8_t)gbk1[i];
    }
    for (size_t i=0;i<gbk_len;i++,c++){
        if (gbk[i] < 0x80){
            unic[c] = gbk[i];
        } else {
            unic[c] = ff_oem2uni((uint16_t) (gbk[i] << 8 | gbk[i + 1]), 936);
            i++;
        }
    }
    c=0;
    size_t i=0;
    uint8_t utf8[gbk_len*2];
    for (;i<gbk_len*2;i++,c++){
        uint8_t ut8[4];
        int r = enc_unicode_to_utf8_one(unic[c],ut8);
        if (i+4<gbk_len*2) {
            if (r == 1) utf8[i] = ut8[0];
            else if (r == 2) utf8[i] = ut8[0], utf8[++i] = ut8[1];
            else if (r == 3) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2];
            else if (r == 4) utf8[i] = ut8[0], utf8[++i] = ut8[1], utf8[++i] = ut8[2], utf8[++i] = ut8[3];
        }
    }
    for (size_t v=0;v<gbk_len*2;v++){
        utf8s[v]=(char)utf8[v];
    }
}






