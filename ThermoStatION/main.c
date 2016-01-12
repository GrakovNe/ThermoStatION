#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/eeprom.h>

#include "lcd_lib.h"

#define MINIMAL_TEMP -85
#define MAXIMAL_TEMP 125

#define LED_PORT PORTA
#define LED_DDR DDRA
#define LED_OK_IO 7
#define LED_ERROR_IO 6

#define WIREPORT PORTA
#define WIREPIN  PINA
#define WIREBIT   0
#define WIREDDR  DDRA

#define BUZZER_PORT  PORTB
#define BUZZER_DDR   DDRB
#define BUZZER_IO    1

#define UP_BTN_PORT   PORTD
#define OK_BTN_PORT   PORTD
#define DOWN_BTN_PORT PORTB
#define UP_BTN_IO     2
#define OK_BTN_IO     3
#define DOWN_BTN_IO   2

#define UP_LEFT_CORNER     0x00
#define UP_RIGHT_CORNER    0x01
#define DOWN_LEFT_CORNER   0x02
#define DOWN_RIGHT_CORNER  0x03
#define DEGREE_SYMBOL      0x04
#define THERMOSTAT_SYMBOL  0x05

#define startSound(); TCCR0=0x02;
#define stopSound(); TCCR0=0x00;

#define MINIMAL_TEMP_EEPROM_ADDRESS (void*)0x00
#define MAXIMAL_TEMP_EEPROM_ADDRESS (void*)0x05
#define THERMOSTATE_EEPROM_ADDRESS  (void*)0x0A

void initialization();
inline void sendSpecSymbolsToDisplay();
inline void drawTemperature();
inline void drawMenu();
inline void checkTemperature();
inline void updateDisplay();
inline void sendUARTData();
void UARTSend(char Symbol);
void UARTSendString(char *String);
int MeansureTemp();
void wireSend(char byte);
char wireRead();

/*LCD and UART text buffer*/
char displayTemp[54];

/*mode of work*/
char workMode = 0;
/*
0 - normal thermometer/thermostat
1 - settings screen
2 - thermostat captured incorrect temp
*/

/*item of menu*/
char menuItem = 0;
/*
0 - switching thermostat
1 - minimal temp
2 - maximal temp
3 - about
*/

/*have information about current temperature*/
int currentTemp = 0;

/*Settings for thermostat*/
struct{
	int minimalTemp;
	int maximalTemp;
	char isActive;
} thermostatSettings;

/*Sound generation timer*/
ISR(TIMER0_OVF_vect){
	static char state;
	static int counter;
	static char buzzerFlag;
	
	if (counter < 4000){
		counter++;
	}
	else{
		counter = 0;
	}
	
	if (counter < 2000){
		buzzerFlag = 1;
	}
	
	else {
		buzzerFlag = 0;
	}
	
	
	if (buzzerFlag){
		if (state == 3){
			state = 0;
			BUZZER_PORT |= (1 << BUZZER_IO);
			return;
		}
		
		state ++;
		BUZZER_PORT &= ~(1 << BUZZER_IO);
		return;
	}

	
	
}

/*Common checks*/
ISR (TIMER1_COMPA_vect){
	TCNT1H=0x00;
	TCNT1L=0x00;
	checkTemperature();
	updateDisplay();
	sendUARTData();
	currentTemp = MeansureTemp();
}

/*UP button*/
ISR (INT0_vect){
	if (workMode == 0){
		return;
	}
	
	switch (menuItem){
		case 0:
		thermostatSettings.isActive = 1;
		break;
		
		case 1:
		thermostatSettings.minimalTemp ++;
		if (thermostatSettings.minimalTemp > MAXIMAL_TEMP){
			thermostatSettings.minimalTemp = MINIMAL_TEMP;
		}
		break;
		
		case 2:
		thermostatSettings.maximalTemp ++;
		if (thermostatSettings.maximalTemp > MAXIMAL_TEMP){
			thermostatSettings.maximalTemp = MINIMAL_TEMP;
		}
		break;
	}
	
	drawMenu();
}

/*Ok button*/
ISR(INT1_vect){
	if (workMode == 0){
		workMode = 1;
		menuItem = 0;
		updateDisplay();
		return;
	}
	
	if (workMode == 2){
		workMode = 0;
		thermostatSettings.isActive = 0;
		checkTemperature();
		return;
	}
	
	if ( (workMode == 1) && (menuItem == 3) ){
		workMode = 0;
		updateDisplay();
		return;
	}
	
	menuItem++;
	eeprom_write_dword(MINIMAL_TEMP_EEPROM_ADDRESS, thermostatSettings.minimalTemp);
	eeprom_write_dword(MAXIMAL_TEMP_EEPROM_ADDRESS, thermostatSettings.maximalTemp);
	eeprom_write_byte(THERMOSTATE_EEPROM_ADDRESS, thermostatSettings.isActive);
	updateDisplay();
	
	
}

/*Down Button*/
ISR(INT2_vect){
	if (workMode == 0){
		return;
	}
	
	switch (menuItem){
		case 0:
			thermostatSettings.isActive = 0;
			break;
		
		case 1:
			thermostatSettings.minimalTemp --;
			if (thermostatSettings.minimalTemp < MINIMAL_TEMP){
				thermostatSettings.minimalTemp = MAXIMAL_TEMP;
			}
			break;
			
		case 2:
			thermostatSettings.maximalTemp --;
			if (thermostatSettings.maximalTemp < MINIMAL_TEMP){
				thermostatSettings.maximalTemp = MAXIMAL_TEMP;
			}
			break;
	}
	
	drawMenu();

}

/*initialization of 1-Wire bus*/
char wireInit(){
	char IsAnswered;
	WIREDDR |= 1<<WIREBIT;
	WIREPORT |= 1<<WIREBIT;
	_delay_ms (1);
	WIREPORT &= ~(1<<WIREBIT);
	_delay_us(480);
	WIREPORT |= 1<<WIREBIT;
	WIREDDR &= ~(1<<WIREBIT);
	_delay_us (60);
	if (WIREPIN & (1<<WIREBIT))
	IsAnswered = 1;
	else
	IsAnswered = 0;
	_delay_us(480);
	return IsAnswered;
}

/*send byte to 1-wire*/
void wireSend(char byte){
	char Symbol = byte;
	WIREDDR |= (1 << WIREBIT);
	for (int i = 0; i < 8; i++){
		WIREPORT |= (1 << WIREBIT);
		_delay_us(6);
		WIREPORT &= ~(1 << WIREBIT);
		_delay_us(10);
		if (Symbol & 1){
			WIREPORT |= (1 << WIREBIT);
		}
		_delay_us(110);
		Symbol = Symbol >> 1;
	}
	WIREPORT |= (1 << WIREBIT);
	WIREDDR &= ~(1 << WIREBIT);
}

/*read byte from 1-wire*/
char wireRead(){
	char Symbol = 0x00;
	WIREPORT |= (1 << WIREBIT);
	_delay_us(2);
	for (int i = 0; i < 8; i++){
		WIREDDR |= (1 << WIREBIT);
		
		WIREPORT |= (1 << WIREBIT);
		_delay_us(2);
		
		WIREPORT &= ~(1<<WIREBIT);
		_delay_us(5);
		
		WIREDDR &= ~(1<<WIREBIT);
		WIREPORT |= (1 << WIREBIT);
		_delay_us(10);
		
		if (WIREPIN & (1<<WIREBIT))
		Symbol |= 0x80>>(7 - i);
		_delay_us(110);
	}
	return Symbol;
}

/*full algorithm for temperature measuring without delays for cooking new value*/
int MeansureTemp(){
	wireInit();
	wireSend(0xCC);
	wireSend(0xBE);

	int Temp[2];
	Temp[0] = wireRead();
	Temp[1] = wireRead();
	int Result = ((Temp[1]<<8) + Temp[0]) >> 4;

	wireInit();
	wireSend(0xCC);
	wireSend(0x44);
	
	return Result;
}

/*will update info on a screen. Uses WorkMode*/
inline void updateDisplay(){
	switch (workMode){
		case 0:
		case 2:
		drawTemperature();
		break;
		case 1:
		displayClear();
		drawMenu();
		break;
	}
}

/*this function will made first-step initialization of MCU*/
void initialization(){
	/*work with IO Pins*/
	BUZZER_DDR  |= (1 << BUZZER_IO);
	
	LED_DDR |= (1 << LED_OK_IO) | (1 << LED_ERROR_IO);
	
	UP_BTN_PORT   |= (1 << UP_BTN_IO);
	OK_BTN_PORT   |= (1 << OK_BTN_IO);
	DOWN_BTN_PORT |= (1 << DOWN_BTN_IO);
	
	/*LCD initialization*/
	displayInit();
	
	/*work with DS18B20*/
	if (wireInit()){
		displayClear();
		displayGoto(1,0);
		displaySendString("NO TEMPERATURE");
		displayGoto(5,1);
		displaySendString("SENSOR");
		while(1);
	}
	MeansureTemp();
	
	/*work with LCD*/
	
	sendSpecSymbolsToDisplay();
	displayClear();
	displayWriteSymbol(UP_LEFT_CORNER);
	displayGoto(0,1);
	displayWriteSymbol(DOWN_LEFT_CORNER);
	displayGoto(15,0);
	displayWriteSymbol(UP_RIGHT_CORNER);
	displayGoto(15,1);
	displayWriteSymbol(DOWN_RIGHT_CORNER);

	displayGoto(1,0);
	displaySendString("INITIALIZATION");
	displayGoto(1,1);
	for (int i = 0; i < 14; i++){
		displayWriteSymbol('=');
		_delay_ms(200);
	}
	
	/*work with Interrupts*/
	
	GICR|=0xE0;
	MCUCR=0x00;
	MCUCSR=0x00;
	GIFR=0xE0;
	
	/*work with UART*/
	UCSRA=0x00;
	UCSRB=0x08;
	UCSRC=0x86;
	UBRRH=0x00;
	UBRRL=0x33;
	
	/*work with timers*/
	/*timer 0 needs for sound generation*/
	TCCR0=0x00;
	
	/*timer 1 needs for checking temp and refresh state*/
	TCCR1A=0x00;
	TCCR1B=0x05;
	OCR1AH=0x1E;
	OCR1AL=0x85;
	
	TIMSK=0x11;
	
	/*updating temperature*/
	MeansureTemp();
	currentTemp = MeansureTemp();
	
	/*work with eeprom memory*/
	
	thermostatSettings.minimalTemp = eeprom_read_dword(MINIMAL_TEMP_EEPROM_ADDRESS);
	thermostatSettings.maximalTemp = eeprom_read_dword(MAXIMAL_TEMP_EEPROM_ADDRESS);
	thermostatSettings.isActive = eeprom_read_word(THERMOSTATE_EEPROM_ADDRESS);
	
	/*Finished*/
	
	TCNT1H = 0x1E;
	TCNT1L = 0x80;
	displayClear();
	sei();
}

/*This function send custom symbols into HD44780 RAM memory*/
inline void sendSpecSymbolsToDisplay(){
	char specialSymbols[6][8] = {
		{0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},  // Code for up-left corner
		{0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},  // Code for up-right corner
		{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x00},  // Code for down-left corner
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F, 0x00},  // Code for down-right corner
		{0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00, 0x00},  // Code for degree symbol
		{0x04, 0x0E, 0x1F, 0x00, 0x1F, 0x0E, 0x04, 0x00}  // Code for thermostat symbol
	};
	for (int i = 0; i < 6; i++){
		displayMakeCustomSymbol(specialSymbols[i], i);
	}

}

/*will draw standby information on a display*/
inline void drawTemperature(){
	displayGoto(0,0);
	sprintf(displayTemp, "NOW:       %+04i", currentTemp);
	displaySendString(displayTemp);
	displayWriteSymbol(DEGREE_SYMBOL);
	
	displayGoto(0,1);
	if (thermostatSettings.isActive){
		displayWriteSymbol(THERMOSTAT_SYMBOL);
	}
	else{
		displayWriteSymbol('t');
	}
	
	displayGoto(3,1);
	
	sprintf(displayTemp, " %+03i", thermostatSettings.minimalTemp);
	displaySendString(displayTemp);
	displayWriteSymbol(DEGREE_SYMBOL);

	sprintf(displayTemp, " - %+04i", thermostatSettings.maximalTemp);
	displaySendString(displayTemp);
	displayWriteSymbol(DEGREE_SYMBOL);
}

/*this function shows menu. Uses WorkMode*/
inline void drawMenu(){
	displayGoto(0,0);
	
	switch (menuItem){
		case 0:
		displaySendString("THERMOSTAT");
		displayGoto(0,1);
		if (thermostatSettings.isActive){
			displaySendString("     SWITCHED ON");
		}
		else {
			displaySendString("    SWITCHED OFF");
		}
		break;
		
		case 1:
		displaySendString("MINIMAL TEMP");
		sprintf(displayTemp, "%+03d", thermostatSettings.minimalTemp);
		displayGoto(12,1);
		displaySendString(displayTemp);
		displayWriteSymbol(DEGREE_SYMBOL);
		break;
		
		case 2:
		displaySendString("MAXIMAL TEMP");
		sprintf(displayTemp, "%+04d", thermostatSettings.maximalTemp);
		displayGoto(11,1);
		displaySendString(displayTemp);
		displayWriteSymbol(DEGREE_SYMBOL);
		break;
		
		case 3:
		displaySendString("AUTHOR: GrakovNe");
		displayGoto(0,1);
		displaySendString("VERSION:    1.00");
	}

}

/*this function checks temperature for the thermostat values according*/
inline void checkTemperature(){
	if ( (thermostatSettings.isActive) && (workMode != 1) ){
		if ( (currentTemp > thermostatSettings.maximalTemp) || (currentTemp < thermostatSettings.minimalTemp) ){
			LED_PORT |= (1 << LED_ERROR_IO);
			LED_PORT &= ~(1 << LED_OK_IO);
			
			workMode = 2;
			
			startSound();
			return;
		}
		
		LED_PORT |= (1 << LED_OK_IO);
		LED_PORT &= ~(1 << LED_ERROR_IO);
		return;
	}
	
	LED_PORT &= ~(1 << LED_ERROR_IO);
	LED_PORT &= ~(1 << LED_OK_IO);
	stopSound();
}

/*will send 1 byte to UART*/
void UARTSend(char symbol){
	while(!( UCSRA & (1 << UDRE)));
	UDR = symbol;
}

/*will send string to UART. String must be ended in \0*/
void UARTSendString(char *string){
	short i = 0;
	char sym;
	while ((sym = string[i]) != '\0'){
		UARTSend(sym);
		i++;
	}
}

/*will send full data about temperature and parameters*/
void sendUARTData(){
	sprintf(displayTemp, "now: %i; min: %i; max: %i; thermostate: %i", currentTemp, thermostatSettings.minimalTemp, thermostatSettings.maximalTemp, thermostatSettings.isActive);
	UARTSendString(displayTemp);
	UARTSend(0x0A);
	UARTSend(0x0D);
}

/*main function*/
int main(void)
{
	initialization();	

	while (1)
	{
	}
}

