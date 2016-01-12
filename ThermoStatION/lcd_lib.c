/************************************************************************/
/* HD44780 LIBRARY FOR WINSTAR DISPLAY                                  */
/* AUTHOR: GRAKOVNE                                                     */
/* DATE 11/01/16                                                        */
/* WEB: GRAKOVNE.ORG                                                    */
/* E-MAIL: GRAKOVNE@YANDEX.RU                                           */
/* опнасъ, янгдюбюрэ ксвьее                                             */
/************************************************************************/

#include "lcd_lib.h"

inline unsigned char swapNibbles(unsigned char data)
{
	asm volatile("swap %0" : "=r" (data) : "0" (data));
	return data;

}

void displayCommandInit(unsigned char data)
{
	_delay_us(40);
	PORT_SIG &= ~(1 << RS);
	#ifdef BUS_4BIT
	PORT_DATA &= 0x0F;
	PORT_DATA |= data & 0xF0;
	#else
	PORT_DATA = data;
	#endif
	PORT_SIG |= (1 << EN);
	_delay_us(2);
	PORT_SIG &= ~(1 << EN);
}

inline static void displaySendPackage(unsigned char data)
{
	#ifdef BUS_4BIT
	PORT_DATA &= 0x0F;
	PORT_DATA |= data & 0xF0;
	PORT_SIG  |= (1 << EN);
	_delay_us(2);
	PORT_SIG &= ~(1 << EN);

	data = swapNibbles(data);
	PORT_DATA &= 0x0F;
	PORT_DATA |= data &0xF0;

	PORT_SIG |= (1 << EN);
	_delay_us(2);
	PORT_SIG &= ~(1 << EN);
	#else
	PORT_DATA = data;
	PORT_SIG |= (1 << EN);
	_delay_us(2);
	PORT_SIG &= ~(1 << EN);
	#endif
	_delay_us(50);
}

inline static void displayWait(void)
{
	#ifdef CHECK_FLAG_BF
	#ifdef BUS_4BIT
	
	unsigned char data;
	DDRX_DATA &= 0x0F;
	PORT_DATA |= 0xF0;
	PORT_SIG |= (1 << RW);
	PORT_SIG &= ~(1 << EN);
	do{
		PORT_SIG |= (1 << EN);
		_delay_us(2);
		data = PIN_DATA & 0xF0;
		PORT_SIG &= ~(1 << EN);
		data = swapNibbles(data);
		PORT_SIG |= (1 << EN);
		_delay_us(2);
		data |= PIN_DATA & 0xF0;
		PORT_SIG &= ~(1 << EN);
		data = swapNibbles(data);
	}while(data & 0x80);
	PORT_SIG &= ~(1 << RW);
	DDRX_DATA |= 0xF0;

	#else
	unsigned char data;
	DDRX_DATA = 0;
	PORT_DATA = 0xFF;
	PORT_SIG |= (1 << RW);
	PORT_SIG &= ~(1 << RS);
	do{
		PORT_SIG |= (1 << EN);
		_delay_us(2);
		data = PIN_DATA;
		PORT_SIG &= ~(1 << EN);
	}while(data & 0x80);
	PORT_SIG &= ~(1 << RW);
	DDRX_DATA = 0xFF;
	#endif
	#else
	_delay_us(40);
	#endif
}

void displaySendCommand(unsigned char data)
{
	displayWait();
	PORT_SIG &= ~(1 << RS);
	displaySendPackage(data);
}

void displaySendData(char data)
{
	displayWait();
	PORT_SIG |= (1 << RS);
	displaySendPackage(data);
}

void displayInit(void)
{
	#ifdef BUS_4BIT
	DDRX_DATA |= 0xF0;
	PORT_DATA |= 0xF0;
	#else
	DDRX_DATA |= 0xFF;
	PORT_DATA |= 0xFF;
	#endif
	
	DDRX_SIG |= (1<<RW)|(1<<RS)|(1<<EN);
	PORT_SIG |= (1<<RW)|(1<<RS)|(1<<EN);
	PORT_SIG &= ~(1 << RW);
	_delay_ms(40);
	
	#ifdef HD44780
	displayCommandInit(0x30);
	_delay_ms(10);
	displayCommandInit(0x30);
	_delay_ms(1);
	displayCommandInit(0x30);
	#endif
	
	#ifdef BUS_4BIT
	displayCommandInit(0x20);
	displaySendCommand(0x28);
	#else
	displaySendCommand(0x38);
	#endif
	displaySendCommand(0x08);
	displaySendCommand(0x0C);
	displaySendCommand(0x01);
	_delay_ms(2);
	displaySendCommand(0x06);
}

void displaySendStringFlash(char *str)
{
	unsigned char data = pgm_read_byte(str);
	while (data)
	{
		displayWait();
		PORT_SIG |= (1 << RS);
		displaySendPackage(data);
		str++;
		data = pgm_read_byte(str);
	}
}

void displaySendString(char *str)
{
	unsigned char data;
	PORT_SIG |= (1 << RS);
	while (*str)
	{
		data = *str++;
		displayWait();
		PORT_SIG |= (1 << RS);
		displaySendPackage(data);
	}
}

void displayClear(void)
{
	displaySendCommand(0x01);
	_delay_ms(2);
}

void displayCursorReset(void){
	displaySendCommand(0x02);
	_delay_ms(2);
}

void displayMakeCustomSymbol(char *symbolBytes, char number){
	displaySendCommand(0x40 + number*8);
	for (int i = 0; i < 8; i++){
		displaySendData(symbolBytes[i]);
	}
	displaySendCommand(0x80);
}

