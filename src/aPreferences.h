//
// Created by FLN1021 on 2024/2/8.
//

#ifndef PAGER_RECEIVE_APREFERENCES_H
#define PAGER_RECEIVE_APREFERENCES_H

#include "networks.hpp"
#include <Preferences.h>
#include <nvs_flash.h>

const int PREF_MAX_LINES = 2500;

class aPreferences {
public:
    aPreferences();

    bool begin(const char *name, bool read_only);

    bool append(lbj_data lbj, rx_info rx, float volt, float temp);

    bool retrieve(lbj_data *lbj, rx_info *rx, String *time_str, uint16_t *line_num, int8_t bias);

    bool clearKeys();

    void toLatest();

    void getStats();

private:
    Preferences pref;
    const char *ns_name;
    bool have_pref;
    bool overflow;
    uint16_t lines;
    int32_t ret_lines; // defaults -1
    uint32_t ids;
    const char *partition_name = "nvs_ext";
};


#endif //PAGER_RECEIVE_APREFERENCES_H
