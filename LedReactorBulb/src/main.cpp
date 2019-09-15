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

LedReactor reactor;

void setup()
{
    Serial.begin(921600);
    reactor.verbose = true;
    LedReactor::init(13, 12, 15, 27, 10);
    reactor.writer->cycle(2.5);
}

void loop()
{
    LedReactor::run();
}
