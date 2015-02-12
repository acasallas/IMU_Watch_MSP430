#pragma once
/*
 * bt.h
 *
 *  Created on: Jul 11, 2014
 *      Author: Alan
 */
#include <msp430.h>
#include <stdint.h>
#include <intrinsics.h>
#include <string.h>

void setupBT();
void transmitBT(uint8_t msg[], uint8_t len);
