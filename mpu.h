#pragma once
/*
 * mpu.h
 *
 *  Created on: Jul 11, 2014
 *      Author: Alan
 */
#include <msp430.h>
#include <stdint.h>
#include <intrinsics.h>
#include <string.h>


#define MPU_ADDR	0x68
#define	MAG_ADDR	0x0C
#define MAL_CNT		150

void startTimerA1();
void toggleI2CLines();

void setupMPU();

void MPUReadMulti(uint8_t reghex, uint8_t msg[], uint8_t len, uint8_t addr);
uint8_t MPURead(uint8_t reghex, uint8_t addr);

void MPUWriteMulti(uint8_t reghex, uint8_t msg[], uint8_t len, uint8_t addr);
void MPUWrite(uint8_t reghex, uint8_t value, uint8_t addr);
