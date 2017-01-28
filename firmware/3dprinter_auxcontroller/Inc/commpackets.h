/*
 * commpackets.h
 *
 *  Created on: Oct 14, 2016
 *      Author: dabit
 */

#ifndef COMMPACKETS_H_
#define COMMPACKETS_H_

// Sensible names for input channel IDs
#define NRTHERMISTORS 6				// Number of thermistor inputs. These start at ADC_AIN0
#define NRANALOG 2					// Number of generic analog inputs used
#define INID_THERMISTOR1	0
#define INID_THERMISTOR2	1
#define INID_THERMISTOR3	2
#define INID_THERMISTOR4	3
#define INID_THERMISTOR5	4
#define INID_THERMISTOR6	5
#define INID_ANALOG1		6
#define INID_ANALOG2		7


// Sensible names for output channel IDs
#define NROUTCHANNELS		9
#define OUTID_DC12V_1		0			// 12VDC channel 1, typically a fan
#define OUTID_DC12V_2		1			// 12VDC channel 2
#define OUTID_DC12V_3		2			// 12VDC channel 3
#define OUTID_DC24V_1		3			// 24VDC channel 1, typically a nozzle heater
#define OUTID_DC24V_2		4			// 24VDC channel 2
#define OUTID_DC24V_3		5			// 24VDC channel 3
#define OUTID_AC230V_1		6			// 230VAC channel 1, typically a room/bed heater. Uses a very low frequency PDM modulation scheme
#define OUTID_AC230V_2		7			// 230VAC channel 2
#define OUTID_AC230V_3		8			// 230VAC channel 3


#pragma pack(push,1)

/* communication packets. Note: all data transferred in little endian */
/* Communicate a PID control value. This is also used for fan PWM; then the PID coeffs are 0.0f and only FF0 is used */
#define PKT_PIDCONTROL 10
typedef struct {
	uint8_t len;			// packet length in bytes, including this one. Size=32
	uint8_t id;				// id=PKT_PIDCONTROL
	uint8_t outID;			// Output channel ID
	int8_t inID;			// Input channel ID used for feedback. Feedback is set to 0.0 when this is negative
	float command;			// Commanded value
	float coeffP;			// PID P coefficient
	float coeffI;			// PID I coefficient
	float coeffD;			// PID D coefficient
	float coeffFF0;			// PID FF0 coefficient
	float minlim, maxlim;	// Minimum and maximum output limit values, Usually 0.0 - 1.0 for fullscale
} sPkt_PIDControl;

/* Communicate a thermistor setup */
#define PKT_THERMISTOR_SETUP 20
typedef struct {
	uint8_t len;			// packet length in bytes, including this one. Size=31
	uint8_t id;				// id=PKT_THERMISTOR_SETUP
	uint8_t ThermistorID;	// Number of the thermistor
	float RefResistorValue;	// Value of the reference resistor
	float SteinhartHart[4];	// The Steinhart-Hart equation A,B,C and D coefficients
	float ValidTempMin;		// Minimum valid temperature in degrees C. Used to check calculation results
	float ValidTempMax;		// Maximum valid temperature in degrees C. Used to check calculation results
} sPkt_ThermistorSetup;

/* Communicate a thermistor value (usually to host) */
#define PKT_THERMISTOR_VALUE 30
typedef struct {
	uint8_t len;			// packet length in bytes, including this one.
	uint8_t id;				// id=PKT_THERMISTOR_VALUE
	uint8_t ThermistorID;	// Number of the thermistor
	uint8_t bIsValid;		// != 0 when the temperature is valid.
	float TempCelcius;		// Temperature in degrees C
} sPkt_ThermistorValue;


/* Signal end of command packets. */
#define PKT_ENDOFCOMMAND 0xff
typedef struct {
	uint8_t len;			// packet length in bytes, including this one = 2
	uint8_t id;				// id=PKT_ENDOFCOMMAND
} sPkt_Endofcommand;
#pragma pack(pop)


#endif /* COMMPACKETS_H_ */
