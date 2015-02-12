/*
 * bt.c
 *
 *  Created on: Jul 11, 2014
 *      Author: Alan
 */

#include <msp430.h>
#include <stdint.h>
#include <intrinsics.h>
#include <string.h>
#include "bt.h"

void setupBT()
{
	//set BT_RESET pin
	P4DIR |= BIT0;
	P4OUT |= BIT0;

	//Set up UCSI_A0 to use UART.
	P3SEL |= BIT4 | BIT5;

	UCA0CTL0 = 0; //Not using SPI on this one
	UCA0CTL1 = UCSSEL1 | UCSWRST; //Use SMCLK | hold in reset
	//SMCLK is running at 20 MHz. Formula is nint(8 * 20 Mhz / 921600) = 174 = 8[21] + [6]:
	UCA0BR0 = 21;	//BRx is 1
	UCA0BR1 = 0x00;
	UCA0MCTL = UCBRS_6; // Oversampling mode, modulate 6 as per formula above
	UCA0CTL1 &= ~UCSWRST; //Release from reset
}

void transmitBT(uint8_t msg[], uint8_t len)
{

	int n = 0;
	for (n = 0; n < len; n++)
	{
		UCA0TXBUF = msg[n];
		while ((UC0IFG & UCA0TXIFG) == 0x00) {}; //Wait for data to be transmitted
	}
}


