#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <endian.h>
#include <math.h>

#include <rtapi_slab.h>
#include <rtapi_ctype.h>
#include <rtapi_math64.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "hal.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bart van Hest");
MODULE_DESCRIPTION("dSpin stepper motor driver for Raspberry Pi");
#ifdef MODULE_SUPPORTED_DEVICE
MODULE_SUPPORTED_DEVICE("dSpin");
#endif

/************* Various important defines ****************/

/* Number of joints/motors */
#define NR_JOINTS 4

/* Raspberry BCM IO numbers for the various wires. 
 * Define one number for each joint. Shared pins may use the same number 
 */
static uint8_t dSPIN_RESET[NR_JOINTS] = { 17 };             /* STBY lines of attached dSpin modules. May be shared between multiple modules */
static uint8_t dSPIN_CS[NR_JOINTS] = { 27 };                    /* CSN lines of attached dSpin modules. May be shared between multiple modules */
static uint8_t dSPIN_CLK[NR_JOINTS] = { 9 };                    /* CK lines of attached dSpin modules. May be shared between multiple modules */
static uint8_t dSPIN_MOSI[NR_JOINTS] = { 10, 23, 5, 13 };       /* SDI lines of attached dSpin modules. Separate per module */
static uint8_t dSPIN_MISO[NR_JOINTS] = { 11, 24, 6, 26 };       /* SDO lines of attached dSpin modules. Separate per module */

/* Minimum delay in microseconds between bit toggles. May be 0 to disable the timing checks */
#define MIN_TIMING 2

/* BEMF compensation parameters for each motor. See datasheet and ST AN4144 for information on how
 * to obtain these 
 */
static uint8_t KVAL_HOLD[NR_JOINTS] = { 0x24, 0x24, 0x13, 0x13 };
static uint8_t KVAL_RUN[NR_JOINTS] = { 0x2F, 0x2F, 0x33, 0x33 };         
static uint8_t ST_SLP[NR_JOINTS] = { 0x40, 0x40, 0x17, 0x17,  }; 
static uint8_t  FN_SLP[NR_JOINTS] = { 0x57, 0x57, 0x4B, 0x4B }; 
static uint16_t INT_SPD[NR_JOINTS] = { 0xE4E, 0xE4E, 0x1026, 0x1026 }; 

/* Extra input/output lines. Define the number of ext inputs and outputs, and define the arrays with BCM pin numbers */
#define NR_EXT_INPUTS 3
#if (NR_EXT_INPUTS>0)
static uint8_t EXTIN_LINES[NR_EXT_INPUTS] = { 16, 20, 21};
#endif

#define NR_EXT_OUTPUTS 0
#if (NR_EXT_OUTPUTS>0)
static uint8_t EXTOUT_LINES[NR_EXT_OUTPUTS] = { };
#endif


/************* Code included from minimal_gpio.c, taken from http://abyz.co.uk/rpi/pigpio/ ****************/

static volatile uint32_t piModel = 1;

static volatile uint32_t piPeriphBase = 0x20000000;
static volatile uint32_t piBusAddr = 0x40000000;

#define SYST_BASE  (piPeriphBase + 0x003000)
#define GPIO_BASE  (piPeriphBase + 0x200000)
#define BSCS_BASE  (piPeriphBase + 0x214000)

#define GPIO_LEN  0xB4
#define SYST_LEN  0x1C
#define BSCS_LEN  0x40

#define GPSET0 7
#define GPSET1 8

#define GPCLR0 10
#define GPCLR1 11

#define GPLEV0 13
#define GPLEV1 14

#define GPPUD     37
#define GPPUDCLK0 38
#define GPPUDCLK1 39

#define SYST_CS  0
#define SYST_CLO 1
#define SYST_CHI 2

static volatile uint32_t  *gpioReg = MAP_FAILED;
static volatile uint32_t  *systReg = MAP_FAILED;
static volatile uint32_t  *bscsReg = MAP_FAILED;

#define PI_BANK (gpio>>5)
#define PI_BIT  (1<<(gpio&0x1F))

/* gpio modes. */

#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

void gpioSetMode(unsigned gpio, unsigned mode)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);
}

int gpioGetMode(unsigned gpio)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   return (*(gpioReg + reg) >> shift) & 7;
}

/* Values for pull-ups/downs off, pull-down and pull-up. */

#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

void gpioSetPullUpDown(unsigned gpio, unsigned pud)
{
   *(gpioReg + GPPUD) = pud;

   usleep(20);

   *(gpioReg + GPPUDCLK0 + PI_BANK) = PI_BIT;

   usleep(20);
  
   *(gpioReg + GPPUD) = 0;

   *(gpioReg + GPPUDCLK0 + PI_BANK) = 0;
}

inline int gpioRead(unsigned gpio)
{
   if ((*(gpioReg + GPLEV0 + PI_BANK) & PI_BIT) != 0) return 1;
   else                                         return 0;
}

inline void gpioWrite(unsigned gpio, unsigned level)
{
   if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}

unsigned gpioHardwareRevision(void)
{
   static unsigned rev = 0;

   FILE * filp;
   char buf[512];
   char term;
   int chars=4; /* number of chars in revision string */

   if (rev) return rev;

   piModel = 0;

   filp = fopen ("/proc/cpuinfo", "r");

   if (filp != NULL)
   {
      while (fgets(buf, sizeof(buf), filp) != NULL)
      {
         if (piModel == 0)
         {
            if (!strncasecmp("model name", buf, 10))
            {
               if (strstr (buf, "ARMv6") != NULL)
               {
                  piModel = 1;
                  chars = 4;
                  piPeriphBase = 0x20000000;
                  piBusAddr = 0x40000000;
               }
               else if (strstr (buf, "ARMv7") != NULL)
               {
                  piModel = 2;
                  chars = 6;
                  piPeriphBase = 0x3F000000;
                  piBusAddr = 0xC0000000;
               }
               else if (strstr (buf, "ARMv8") != NULL)
               {
                  piModel = 2;
                  chars = 6;
                  piPeriphBase = 0x3F000000;
                  piBusAddr = 0xC0000000;
               }
            }
         }

         if (!strncasecmp("revision", buf, 8))
         {
            if (sscanf(buf+strlen(buf)-(chars+1),
               "%x%c", &rev, &term) == 2)
            {
               if (term != '\n') rev = 0;
            }
         }
      }

      fclose(filp);
   }
   return rev;
}

/* Returns the number of microseconds after system boot. Wraps around
   after 1 hour 11 minutes 35 seconds.
*/
inline uint32_t gpioTick(void) { return systReg[SYST_CLO]; }


/* Map in registers. */
static uint32_t * initMapMem(int fd, uint32_t addr, uint32_t len)
{
    return (uint32_t *) mmap(0, len,
       PROT_READ|PROT_WRITE|PROT_EXEC,
       MAP_SHARED|MAP_LOCKED,
       fd, addr);
}
/* Taken from hm2_rpspi.c */
int rtapi_open_as_root(const char *filename, int mode) {
	
	if (geteuid() != 0) {
		fprintf (stderr, "Debug: euid: %d uid %d\n", geteuid(), getuid());
		fprintf (stderr, "EUID is not 0 (root). Trying seteuid()...\n");
		seteuid(0);
		fprintf (stderr, "euid: %d uid %d\n", geteuid(), getuid());
	}		
	setfsuid(geteuid());
	int r = open(filename, mode);
	setfsuid(getuid());
	return r;
}

int gpioInitialise(void)
{
   int fd;

   gpioHardwareRevision(); /* sets piModel, needed for peripherals address */

   fd = rtapi_open_as_root("/dev/mem", O_RDWR | O_SYNC) ;

   if (fd < 0)
   {
      fprintf(stderr,
         "This program needs root privileges.  Try using sudo\n");
      return -1;
   }

   gpioReg  = initMapMem(fd, GPIO_BASE,  GPIO_LEN);
   systReg  = initMapMem(fd, SYST_BASE,  SYST_LEN);
   bscsReg  = initMapMem(fd, BSCS_BASE,  BSCS_LEN);

   close(fd);

   if ((gpioReg == MAP_FAILED) ||
       (systReg == MAP_FAILED) ||
       (bscsReg == MAP_FAILED))
   {
      fprintf(stderr,
         "Bad, mmap failed\n");
      return -1;
   }
   return 0;
}
/************* End of code included from minimal_gpio.c ****************/


/************* dSpin control code. Code is partially taken from https://github.com/blerchin/dSPIN_raspi ****************/

/* parameters */
#define dSPIN_ABS_POS              0x01
#define dSPIN_EL_POS               0x02
#define dSPIN_MARK                 0x03
#define dSPIN_SPEED                0x04
#define dSPIN_ACC                  0x05
#define dSPIN_DEC                  0x06
#define dSPIN_MAX_SPEED            0x07
#define dSPIN_MIN_SPEED            0x08
#define dSPIN_FS_SPD               0x15
#define dSPIN_KVAL_HOLD            0x09
#define dSPIN_KVAL_RUN             0x0A
#define dSPIN_KVAL_ACC             0x0B
#define dSPIN_KVAL_DEC             0x0C
#define dSPIN_INT_SPD              0x0D
#define dSPIN_ST_SLP               0x0E
#define dSPIN_FN_SLP_ACC           0x0F
#define dSPIN_FN_SLP_DEC           0x10
#define dSPIN_K_THERM              0x11
#define dSPIN_ADC_OUT              0x12
#define dSPIN_OCD_TH               0x13
#define dSPIN_STALL_TH             0x14
#define dSPIN_STEP_MODE            0x16
#define dSPIN_ALARM_EN             0x17
#define dSPIN_CONFIG               0x18
#define dSPIN_STATUS               0x19

#define dSPIN_RUN_FWD                  0x51000000
#define dSPIN_RUN_REV                  0x50000000
#define dSPIN_GO_UNTIL             0x82
#define dSPIN_RELEASE_SW           0x92
#define dSPIN_RESET_DEVICE         0xC0
#define dSPIN_SOFT_HIZ             0xA0
#define dSPIN_GET_STATUS           0xD0

static uint32_t timestart, timeend;

/* minTiming_start / minTiming_end ensure that the code inbetween these calls take a minimum amount of time.
    Used to throttle the speed of the bitbanged SPI
 */
inline void minTiming_start(void)
{
        #if MIN_TIMING > 0
                timestart = gpioTick();
        #endif
}

inline void minTiming_end(uint32_t microseconds)
{
        #if  MIN_TIMING > 0
                timeend=gpioTick();
                if (timeend < timestart) {
                       microseconds -= 0xffffffff-timestart;
                       if (microseconds>0x80000000)     /* overflow? Then our time has passed */
                                return;
                       timestart=0;
                }
                while ((timeend-timestart) < microseconds) {
                        timeend = gpioTick();
                }
       #else
                /* Insert a few clockcycles delay as an insurance policy. Although the inefficient way we toggle I/O
                    is already slow enough when using short wires */
                 for (timestart=0;timestart<150;timestart++)
                        asm volatile ("mov r0, r0");
       #endif        
}



/* Write an 8-bit command */
inline void dSpin_writecommand(uint8_t command)
{
        register int i,j;
        minTiming_start();
        for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CS[j], 0);
        minTiming_end(MIN_TIMING);
        /* transmit command */
        for (i=0;i<8;i++) {
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CLK[j], 0);
                if (command & 0x80) 
                        for (j=0;j<NR_JOINTS;j++)  gpioWrite (dSPIN_MOSI[j], 1);
                else
                        for (j=0;j<NR_JOINTS;j++)  gpioWrite (dSPIN_MOSI[j], 0);
                command <<= 1;
                minTiming_end(MIN_TIMING);
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CLK[j], 1);
                minTiming_end(MIN_TIMING);
        }
        minTiming_start();
        for (j=0;j<NR_JOINTS;j++)  gpioWrite (dSPIN_MOSI[j], 0);
        for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CS[j], 1);
        minTiming_end(MIN_TIMING);
}

/* Write a single byte of the command payload */
inline void dSpin_writebyte(uint8_t bytes[NR_JOINTS])
{
        register int i,j;
        minTiming_start();
        for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CS[j], 0);
        minTiming_end(MIN_TIMING);
        /* transmit payload byte */
        for (i=0;i<8;i++) {
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CLK[j], 0);
                for (j=0;j<NR_JOINTS;j++) {
                        if (bytes[j] & 0x80)
                                gpioWrite (dSPIN_MOSI[j], 1);
                        else
                                gpioWrite (dSPIN_MOSI[j], 0);
                }
                for (j=0;j<NR_JOINTS;j++) 
                        bytes[j]<<=1;
                minTiming_end(MIN_TIMING);
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CLK[j], 1);
                minTiming_end(MIN_TIMING);
        }
        minTiming_start();
        for (j=0;j<NR_JOINTS;j++)  gpioWrite (dSPIN_MOSI[j], 0);
        for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CS[j], 1);
        minTiming_end(MIN_TIMING);
}


/* Fetch a single byte */
inline void dSpin_readbyte(uint32_t received[NR_JOINTS])
{
        register int i,j;
        minTiming_start();
        for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CS[j], 0);
        minTiming_end(MIN_TIMING);
        for (i=0;i<8;i++) {
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CLK[j], 0);
                minTiming_end(MIN_TIMING);
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++)  {
                        received[j]<<=1;
                        received[j] |= gpioRead(dSPIN_MISO[j]);                        
                }                
                minTiming_end(MIN_TIMING);
                minTiming_start();
                for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CLK[j], 1);
                minTiming_end(MIN_TIMING);
        }
        minTiming_start();
        for (j=0;j<NR_JOINTS;j++) gpioWrite (dSPIN_CS[j], 1);
        minTiming_end(MIN_TIMING);
}

/* Read a parameter that returns 2 bytes as the payload */
void dSpin_readparam1byte(uint8_t command, uint32_t received[NR_JOINTS])
{
        register int i,j;
        command |= 0x20;
        for (j=0;j<NR_JOINTS;j++) received[j] = 0;
        dSpin_writecommand (command);
        /* Fetch 8 databits */
        dSpin_readbyte (received);
}

/* Read a parameter that returns 2 bytes as the payload */
void dSpin_readparam2bytes(uint8_t command, uint32_t received[NR_JOINTS])
{
        register int i,j;
        command |= 0x20;
        for (j=0;j<NR_JOINTS;j++) received[j] = 0;
        dSpin_writecommand (command);
        /* Fetch 16 databits */
        dSpin_readbyte (received);
        dSpin_readbyte (received);        
}

/* Read a parameter that returns 3 bytes as the payload */
void dSpin_readparam3bytes(uint8_t command, uint32_t received[NR_JOINTS])
{
        register int j;
        command |= 0x20;
        for (j=0;j<NR_JOINTS;j++) received[j] = 0;
        dSpin_writecommand (command);
        /* Fetch 24 databits */
        dSpin_readbyte (received);
        dSpin_readbyte (received);
        dSpin_readbyte (received);         
}

/* Read a parameter that returns 2 bytes as the payload */
void dSpin_getstatus(uint32_t received[NR_JOINTS])
{
        register int i,j;
        for (j=0;j<NR_JOINTS;j++) received[j] = 0;
        dSpin_writecommand (0xd0);
        /* Fetch 16 databits */
        dSpin_readbyte (received);
        dSpin_readbyte (received);        
}


/* Write a parameter that requires a 1-byte payload  */
void dSpin_writeparam1byte(uint8_t command, uint32_t payload[NR_JOINTS])
{
        register int j;
        uint8_t bytes[NR_JOINTS];
        dSpin_writecommand (command);
        /* Write 8 databits */
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)(payload[j] & 0xff);
        }
        dSpin_writebyte (bytes);
}

/* Write a parameter that requires a 2-byte payload  */
void dSpin_writeparam2byte(uint8_t command, uint32_t payload[NR_JOINTS])
{
        register int j;
        uint8_t bytes[NR_JOINTS];
        dSpin_writecommand (command);
        /* Write 16 databits, MSB first */
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)((payload[j] & 0xff00)>>8);
        }
        dSpin_writebyte (bytes);
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)(payload[j] & 0xff);
        }
        dSpin_writebyte (bytes);
}

/* Send a RUN command to the attached drives, with speed in steps/sec as the parameter */
void dSpin_run (float speed[NR_JOINTS])
{
        register int i,j;
        uint32_t cmd[NR_JOINTS];
        uint8_t bytes[NR_JOINTS];
        float currspeed;
        /* Calculate speed in dSpin units and add fwd/rev command */
        for (j=0;j<NR_JOINTS;j++) {
                cmd[j]=(uint32_t)fabs(speed[j] * 67.106);
                if (cmd[j] > 0x000fffff) 
                        cmd[j] = 0x000fffff;
                if (speed[j] > 0.0f)
                        cmd[j] |= dSPIN_RUN_FWD;
                else
                        cmd[j] |= dSPIN_RUN_REV;                
        }
        /* Write all 32 bits, MSB first */
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)((cmd[j] & 0xff000000)>>24);
        }
        dSpin_writebyte (bytes);
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)((cmd[j] & 0xff0000)>>16);
        }
        dSpin_writebyte (bytes);
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)((cmd[j] & 0xff00)>>8);
        }
        dSpin_writebyte (bytes);
        for (j=0;j<NR_JOINTS;j++) {
                bytes[j] = (uint8_t)(cmd[j] & 0xff);
        }
        dSpin_writebyte (bytes);
}

/* Setup I/O lines and reset chips */
int initdSpin(void)
{
        int j;
        uint32_t datastore[NR_JOINTS];
        for (j=0;j<NR_JOINTS;j++) {
                /* Setup IO directions. All are output, except for MISO */
                gpioSetMode (dSPIN_RESET[j], PI_INPUT);
                gpioSetMode (dSPIN_CS[j], PI_INPUT);
                gpioSetMode (dSPIN_CLK[j], PI_INPUT);
                gpioSetMode (dSPIN_MOSI[j], PI_INPUT);
                gpioSetMode (dSPIN_MISO[j], PI_INPUT);
                gpioSetMode (dSPIN_RESET[j], PI_OUTPUT);
                gpioSetMode (dSPIN_CS[j], PI_OUTPUT);
                gpioSetMode (dSPIN_CLK[j], PI_OUTPUT);
                gpioSetMode (dSPIN_MOSI[j], PI_OUTPUT);
                gpioSetPullUpDown(dSPIN_MISO[j], PI_PUD_DOWN);
                /* Put chips in reset */
                gpioWrite (dSPIN_RESET[j], 0);
                gpioWrite (dSPIN_CS[j], 1);
                gpioWrite (dSPIN_CLK[j], 0);
                gpioWrite (dSPIN_MOSI[j], 0);                
        }
        usleep (10);            /* Minimal reset time, tstby_min in L6470 datasheet  */
        /* release reset */
        for (j=0;j<NR_JOINTS;j++) {
                gpioWrite (dSPIN_RESET[j], 1);    
        }
        usleep (45);            /* maximum logic wake up time, tlogicwu on page 14 of L6470 dataheet */
        /* Read config registers. Should power up to 0x2E88 for L6470 */
        dSpin_readparam2bytes (dSPIN_CONFIG, datastore);
        for (j=0;j<NR_JOINTS;j++) {
                rtapi_print("joint %d config register: %04x\n", j, datastore[j]);
        }    
        /* Setup step mode register, 128usteps, no SYNC output */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 0x07;
        dSpin_writeparam1byte  (dSPIN_STEP_MODE, datastore);
        /* Set max speed to 15610 steps/sec (15.25steps/s per bit). The motion controller must limit this to sensible values */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 0x3ff;
        dSpin_writeparam2byte  (dSPIN_MAX_SPEED, datastore);
        /* Set min speed to 0 and enable low speed optimization up to 10 steps/sec (0.238 step/s per bit)*/
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 5 | 0x1000;
        dSpin_writeparam2byte  (dSPIN_MIN_SPEED, datastore);       
        /* Set fullstepping treshold speed to 397 steps/s (15.25 steps/s per bit) */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 26;
        dSpin_writeparam2byte  (dSPIN_FS_SPD, datastore);
        /* Set max acceleration and deceleration values. Base these on 1G using 2.5 step/mm, which calculates to
            25000 steps/s/s. Scale by 0.137438 to get the required 12-bit value (3436d). This really should be a parameter */
         for (j=0;j<NR_JOINTS;j++) datastore[j] = 3436;
        dSpin_writeparam2byte  (dSPIN_ACC, datastore);
        dSpin_writeparam2byte  (dSPIN_DEC, datastore);
        /* overcurrent treshold. 3Amp motors may use 4.2A peak so 4.5A is appropriate, step size is 375mA starting at 375mA for 00 */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 11;
        dSpin_writeparam1byte  (dSPIN_OCD_TH, datastore);
        /* PWM/H-bridge parameters. 16MHz internal clock, no oscout, 46.9kHz PWM, 320V/us slewrate, 
            bridge shutdown on overcurrent, no voltage compensation, external switch no hardstop */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 0x1410;
        dSpin_writeparam2byte  (dSPIN_CONFIG, datastore);
        /* Stall detect treshold at maximum of 4A */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = 0x7f;
        dSpin_writeparam1byte  (dSPIN_STALL_TH, datastore);
        /* Program the BEMF compensation values. Use the same value for acc, dec and run phase */
        for (j=0;j<NR_JOINTS;j++) datastore[j] = (uint32_t)KVAL_HOLD[j];
        dSpin_writeparam1byte  (dSPIN_KVAL_HOLD, datastore);
        for (j=0;j<NR_JOINTS;j++) datastore[j] = (uint32_t)KVAL_RUN[j];
        dSpin_writeparam1byte  (dSPIN_KVAL_RUN, datastore);
        dSpin_writeparam1byte  (dSPIN_KVAL_ACC, datastore);
        dSpin_writeparam1byte  (dSPIN_KVAL_DEC, datastore);
        for (j=0;j<NR_JOINTS;j++) datastore[j] = (uint32_t)ST_SLP[j];
        dSpin_writeparam1byte  (dSPIN_ST_SLP, datastore); 
        for (j=0;j<NR_JOINTS;j++) datastore[j] = (uint32_t)FN_SLP[j];
        dSpin_writeparam1byte  (dSPIN_FN_SLP_ACC, datastore);
        dSpin_writeparam1byte  (dSPIN_FN_SLP_DEC, datastore);      
        for (j=0;j<NR_JOINTS;j++) datastore[j] = (uint32_t)INT_SPD[j];
        dSpin_writeparam2byte  (dSPIN_INT_SPD, datastore);             
}

/************* end of dSpin control code ****************/

/* Setup external input lines as input */
void initextio(void)
{
	int j;        
	#if (NR_EXT_INPUTS>0)
		for (j=0;j<NR_EXT_INPUTS;j++) {
			gpioSetMode (EXTIN_LINES[j], PI_INPUT);
            gpioSetPullUpDown(EXTIN_LINES[j], PI_PUD_UP);
		}
	#endif
	#if (NR_EXT_OUTPUTS>0)
		for (j=0;j<NR_EXT_OUTPUTS;j++) {
			gpioSetMode (EXTOUT_LINES[j], PI_OUTPUT);
			gpioWrite (EXTOUT_LINES[j], 0);
		}
	#endif
}

/* Component id */
static int comp_id;
	
struct __comp_state {
    struct __comp_state *_next;
    hal_float_t *velocity_cmd_joint[NR_JOINTS];
    hal_float_t *position_fb_joint[NR_JOINTS];
    hal_bit_t *enable;                                               /* enable. When disabled, bridge is in hi-Z */
    hal_bit_t *steploss_error[NR_JOINTS];            /* Step loss error on either coil A or B */
    hal_bit_t *overcurrent_error[NR_JOINTS];       /* overcurrent error */
    hal_bit_t *thermal_error[NR_JOINTS];           /* Thermal warning or shutdown */
	hal_bit_t *switch_status[NR_JOINTS];           /* Switch status bit */
	
	#if (NR_EXT_INPUTS>0)
	hal_bit_t *extin[NR_EXT_INPUTS];				/* Auxiliary external inputs */
	hal_bit_t *extin_not[NR_EXT_INPUTS];			/* Auxiliary external inputs, inverted */
    #endif
	#if (NR_EXT_OUTPUTS>0)
	hal_bit_t *extout[NR_EXT_OUTPUTS];				/* Auxiliary external inputs */
	#endif
    
    hal_float_t scale_joint[NR_JOINTS];		/* Scale from units to steps */
    hal_u32_t status_joint[NR_JOINTS];
};

struct __comp_state *__comp_first_inst=0, *__comp_last_inst=0;

static void _(struct __comp_state *__comp_inst, long period);
static int __comp_get_data_size(void);
#undef TRUE
#define TRUE (1)
#undef FALSE
#define FALSE (0)
#undef true
#define true (1)
#undef false
#define false (0)
#undef fperiod
#define fperiod ((double)period * 1.0e-9)

static int export(char *prefix, long extra_arg) {
    char buf[HAL_NAME_LEN + 1];
    int r = 0,i;
    int sz = sizeof(struct __comp_state) + __comp_get_data_size();
    struct __comp_state *inst = hal_malloc(sz);
    memset(inst, 0, sz);
    for (i=0;i<NR_JOINTS;i++) {
		r = hal_pin_float_newf(HAL_IN, &(inst->velocity_cmd_joint[i]), comp_id,
		    "%s.velocity-cmd-joint%d", prefix,i);
		if(r != 0) return r;
		r = hal_pin_float_newf(HAL_OUT, &(inst->position_fb_joint[i]), comp_id,
		    "%s.position-fb-joint%d", prefix,i);
		if(r != 0) return r;
		r = hal_param_float_newf(HAL_RW, &(inst->scale_joint[i]), comp_id,
		    "%s.scale-joint%d", prefix,i);
		if(r != 0) return r;
		r = hal_param_u32_newf(HAL_RW, &(inst->status_joint[i]), comp_id,
		    "%s.status-joint%d", prefix,i);
		if(r != 0) return r;
		r = hal_pin_bit_newf(HAL_OUT, &(inst->steploss_error[i]), comp_id,
                    "%s.steploss-error-joint%d", prefix, i);
        if(r != 0) return r;
		r = hal_pin_bit_newf(HAL_OUT, &(inst->overcurrent_error[i]), comp_id,
			"%s.overcurrent-error-joint%d", prefix, i);
		if(r != 0) return r;
		r = hal_pin_bit_newf(HAL_OUT, &(inst->thermal_error[i]), comp_id,
			"%s.thermal-error-joint%d", prefix, i);
		if(r != 0) return r;                    
		r = hal_pin_bit_newf(HAL_OUT, &(inst->switch_status[i]), comp_id,
			"%s.switch-status-joint%d", prefix, i);
		if(r != 0) return r;                    
	}
	/* Ext inputs */
	#if (NR_EXT_INPUTS>0)
		for (i=0;i<NR_EXT_INPUTS;i++) {
			r = hal_pin_bit_newf(HAL_OUT, &(inst->extin[i]), comp_id,
			"%s.extin%d", prefix, i);
			if(r != 0) return r;
			r = hal_pin_bit_newf(HAL_OUT, &(inst->extin_not[i]), comp_id,
			"%s.extin-not%d", prefix, i);
			if(r != 0) return r;
		}
	#endif
	/* Ext outputs */
	#if (NR_EXT_OUTPUTS>0)
		for (i=0;i<NR_EXT_OUTPUTS;i++) {
			r = hal_pin_bit_newf(HAL_IN, &(inst->extout[i]), comp_id,
			"%s.extout%d", prefix, i);
		if(r != 0) return r;
		}
	#endif
    r = hal_pin_bit_newf(HAL_IN, &(inst->enable), comp_id,
        "%s.enable", prefix);
    if(r != 0) return r;
		
    /* Setup defaults */
    //inst->InitialiseDelay = 3;
    *inst->enable = 0;
    for (i=0;i<NR_JOINTS;i++) {
    	inst->scale_joint[i] = 1.0;	
    	 *inst->velocity_cmd_joint[i] = 0.0;
    	 *inst->position_fb_joint[i] = 0.0;
    }
    
    rtapi_snprintf(buf, sizeof(buf), "%s", prefix);
    r = hal_export_funct(buf, (void(*)(void *inst, long))_, inst, 1, 0, comp_id);
    if(r != 0) return r;
    if(__comp_last_inst) __comp_last_inst->_next = inst;
    __comp_last_inst = inst;
    if(!__comp_first_inst) __comp_first_inst = inst;
    return 0;
}

static int __comp_get_data_size(void) { return 0; }
int rtapi_app_main(void) {

    int ret,i;
    rtapi_print("loading dspin module\n");
    ret = hal_init("dspin");
    if (ret < 0)
        return ret;
    comp_id = ret;
    /* Open GPIO */
    if (gpioInitialise() < 0) {
         rtapi_print("GPIO initialise went wrong\n");
         hal_exit(comp_id);
    }
    /* Initialise dSpin drives */
    initdSpin();
	/* Initialise extra IO */
	initextio();
    /* Export pins and finalize HAL component creation */
    ret = export("dspin", 0);
    if(ret) {
        hal_exit(comp_id);
    } else {
        hal_ready(comp_id);
    }
    return ret;
}

void rtapi_app_exit(void) {
     dSpin_writecommand (dSPIN_SOFT_HIZ);
    hal_exit(comp_id);
    rtapi_print("dSpin module unloaded\n");
}

static void _(struct __comp_state *__comp_inst, long period) {
	int i,j;
	static uint16_t counter = 0;
	float speed[NR_JOINTS];
	int32_t pos[NR_JOINTS];	
	if (!(*__comp_inst->enable)) {
	        /* We are disabled, put bridge in Hi-Z */
	        if (counter==0)
	                dSpin_writecommand (dSPIN_SOFT_HIZ);
	        if (counter < 0xffff)
	                counter++;
	} else {
	        counter = 0;
	        /* We are enabled. */
	        /* get the position feedback .. */
	        dSpin_readparam3bytes (dSPIN_ABS_POS, (uint32_t *)pos);
	        for (j=0;j<NR_JOINTS;j++) { 
	                /* Sign-extend the 22-bit signed position value to 32-bit */
	                if (pos[j] & 0x00200000)
	                        pos[j] |= 0xffc00000;
	                /* Calculate the current position. Note the 128-ustep correction */
	                *__comp_inst->position_fb_joint[j] = ((hal_float_t)pos[j]) / (__comp_inst->scale_joint[j] * 128.0);
	        }
                /* Get status */
	        for (j=0;j<NR_JOINTS;j++) { 
	                dSpin_getstatus((uint32_t *)__comp_inst->status_joint+j);
	                /* Decode a few status bits */
	                *__comp_inst->steploss_error[j] = ((__comp_inst->status_joint[j] & 0x6000)==0x6000) ? false : true;
	                *__comp_inst->overcurrent_error[j] = ((__comp_inst->status_joint[j] & 0x1000)==0x1000) ? false : true;
	                *__comp_inst->thermal_error[j] = ((__comp_inst->status_joint[j] & 0x0c00)==0x0c00) ? false : true;
					*__comp_inst->switch_status[j] = ((__comp_inst->status_joint[j] & 0x0004)==0x0004) ? true : false;
	        }
	        /* Run the motor at the commanded speed... */
	        for (j=0;j<NR_JOINTS;j++) { 
	                speed[j] = ((*__comp_inst->velocity_cmd_joint[j]) * __comp_inst->scale_joint[j]);
	        }
	        dSpin_run (speed);	
			/* Process extra IO */
			#if (NR_EXT_INPUTS>0)
				for (j=0;j<NR_EXT_INPUTS;j++) {
					*__comp_inst->extin[j] = gpioRead(EXTIN_LINES[j]);
					*__comp_inst->extin_not[j] = *__comp_inst->extin[j] ? false : true;
				}
			#endif
			#if (NR_EXT_OUTPUTS>0)
				for (j=0;j<NR_EXT_OUTPUTS;j++) {
					gpioWrite (EXTOUT_LINES[j], *__comp_inst->extout[j]);
				}
			#endif
	}
}


