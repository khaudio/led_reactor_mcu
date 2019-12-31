/*
    LED Reactor Mesh Client
    Copyright (C) 2018 K Hughes Production, LLC

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
TODO:
    switch to paho mqtt
        ssl/tls
*/

#include "LedReactor.h"

// Set verbosity for testing
#define VERBOSE         false

// Create reactor object
LedReactor reactor;

void setup()
{
    // Only start serial if testing; Serial calls can slow down operation
    #if VERBOSE
        Serial.begin(921600);
    #endif
    reactor.verbose = VERBOSE;

    LedReactor::init(
            13,     // Red pin
            12,     // Green pin
            15,     // Blue pin
            27,     // White pin
            10      // Bit depth
        );

    // Cycle once to signify boot/reboot
    reactor.writer->cycle(2.5);

    /* Set initial values to dimmed white (full power produces too much heat)
    This level of white can be overridden, but produces sufficient default
    levels of light */
    reactor.writer->set(std::array<uint16_t, 4>{0, 0, 0, 255});
}

void loop()
{
    // Required to be called in loop for proper operation
    LedReactor::run();
}
