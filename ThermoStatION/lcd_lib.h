/************************************************************************/
/* HD44780 LIBRARY FOR WINSTAR DISPLAY                                  */
/* AUTHOR: GRAKOVNE                                                     */
/* DATE 11/01/16                                                        */
/* WEB: GRAKOVNE.ORG                                                    */
/* E-MAIL: GRAKOVNE@YANDEX.RU                                           */
/* опнасъ, янгдюбюрэ ксвьее                                             */
/************************************************************************/

#define F_CPU 8000000UL
#ifndef LCD_LIB_H
#define LCD_LIB_H
#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#define PORT_DATA PORTC
#define PIN_DATA  PINC
#define DDRX_DATA DDRC

#define PORT_SIG PORTC
#define PIN_SIG  PINC
#define DDRX_SIG DDRC

#define RS 0
#define RW 1
#define EN 2

#define BUS_4BIT
#define HD44780

#define displayGoto(x,y)        displaySendCommand(((((y)& 1)*0x40)+((x)& 15))|128) 
#define displayWriteRTL()       displaySendCommand(0x04)
#define displayWriteLTR()       displaySendCommand(0x06)
#define diplayTurnOff()         displaySendCommand(0x08)
#define displayTurnOn()         displaySendCommand(0x0C)
#define displayShowRectCursor() displaySendCommand(0x0D)
#define displayShowLineCursor() displaySendCommand(0x0E)
#define displayShowBothCursor() displaySendCommand(0x0F)
#define displayHideCursors()    displaySendCommand(0x0C)
#define displayScrollLeft()     displaySendCommand(0x05)
#define displayScrollRight()    displaySendCommand(0x07)
#define displayCursorLeft()     displaySendCommand(0x10)
#define displayCursorRight()    displaySendCommand(0x14)
#define displayWriteSymbol(sym) displaySendData(sym)

void displayInit(void);
void displayClear(void);
void displayCursorReset(void);
void displaySendData(char data); 
void displaySendCommand(unsigned char data); 
void displaySendStringFlash(char *str); 
void displaySendString(char *str);
void displayMakeCustomSymbol(char *symbolBytes, char number);
#endif