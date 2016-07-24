/* trackuino copyright (C) 2010  EA5HAV Javi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// Mpide 22 fails to compile Arduino code because it stupidly defines ARDUINO
// as an empty macro (hence the +0 hack). UNO32 builds are fine. Just use the
// real Arduino IDE for Arduino builds. Optionally complain to the Mpide
// authors to fix the broken macro.
#if (ARDUINO + 0) == 0
#error "Oops! We need the real Arduino IDE (version 22 or 23) for Arduino builds."
#error "See trackuino.pde for details on this"

// Refuse to compile on arduino version 21 or lower. 22 includes an
// optimization of the USART code that is critical for real-time operation
// of the AVR code.
#elif (ARDUINO + 0) < 22
#error "Oops! We need Arduino 22 or 23"
#error "See trackuino.pde for details on this"

#endif


#define max_buffer_length 100


// Trackuino custom libs
#include "config.h"
#include "afsk_avr.h"
#include "aprs.h"
#include "pin.h"
//#include "gps.h"

// Arduino/AVR libs
#include <Arduino.h>
#include <SoftwareSerial.h>

#define debug 1

SoftwareSerial due_link(5,6);
SoftwareSerial RS_UV3(8,9);

// Module constants
static const uint32_t VALID_POS_TIMEOUT = 2000;  // ms

void new_memset(char *a, char val, int length);

// Module variables
//static int32_t next_aprs = 0;

char lat_buffer[max_buffer_length] = {};
char lon_buffer[max_buffer_length] = {};
char tim_buffer[max_buffer_length] = {};
char alt_buffer[max_buffer_length] = {};
char msg_buffer[max_buffer_length] = {};



void setup()
{
  Serial.begin(115200);
  due_link.begin(19200);
  RS_UV3.begin(19200);

  pinMode(13, OUTPUT);

#ifdef DEBUG_RESET
  Serial.println("RESET");
#endif
  afsk_setup();
  //gps_setup();
  char lat[] = {"125.00N"};
  char lon[] = {"25W"};
  char alt[] = {"00000H"};
  aprs_send(lat, lon, alt, alt, alt);
  while (afsk_flush()) {
    pin_write(LED_PIN, HIGH);
  }
  /*
  // Do not start until we get a valid time reference
  // for slotted transmissions.
  if (APRS_SLOT >= 0) {
  do {
  while (! Serial.available())

  } while (! gps_decode(Serial.read()));
  next_aprs = millis() + 1000 *
  (APRS_PERIOD - (gps_seconds + APRS_PERIOD - APRS_SLOT) % APRS_PERIOD);
  }
  else {
  next_aprs = millis();
  }
  */
  //next_aprs = millis() + 1000 * APRS_PERIOD;


}


void loop()
{
  //setup interrupt from due to signal uno to start listenning
  //while not interrupted, listen to radio serial

  // Time for another APRS frame
  //if ((int32_t) (millis() - next_aprs) >= 0) {
  due_link.listen();
  if(due_link.available()) {
    String trans = due_link.readStringUntil('\n');
    Serial.print("TRANS: ");
    Serial.println(trans);
    lat_buffer[0] = 0;
    lon_buffer[0] = 0;
    tim_buffer[0] = 0;
    alt_buffer[0] = 0;
    msg_buffer[0] = 0;
    int written = due_link.readBytesUntil('\t',lat_buffer, max_buffer_length);
    lat_buffer[written] = 0;
    written = due_link.readBytesUntil('\t',lon_buffer, max_buffer_length);
    lon_buffer[written] = 0;
    written = due_link.readBytesUntil('\t',tim_buffer, max_buffer_length);
    tim_buffer[written] = 0;
    written = due_link.readBytesUntil('\t',alt_buffer, max_buffer_length);
    alt_buffer[written] = 0;
    written = due_link.readBytesUntil('\n',msg_buffer, max_buffer_length); //message can only be 100 bytes!
    msg_buffer[written] = 0;
#ifdef debug
    //prints in strange order
    //println(recieved) then time, alt, msg but no lat or lon
    Serial.println("due_link: ");
    Serial.println(lat_buffer);
    Serial.println(lon_buffer);
    Serial.println(tim_buffer);
    Serial.println(alt_buffer);
    Serial.println(msg_buffer);
    Serial.println(" GPS data Recieved");
#endif

    aprs_send(lat_buffer, lon_buffer, tim_buffer, alt_buffer, msg_buffer);

    while (afsk_flush()) {
      pin_write(LED_PIN, HIGH);
    }
    pin_write(LED_PIN, LOW);
#ifdef DEBUG_MODEM
    // Show modem ISR stats from the previous transmission
    afsk_debug();
#endif
  }

  if (RS_UV3.available()) {
    byte a_buffer[max_buffer_length] = {};
    int buffer_length = 0;
    buffer_length = (int)RS_UV3.readBytes(a_buffer, max_buffer_length);
    Serial.print("From Radio: ");
    Serial.write(a_buffer, buffer_length);
  }

  if (Serial.available()) {
    Serial.println();
    byte a_buffer[max_buffer_length] = {};
    int buffer_length = 0;
    buffer_length = (int)Serial.readBytes(a_buffer, max_buffer_length);
    Serial.print("To Radio: ");
    Serial.write(a_buffer, buffer_length);
  }

}

void new_memset(char *a, char val, int length) {
  for(int i=0; i<length; i++){
    *(a+i) = val;
  }
}
