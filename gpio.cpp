/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * GPIO functions
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

#include "gpio.h"
#include <unistd.h>

#define BUFFER_MAX 64

/** Export gpio pin */
static byte GPIOExport (int pin) {
    char buffer[BUFFER_MAX];
    int  fd, len;

    fd = open ("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        DEBUG_PRINTLN ("failed to open export for writing");
        return 0;
    }

    len = snprintf (buffer, sizeof (buffer), "%d", pin);
    write (fd, buffer, len);
    close (fd);
    return 1;
}

/** Set pin mode, in or out */
void pinMode (int pin, byte mode) {
    static const char dir_str[] = "in\0out";

    char path[BUFFER_MAX];
    int  fd;

    snprintf (path, BUFFER_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    struct stat st;
    if (stat (path, &st)) {
        if (!GPIOExport (pin))
            return;
    }

    fd = open (path, O_WRONLY);
    if (fd < 0) {
        DEBUG_PRINTLN ("failed to open gpio direction for writing");
        return;
    }

    if (-1 == write (fd,
                     &dir_str[(INPUT == mode) || (INPUT_PULLUP == mode) ? 0 : 3],
                     (INPUT == mode) || (INPUT_PULLUP == mode) ? 2 : 3)) {
        DEBUG_PRINTLN ("failed to set direction");
        return;
    }

    close (fd);
    if (mode == INPUT_PULLUP) {
        char cmd[BUFFER_MAX];
        snprintf (cmd, BUFFER_MAX, "gpio -g mode %d up", pin);
        system (cmd);
    }
    return;
}

/** Open file for digital pin */
int gpio_fd_open (int pin, int mode) {
    char path[BUFFER_MAX];
    int  fd;

    snprintf (path, BUFFER_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open (path, mode);
    if (fd < 0) {
        DEBUG_PRINTLN ("failed to open gpio");
        return -1;
    }
    return fd;
}

/** Close file */
void gpio_fd_close (int fd) {
    close (fd);
}

/** Read digital value */
byte digitalRead (int pin) {
    char value_str[3];

    int fd = gpio_fd_open (pin, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    if (read (fd, value_str, 3) < 0) {
        DEBUG_PRINTLN ("failed to read value");
        return 0;
    }

    close (fd);
    return atoi (value_str);
}

/** Write digital value given file descriptor */
void gpio_write (int fd, byte value) {
    static const char value_str[] = "01";

    if (1 != write (fd, &value_str[LOW == value ? 0 : 1], 1)) {
        DEBUG_PRINT ("failed to write value on pin ");
    }
}

/** Write digital value */
void digitalWrite (int pin, byte value) {
    int fd = gpio_fd_open (pin);
    if (fd < 0) {
        return;
    }
    gpio_write (fd, value);
    close (fd);
}
