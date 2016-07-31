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

// Trackuino custom libs
#include "config.h"
#include "afsk_avr.h"
#include "aprs.h"
#include "pin.h"
#include <Arduino.h>
#include <SoftwareSerial.h>

#define debug 1
#define max_buffer_length 100

SoftwareSerial due_link(5,6);
SoftwareSerial RS_UV3(8,9);

char lat_buffer[max_buffer_length] = {};
char lon_buffer[max_buffer_length] = {};
char tim_buffer[max_buffer_length] = {};
char alt_buffer[max_buffer_length] = {};
char msg_buffer[max_buffer_length] = {};
char param_buffer[max_buffer_length] = {};

void clearSerialBuffers();
void transmitService(char *lat, char *lon, char *time, char *alt, char *msg);
void radioReset();

void setup(){
  Serial.begin(115200);
  due_link.begin(19200); //all communication between due an uno will be terminated by \r
  RS_UV3.begin(19200);
  pinMode(13, OUTPUT);
  radioReset();
  afsk_setup();

#ifdef debug
  Serial.println("Reseting by own volition");
  RS_UV3.print("PW0\r");//This sets to LOW power!!! or does it?
  char lat[] = {"4916.38"};
  char lon[] = {"12255.28"};
  char tim[] = {"280720"};
  char alt[] = {"000"};
  char msg[] = {"http://sfusat.com"};
  RS_UV3.flush();
  delay(50);
  transmitService(lat, lon, tim, alt, msg);
#endif

  due_link.listen();
}


void loop(){
  due_link.listen();
  if(due_link.available()) {
    char commandChar = due_link.read();
    if(commandChar == 'a'){
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
      written = due_link.readBytesUntil('\r',msg_buffer, max_buffer_length); //message can only be 100 bytes!
      msg_buffer[written] = 0;
  #ifdef debug
      Serial.print("GPS recieved: ");
      Serial.print(lat_buffer);
      Serial.print("N ");
      Serial.print(lon_buffer);
      Serial.print("W ");
      Serial.print(tim_buffer);
      Serial.print("epoch ");
      Serial.print(alt_buffer);
      Serial.print("m ");
      Serial.println(msg_buffer);
  #endif
      transmitService(lat_buffer, lon_buffer, tim_buffer, alt_buffer, msg_buffer);
    }else if(commandChar == 'v'){
      param_buffer[0] = 0;
      RS_UV3.listen();
      RS_UV3.print("vt\r");
      int written = RS_UV3.readBytesUntil('\r', param_buffer, max_buffer_length);
      param_buffer[written] = '\r';
      param_buffer[written+1] = 0;//Still need to NULL terminate
      due_link.write(param_buffer);
      due_link.flush();
      #ifdef debug
      Serial.print("voltage: ");
      Serial.write(param_buffer);
      #endif
      clearSerialBuffers();
      due_link.listen();
    } else if(commandChar == 't'){
      param_buffer[0] = 0;
      RS_UV3.listen();
      RS_UV3.print("tp\r");
      int written = RS_UV3.readBytesUntil('\r', param_buffer, max_buffer_length);
      param_buffer[written] = '\r';
      param_buffer[written+1] = 0;//Still need to NULL terminate
      due_link.write(param_buffer);
      due_link.flush();
      #ifdef debug
      Serial.print("temperature: ");
      Serial.write(param_buffer);
      #endif
      clearSerialBuffers();
      due_link.listen();
    } else if(commandChar == 's'){//Startup setup
      due_link.print("ack\r"); //getting ack \n at due (space in between ack and \n)
      #ifdef debug
      Serial.println("resetting by command of Due");
      #endif
      radioReset();
      due_link.listen();
    } else {
      #ifdef debug
      Serial.println("Invalid command char from Due, discarding buffer");
      #endif
      clearSerialBuffers();
    }
  }
}

void transmitService(char *lat, char *lon, char *time, char *alt, char *msg){
  RS_UV3.print("pd0\r");//Power up the transiever
  RS_UV3.flush();//Ensure write is complete
  delay(2000);//Ensure the transiever has powered on
  aprs_send(lat_buffer, lon_buffer, tim_buffer, alt_buffer, msg_buffer); //lat, lon, time is decimal number only, N,W,H added in aprs.cpp
  while (afsk_flush()) {
    pin_write(LED_PIN, HIGH);
  }
  pin_write(LED_PIN, LOW);
  #ifdef DEBUG_MODEM
  // Show modem ISR stats from the previous transmission
  afsk_debug();
  #endif
  delay(50);//This is likely not necassary
  RS_UV3.print("pd1\r");
  RS_UV3.flush();
  clearSerialBuffers();
}

void clearSerialBuffers(){
  while(RS_UV3.available()){//Clear the read buffer in case anything is in it
    RS_UV3.read();
  }
  while(due_link.available()){
    due_link.read();
  }
}

void radioReset(){
  RS_UV3.listen();

  RS_UV3.print("fs144390\r");
  RS_UV3.flush();
  delay(50);

  RS_UV3.print("sq9\r");//Set squelch to 9, ignore all incoming traffic
  RS_UV3.flush();
  delay(50);

  RS_UV3.print("PW0\r");//This sets to LOW power!!! or does it?
  RS_UV3.flush();
  delay(50);
  //last item: RS_UV3 is placed into low power mode in order to save battery. It will then be woken whenever data need to be sent
  RS_UV3.print("pd1\r");//Turn off the transiever chip
  RS_UV3.flush();
  delay(50);//Delay for 50 milliseconds to ensure command is copleted
  clearSerialBuffers();
}
