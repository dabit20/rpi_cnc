/*
 * housekeeping.h
 *
 *  Created on: Oct 14, 2016
 *      Author: dabit
 */

#ifndef HOUSEKEEPING_H_
#define HOUSEKEEPING_H_

#define TICKS_PER_SEC 30000								/* Timer ISR rate, equal to DC_FAST modulator speed. Must be higher than 1100Hz */
#define LOWSPEED_DIV ((uint16_t)(TICKS_PER_SEC/105.0))	/* divisor used for the lowspeed PWM at 105Hz */
#define COMM_TIMEOUT (10 * TICKS_PER_SEC)				/* we want a packet at least every 10 seconds */

#define NR_ADC_CHANNELS 8								/* Number of enabled ADC channels */

#ifndef bool
typedef uint8_t bool;
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

/* Modulator speeds. DC can be pulsed much faster than zero-crossing AC. Intermediate values may be added later */
typedef enum  {
	DC_FAST,				// Use a base frequency of 30kHz for the PDM modulator
	AC_SLOW					// Use a base frequency of 105Hz for the PDM modulator
} Modulator_Speed;

/* Output modulator data */
typedef struct {
	GPIO_TypeDef* GPIOport;	// I/O port for the modulator pin
	uint16_t GPIOpin;		// I/O pin number
	Modulator_Speed Speed;	// The modulator speed to use
	uint32_t accumulator;	// Accumulator used for the PDM modulator. 'overflows' at 0x00800000
	uint32_t increment;		// Accumulator increment.
} OutModulator;

/* PID controller variables */
typedef struct {
	// 'public' variables
	float Kp, Ki, Kd, FF0;	// P, I, D and 0th order feedforward
	float minlim, maxlim;	// Output minimum and maximum limits. Range is usually 0.0f - 1.0f
	int8_t inID;			// Input ID, when used. Set to -1 when directly entering the feedback value
	float command, feedback;// PID command and feedback values
	float output;			// PID output. 1.0 gives the maximum modulator output
	// 'private' variables
	float integrator;		// integrator
	float preverror;		// previous error, used for Kd term
} OutPID;

/* Thermistor data structure */
typedef struct {
	// Setup/parameter values
	float RefResistorValue;	// Value of the reference resistor. Interface to ADC is reference resistor to Vref-, thermistor to Vref+
	float SteinhartHart[4];	// The Steinhart-Hart equation A,B,C and D coefficients
	float ValidTempMin;		// Minimum valid temperature in degrees C. Used to check calculation results
	float ValidTempMax;		// Maximum valid temperature in degrees C. Used to check calculation results
	// Output values
	float Temperature;		// Calculated
	bool bIsValid;
} Thermistor;

extern volatile bool bGotUSBData, IsRunning;				/* Set to true when we received a packet from the host */
extern volatile int32_t CommTimeoutCtr;		/* Communications timeout downcounter */
extern OutPID PID[NROUTCHANNELS];
// Thermistor housekeeping structures
extern Thermistor thermistor[NRTHERMISTORS];


#endif /* HOUSEKEEPING_H_ */
