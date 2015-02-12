/*
 * mpu.c
 *
 *  Created on: Jul 11, 2014
 *      Author: Alan
 */
#include <msp430.h>
#include <stdint.h>
#include <intrinsics.h>
#include <string.h>
#include <stdio.h>

#include "mpu.h"
#include "bt.h"

#define DELAY_1MS(x)     __delay_cycles(1048L*x)

//Timer A1 - Used to detect when certain while loops in I2C communication are taking too long.
void startTimerA1()
{
    TA1CCR0 = 65535; //Set to .026 seconds
    TA1CTL = MC_1 | ID_3 | TASSEL_2 | TACLR;
    //     off|divides by 8 | uses SMCLK | reset timer
}

void stopTimerA1()
{
	TA1CTL = MC_0;
}

//Toggles SDA and SCL lines in case they have become stuck as a result of a previous shutdown
void toggleI2CLines()
{
	P3DIR |= BIT1 | BIT2; //set to GPIO

	uint8_t counter = 0; //toggle both lines 255 times (arbitrary number)
	for (counter = 0; counter < 255; counter++) {
		P3OUT ^= BIT1;
		P3OUT ^= BIT2;
		DELAY_1MS(1);
	}
}


void setupMPU()
{
	toggleI2CLines();

	//set UCB0 pins
	P3SEL |= BIT1 | BIT2;

	//Set up UCB0 to use I2C
	UCB0CTL0 = UCMST | UCMODE_3 | UCSYNC; //master | I2C | synch
	UCB0CTL1 = 0xC0 | UCSWRST; //Use SMCLK | hold in reset
	UCB0BR1 = 0;
	UCB0BR0 = 60; //Divider for: 333 Khz
	UCB0I2COA = 0; //Set own address to 0
	UCB0CTL1 &= ~UCSWRST; //Release from reset



	MPUWrite(0x6B,0x03, MPU_ADDR);	//PWR_MGMT_1 , turn sleep mode off and use gyro-z as MPU clock
	MPUWrite(0x37,0x02, MPU_ADDR);	//INT_PIN_CFG, set bypass pin to access magnetometer


}

//Writes 'value' into register on MPU with address 'reghex'
//Wraps around the more general MPUWriteMulti() function
void MPUWrite(uint8_t reghex, uint8_t value, uint8_t addr)
{
	MPUWriteMulti(reghex, &value, 1, addr);
}

//Function writes contents of msg[] array into MPU registers, starting at register address 'reghex'
//len is lenght of msg[] array, cannot be 0.
void MPUWriteMulti(uint8_t reghex, uint8_t msg[], uint8_t len, uint8_t addr)
{
	if (len < 1) return; //lenght of msg[] can't be less than 1

	UCB0I2CSA = addr; //set address
	UCB0CTL1 |= UCTR + UCTXSTT; //set mode to TRANSMITTER and start bit
	while ((UC0IFG & UCB0TXIFG) == 0x00) {}; //make sure you can write to TXBUF
	UCB0TXBUF = reghex;		//write address onto TXBUF


	//Wait for address to be sent AND set timer A1 in case we get stuck
	startTimerA1();
	while ((UCB0CTL1 & UCTXSTT) && !(TA1CTL & TAIFG)) {}; //wait for address to be sent
	//if we timed out, abandon function
	if (TA1CTL & TAIFG) {
		stopTimerA1();
		return;
	}
	stopTimerA1();


	if ( (UCB0STAT & UCNACKIFG) != 0) //if address not ackowledged...
	{
		UCB0CTL1 |= UCTXSTP; //stop communication.
		while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through
		return; //abandon function
	}

	while ((UC0IFG & UCB0TXIFG) == 0x00) {}; //wait for data to be transmitted;

	//If multi-byte message, send all but last byte
	if (len > 1)
	{
		int count = 0;
		for (count = 0; count < (len-1); count++)
		{
			UCB0TXBUF = msg[count];
			while ((UC0IFG & UCB0TXIFG) == 0x00) {};

			if ( (UCB0STAT & UCNACKIFG) != 0) //if address not ackowledged...
			{
				UCB0CTL1 |= UCTXSTP; //stop communication.
				while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through
				return; //abandon function
			}

		}
	}

	//Now send last byte
	UCB0TXBUF = msg[len-1]; //put message into buffer

	while ((UC0IFG & UCB0TXIFG) == 0x00) {}; //wait for data to be transmitted;
	UCB0CTL1 |= UCTXSTP; //set stop bit
	while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through

}


//This function returns the value in the register given by "reghex" in the MPU 9150.
//It wraps around the more general function MPUReadMulti
uint8_t MPURead(uint8_t reghex, uint8_t addr)
{
	uint8_t msg[1];
	MPUReadMulti(reghex, msg, 1, addr);
	return msg[0];
}


//Function reads values of registers in MPU starting at address 'reghex' and feeds them into the msg[] array.
//You must indicate the lenght of the array and the lenght cannot be 0.
void MPUReadMulti(uint8_t reghex, uint8_t msg[], uint8_t len, uint8_t addr)
{
	if (len < 1 ) return; //lenght of msg[] can't be less than 1.

	//Part One: Send register address to read from

	UCB0I2CSA = addr; //set address
	UCB0CTL1 |= UCTR + UCTXSTT; //set mode to TRANSMITTER and start bit

	while ((UC0IFG & UCB0TXIFG) == 0x00) {}; //make sure we can send
	UCB0TXBUF = reghex;		//send register address

	//Wait for address to be sent AND set timer A1 in case we get stuck
	startTimerA1();
	while ((UCB0CTL1 & UCTXSTT) && !(TA1CTL & TAIFG)) {}; //wait for address to be sent
	//if we timed out, abandon function
	if (TA1CTL & TAIFG) {
		stopTimerA1();
		return;
	}
	stopTimerA1();

	if ( (UCB0STAT & UCNACKIFG) != 0) //if address not ackowledged...
	{
		UCB0CTL1 |= UCTXSTP; //stop communication.
		while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through
		return; //function failed.

	}

	while ((UC0IFG & UCB0TXIFG) == 0x00) {}; //wait for data to be transmitted;


	//Part Two: Read bytes

	UCB0CTL1 &= ~UCTR; //set mode to RECIEVER
	UCB0CTL1 |= UCTXSTT; //repeated start signal
	while ((UCB0CTL1 & UCTXSTT) != 0) {}; //wait for address to be sent

	if ( (UCB0STAT & UCNACKIFG) != 0) //if address not ackowledged...
	{
		UCB0CTL1 |= UCTXSTP; //stop communication.
		while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through
		return; //function failed.
	}

	if (len > 1) {
		//read all bytes except the last one
		int count = 0;
		for (count = 0; count < (len-1); count++)
		{
			while ((UC0IFG & UCB0RXIFG) == 0x00) {}; //wait for data to be received;
			msg[count] = UCB0RXBUF; //read messsage from buffer

			if ( (UCB0STAT & UCNACKIFG) != 0) //if address not ackowledged...
			{
				UCB0CTL1 |= UCTXSTP; //stop communication.
				while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through
				return; //function failed.
			}
		}
	}

	//now send the last byte
	UCB0CTL1 |= UCTXSTP; //set stop bit
	while ((UC0IFG & UCB0RXIFG) == 0x00) {}; //wait for data to be received;
	msg[len-1] = UCB0RXBUF; //read messsage from buffer
	while ((UCB0CTL1 & UCTXSTP) != 0) {}; //wait for stop to go through


}




