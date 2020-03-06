/*
	Highvoltage Programmer for AVR using ESP8266
	R. J. Tidey 2020/02/29
*/
#include "BaseConfig.h"
#define AP_AUTHID ""
#define	PIN_12V		14	// Control 12V programming voltage High off
#define	PIN_5V		15	// Control power to AVR LOW off
#define	PIN_SCO		4	// AVR SCI ATTiny Pin 2
#define	PIN_SDI		5	// AVR SDO ATTiny Pin 7
#define	PIN_SIO		12	// AVR SII ATTiny Pin 6
#define	PIN_SDO		13	// AVR SDI ATTiny Pin 5 

byte fuseHigh = 93;
byte fuseLow = 225;   
byte fuseExt = 254;   

void setupStart() {
	digitalWrite(PIN_12V, HIGH);	// 12V off
	digitalWrite(PIN_5V, LOW);		// 5V off
	digitalWrite(PIN_SCO, LOW);		// Clock low
	pinMode(PIN_5V, OUTPUT);
	pinMode(PIN_12V, OUTPUT);
	pinMode(PIN_SDO, OUTPUT);
	pinMode(PIN_SIO, OUTPUT);
	pinMode(PIN_SCO, OUTPUT);
	pinMode(PIN_SDI, OUTPUT);
}

void handleWriteFuses() {
	String temp1, temp2;
	int val1, val2;
	if (AP_AUTHID != "" && server.arg("auth") != AP_AUTHID) {
		Serial.println("Unauthorized");
		server.send(200, "text/html", "Unauthorized");
	} else {
		temp1 = server.arg("fuseHigh");
		temp2 = server.arg("fuseLow");
		if(temp1.length() && temp2.length()) {
			fuseHigh = temp1.toInt();
			fuseLow = temp2.toInt();
			writeFuses();
			server.send(200, "text/html", "fuses written");
		} else {
			server.send(200, "text/html", "bad arguments");
		}
	}
}

void handleReadFuses() {
	readFuses();
	String status = String(fuseHigh);
	status += ":" + String(fuseLow);
	status += ":" + String(fuseExt);
	server.send(200, "text/html", status);
} 

void extraHandlers() {
	server.on("/writeFuses", handleWriteFuses);
	server.on("/readFuses", handleReadFuses);
}
 
void setupEnd() {
}

//functions for reading and writing fuses
void pulseClock() {
	digitalWrite(PIN_SCO, HIGH);
  	digitalWrite(PIN_SCO, LOW);
}

int shiftControl(byte val, byte val1)
{
	unsigned long t = millis();
	int i;
	int inData = 0;
	//Wait for PIN_SDI high
	while (!digitalRead(PIN_SDI)) {
		if(millis() -t > 200) {
			Serial.println("Shift control timeout waiting for SDI");
			return -1;
		}
	}
	Serial.println("Shift data");
	//Start bit
	digitalWrite(PIN_SDO, LOW);
	digitalWrite(PIN_SIO, LOW);
	pulseClock();
    
	int mask = 128;    
	for (i = 0; i < 8; i++)  {
		digitalWrite(PIN_SDO, (val & mask) ? 1:0);
        digitalWrite(PIN_SIO, (val1 & mask) ? 1:0);
		mask >>=1;
        inData <<=1;
        inData |= digitalRead(PIN_SDI);
        pulseClock();
	}
	//End bits
	digitalWrite(PIN_SDO, LOW);
	digitalWrite(PIN_SIO, LOW);
	pulseClock();
	pulseClock();
	return inData;
}

void startHV() {
    // Initialize pins to enter programming mode
	Serial.println("StartHV" + String(millis()));
    pinMode(PIN_SDI, OUTPUT);  //Temporary
    digitalWrite(PIN_SDO, LOW);
    digitalWrite(PIN_SIO, LOW);
    digitalWrite(PIN_SDI, LOW);
    digitalWrite(PIN_5V, LOW);  // Power off 5V
    digitalWrite(PIN_12V, HIGH); // Power off 12V
    delayMicroseconds(200);
	
    // Enter High-voltage Serial programming mode
    digitalWrite(PIN_5V, HIGH);  // Apply power to AVR device
    delayMicroseconds(50);
    digitalWrite(PIN_12V, LOW);   //Turn on 12v
    delayMicroseconds(10);
    pinMode(PIN_SDI, INPUT);   //Release PIN_SDI
    delayMicroseconds(300);
}

void endHV() {
	Serial.println("EndHV" + String(millis()));
    digitalWrite(PIN_12V, HIGH);   //Turn off 12v
    digitalWrite(PIN_5V, LOW);   //Turn off 12v
}

int writeFuse(byte fuse, byte ctl1, byte ctl2) {
	int ret = 0;
    ret = shiftControl(0x40, 0x4C);
    if(ret >=0) ret = shiftControl(fuse, 0x2C);
	if(ret >=0) ret = shiftControl(0x00, ctl1);
    if(ret >=0) ret = shiftControl(0x00, ctl2);
	return ret;
}

int writeFuses() {
	int ret;
	Serial.println("write high:" + String(fuseHigh,HEX) + " write low:" + String(fuseLow, HEX));
    startHV();
    //Write fuseHigh
    Serial.println("Writing fuseHigh:" + String(millis()));
	ret = writeFuse(fuseHigh, 0x74, 0x7C);
    
    if(ret >= 0) {
		//Write fuseLow
		Serial.println("Writing fuseLow:" + String(millis()));
		writeFuse(fuseLow, 0x64, 0x6C);
	}
	endHV();
	return ret;
}

int readFuses(){
	int ret = 0;
	startHV();
     //Read fuseLow
	Serial.println("Reading fuseLow:" + String(millis()));
    ret = shiftControl(0x04, 0x4C);
    if(ret >=0) ret = shiftControl(0x00, 0x68);
	if(ret >=0) ret = shiftControl(0x00, 0x6C);
	if(ret >= 0) {
		fuseLow = ret;
		Serial.println("Reading fuseHigh:" + String(millis()));
		//Read fuseHigh
		ret = shiftControl(0x04, 0x4C);
		if(ret >=0) ret = shiftControl(0x00, 0x7A);
		if(ret >=0) ret = shiftControl(0x00, 0x7E);
		if(ret >= 0) {
			fuseHigh = ret;
			Serial.println("Reading fuseExt:" + String(millis()));
			//Read fuseExt
			ret = shiftControl(0x04, 0x4C);
			if(ret >=0) ret = shiftControl(0x00, 0x6A);
			if(ret >=0) ret = shiftControl(0x00, 0x6E);
			if(ret >=0) {
				fuseExt = ret;
			}
		}
	}
	endHV();
	Serial.println("fuses H:" + String(fuseHigh,HEX) + " L:" + String(fuseLow,HEX) + " E:" + String(fuseExt,HEX));
	return ret;
}

void loop() {
	server.handleClient();
	wifiConnect(1);
	delaymSec(10);
}
