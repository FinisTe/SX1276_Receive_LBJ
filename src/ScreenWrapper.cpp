//
// Created by FLN1021 on 2024/2/10.
//

#include "ScreenWrapper.h"

bool ScreenWrapper::setDisplay(DISPLAY_MODEL *display_ptr) {
    this->display = display_ptr;
    if (!this->display)
        return false;

    return true;
}

void ScreenWrapper::updateInfo() {
    if (!display || !update || !enabled)
        return;

    char buffer[32];
    // update top
    if (update_top) {
        display->setDrawColor(0);
        display->setFont(u8g2_font_squeezed_b7_tr);
        display->drawBox(0, 0, 98, 8);
        display->setDrawColor(1);
        if (!getLocalTime(&time_info, 0))
            display->drawStr(0, 7, "NO SNTP");
        else {
            sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                    time_info.tm_hour, time_info.tm_min);
            display->drawStr(0, 7, buffer);
        }
#ifdef HAS_RTC
        if (have_rtc) {
            sprintf(buffer, "%dC", (int) rtc.getTemperature());
            display->drawStr(80, 7, buffer);
        }
#endif

        // update bottom
        display->setDrawColor(0);
        display->drawBox(0, 56, 128, 8);
        display->setDrawColor(1);
        if (!no_wifi) {
            String ipa = WiFi.localIP().toString();
            display->drawStr(0, 64, ipa.c_str());
        } else
            display->drawStr(0, 64, "WIFI OFF");
    } else {
        display->setDrawColor(0);
        display->drawBox(73, 56, 56, 8);
        display->setDrawColor(1);
    }
    sprintf(buffer, "%.1f", getBias(actual_frequency));
    display->drawStr(73, 64, buffer);
    if (sd1.status() && WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "D");
    else if (sd1.status())
        display->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "N");
    sprintf(buffer, "%2d", ets_get_cpu_frequency() / 10);
    display->drawStr(96, 64, buffer);
    voltage = battery.readVoltage() * 2;
    sprintf(buffer, "%1.2f", voltage); // todo: Implement average voltage reading.
    if (voltage < 3.15 && !low_volt_warned) {
        Serial.printf("Warning! Low Voltage detected, %1.2fV\n", voltage);
        sd1.append("低压警告，电池电压%1.2fV\n", voltage);
        low_volt_warned = true;
    }
    display->drawStr(108, 64, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showInitComp() {
    if (!display)
        return;
    display->clearBuffer();
    display->setFont(u8g2_font_squeezed_b7_tr);
    // bottom (0,56,128,8)
    String ipa = WiFi.localIP().toString();
    display->drawStr(0, 64, ipa.c_str());
    if (have_sd && WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "D");
    else if (have_sd)
        display->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "N");
    char buffer[32];
    sprintf(buffer, "%2d", ets_get_cpu_frequency() / 10);
    display->drawStr(96, 64, buffer);
    sprintf(buffer, "%1.2f", battery.readVoltage() * 2);
    display->drawStr(108, 64, buffer);
    // top (0,0,128,8)
    if (!getLocalTime(&time_info, 0))
        display->drawStr(0, 7, "NO SNTP");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        display->drawStr(0, 7, buffer);
    }
    display->sendBuffer();
}

void ScreenWrapper::pword(const char *msg, int xloc, int yloc) {
    int dspW = display->getDisplayWidth();
    int strW = 0;
    char glyph[2];
    glyph[1] = 0;
    for (const char *ptr = msg; *ptr; *ptr++) {
        glyph[0] = *ptr;
        strW += display->getStrWidth(glyph);
        ++strW;
        if (xloc + strW > dspW) {
            int sxloc = xloc;
            while (msg < ptr) {
                glyph[0] = *msg++;
                xloc += display->drawStr(xloc, yloc, glyph);
            }
            strW -= xloc - sxloc;
            yloc += display->getMaxCharHeight();
            xloc = 0;
        }
    }
    while (*msg) {
        glyph[0] = *msg++;
        xloc += display->drawStr(xloc, yloc, glyph);
    }
}

void ScreenWrapper::showSTR(const String &str) {
    if (!display)
        return;
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    // display->setFont(FONT_12_GB2312);
    display->setFont(u8g2_font_squeezed_b7_tr);
    pword(str.c_str(), 0, 19);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ0(const struct lbj_data &l, const struct rx_info &r) {
    if (!display)
        return;
    // box y 9->55
    char buffer[128];
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    display->setFont(u8g2_font_wqy15_t_custom);
    if (l.direction == FUNCTION_UP) {
        sprintf(buffer, "车  次 %s 上行", l.train);
    } else if (l.direction == FUNCTION_DOWN)
        sprintf(buffer, "车  次 %s 下行", l.train);
    else
        sprintf(buffer, "车  次 %s %d", l.train, l.direction);
    display->drawUTF8(0, 21, buffer);
    sprintf(buffer, "速  度  %s KM/H", l.speed);
    display->drawUTF8(0, 37, buffer);
    sprintf(buffer, "公里标 %s KM", l.position);
    display->drawUTF8(0, 53, buffer);
    // draw RSSI
    display->setDrawColor(0);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", r.rssi);
    display->drawStr(99, 7, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ1(const struct lbj_data &l, const struct rx_info &r) {
    if (!display)
        return;
    char buffer[128];
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    display->setFont(FONT_12_GB2312);
    // line 1
    sprintf(buffer, "车:%s%s", l.lbj_class, l.train);
    display->drawUTF8(0, 19, buffer);
    sprintf(buffer, "速:%sKM/H", l.speed);
    display->drawUTF8(68, 19, buffer);
    // line 2
    sprintf(buffer, "线:%s", l.route_utf8);
    display->drawUTF8(0, 31, buffer);
    display->drawBox(67, 21, 13, 12);
    display->setDrawColor(0);
    if (l.direction == FUNCTION_UP)
        display->drawUTF8(68, 31, "上");
    else if (l.direction == FUNCTION_DOWN)
        display->drawUTF8(68, 31, "下");
    else {
        sprintf(buffer, "%d", l.direction);
        display->drawStr(71, 31, buffer);
    }
    display->setDrawColor(1);
    sprintf(buffer, "%sK", l.position);
    display->drawUTF8(86, 31, buffer);
    // line 3
    sprintf(buffer, "号:%s", l.loco);
    display->drawUTF8(0, 43, buffer);
    if (l.loco_type.length())
        display->drawUTF8(72, 43, l.loco_type.c_str());
    // line 4
    String pos;
    if (l.pos_lat_deg[1] && l.pos_lat_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lat_deg, l.pos_lat_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lat);
        pos += String(buffer);
    }
    if (l.pos_lon_deg[1] && l.pos_lon_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lon_deg, l.pos_lon_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lon);
        pos += String(buffer);
    }
//    sprintf(buffer,"%s°%s'%s°%s'",l.pos_lat_deg,l.pos_lat_min,l.pos_lon_deg,l.pos_lon_min);
    display->drawUTF8(0, 54, pos.c_str());
    // draw RSSI
    display->setDrawColor(0);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", r.rssi);
    display->drawStr(99, 7, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ2(const struct lbj_data &l, const struct rx_info &r) {
    if (!display)
        return;
    char buffer[128];
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    display->setFont(u8g2_font_wqy15_t_custom);
    sprintf(buffer, "当前时间 %s ", l.time);
    display->drawUTF8(0, 21, buffer);
    // draw RSSI
    display->setDrawColor(0);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", r.rssi);
    display->drawStr(99, 7, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ(const lbj_data &l, const rx_info &r) {
    if (!display || !update || !enabled)
        return;

    if (l.type == 0)
        showLBJ0(l, r);
    else if (l.type == 1) {
        showLBJ1(l, r);
    } else if (l.type == 2) {
        showLBJ2(l, r);
    }
}

void ScreenWrapper::showLBJ(const struct lbj_data &l, const struct rx_info &r, const String &time_str, uint16_t lines,
                            uint32_t id, float temp) {
    if (!display)
        return;
    if (update_top)
        update_top = false;

    // show msg rx time
    display->setDrawColor(0);
    display->setFont(u8g2_font_squeezed_b7_tr);
    display->drawBox(0, 0, 97, 8);
    display->setDrawColor(1);
    if (std::stoi(time_str.substring(0, 4).c_str()) < 2016)
        display->drawStr(0, 7, "NO SNTP");
    else {
        display->drawStr(0, 7, (time_str.substring(0, 16)).c_str());
    }

    char buffer[32];
    // show temp
    if (fabs(temp - 0.01) > 0.001) {
        sprintf(buffer, "%dC", (int) temp);
        display->drawStr(80, 7, buffer);
    }

    // show msg lines
    display->setDrawColor(0);
    display->drawBox(0, 56, 72, 8);
    display->setDrawColor(1);
    sprintf(buffer, "%04d,%u", lines, id);
    display->drawStr(0, 64, buffer);
    // if (!no_wifi) {
    //     String ipa = WiFi.localIP().toString();
    //     display->drawStr(0, 64, ipa.c_str());
    // } else
    //     display->drawStr(0, 64, "WIFI OFF");
    display->sendBuffer();

    if (l.type == 0)
        showLBJ0(l, r);
    else if (l.type == 1) {
        showLBJ1(l, r);
    } else if (l.type == 2) {
        showLBJ2(l, r);
    }
}

void ScreenWrapper::resumeUpdate() {
    update_top = true;
    updateSleepTimestamp();
}

void ScreenWrapper::showSelectedLBJ(aPreferences *flash_cls, int8_t bias) {
    lbj_data lbj;
    rx_info rx;
    String rx_time;
    uint16_t line;
    uint32_t id;
    float temp;
    if (flash_cls->retrieve(&lbj, &rx, &rx_time, &line, &id, &temp, bias)) {
        showLBJ(lbj, rx, rx_time, line, id, temp);
    }
}

void ScreenWrapper::showListening() {
    if (!display)
        return;
    display->setFont(FONT_12_GB2312);
    display->setDrawColor(0);
    // display->drawBox(0, 42, 128, 14);
    display->drawBox(0, 8, 128, 48);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->drawStr(0, 52, "Listening...");
    display->sendBuffer();
}

void ScreenWrapper::clearTop(top_sectors sector, bool sendBuffer) {
    // if (!display)
    //     return;
    bool set_color = false;
    if (display->getDrawColor() != 0) {
        set_color = true;
        display->setDrawColor(0);
    }
    switch (sector) {
        case TOP_SECTOR_TIME:
            display->drawBox(0, 0, 79, 8);
            break;
        case TOP_SECTOR_TEMPERATURE:
            display->drawBox(80, 0, 98, 8);
            break;
        case TOP_SECTOR_RSSI:
            display->drawBox(99, 0, 128, 8);
            break;
        case TOP_SECTOR_ALL:
            display->drawBox(0, 0, 128, 8);
            break;
    }
    if (set_color)
        display->setDrawColor(1);
    if (sendBuffer)
        display->sendBuffer();
}

void ScreenWrapper::clearCenter(bool sendBuffer) {
    bool set_color = false;
    if (display->getDrawColor() != 0) {
        set_color = true;
        display->setDrawColor(0);
    }

    display->drawBox(0, 8, 128, 48);

    if (set_color)
        display->setDrawColor(1);
    if (sendBuffer)
        display->sendBuffer();
}

void ScreenWrapper::clearBottom(bottom_sectors sector, bool sendBuffer) {
    bool set_color = false;
    if (display->getDrawColor() != 0) {
        set_color = true;
        display->setDrawColor(0);
    }
    switch (sector) {
        case BOTTOM_SECTOR_IP:
            display->drawBox(0, 56, 72, 8);
            break;
        case BOTTOM_SECTOR_PPM:
            display->drawBox(73, 56, 15, 8); // 73->88
            break;
        case BOTTOM_SECTOR_IND:
            display->drawBox(89, 56, 6, 8); // 89->95
            break;
        case BOTTOM_SECTOR_CPU:
            display->drawBox(96, 56, 11, 8); // 96->107
            break;
        case BOTTOM_SECTOR_BAT:
            display->drawBox(108, 56, 20, 8); // 108->128
            break;
        case BOTTOM_SECTOR_ALL:
            display->drawBox(0, 56, 128, 8);
            break;
    }
    if (set_color)
        display->setDrawColor(1);
    if (sendBuffer)
        display->sendBuffer();
}

void ScreenWrapper::clearAll() {
    display->clearBuffer();
}

void ScreenWrapper::setFlash(aPreferences *flash_cls) {
    flash = flash_cls;
}

void ScreenWrapper::showSelectedLBJ(int8_t bias) {
    showSelectedLBJ(flash, bias);
}

void ScreenWrapper::showInfo(int8_t page) {
    if (!display)
        return;
    if (page > 3 || page < 1)
        return;
    String tokens[28];
    flash->retrieve(tokens, sizeof tokens, 0);
    /* Standard format of cache:
     * 条目数,电压,系统时间,温度,日期,时间,type,train,direction,speed,position,time,info2_hex,loco_type,lbj_class,loco,route,
     * route_utf8,pos_lon_deg,pos_lon_min,pos_lat_deg,pos_lat_min,pos_lon,pos_lat,rssi,fer,ppm,id
     */
    clearAll();
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "接收信息");
    char buffer[34];
    sprintf(buffer, "%d", page);
    display->drawUTF8(118, 12, buffer);
    display->drawHLine(0, 14, 128);

    switch (page) {
        case 1: {
            display->drawUTF8(0, 26, ("条目: " + tokens[0] + "," + tokens[27]).c_str());
            display->drawUTF8(0, 38, ("接收日期: " + tokens[4]).c_str());
            display->drawUTF8(0, 50, ("接收时间: " + tokens[5]).c_str());
            uint64_t time = std::stoull(tokens[2].c_str());
            display->drawUTF8(0, 62, ("系统时间: " + String(time / 1000) + " ms").c_str());
            break;
        }
        case 2: {
            display->drawUTF8(0, 26, ("电压:" + tokens[1] + "V 温度:" + tokens[3] + "C").c_str());
            float fer = std::stof(tokens[25].c_str());
            sprintf(buffer, "测量频偏: %.2f Hz", fer);
            display->drawUTF8(0, 38, buffer);
            fer = std::stof(tokens[26].c_str());
            sprintf(buffer, "设定频偏: %.2f ppm", fer);
            display->drawUTF8(0, 50, buffer);
            break;
        }
        case 3: {
            if (tokens[12].length()) {
                // display->drawUTF8(0,12,("I2HEX: "+tokens[12]).c_str());
                pword(("I2HEX: " + tokens[12]).c_str(), 0, 26);
            }
            break;
        }
        default:
            break;
    }
    display->sendBuffer();
}

void ScreenWrapper::pwordUTF8(const String &msg, int xloc, int yloc, int xmax, int ymax) {
    int Width = xmax - xloc;
    int Height = ymax - yloc;
    int StrW = display->getUTF8Width(msg.c_str());
    int8_t CharHeight = display->getMaxCharHeight();
    auto lines = Height / CharHeight;
    // Serial.printf("[D] lines %d, Height %d, CharH %d\n",lines,Height,CharHeight);

    String str = msg;
    for (int i = 0, j = yloc; i <= lines; ++i, j += CharHeight) {
        auto c = str.length();
        while (StrW > Width) {
            StrW = display->getUTF8Width(str.substring(0, c).c_str());
            --c;
        }
        // Serial.printf("[D] %d, %d\n",xloc,j);
        if (c == str.length()) {
            display->drawUTF8(xloc, j, str.substring(0, c).c_str());
            break;
        }
        display->drawUTF8(xloc, j, str.substring(0, c + 1).c_str());
        str = str.substring(c - 1, str.length());
        // Serial.println("[D] " + str);
    }

    // display->sendBuffer();
}

bool ScreenWrapper::isEnabled() const {
    return enabled;
}

void ScreenWrapper::setEnable(bool is_enable) {
    // if (is_enable)
    //     display->setPowerSave(false);
    display->setPowerSave(!is_enable);
    enabled = is_enable;
}

bool ScreenWrapper::isAutoSleep() const {
    return auto_sleep;
}

void ScreenWrapper::setSleep(bool is_sleep) {
    if (!auto_sleep || !enabled)
        return;
    display->setPowerSave(is_sleep);
    sleep = is_sleep;
}

bool ScreenWrapper::isSleep() const {
    return sleep;
}

void ScreenWrapper::autoSleep() {
    // if (!update_top || !update)
    //     return;
    if (millis64() - last_operation_time > AUTO_SLEEP_TIMEOUT && !isSleep()) {
        setSleep(true);
        // updateSleepTimestamp();
    }
}

void ScreenWrapper::updateSleepTimestamp() {
    last_operation_time = millis64();
}