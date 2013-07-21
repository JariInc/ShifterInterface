/*
 * USB-Joystickadapter Firmware
 *	
 * based on project hid-mouse, a very simple HID example by Christian Starkjohann, OBJECTIVE DEVELOPMENT Software GmbH
 * and ATtiny2313 USB Joyadapter firmware by Grigori Goronzy.	
 * adapted for ATtiny4313, changes for ATtiny2313 with new compiler by Andreas Paul 05/2011
 * 
 * Name: main.c
 * Project: USB-Joystickadapter
 * Author: Andreas Paul, (Christian Starkjohann, Grigori Goronzy) 
 * Creation Date: 2008-04-07
 * Tabsize: 4
 * 
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 * This Revision: $Id: $
 */

/*
	Joysticks need to be connected in the following order:

	A:
	PD3-PD6: directional (left-right-up-down)
	PD0-PD1: buttons

	B:
	PB2-PB5: directional (left-right-up-down)
	PB6-PB7: buttons
*/

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
#include "oddebug.h"        /* This is also an example for using debug macros */

#define JOY_A (PIND & ~_BV(PD2))
#define JOY_B (PINB & ~(_BV(PB0)|_BV(PB1)))

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

const char usbHidReportDescriptor[29] PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x15, 0x00,                    // LOGICAL_MINIMUM (0)
    0x09, 0x04,                    // USAGE (Joystick)
    0xa1, 0x01,                    // COLLECTION (Application)
    
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x02,                    //   USAGE_MAXIMUM (Button 2)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x55, 0x00,                    //   UNIT_EXPONENT (0)
    0x65, 0x00,                    //   UNIT (None)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    
    0xc0                           // END_COLLECTION
};

/* This is the same report descriptor as seen in a Logitech mouse. The data
 * described by this descriptor consists of 4 bytes:
 *      .  .  .  .  . B2 B1 B0 .... one byte with mouse button states
 *     X7 X6 X5 X4 X3 X2 X1 X0 .... 8 bit signed relative coordinate x
 *     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 .... 8 bit signed relative coordinate y
 *     W7 W6 W5 W4 W3 W2 W1 W0 .... 8 bit signed relative coordinate wheel
 */
typedef struct{
	uchar   buttonMask;
}report_t;

static report_t reportBuffer;
static uchar    idleRate;   /* repeat rate for keyboards, never used for mice */

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.
     */
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        DBG1(0x50, &rq->bRequest, 1);   /* debug output: print our request */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            usbMsgPtr = (void *)&reportBuffer;
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        /* no vendor specific requests implemented */
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

/* ------------------------------------------------------------------------- */

static void hardwareInit(void)
{
	PORTD = ~_BV(PD2);
    DDRD = 0;
    PORTB = 0;
    DDRB = USBMASK;
	_delay_ms(20);
    DDRB = 0;
    PORTB = ~USBMASK;
}
 
/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{
uchar   i;
uchar keyA, keyB, lastKeyA = 0xfa, lastKeyB = 0xfa; 

    wdt_enable(WDTO_1S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
	
	odDebugInit();
    DBG1(0x00, 0, 0);       /* debug output: main starts */
	hardwareInit(); 
    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
    sei();
    DBG1(0x01, 0, 0);       /* debug output: main loop starts */
    for(;;){                /* main event loop */
        DBG1(0x02, 0, 0);   /* debug output: main loop iterates */
        wdt_reset();
        usbPoll();
		
        keyA = ~PIND & 0b11;
		
        if(usbInterruptIsReady()){
            /* called after every poll of the interrupt endpoint */
			if(keyA != lastKeyA) {
				usbSetInterrupt((void *)&keyA, sizeof(uchar));
				lastKeyA = keyA;
			}
        }
    }
}

/* ------------------------------------------------------------------------- */
