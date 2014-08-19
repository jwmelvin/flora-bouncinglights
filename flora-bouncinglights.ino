#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_LSM303.h>
#include <CapacitiveSensor.h>

#define debug true
#define btnTHRESH 100
#define milLOOPBTN 2000
#define milLOOPLED 50

#define FRONTCIRCLE true
#define RESETACCEL_WITHMEASURE true

#define pinPIXELS 10
#define numPIXFRONT 12
#define numSHIFTFRONT 3
#define numPIXBACK 26
#define wheelAccelMAX 170

// change this to display different patterns
// currently 0 & 1 are valid
uint8_t pattern = 0; 

const uint8_t numPIXELS = numPIXFRONT + numPIXBACK;
// LED pixels
Adafruit_NeoPixel strip = \
	Adafruit_NeoPixel(numPIXELS, pinPIXELS, NEO_GRB + NEO_KHZ800);
uint32_t colorPixel[numPIXBACK];
uint8_t iPixelMap;

// color sensor
Adafruit_TCS34725 colorsense = \
	Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
uint16_t clear, red, green, blue;
uint32_t colorMeasured = strip.Color(255, 0, 0);
float hueMeasured;

// accelerometer + magnemometer
Adafruit_LSM303 lsm;
float accelZeroX;
float accelX_last;
float accelX;
float accelX_max;

// capactive touch
CapacitiveSensor   btn1 = CapacitiveSensor(12,6);
long btnValue;

// timing loops
long milLoopBtn_last;
long milLoopLED_last;

void setup() {
	if(debug) Serial.begin(115200);
	colorsense.begin();
	colorsense.setInterrupt(true);  // turn off LED
	lsm.begin();
	strip.begin();
	strip.setBrightness(16);
	strip.show(); // Initialize all pixels to 'off'
	accelZero();
}

void loop() {
	if (millis()-milLoopLED_last >= milLOOPLED){
		milLoopLED_last=millis();
		accelRead();
		displayUpdate();
	}
	if (millis()-milLoopBtn_last >= milLOOPBTN){
		checkButton();
		milLoopBtn_last=millis();
	}
}

void displayUpdate(){
	if(debug){Serial.print("displayUpdate "); Serial.println(millis());}
	if (pattern == 0) {
		/* NEW DESIGN, tickertape of acceleration history shown by color */
		uint8_t iDisplay, i, colorCurrent;
		// set the color of the latest pixel
		// base point set by measured color, shifted by acceleration
		if (accelX > 0){
			colorCurrent = int(255*hueMeasured + wheelAccelMAX * accelX / accelX_max);
			colorPixel[iPixelMap] = Wheel(colorCurrent);
		} else colorPixel[iPixelMap] = 0;
		// send the stream of pixels to the display, going back through history
		// and wrap around the history array when necessary
		// for the Front display, shift around to relocate start point
		for (i=0; i<numPIXFRONT; i++){
			if (FRONTCIRCLE){
				if (iPixelMap >= i) iDisplay = iPixelMap - i;
				else iDisplay = numPIXBACK + (iPixelMap - i);
			} else {
				if (i<numPIXFRONT/2){
					// move backwards through history (iPixelMap array)
					if (iPixelMap >= i) iDisplay = iPixelMap - i;
					else iDisplay = numPIXBACK + (iPixelMap - i);
				} else {
					//move forwards through history (iPixelMap array)
					// if (i >= numPIXFRONT) i = numPIXFRONT-i-1;
					if (iPixelMap >= i) iDisplay = iPixelMap - (numPIXFRONT-i-1);
					else iDisplay = numPIXBACK + (iPixelMap - (numPIXFRONT-i-1));
				}			
			}
			strip.setPixelColor(shiftPixelFront(i), colorPixel[iDisplay]);
			if(debug){
				Serial.print(" pixF:");Serial.print(shiftPixelFront(i));
				Serial.print(" clrF:");Serial.print(colorPixel[iDisplay]);
			}
			//strip.setPixelColor(i, colorPixel[iDisplay]);
		}
		for (i=numPIXFRONT; i<numPIXBACK+numPIXFRONT; i++){
			if (iPixelMap >= (i - numPIXFRONT)) iDisplay = iPixelMap - (i - numPIXFRONT);
			else iDisplay = numPIXBACK + (iPixelMap - (i - numPIXFRONT));
			strip.setPixelColor(i, colorPixel[iDisplay]);
			if(debug){
				Serial.print(" pixB:");Serial.print(i);
				Serial.print(" clrB:");Serial.print(colorPixel[iDisplay]);
			}
		}
		/*
		if(debug){
			Serial.print("iPM:");Serial.print(iPixelMap);
			Serial.print(" cCur:");Serial.print(colorCurrent);
		}*/
		// advance to the next pixel in the history array for next time
		iPixelMap++;
		if (iPixelMap > numPIXBACK - 1) iPixelMap = 0;
	} else if (pattern == 1) {
		/* ORIGINAL DESIGN, bar graph of instant acceleration */
		// front
		uint8_t numShow = int(numPIXFRONT * accelX / accelX_max); 
		for (uint8_t i=0; i<numShow; i++){
			strip.setPixelColor(i, colorMeasured);
		}
		for (uint8_t i=numShow; i<numPIXFRONT; i++){
			strip.setPixelColor(i, 0);
		}
		// back
		numShow = int(numPIXBACK * accelX / accelX_max); 
		for (uint8_t i=0; i<numShow; i++){
			strip.setPixelColor(i + numPIXFRONT, colorMeasured);
		}
		for (uint8_t i=numShow; i<numPIXBACK; i++){
			strip.setPixelColor(i + numPIXFRONT, 0);
		}
	}
	strip.show();
}

void checkButton(){
	btnValue =  btn1.capacitiveSensor(10);
	if(debug){Serial.print("Btn="); Serial.println(btnValue);}
	if(btnValue > btnTHRESH){
		colorMeasure();
	}
}

void accelZero(){
	lsm.read();
	accelZeroX = lsm.accelData.x;
	if(debug){Serial.print("a0X="); Serial.println(accelZeroX);}
}

void colorMeasure(){
	float r, g, b;
	colorsense.setInterrupt(false);      // turn on LED
	delay(60);  // takes 50ms to read
	colorsense.getRawData(&red, &green, &blue, &clear);
	colorsense.setInterrupt(true);  // turn off LED
	uint32_t sum = clear;
	r = red; r /= sum;
	g = green; g /= sum;
	b = blue; b /= sum;
	r *= 256; g *= 256; b *= 256;
	hueMeasured = rgb2h(r,g,b);
	uint8_t tmpR, tmpG, tmpB;
	hsv2rgb(hueMeasured,1.0,1.0, tmpR,tmpG,tmpB);
	colorMeasured = strip.Color(tmpR,tmpG,tmpB); 
	colorWipe(colorMeasured, 100);
	if (RESETACCEL_WITHMEASURE) {
		accelZero();
		accelX_max = 0;
	}
}

void accelRead(){
	lsm.read();	
	accelX = lsm.accelData.x - accelZeroX;
		if(debug){
			Serial.print("aX_raw="); Serial.print(lsm.accelData.x);
			Serial.print(" aX="); Serial.println(accelX);
		}
	if (accelX < 0) accelX = 0;
	if (accelX > accelX_max) accelX_max = accelX;
	accelX_last = accelX;
}

int shiftPixelFront(uint8_t pos){
	if (pos >= numPIXFRONT - numSHIFTFRONT) pos = pos - numPIXFRONT + numSHIFTFRONT; 
	else pos = pos + numSHIFTFRONT;
	return pos;
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

double rgb2h(uint8_t r, uint8_t g, uint8_t b) {
    double rd = (double) r/255;
    double gd = (double) g/255;
    double bd = (double) b/255;
    double max = max(rd, max(gd, bd)), min = min(rd, min(gd, bd));
    double h;
    double d = max - min;

    if (max == min) { 
        h = 0; // achromatic
    } else {
        if (max == rd) {
            h = (gd - bd) / d + (gd < bd ? 6 : 0);
        } else if (max == gd) {
            h = (bd - rd) / d + 2;
        } else if (max == bd) {
            h = (rd - gd) / d + 4;
        }
        h /= 6;
	}
	return h;
}

#define LED_BRIGHT 255
void hsv2rgb ( float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
	byte var_i;
	float var_1, var_2, var_3, var_h, var_r, var_g, var_b;

	if ( s == 0 ) { //HSV values = 0 Ã· 1
		r = v * LED_BRIGHT;
		g = v * LED_BRIGHT;
		b = v * LED_BRIGHT;
	}
	else {
		var_h = h * 6;
		if ( var_h == 6 ) var_h = 0;	// H must be < 1
		var_i = byte( var_h ) ;		// Or ... var_i = floor( var_h )
		var_1 = v * ( 1 - s );
		var_2 = v * ( 1 - s * ( var_h - var_i ) );
		var_3 = v * ( 1 - s * ( 1 - ( var_h - var_i ) ) );

		switch (var_i){
			case 0:
				var_r = v;
				var_g = var_3;
				var_b = var_1;
			break;
			
			case 1:
				var_r = var_2;
				var_g = v;
				var_b = var_1;
			break;
			
			case 2:
				var_r = var_1;
				var_g = v;
				var_b = var_3;
			break;
			
			case 3:
				var_r = var_1;
				var_g = var_2;
				var_b = v;
			break;
			
			case 4:
				var_r = var_3;
				var_g = var_1;
				var_b = v;
			break;
			
			default:
				var_r = v;
				var_g = var_1;
				var_b = var_2;
			break;
		}
		r = var_r * LED_BRIGHT;
		g = var_g * LED_BRIGHT;
		b = var_b * LED_BRIGHT;
	}
}

/*
void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void colorRandom(){
	uint8_t colorRand = random(0,255);
	for (uint8_t i=0; i<strip.numPixels(); i++){
		strip.setPixelColor(i, Wheel(colorRand));
	}
	strip.show();
}

void colorBlank(){
	for (uint8_t i=0; i<strip.numPixels(); i++){
		strip.setPixelColor(i, 0);
	}
	strip.show();
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}
*/