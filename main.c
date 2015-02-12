#include <msp430.h>
#include <stdint.h>
#include <intrinsics.h>
#include <string.h>

#include "bt.h"
#include "mpu.h"

/*
 * IMU_Watch. CPU: MSP430G2955
 */

#define DELAY_1MS(x)     __delay_cycles(1048L*x)

uint8_t malfunctionCounter = MAL_CNT; //down-ticking counter to MPU Reset

//Timer A0 - Responsible for setting off Interrupt that measures accel, gyro, and magnetometer
void startTimerA0()
{
    TA0CCR0 = 24999; //Counting up to 25000 gives hrtz of 100
    TA0CCTL0 = CCIE; //enable interrupt
    TA0CTL = MC_1 | ID_3 | TASSEL_2 | TACLR;
    //     up mode|div by 8 | use SMCLK | reset timer

}

//Resets MPU if any of the sensors start returning all 0's.
void resetMPU()
{
	TA0CTL = MC_0; //stop timerA0

	MPUWrite(0x6B,0x80, MPU_ADDR); //Reset MPU
	while (MPURead(0x6B, MPU_ADDR) & 0x80) {}; //Wait for device reset to finish

	setupMPU(); //sets up MPU (gyro, accel, and compass) using UCB0 with I2C

	startTimerA0(); //starts Timer A

}

//sums an array
inline uint8_t sumArray(uint8_t arr[], uint8_t len) {
	uint8_t sum = 0;
	for (;len > 0;len--) {
		sum+= arr[len-1];
	}
	return sum;
}

void setDCOSpeed()
{
	//sets DCO, and thus SMCLK, to exactly 20 MHz
	DCOCTL = 0xE0; //DCOx = 7, MODx = 0
	BCSCTL1 = 0x0F; //RSELx = 15;

}

/*
 * Just testing (used with oscilloscope to measure frequency of clock).
void toggleLineTest()
{
		P3DIR |= BIT1; //set to GPIO

		TA0CCR0 = 1; //Counting up to 10485 should give a Hz of 100.2
	    TA0CTL = MC_1 | ID_3 | TASSEL_2 | TACLR;
	    //     up mode|div by 1 | use SMCLK | reset timer

	    while (1) {
	    	while (!(TA0CTL & TAIFG)) {}; //wait for overflow
	    	TA0CTL &= ~TAIFG; //clear overflow flag
	    	P3OUT ^= BIT1;
	    }
}
*/

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    setDCOSpeed();

    //toggleLineTest();

    setupBT(); //sets up Bluetooth using UCA0 with UART at 115200 baud rate

    setupMPU(); //sets up MPU (gyro, accel, and compass) using UCB0 with I2C

    startTimerA0(); //starts Timer A

    __enable_interrupt(); //always needed for interrupts

    while (1) {
    	__low_power_mode_0(); //Mode 0 allows SMCLK to keep running
    }
	
	return 0;
}

/*
 * Interrupt will go off every .01s (100 Hz). It then retrieves data from MPU and sends it via Bluetooth. If this all takes more than .01s, I'm
 *not sure what can be done, because of the lack of multi-threading.
 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void TA0_ISR (void)
{
	//Read Accelerometer
	uint8_t accelD[6] = {0,0,0,0,0,0};
	MPUReadMulti(0x3B, accelD, 6, MPU_ADDR);

	//Read Gyro
	uint8_t gyroD[6] = {0,0,0,0,0,0};
	MPUReadMulti(0x43,gyroD, 6, MPU_ADDR);

	//Read Magnetometer (we must trigger measurement first for this device)
	MPUWrite(0x0A, 0x01, MAG_ADDR); //trigger measurement
	uint8_t isDataReady = 0;
	while (!isDataReady)
	{
		isDataReady = MPURead(0x02, MAG_ADDR);	//loop until measurement has been taken
	}
	uint8_t magnetD[6] = {0,0,0,0,0,0};
	MPUReadMulti(0x03, magnetD, 6, MAG_ADDR);

	//Now transmit data. Beginning of transmission will be sequence 0xFF, 0x00, 0x00, 0xFF.
	uint8_t header[] = {'G', 'E', 0x15};
	transmitBT(header,3);

	transmitBT(accelD,6); //transmit accelerometer
	transmitBT(gyroD,6); //transmit gyro
	transmitBT(magnetD,6); //transmit magnetometer


	//If any of the sensors are returning 0 (will happen if I2C or MPU are malfunctioning) for MAL_CNT
	//consecutive times, resetMPU() will be called.
	if( !sumArray(accelD,6)  || !sumArray(gyroD,6)  || !sumArray(magnetD,6) ){
		malfunctionCounter--;
	} else {
		malfunctionCounter = MAL_CNT;
	}

	if (malfunctionCounter < 1) {
		resetMPU();
	}



}
