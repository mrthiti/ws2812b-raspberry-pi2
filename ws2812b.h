#ifndef WS2812B_H
#define WS2812B_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Base addresses for GPIO, PWM, and PWM clock.
// These will be "memory mapped" into virtual RAM so that they can be written and read directly.
// -------------------------------------------------------------------------------------------------
#define PERI_BASE       0x3F000000
#define GPIO_BASE               (PERI_BASE + 0x200000) // GPIO controller
#define PWM_BASE                (PERI_BASE + 0x20C000) // PWM controller
#define CLOCK_BASE              (PERI_BASE + 0x101000) // PWM clock manager
#define DMA_BASE                (PERI_BASE + 0x007000) // DMA controller


// Memory offsets for the PWM clock register, which is completely undocumented!
// -------------------------------------------------------------------------------------------------
#define PWM_CLK_CNTL 40         // Control (on/off)
#define PWM_CLK_DIV  41         // Divisor (bits 11:0 are *quantized* floating part, 31:12 integer part)


// PWM Register Addresses (page 141)
// These are divided by 4 because the register offsets in the guide are in bytes (8 bits) but
// the pointers we use in this program are in words (32 bits). Buss' original defines are in
// word offsets, e.g. PWM_RNG1 was 4 and PWM_DAT1 was 5. This is functionally the same, but it
// matches the numbers supplied in the guide.
// -------------------------------------------------------------------------------------------------
#define PWM_CTL  0x00           // Control Register
#define PWM_STA  (0x04 / 4)     // Status Register
#define PWM_DMAC (0x08 / 4)     // DMA Control Register
#define PWM_RNG1 (0x10 / 4)     // Channel 1 Range
#define PWM_DAT1 (0x14 / 4)     // Channel 1 Data
#define PWM_FIF1 (0x18 / 4)     // FIFO (for both channels - bytes are interleaved if both active)
#define PWM_RNG2 (0x20 / 4)     // Channel 2 Range
#define PWM_DAT2 (0x24 / 4)     // Channel 2 Data


// PWM_CTL register bit offsets
// -------------------------------------------------------------------------------------------------
#define PWM_CTL_MSEN2   15      // Channel 2 - 0: Use PWM algorithm. 1: Use M/S (serial) algorithm.
#define PWM_CTL_USEF2   13      // Channel 2 - 0: Use PWM_DAT2. 1: Use FIFO.
#define PWM_CTL_POLA2   12      // Channel 2 - Invert output polarity (if set, 0=high and 1=low)
#define PWM_CTL_SBIT2   11      // Channel 2 - Silence bit (default line state when not transmitting)
#define PWM_CTL_RPTL2   10      // Channel 2 - Repeat last data in FIFO
#define PWM_CTL_MODE2   9       // Channel 2 - Mode. 0=PWM, 1=Serializer
#define PWM_CTL_PWEN2   8       // Channel 2 - Enable PWM
#define PWM_CTL_CLRF1   6       // Clear FIFO
#define PWM_CTL_MSEN1   7       // Channel 1 - 0: Use PWM algorithm. 1: Use M/S (serial) algorithm.
#define PWM_CTL_USEF1   5       // Channel 1 - 0: Use PWM_DAT1. 1: Use FIFO.
#define PWM_CTL_POLA1   4       // Channel 1 - Invert output polarity (if set, 0=high and 1=low)
#define PWM_CTL_SBIT1   3       // Channel 1 - Silence bit (default line state when not transmitting)
#define PWM_CTL_RPTL1   2       // Channel 1 - Repeat last data in FIFO
#define PWM_CTL_MODE1   1       // Channel 1 - Mode. 0=PWM, 1=Serializer
#define PWM_CTL_PWEN1   0       // Channel 1 - Enable PWM


// PWM_STA register bit offsets
// -------------------------------------------------------------------------------------------------
#define PWM_STA_STA4    12      // Channel 4 State
#define PWM_STA_STA3    11      // Channel 3 State
#define PWM_STA_STA2    10      // Channel 2 State
#define PWM_STA_STA1    9       // Channel 1 State
#define PWM_STA_BERR    8       // Bus Error
#define PWM_STA_GAPO4   7       // Gap Occurred on Channel 4
#define PWM_STA_GAPO3   6       // Gap Occurred on Channel 3
#define PWM_STA_GAPO2   5       // Gap Occurred on Channel 2
#define PWM_STA_GAPO1   4       // Gap Occurred on Channel 1
#define PWM_STA_RERR1   3       // FIFO Read Error
#define PWM_STA_WERR1   2       // FIFO Write Error
#define PWM_STA_EMPT1   1       // FIFO Empty
#define PWM_STA_FULL1   0       // FIFO Full


// PWM_DMAC bit offsets
// -------------------------------------------------------------------------------------------------
#define PWM_DMAC_ENAB   31      // 0: DMA Disabled. 1: DMA Enabled.
#define PWM_DMAC_PANIC  8       // Bits 15:8. Threshold for PANIC signal. Default 7.
#define PWM_DMAC_DREQ   0       // Bits 7:0. Threshold for DREQ signal. Default 7.


// PWM_RNG1, PWM_RNG2
// --------------------------------------------------------------------------------------------------
// Defines the transmission range. In PWM mode, evenly spaced pulses are sent within a period
// of length defined in these registers. In serial mode, serialized data is sent within the
// same period. The value is normally 32. If less, data will be truncated. If more, data will
// be padded with zeros.


// DAT1, DAT2
// --------------------------------------------------------------------------------------------------
// NOTE: These registers are not useful for our purposes - we will use the FIFO instead!
// Stores 32 bits of data to be sent when USEF1/USEF2 is 0. In PWM mode, defines how many
// pulses will be sent within the period specified in PWM_RNG1/PWM_RNG2. In serializer mode,
// defines a 32-bit word to be transmitted.


// FIF1
// --------------------------------------------------------------------------------------------------
// 32-bit-wide register used to "stuff" the FIFO, which has 16 32-bit words. (So, if you write
// it 16 times, it will fill the FIFO.)
// See also:    PWM_STA_EMPT1 (FIFO empty)
//                              PWM_STA_FULL1 (FIFO full)
//                              PWM_CTL_CLRF1 (Clear FIFO)


// Bitwise Operations
// --------------------------------------------------------------------------------------------------
//   Set bit B in word W: W |= (1 << B)
// Clear bit B in word W: W &= ~(1 << B)
//   Get bit B in word W: bit = W & (1 << B)
#define SETBIT(word, bit) word |= 1<<bit
#define CLRBIT(word, bit) word &= ~(1<<bit)

// Page and block sizes for when we memory-map the registers
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  //   sets bits which are 1, ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1, ignores bits which are 0

// For convenience
#define true 1
#define false 0

// PWM waveform buffer, 16 32-bit words - enough to hold 170 wire bits
#define PWM_WAVEFORM_LENGTH 16

#define LED_BUFFER_LENGTH 5

// LED buffer (this will be translated into pulses in PWMWaveform[])
typedef struct Color_t {
        unsigned char r;
        unsigned char g;
        unsigned char b;
} Color_t;


class ws2812b{
	public:
		ws2812b( unsigned int numLED );
		unsigned char setPixelColor(unsigned int pixel, unsigned char r, unsigned char g, unsigned char b);
		void initHardware();
        void clearLEDBuffer();
        void show();
	
	private:
		unsigned int numLEDs;	// How many LEDs there are on the chain

        // I/O access
        volatile unsigned *gpio;
        volatile unsigned *pwm;
        volatile unsigned *clk;
        volatile unsigned *dma;

        unsigned int PWMWaveform[PWM_WAVEFORM_LENGTH];

        Color_t LEDBuffer[LED_BUFFER_LENGTH];
	
		volatile unsigned *mapRegisterMemory(int base);
		void setupRegisterMemoryMappings();
		void clearPWMBuffer();
		void enablePWM(unsigned char state);
		unsigned char FIFOEmpty();
		Color_t RGB2Color(unsigned char r, unsigned char g, unsigned char b);
		void printBinary(unsigned int i, unsigned int bits);
		void setPWMBit(unsigned int bitPos, unsigned char bit);
		unsigned char getPWMBit(unsigned int bitPos);
		void dumpLEDBuffer();
		unsigned int reverseWord(unsigned int word);
		void dumpPWMBuffer();
		void clearPWMErrors();
		void clearFIFO();
		void dumpPWMStatus();
        void dumpPWMControl(unsigned int word);
		
};

#endif // WS2812B_H
