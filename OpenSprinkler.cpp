/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX/ESP8266) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * OpenSprinkler library
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "OpenSprinkler.h"
#include "gpio.h"
#include "server.h"

/** Declare static data members */
OSMqtt    OpenSprinkler::mqtt;
NVConData OpenSprinkler::nvdata;
ConStatus OpenSprinkler::status;
ConStatus OpenSprinkler::old_status;
byte      OpenSprinkler::hw_type;
byte      OpenSprinkler::hw_rev;

byte     OpenSprinkler::nboards;
byte     OpenSprinkler::nstations;
byte     OpenSprinkler::station_bits[MAX_NUM_BOARDS];
uint16_t OpenSprinkler::baseline_current;

ulong  OpenSprinkler::sensor1_on_timer;
ulong  OpenSprinkler::sensor1_off_timer;
time_t OpenSprinkler::sensor1_active_lasttime;
ulong  OpenSprinkler::sensor2_on_timer;
ulong  OpenSprinkler::sensor2_off_timer;
time_t OpenSprinkler::sensor2_active_lasttime;
time_t OpenSprinkler::raindelay_on_lasttime;

ulong   OpenSprinkler::flowcount_log_start;
ulong   OpenSprinkler::flowcount_rt;
time_t  OpenSprinkler::checkwt_lasttime;
time_t  OpenSprinkler::checkwt_success_lasttime;
time_t  OpenSprinkler::powerup_lasttime;
uint8_t OpenSprinkler::last_reboot_cause = REBOOT_CAUSE_NONE;
byte    OpenSprinkler::weather_update_flag;

// todo future: the following attribute bytes are for backward compatibility
byte OpenSprinkler::attrib_mas[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_igs[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_mas2[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_igs2[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_igrd[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_dis[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_seq[MAX_NUM_BOARDS];
byte OpenSprinkler::attrib_spe[MAX_NUM_BOARDS];

extern char tmp_buffer[];
extern char ether_buffer[];

byte OpenSprinkler::pin_sr_data = PIN_SR_DATA;

// todo future: LCD define for Linux-based systems

/** Option json names (stored in PROGMEM to reduce RAM usage) */
// IMPORTANT: each json name is strictly 5 characters
// with 0 fillings if less
#define OP_JSON_NAME_STEPSIZE 5
// for Integer options
const char iopt_json_names[] PROGMEM = "fwv\0\0"
                                       "tz\0\0\0"
                                       "ntp\0\0"
                                       "dhcp\0"
                                       "ip1\0\0"
                                       "ip2\0\0"
                                       "ip3\0\0"
                                       "ip4\0\0"
                                       "gw1\0\0"
                                       "gw2\0\0"
                                       "gw3\0\0"
                                       "gw4\0\0"
                                       "hp0\0\0"
                                       "hp1\0\0"
                                       "hwv\0\0"
                                       "ext\0\0"
                                       "seq\0\0"
                                       "sdt\0\0"
                                       "mas\0\0"
                                       "mton\0"
                                       "mtof\0"
                                       "urs\0\0"
                                       "rso\0\0"
                                       "wl\0\0\0"
                                       "den\0\0"
                                       "ipas\0"
                                       "devid"
                                       "con\0\0"
                                       "lit\0\0"
                                       "dim\0\0"
                                       "bst\0\0"
                                       "uwt\0\0"
                                       "ntp1\0"
                                       "ntp2\0"
                                       "ntp3\0"
                                       "ntp4\0"
                                       "lg\0\0\0"
                                       "mas2\0"
                                       "mton2"
                                       "mtof2"
                                       "fwm\0\0"
                                       "fpr0\0"
                                       "fpr1\0"
                                       "re\0\0\0"
                                       "dns1\0"
                                       "dns2\0"
                                       "dns3\0"
                                       "dns4\0"
                                       "sar\0\0"
                                       "ife\0\0"
                                       "sn1t\0"
                                       "sn1o\0"
                                       "sn2t\0"
                                       "sn2o\0"
                                       "sn1on"
                                       "sn1of"
                                       "sn2on"
                                       "sn2of"
                                       "subn1"
                                       "subn2"
                                       "subn3"
                                       "subn4"
                                       "wimod"
                                       "reset";

/** Option maximum values (stored in PROGMEM to reduce RAM usage) */
const byte iopt_max[] PROGMEM = {0,
                                 108,
                                 1,
                                 1,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 0,
                                 MAX_EXT_BOARDS,
                                 1,
                                 255,
                                 MAX_NUM_STATIONS,
                                 255,
                                 255,
                                 255,
                                 1,
                                 250,
                                 1,
                                 1,
                                 255,
                                 255,
                                 255,
                                 255,
                                 250,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 1,
                                 MAX_NUM_STATIONS,
                                 255,
                                 255,
                                 0,
                                 255,
                                 255,
                                 1,
                                 255,
                                 255,
                                 255,
                                 255,
                                 1,
                                 255,
                                 255,
                                 1,
                                 255,
                                 1,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 255,
                                 1};

// string options do not have maximum values

/** Integer option values (stored in RAM) */
byte OpenSprinkler::iopts[] = {
    OS_FW_VERSION,  // firmware version
    28,             // default time zone: GMT-5
    1,              // 0: disable NTP sync, 1: enable NTP sync
    1,              // 0: use static ip, 1: use dhcp
    0,              // this and next 3 bytes define static ip
    0,
    0,
    0,
    0,  // this and next 3 bytes define static gateway ip
    0,
    0,
    0,
    144,  // this and next byte define http port number
    31,
    OS_HW_VERSION,
    0,    // number of 8-station extension board. 0: no extension boards
    1,    // the option 'sequential' is now retired
    120,  // station delay time (-10 minutes to 10 minutes).
    0,    // index of master station. 0: no master station
    120,  // master on time adjusted time (-10 minutes to 10 minutes)
    120,  // master off adjusted time (-10 minutes to 10 minutes)
    0,    // urs (retired)
    0,    // rso (retired)
    100,  // water level (default 100%),
    1,    // device enable
    0,    // 1: ignore password; 0: use password
    0,    // device id
    150,  // lcd contrast
    100,  // lcd backlight
    50,   // lcd dimming
    80,   // boost time (only valid to DC and LATCH type)
    0,    // weather algorithm (0 means not using weather algorithm)
    0,    // this and the next three bytes define the ntp server ip
    0,
    0,
    0,
    1,            // enable logging: 0: disable; 1: enable.
    0,            // index of master2. 0: no master2 station
    120,          // master2 on adjusted time
    120,          // master2 off adjusted time
    OS_FW_MINOR,  // firmware minor version
    100,          // this and next byte define flow pulse rate (100x)
    0,            // default is 1.00 (100)
    0,            // set as remote extension
    8,            // this and the next three bytes define the custom dns server ip
    8,
    8,
    8,
    0,    // special station auto refresh
    0,    // ifttt enable bits
    0,    // sensor 1 type (see SENSOR_TYPE macro defines)
    1,    // sensor 1 option. 0: normally closed; 1: normally open.	default 1.
    0,    // sensor 2 type
    1,    // sensor 2 option. 0: normally closed; 1: normally open. default 1.
    0,    // sensor 1 on delay
    0,    // sensor 1 off delay
    0,    // sensor 2 on delay
    0,    // sensor 2 off delay
    255,  // subnet mask 1
    255,  // subnet mask 2
    255,  // subnet mask 3
    0,
    WIFI_MODE_AP,  // wifi mode
    0              // reset
};

/** String option values (stored in RAM) */
const char * OpenSprinkler::sopts[] = {DEFAULT_PASSWORD,
                                       DEFAULT_LOCATION,
                                       DEFAULT_JAVASCRIPT_URL,
                                       DEFAULT_WEATHER_URL,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING,
                                       DEFAULT_EMPTY_STRING};

/** Calculate local time (UTC time plus time zone offset) */
time_t OpenSprinkler::now_tz() {
    return now() + (int32_t)3600 / 4 * (int32_t) (iopts[IOPT_TIMEZONE] - 48);
}

#include "etherport.h"
#include "server.h"
#include "utils.h"
#include <net/if.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>

/** Initialize network with the given mac address and http port */
byte OpenSprinkler::start_network() {
    unsigned int port = (unsigned int)(iopts[IOPT_HTTPPORT_1] << 8) + (unsigned int)iopts[IOPT_HTTPPORT_0];
    if (m_server) {
        delete m_server;
        m_server = 0;
    }

    m_server = new EthernetServer (port);
    return m_server->begin();
}

bool OpenSprinkler::network_connected (void) {
    return true;
}

// Return mac of first recognised interface and fallback to software mac
// Note: on OSPi, operating system handles interface allocation so 'wired' ignored
bool OpenSprinkler::load_hardware_mac (byte * mac, bool wired) {
    const char * if_names[] = {"eth0", "eth1", "wlan0", "wlan1"};
    struct ifreq ifr;
    int          fd;

    // Fallback to asoftware mac if interface not recognised
    mac[0] = 0x00;
    mac[1] = 0x69;
    mac[2] = 0x69;
    mac[3] = 0x2D;
    mac[4] = 0x31;
    mac[5] = iopts[IOPT_DEVICE_ID];

    if (m_server == NULL)
        return true;

    if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) == 0)
        return true;

    // Returns the mac address of the first interface if multiple active
    for (size_t i = 0; i < sizeof (if_names) / sizeof (const char *); i++) {
        strncpy (ifr.ifr_name, if_names[i], sizeof (ifr.ifr_name));
        if (ioctl (fd, SIOCGIFHWADDR, &ifr) != -1) {
            memcpy (mac, ifr.ifr_hwaddr.sa_data, 6);
            break;
        }
    }
    close (fd);
    return true;
}

/** Reboot controller */
void OpenSprinkler::reboot_dev (uint8_t cause) {
    nvdata.reboot_cause = cause;
    nvdata_save();
    sync();  // add sync to prevent file corruption
    reboot (RB_AUTOBOOT);
}

/** Launch update script */
void OpenSprinkler::update_dev() {
    char cmd[1000];
    sprintf (cmd, "cd %s & ./updater.sh", get_runtime_path());
    system (cmd);
}

/** Initialize pins, controller variables, LCD */
void OpenSprinkler::begin() {
    hw_type = HW_TYPE_UNKNOWN;
    hw_rev  = 0;

    // shift register setup
    pinMode (PIN_SR_OE, OUTPUT);
    // pull shift register OE high to disable output
    digitalWrite (PIN_SR_OE, HIGH);
    pinMode (PIN_SR_LATCH, OUTPUT);
    digitalWrite (PIN_SR_LATCH, HIGH);

    pinMode (PIN_SR_CLOCK, OUTPUT);

    pin_sr_data = PIN_SR_DATA;
    // detect RPi revision
    unsigned int rev = detect_rpi_rev();
    if (rev == 0x0002 || rev == 0x0003)
        pin_sr_data = PIN_SR_DATA_ALT;
    // if this is revision 1, use PIN_SR_DATA_ALT
    pinMode (pin_sr_data, OUTPUT);

    // Reset all stations
    clear_all_station_bits();
    apply_all_station_bits();

    // pull shift register OE low to enable output
    digitalWrite (PIN_SR_OE, LOW);
    // Rain sensor port set up
    pinMode (PIN_SENSOR1, INPUT_PULLUP);
#if defined(PIN_SENSOR2)
    pinMode (PIN_SENSOR2, INPUT_PULLUP);
#endif

    // Default controller status variables
    // Static variables are assigned 0 by default
    // so only need to initialize non-zero ones
    status.enabled     = 1;
    status.safe_reboot = 0;

    old_status = status;

    nvdata.sunrise_time = 360;   // 6:00am default sunrise
    nvdata.sunset_time  = 1080;  // 6:00pm default sunset
    nvdata.reboot_cause = REBOOT_CAUSE_POWERON;

    nboards   = 1;
    nstations = nboards * 8;

    // set rf data pin
    pinModeExt (PIN_RFTX, OUTPUT);
    digitalWriteExt (PIN_RFTX, LOW);

    DEBUG_PRINTLN (get_runtime_path());
}

/** Apply all station bits
 * !!! This will activate/deactivate valves !!!
 */
void OpenSprinkler::apply_all_station_bits() {
    digitalWrite (PIN_SR_LATCH, LOW);
    byte bid, s, sbits;

    // Shift out all station bit values
    // from the highest bit to the lowest
    for (bid = 0; bid <= MAX_EXT_BOARDS; bid++) {
        if (status.enabled)
            sbits = station_bits[MAX_EXT_BOARDS - bid];
        else
            sbits = 0;

        for (s = 0; s < 8; s++) {
            digitalWrite (PIN_SR_CLOCK, LOW);
            digitalWrite (pin_sr_data, (sbits & ((byte)1 << (7 - s))) ? HIGH : LOW);
            digitalWrite (PIN_SR_CLOCK, HIGH);
        }
    }

    digitalWrite (PIN_SR_LATCH, HIGH);


    if (iopts[IOPT_SPE_AUTO_REFRESH]) {
        // handle refresh of RF and remote stations
        // we refresh the station whose index is the current time modulo MAX_NUM_STATIONS
        static byte last_sid = 0;
        byte        sid      = now() % MAX_NUM_STATIONS;
        if (sid != last_sid) {  // avoid refreshing the same station twice in a roll
            last_sid = sid;
            bid      = sid >> 3;
            s        = sid & 0x07;
            switch_special_station (sid, (station_bits[bid] >> s) & 0x01);
        }
    }
}

/** Read rain sensor status */
void OpenSprinkler::detect_binarysensor_status (ulong curr_time) {
    // sensor_type: 0 if normally closed, 1 if normally open
    if (iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_RAIN || iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_SOIL) {
        pinModeExt (PIN_SENSOR1, INPUT_PULLUP);  // this seems necessary for OS 3.2
        byte val       = digitalReadExt (PIN_SENSOR1);
        status.sensor1 = (val == iopts[IOPT_SENSOR1_OPTION]) ? 0 : 1;
        if (status.sensor1) {
            if (!sensor1_on_timer) {
                // add minimum of 5 seconds on delay
                ulong delay_time  = (ulong)iopts[IOPT_SENSOR1_ON_DELAY] * 60;
                sensor1_on_timer  = curr_time + (delay_time > 5 ? delay_time : 5);
                sensor1_off_timer = 0;
            } else {
                if (curr_time > sensor1_on_timer) {
                    status.sensor1_active = 1;
                }
            }
        } else {
            if (!sensor1_off_timer) {
                ulong delay_time  = (ulong)iopts[IOPT_SENSOR1_OFF_DELAY] * 60;
                sensor1_off_timer = curr_time + (delay_time > 5 ? delay_time : 5);
                sensor1_on_timer  = 0;
            } else {
                if (curr_time > sensor1_off_timer) {
                    status.sensor1_active = 0;
                }
            }
        }
    }

#if defined(PIN_SENSOR2)
    if (iopts[IOPT_SENSOR2_TYPE] == SENSOR_TYPE_RAIN || iopts[IOPT_SENSOR2_TYPE] == SENSOR_TYPE_SOIL) {
        pinModeExt (PIN_SENSOR2, INPUT_PULLUP);  // this seems necessary for OS 3.2
        byte val       = digitalReadExt (PIN_SENSOR2);
        status.sensor2 = (val == iopts[IOPT_SENSOR2_OPTION]) ? 0 : 1;
        if (status.sensor2) {
            if (!sensor2_on_timer) {
                // add minimum of 5 seconds on delay
                ulong delay_time  = (ulong)iopts[IOPT_SENSOR2_ON_DELAY] * 60;
                sensor2_on_timer  = curr_time + (delay_time > 5 ? delay_time : 5);
                sensor2_off_timer = 0;
            } else {
                if (curr_time > sensor2_on_timer) {
                    status.sensor2_active = 1;
                }
            }
        } else {
            if (!sensor2_off_timer) {
                ulong delay_time  = (ulong)iopts[IOPT_SENSOR2_OFF_DELAY] * 60;
                sensor2_off_timer = curr_time + (delay_time > 5 ? delay_time : 5);
                sensor2_on_timer  = 0;
            } else {
                if (curr_time > sensor2_off_timer) {
                    status.sensor2_active = 0;
                }
            }
        }
    }
#endif
}


/** Return program switch status */
byte OpenSprinkler::detect_programswitch_status (ulong curr_time) {
    byte ret = 0;
    if (iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_PSWITCH) {
        static ulong keydown_time = 0;
        pinModeExt (PIN_SENSOR1, INPUT_PULLUP);  // this seems necessary for OS 3.2
        status.sensor1 = (digitalReadExt (PIN_SENSOR1) != iopts[IOPT_SENSOR1_OPTION]);
        byte val       = (digitalReadExt (PIN_SENSOR1) == iopts[IOPT_SENSOR1_OPTION]);
        if (!val && !keydown_time)
            keydown_time = curr_time;
        else if (val && keydown_time && (curr_time > keydown_time)) {
            keydown_time = 0;
            ret |= 0x01;
        }
    }
#if defined(PIN_SENSOR2)
    if (iopts[IOPT_SENSOR2_TYPE] == SENSOR_TYPE_PSWITCH) {
        static ulong keydown_time_2 = 0;
        pinModeExt (PIN_SENSOR2, INPUT_PULLUP);  // this seems necessary for OS 3.2
        status.sensor2 = (digitalReadExt (PIN_SENSOR2) != iopts[IOPT_SENSOR2_OPTION]);
        byte val       = (digitalReadExt (PIN_SENSOR2) == iopts[IOPT_SENSOR2_OPTION]);
        if (!val && !keydown_time_2)
            keydown_time_2 = curr_time;
        else if (val && keydown_time_2 && (curr_time > keydown_time_2)) {
            keydown_time_2 = 0;
            ret |= 0x02;
        }
    }
#endif
    return ret;
}

void OpenSprinkler::sensor_resetall() {
    sensor1_on_timer          = 0;
    sensor1_off_timer         = 0;
    sensor1_active_lasttime   = 0;
    sensor2_on_timer          = 0;
    sensor2_off_timer         = 0;
    sensor2_active_lasttime   = 0;
    old_status.sensor1_active = status.sensor1_active = 0;
    old_status.sensor2_active = status.sensor2_active = 0;
}


/** Read the number of 8-station expansion boards */
// AVR has capability to detect number of expansion boards
int OpenSprinkler::detect_exp() {
    return -1;
}

/** Convert hex code to ulong integer */
static ulong hex2ulong (byte * code, byte len) {
    char  c;
    ulong v = 0;
    for (byte i = 0; i < len; i++) {
        c = code[i];
        v <<= 4;
        if (c >= '0' && c <= '9') {
            v += (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            v += 10 + (c - 'A');
        } else if (c >= 'a' && c <= 'f') {
            v += 10 + (c - 'a');
        } else {
            return 0;
        }
    }
    return v;
}

/** Parse RF code into on/off/timeing sections */
uint16_t OpenSprinkler::parse_rfstation_code (RFStationData * data, ulong * on, ulong * off) {
    ulong v;
    v = hex2ulong (data->on, sizeof (data->on));
    if (!v)
        return 0;
    if (on)
        *on = v;
    v = hex2ulong (data->off, sizeof (data->off));
    if (!v)
        return 0;
    if (off)
        *off = v;
    v = hex2ulong (data->timing, sizeof (data->timing));
    if (!v)
        return 0;
    return v;
}

/** Get station data */
void OpenSprinkler::get_station_data (byte sid, StationData * data) {
    file_read_block (STATIONS_FILENAME, data, (uint32_t)sid * sizeof (StationData), sizeof (StationData));
}

/** Set station data */
void OpenSprinkler::set_station_data (byte sid, StationData * data) {
    file_write_block (STATIONS_FILENAME, data, (uint32_t)sid * sizeof (StationData), sizeof (StationData));
}

/** Get station name */
void OpenSprinkler::get_station_name (byte sid, char tmp[]) {
    tmp[STATION_NAME_SIZE] = 0;
    file_read_block (STATIONS_FILENAME,
                     tmp,
                     (uint32_t)sid * sizeof (StationData) + offsetof (StationData, name),
                     STATION_NAME_SIZE);
}

/** Set station name */
void OpenSprinkler::set_station_name (byte sid, char tmp[]) {
    // todo: store the right size
    tmp[STATION_NAME_SIZE] = 0;
    file_write_block (STATIONS_FILENAME,
                      tmp,
                      (uint32_t)sid * sizeof (StationData) + offsetof (StationData, name),
                      STATION_NAME_SIZE);
}

/** Get station type */
byte OpenSprinkler::get_station_type (byte sid) {
    return file_read_byte (STATIONS_FILENAME, (uint32_t)sid * sizeof (StationData) + offsetof (StationData, type));
}

/** Get station attribute */
/*void OpenSprinkler::get_station_attrib(byte sid, StationAttrib *attrib); {
        file_read_block(STATIONS_FILENAME, attrib, (uint32_t)sid*sizeof(StationData)+offsetof(StationData, attrib),
sizeof(StationAttrib));
}*/

/** Save all station attribs to file (backward compatibility) */
void OpenSprinkler::attribs_save() {
    // re-package attribute bits and save
    byte          bid, s, sid = 0;
    StationAttrib at;
    byte          ty = STN_TYPE_STANDARD;
    for (bid = 0; bid < MAX_NUM_BOARDS; bid++) {
        for (s = 0; s < 8; s++, sid++) {
            at.mas  = (attrib_mas[bid] >> s) & 1;
            at.igs  = (attrib_igs[bid] >> s) & 1;
            at.mas2 = (attrib_mas2[bid] >> s) & 1;
            at.igs2 = (attrib_igs2[bid] >> s) & 1;
            at.igrd = (attrib_igrd[bid] >> s) & 1;
            at.dis  = (attrib_dis[bid] >> s) & 1;
            at.seq  = (attrib_seq[bid] >> s) & 1;
            at.gid  = 0;
            file_write_block (STATIONS_FILENAME,
                              &at,
                              (uint32_t)sid * sizeof (StationData) + offsetof (StationData, attrib),
                              1);  // attribte bits are 1 byte long
            if (attrib_spe[bid] >> s == 0) {
                // if station special bit is 0, make sure to write type STANDARD
                file_write_block (STATIONS_FILENAME,
                                  &ty,
                                  (uint32_t)sid * sizeof (StationData) + offsetof (StationData, type),
                                  1);  // attribte bits are 1 byte long
            }
        }
    }
}

/** Load all station attribs from file (backward compatibility) */
void OpenSprinkler::attribs_load() {
    // load and re-package attributes
    byte          bid, s, sid = 0;
    StationAttrib at;
    byte          ty;
    memset (attrib_mas, 0, nboards);
    memset (attrib_igs, 0, nboards);
    memset (attrib_mas2, 0, nboards);
    memset (attrib_igs2, 0, nboards);
    memset (attrib_igrd, 0, nboards);
    memset (attrib_dis, 0, nboards);
    memset (attrib_seq, 0, nboards);
    memset (attrib_spe, 0, nboards);

    for (bid = 0; bid < MAX_NUM_BOARDS; bid++) {
        for (s = 0; s < 8; s++, sid++) {
            file_read_block (STATIONS_FILENAME,
                             &at,
                             (uint32_t)sid * sizeof (StationData) + offsetof (StationData, attrib),
                             sizeof (StationAttrib));
            attrib_mas[bid] |= (at.mas << s);
            attrib_igs[bid] |= (at.igs << s);
            attrib_mas2[bid] |= (at.mas2 << s);
            attrib_igs2[bid] |= (at.igs2 << s);
            attrib_igrd[bid] |= (at.igrd << s);
            attrib_dis[bid] |= (at.dis << s);
            attrib_seq[bid] |= (at.seq << s);
            file_read_block (STATIONS_FILENAME, &ty, (uint32_t)sid * sizeof (StationData) + offsetof (StationData, type), 1);
            if (ty != STN_TYPE_STANDARD) {
                attrib_spe[bid] |= (1 << s);
            }
        }
    }
}

/** verify if a string matches password */
byte OpenSprinkler::password_verify (char * pw) {
    return (file_cmp_block (SOPTS_FILENAME, pw, SOPT_PASSWORD * MAX_SOPTS_SIZE) == 0) ? 1 : 0;
}

// ==================
// Schedule Functions
// ==================

/** Switch special station */
void OpenSprinkler::switch_special_station (byte sid, byte value) {
    // check if this is a special station
    byte stype = get_station_type (sid);
    if (stype != STN_TYPE_STANDARD) {
        // read station data
        StationData * pdata = (StationData *)tmp_buffer;
        get_station_data (sid, pdata);
        switch (stype) {

            case STN_TYPE_RF:
                switch_rfstation ((RFStationData *)pdata->sped, value);
                break;

            case STN_TYPE_REMOTE:
                switch_remotestation ((RemoteStationData *)pdata->sped, value);
                break;

            case STN_TYPE_GPIO:
                switch_gpiostation ((GPIOStationData *)pdata->sped, value);
                break;

            case STN_TYPE_HTTP:
                switch_httpstation ((HTTPStationData *)pdata->sped, value);
                break;
        }
    }
}


/** Callback function for browseUrl calls */
void httpget_callback (byte status, uint16_t off, uint16_t len) {}


void record_to_database (const char * postval) {
    EthernetClient      client;
    struct hostent *    host;
    static const char * server = "carbon.local";

    host = gethostbyname (server);
    if (!host) {
        DEBUG_PRINT ("can't resolve http server - ");
        DEBUG_PRINTLN (server);
        return;
    }

    if (!client.connect ((uint8_t *)host->h_addr, 8086)) {
        client.stop();
        return;
    }

    char postBuffer[1500];
    sprintf (postBuffer,
             "POST /write?db=ospi HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Accept: */*\r\n"
             "Content-Length: %ld\r\n"
             "Content-Type: application/json\r\n"
             "\r\n%s",
             host->h_name,
             strlen (postval),
             postval);
    client.write ((uint8_t *)postBuffer, strlen (postBuffer));

    bzero (ether_buffer, ETHER_BUFFER_SIZE);

    time_t timeout = now() + 5;  // 5 seconds timeout
    while (now() < timeout) {
        int len = client.read ((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
        if (len <= 0) {
            if (!client.connected())
                break;
            else
                continue;
        }
        httpget_callback (0, 0, ETHER_BUFFER_SIZE);
    }

    client.stop();
}


/** Record valve status
 * Send a valve's on/off status to remote influx database
 */
void record_valve_status (byte valve_num, bool status) {
    static char postval[TMP_BUFFER_SIZE];
    sprintf (postval, "valve%02d value=%d", valve_num, status ? 1 : 0);
    record_to_database (postval);
}


/** Record current valve
 * Send the currently-on valve to emote influx database
 */
void record_current_valve (byte valve_num) {
    static char postval[TMP_BUFFER_SIZE];
    sprintf (postval, "valves value=%d", valve_num);
    record_to_database (postval);
}


/** Set station bit
 * This function sets/resets the corresponding station bit variable
 * You have to call apply_all_station_bits next to apply the bits
 * (which results in physical actions of opening/closing valves).
 */
byte OpenSprinkler::set_station_bit (byte sid, byte value) {
    record_valve_status (sid + 1, !!value);

    byte * data = station_bits + (sid >> 3);  // pointer to the station byte
    byte   mask = (byte)1 << (sid & 0x07);    // mask
    if (value) {
        if ((*data) & mask)
            return 0;  // if bit is already set, return no change
        else {
            (*data) = (*data) | mask;
            switch_special_station (sid, 1);  // handle special stations
            record_current_valve (sid + 1);
            return 1;
        }
    } else {
        if (!((*data) & mask))
            return 0;  // if bit is already reset, return no change
        else {
            (*data) = (*data) & (~mask);
            switch_special_station (sid, 0);  // handle special stations
            record_current_valve (0);
            return 255;
        }
    }
    return 0;
}

/** Clear all station bits */
void OpenSprinkler::clear_all_station_bits() {
    byte sid;
    for (sid = 0; sid <= MAX_NUM_STATIONS; sid++) { set_station_bit (sid, 0); }
}

int rf_gpio_fd = -1;

/** Transmit one RF signal bit */
void transmit_rfbit (ulong lenH, ulong lenL) {
    gpio_write (rf_gpio_fd, 1);
    delayMicrosecondsHard (lenH);
    gpio_write (rf_gpio_fd, 0);
    delayMicrosecondsHard (lenL);
}

/** Transmit RF signal */
void send_rfsignal (ulong code, ulong len) {
    ulong len3  = len * 3;
    ulong len31 = len * 31;
    for (byte n = 0; n < 15; n++) {
        int i = 23;
        // send code
        while (i >= 0) {
            if ((code >> i) & 1) {
                transmit_rfbit (len3, len);
            } else {
                transmit_rfbit (len, len3);
            }
            i--;
        };
        // send sync
        transmit_rfbit (len, len31);
    }
}

/** Switch RF station
 * This function takes a RF code,
 * parses it into signals and timing,
 * and sends it out through RF transmitter.
 */
void OpenSprinkler::switch_rfstation (RFStationData * data, bool turnon) {
    ulong    on, off;
    uint16_t length = parse_rfstation_code (data, &on, &off);
    // pre-open gpio file to minimize overhead
    rf_gpio_fd = gpio_fd_open (PIN_RFTX);
    send_rfsignal (turnon ? on : off, length);
    gpio_fd_close (rf_gpio_fd);
    rf_gpio_fd = -1;
}

/** Switch GPIO station
 * Special data for GPIO Station is three bytes of ascii decimal (not hex)
 * First two bytes are zero padded GPIO pin number.
 * Third byte is either 0 or 1 for active low (GND) or high (+5V) relays
 */
void OpenSprinkler::switch_gpiostation (GPIOStationData * data, bool turnon) {
    byte gpio        = (data->pin[0] - '0') * 10 + (data->pin[1] - '0');
    byte activeState = data->active - '0';

    pinMode (gpio, OUTPUT);
    if (turnon)
        digitalWrite (gpio, activeState);
    else
        digitalWrite (gpio, 1 - activeState);
}

/** Callback function for switching remote station */
void remote_http_callback (char * buffer) {
    /*
            DEBUG_PRINTLN(buffer);
    */
}

#define SERVER_CONNECT_NTRIES 3

int8_t OpenSprinkler::send_http_request (uint32_t ip4, uint16_t port, char * p, void (*callback) (char *), uint16_t timeout) {
    static byte ip[4];
    ip[0] = ip4 >> 24;
    ip[1] = (ip4 >> 16) & 0xff;
    ip[2] = (ip4 >> 8) & 0xff;
    ip[3] = ip4 & 0xff;

    EthernetClient   etherClient;
    EthernetClient * client = &etherClient;
    if (!client->connect (ip, port)) {
        client->stop();
        return HTTP_RQT_CONNECT_ERR;
    }

    uint16_t len = strlen (p);
    if (len > ETHER_BUFFER_SIZE)
        len = ETHER_BUFFER_SIZE;
    client->write ((uint8_t *)p, len);
    memset (ether_buffer, 0, ETHER_BUFFER_SIZE);
    uint32_t stoptime = millis() + timeout;

    while (millis() < stoptime) {
        int len = client->read ((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
        if (len <= 0) {
            if (!client->connected())
                break;
            else
                continue;
        }
    }

    client->stop();
    if (strlen (ether_buffer) == 0)
        return HTTP_RQT_EMPTY_RETURN;
    if (callback)
        callback (ether_buffer);
    return HTTP_RQT_SUCCESS;
}

int8_t
OpenSprinkler::send_http_request (const char * server, uint16_t port, char * p, void (*callback) (char *), uint16_t timeout) {
    EthernetClient   etherClient;
    EthernetClient * client = &etherClient;
    struct hostent * host;
    host = gethostbyname (server);
    if (!host) {
        DEBUG_PRINT ("can't resolve http station - ");
        DEBUG_PRINTLN (server);
        return HTTP_RQT_CONNECT_ERR;
    }
    if (!client->connect ((uint8_t *)host->h_addr, port)) {
        client->stop();
        return HTTP_RQT_CONNECT_ERR;
    }

    uint16_t len = strlen (p);
    if (len > ETHER_BUFFER_SIZE)
        len = ETHER_BUFFER_SIZE;
    client->write ((uint8_t *)p, len);

    memset (ether_buffer, 0, ETHER_BUFFER_SIZE);
    uint32_t stoptime = millis() + timeout;

    while (millis() < stoptime) {
        int len = client->read ((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
        if (len <= 0) {
            if (!client->connected())
                break;
            else
                continue;
        }
    }

    client->stop();
    if (strlen (ether_buffer) == 0)
        return HTTP_RQT_EMPTY_RETURN;
    if (callback)
        callback (ether_buffer);
    return HTTP_RQT_SUCCESS;
}

int8_t OpenSprinkler::send_http_request (char * server_with_port, char * p, void (*callback) (char *), uint16_t timeout) {
    char * server = strtok (server_with_port, ":");
    char * port   = strtok (NULL, ":");
    return send_http_request (server, (port == NULL) ? 80 : atoi (port), p, callback, timeout);
}

/** Switch remote station
 * This function takes a remote station code,
 * parses it into remote IP, port, station index,
 * and makes a HTTP GET request.
 * The remote controller is assumed to have the same
 * password as the main controller
 */
void OpenSprinkler::switch_remotestation (RemoteStationData * data, bool turnon) {
    RemoteStationData copy;
    memcpy ((char *)&copy, (char *)data, sizeof (RemoteStationData));

    uint32_t ip4  = hex2ulong (copy.ip, sizeof (copy.ip));
    uint16_t port = (uint16_t)hex2ulong (copy.port, sizeof (copy.port));

    byte ip[4];
    ip[0] = ip4 >> 24;
    ip[1] = (ip4 >> 16) & 0xff;
    ip[2] = (ip4 >> 8) & 0xff;
    ip[3] = ip4 & 0xff;

    // use tmp_buffer starting at a later location
    // because remote station data is loaded at the beginning
    char *       p  = tmp_buffer;
    BufferFiller bf = p;
    // MAX_NUM_STATIONS is the refresh cycle
    uint16_t timer = iopts[IOPT_SPE_AUTO_REFRESH] ? 2 * MAX_NUM_STATIONS : 64800;
    bf.emit_p (PSTR ("GET /cm?pw=$O&sid=$D&en=$D&t=$D"),
               SOPT_PASSWORD,
               (int)hex2ulong (copy.sid, sizeof (copy.sid)),
               turnon,
               timer);
    bf.emit_p (PSTR (" HTTP/1.0\r\nHOST: $D.$D.$D.$D\r\n\r\n"), ip[0], ip[1], ip[2], ip[3]);

    send_http_request (ip4, port, p, remote_http_callback);
}

/** Switch http station
 * This function takes an http station code,
 * parses it into a server name and two HTTP GET requests.
 */
void OpenSprinkler::switch_httpstation (HTTPStationData * data, bool turnon) {
    HTTPStationData copy;
    // make a copy of the HTTP station data and work with it
    memcpy ((char *)&copy, (char *)data, sizeof (HTTPStationData));
    char * server  = strtok ((char *)copy.data, ",");
    char * port    = strtok (NULL, ",");
    char * on_cmd  = strtok (NULL, ",");
    char * off_cmd = strtok (NULL, ",");
    char * cmd     = turnon ? on_cmd : off_cmd;

    char *       p  = tmp_buffer;
    BufferFiller bf = p;
    bf.emit_p (PSTR ("GET /$S HTTP/1.0\r\nHOST: $S\r\n\r\n"), cmd, server);

    send_http_request (server, atoi (port), p, remote_http_callback);
}

/** Setup function for options */
void OpenSprinkler::options_setup() {
    // Check reset conditions:
    if (file_read_byte (IOPTS_FILENAME, IOPT_FW_VERSION) < 219 ||  // fw version is invalid (<219)
        !file_exists (DONE_FILENAME) ||                            // done file doesn't exist
        file_read_byte (IOPTS_FILENAME, IOPT_RESET) == 0xAA) {     // reset flag is on

        DEBUG_PRINT ("resetting options...");

        // 0. remove existing files
        if (file_read_byte (IOPTS_FILENAME, IOPT_RESET) == 0xAA) {
            // this is an explicit reset request, simply perform a format
            // todo future: delete log files
        }

        remove_file (DONE_FILENAME);
        /*remove_file(IOPTS_FILENAME);
        remove_file(SOPTS_FILENAME);
        remove_file(STATIONS_FILENAME);
        remove_file(NVCON_FILENAME);
        remove_file(PROG_FILENAME);*/

        // 1. write all options
        iopts_save();
        // wipe out sopts file first
        memset (tmp_buffer, 0, MAX_SOPTS_SIZE);
        for (int i = 0; i < NUM_SOPTS; i++) {
            file_write_block (SOPTS_FILENAME, tmp_buffer, (ulong)MAX_SOPTS_SIZE * i, MAX_SOPTS_SIZE);
        }
        // write string options
        for (int i = 0; i < NUM_SOPTS; i++) { sopt_save (i, sopts[i]); }

        // 2. write station data
        StationData * pdata = (StationData *)tmp_buffer;
        pdata->name[0]      = 'S';
        pdata->name[3]      = 0;
        pdata->name[4]      = 0;
        StationAttrib at;
        memset (&at, 0, sizeof (StationAttrib));
        at.mas         = 1;
        at.seq         = 1;
        pdata->attrib  = at;  // mas:1 seq:1
        pdata->type    = STN_TYPE_STANDARD;
        pdata->sped[0] = '0';
        pdata->sped[1] = 0;
        for (int i = 0; i < MAX_NUM_STATIONS; i++) {
            int sid = i + 1;
            if (i < 99) {
                pdata->name[1] = '0' + (sid / 10);  // default station name
                pdata->name[2] = '0' + (sid % 10);
            } else {
                pdata->name[1] = '0' + (sid / 100);
                pdata->name[2] = '0' + ((sid % 100) / 10);
                pdata->name[3] = '0' + (sid % 10);
            }
            file_write_block (STATIONS_FILENAME, pdata, sizeof (StationData) * i, sizeof (StationData));
        }

        attribs_load();  // load and repackage attrib bits (for backward compatibility)

        // 3. write non-volatile controller status
        nvdata.reboot_cause = REBOOT_CAUSE_RESET;
        nvdata_save();
        last_reboot_cause = nvdata.reboot_cause;

        // 4. write program data: just need to write a program counter: 0
        file_write_byte (PROG_FILENAME, 0, 0);

        // 5. write 'done' file
        file_write_byte (DONE_FILENAME, 0, 1);
    } else {

        iopts_load();
        nvdata_load();
        last_reboot_cause   = nvdata.reboot_cause;
        nvdata.reboot_cause = REBOOT_CAUSE_POWERON;
        nvdata_save();
        attribs_load();
    }
}

/** Load non-volatile controller status data from file */
void OpenSprinkler::nvdata_load() {
    file_read_block (NVCON_FILENAME, &nvdata, 0, sizeof (NVConData));
    old_status = status;
}

/** Save non-volatile controller status data */
void OpenSprinkler::nvdata_save() {
    file_write_block (NVCON_FILENAME, &nvdata, 0, sizeof (NVConData));
}

/** Load integer options from file */
void OpenSprinkler::iopts_load() {
    file_read_block (IOPTS_FILENAME, iopts, 0, NUM_IOPTS);
    nboards                = iopts[IOPT_EXT_BOARDS] + 1;
    nstations              = nboards * 8;
    status.enabled         = iopts[IOPT_DEVICE_ENABLE];
    iopts[IOPT_FW_VERSION] = OS_FW_VERSION;
    iopts[IOPT_FW_MINOR]   = OS_FW_MINOR;
    /* Reject the former default 50.97.210.169 NTP IP address as
     * it no longer works, yet is carried on by people's saved
     * configs when they upgrade from older versions.
     * IOPT_NTP_IP1 = 0 leads to the new good default behavior. */
    if (iopts[IOPT_NTP_IP1] == 50 && iopts[IOPT_NTP_IP2] == 97 && iopts[IOPT_NTP_IP3] == 210 && iopts[IOPT_NTP_IP4] == 169) {
        iopts[IOPT_NTP_IP1] = 0;
        iopts[IOPT_NTP_IP2] = 0;
        iopts[IOPT_NTP_IP3] = 0;
        iopts[IOPT_NTP_IP4] = 0;
    }
}

/** Save integer options to file */
void OpenSprinkler::iopts_save() {
    file_write_block (IOPTS_FILENAME, iopts, 0, NUM_IOPTS);
    nboards        = iopts[IOPT_EXT_BOARDS] + 1;
    nstations      = nboards * 8;
    status.enabled = iopts[IOPT_DEVICE_ENABLE];
}

/** Load a string option from file */
void OpenSprinkler::sopt_load (byte oid, char * buf) {
    file_read_block (SOPTS_FILENAME, buf, MAX_SOPTS_SIZE * oid, MAX_SOPTS_SIZE);
    buf[MAX_SOPTS_SIZE] = 0;  // ensure the string ends properly
}

/** Load a string option from file, return String */
String OpenSprinkler::sopt_load (byte oid) {
    sopt_load (oid, tmp_buffer);
    String str = tmp_buffer;
    return str;
}

/** Save a string option to file */
bool OpenSprinkler::sopt_save (byte oid, const char * buf) {
    // smart save: if value hasn't changed, don't write
    if (file_cmp_block (SOPTS_FILENAME, buf, (ulong)MAX_SOPTS_SIZE * oid) == 0)
        return false;
    int len = strlen (buf);
    if (len >= MAX_SOPTS_SIZE) {
        file_write_block (SOPTS_FILENAME, buf, (ulong)MAX_SOPTS_SIZE * oid, MAX_SOPTS_SIZE);
    } else {
        // copy ending 0 too
        file_write_block (SOPTS_FILENAME, buf, (ulong)MAX_SOPTS_SIZE * oid, len + 1);
    }
    return true;
}

// ==============================
// Controller Operation Functions
// ==============================

/** Enable controller operation */
void OpenSprinkler::enable() {
    status.enabled            = 1;
    iopts[IOPT_DEVICE_ENABLE] = 1;
    iopts_save();
}

/** Disable controller operation */
void OpenSprinkler::disable() {
    status.enabled            = 0;
    iopts[IOPT_DEVICE_ENABLE] = 0;
    iopts_save();
}

/** Start rain delay */
void OpenSprinkler::raindelay_start() {
    status.rain_delayed = 1;
    nvdata_save();
}

/** Stop rain delay */
void OpenSprinkler::raindelay_stop() {
    status.rain_delayed = 0;
    nvdata.rd_stop_time = 0;
    nvdata_save();
}
