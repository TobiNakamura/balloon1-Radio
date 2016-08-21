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
#define max_buffer_length 50 //placeholder, replace with more accurate numbers
#define squelchPin 7 //connected to DOUT of

SoftwareSerial due_link(5,6);
SoftwareSerial RS_UV3(8,9);

uint8_t stat_buffer_count = 0;
char status_buffer[max_buffer_length] = {}; //debug data to be sent to due for storage
//Format as follows
//system reset: 00
//due forced radio reset: 01
char lat_buffer[] = {"4916.38"};
char lon_buffer[] = {"12255.28"};
char tim_buffer[] = {"280720"};
char alt_buffer[] = {"0000000000"};
char msg_buffer[100] = {};
char param_buffer[10] = {};
char cmd_temperature[] = {"TP\r"};
char cmd_voltage[] = {"VT\r"};
char parsed_data[5] = {}; // used for parsing data from radio before sending it to due
uint8_t written = 0; //number of bytes written into a buffer
char commandChar = 0;
char cw_buffer[16] = {};

void clearSerialBuffers();
void transmitService(char *lat, char *lon, char *time, char *alt, char *msg);
void radioReset();
void getRadioStatus(char *command);
void sendCW(char *lat, char *lon);

void setup(){
  Serial.begin(115200);
  due_link.begin(19200); //all communication between due an uno will be terminated by \r
  RS_UV3.begin(19200);
  pinMode(13, OUTPUT);
  pinMode(squelchPin, INPUT);
  radioReset();
  afsk_setup();

  pin_write(LED_PIN, LOW);

#ifdef debug
  Serial.println("Uno System Reset");

  transmitService(lat_buffer, lon_buffer, tim_buffer, alt_buffer, msg_buffer);
    delay(2000);

#endif

  due_link.listen();

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

  param_buffer[0] = 0;
  RS_UV3.listen();
  RS_UV3.print("tp\r");
  written = RS_UV3.readBytesUntil('\r', param_buffer, max_buffer_length);
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
}


void loop(){
  if(stat_buffer_count>=max_buffer_length){
    //is there a way to shift the buffer?
    ///max_buffer_length = 0;
  }


  due_link.listen();
  if(due_link.available()) {
    commandChar = due_link.read();
    Serial.print("rcv: ");
    Serial.write(commandChar);
    if(commandChar == 'a'){
      lat_buffer[0] = 0;
      lon_buffer[0] = 0;
      tim_buffer[0] = 0;
      alt_buffer[0] = 0;
      msg_buffer[0] = 0;
      written = due_link.readBytesUntil('\t',lat_buffer, max_buffer_length);
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
      Serial.print("epoch ");
      Serial.print(tim_buffer);
      Serial.print("altitude ");
      Serial.print(alt_buffer);
      Serial.print("m ");
      Serial.println(msg_buffer);
  #endif
      transmitService(lat_buffer, lon_buffer, tim_buffer, alt_buffer, msg_buffer);
    }else if(commandChar == 'v'){
      getRadioStatus(cmd_voltage);
      strncpy(parsed_data, param_buffer+5, 4);
      parsed_data[4] = '\r';
      parsed_data[5] = 0;
      due_link.write(parsed_data);
    } else if(commandChar == 't'){
      getRadioStatus(cmd_temperature);
      strncpy(parsed_data, param_buffer+6, 2);
      parsed_data[2] = '\r';
      parsed_data[3] = 0;
      due_link.write(parsed_data);
    } else if(commandChar == 's'){//Startup setup
      due_link.print("ack\r"); //getting ack \n at due (space in between ack and \n)
      #ifdef debug
      Serial.println("Due: Resetting Radio Configuration");
      #endif
      radioReset();
      due_link.listen();
    } else if(commandChar == 'd'){
      due_link.write(digitalRead(squelchPin));
      stat_buffer_count = 0;
    } else {
      #ifdef debug
      Serial.println("Invalid command char from Due, discarding buffer");
      #endif
      clearSerialBuffers();
    }
  }
}


void getRadioStatus(char *command) {
  param_buffer[0] = 0;
  RS_UV3.listen();
  RS_UV3.print(command);
  written = RS_UV3.readBytesUntil('\r', param_buffer, max_buffer_length);
  param_buffer[written] = '\r';
  param_buffer[written+1] = 0;//Still need to NULL terminate
  due_link.write(param_buffer);
  due_link.flush();
  #ifdef debug
  Serial.write(param_buffer);
  Serial.println();
  #endif
  clearSerialBuffers();
  due_link.listen();
}

void transmitService(char *lat, char *lon, char *tim, char *alt, char *msg){
  RS_UV3.print("pd0\r");//Power up the transiever
  RS_UV3.flush();//Ensure write is complete
  delay(1000);//Ensure the transiever has powered on
  aprs_send(lat, lon, tim, alt, msg); //lat, lon, time is decimal number only, N,W,H added in aprs.cpp
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

void sendCW(char *lat, char *lon) {
  cw_buffer[0] = 0;
  strcpy(cw_buffer, "CW");
  strcat(cw_buffer, lat);
  strcat(cw_buffer, lon);
  cw_buffer[14] = '\r';
  cw_buffer[15] = 0;
  RS_UV3.write(cw_buffer);
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

  //check if the radio is on channel 0
  //and then power off
  RS_UV3.print("fs144390\r");
  RS_UV3.flush();
  delay(50);

  RS_UV3.print("SQ8\r");//Set squelch to 8 for proximity detection
  RS_UV3.flush();
  delay(50);

  RS_UV3.print("AI0\r");//digital pin to show squelch
  RS_UV3.flush();
  delay(50);

  RS_UV3.print("PW1\r");//This sets to HIGH power!!! Tobi confirms
  RS_UV3.flush();
  delay(50);

    RS_UV3.print("DP0\r");//
    RS_UV3.flush();
    delay(50);

      RS_UV3.print("AF0\r");//
      RS_UV3.flush();
      delay(50);

        RS_UV3.print("HP0\r");//
        RS_UV3.flush();
        delay(50);

  //last item: RS_UV3 is placed into low power mode in order to save battery. It will then be woken whenever data need to be sent
  RS_UV3.print("pd1\r");//Turn off the transiever chip
  RS_UV3.flush();
  delay(50);//Delay for 50 milliseconds to ensure command is copleted
  clearSerialBuffers();
}
