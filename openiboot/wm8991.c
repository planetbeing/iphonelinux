/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: wm8985.c 21798 2009-07-12 09:43:44Z kugel $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "i2c.h"
#include "wmcodec.h"
#include "hardware/i2s.h"
#include "hardware/wmcodec.h"
#include "clock.h"
#include "dma.h"
#include "util.h"
#include "openiboot-asmhelpers.h"
#include "gpio.h"

const void* pcm_buffer;
uint32_t pcm_buffer_size;

int dma_controller = -1;
int dma_channel = -1;

volatile static int transfersDone;

volatile static int stopTransfers;

void audiohw_preinit();
static void switch_hp_speakers(int use_speakers);

static uint16_t regcache[0x40];

void audiohw_init()
{
	clock_gate_switch(I2S0_CLOCK, ON);
	clock_gate_switch(I2S1_CLOCK, ON);
	audiohw_preinit();
	memset(regcache, 0, sizeof(regcache));
}

void wmcodec_write(int reg, int data)
{
	uint8_t buffer[3];
	buffer[0] = reg;
	buffer[1] = (data >> 8) & 0xFF;
	buffer[2] = data & 0xFF;

	i2c_tx(WMCODEC_I2C, WMCODEC_I2C_SLAVE_ADDR, buffer, sizeof(buffer));
	regcache[reg] = data;
}

int wmcodec_read(int reg)
{
	uint8_t registers[1];
	uint8_t out[2];
	uint16_t data;

	registers[0] = reg;

	i2c_rx(WMCODEC_I2C, WMCODEC_I2C_SLAVE_ADDR, registers, 1, out, 2);
	data = (out[0] << 8) | out[1];

	regcache[reg] = data;
	return data;
}

static void iis_transfer_done(int status, int controller, int channel)
{
	++transfersDone;
	CleanCPUDataCache();
	dma_finish(controller, channel, 0);
	dma_controller = -1;
	dma_channel = -1;
	bufferPrintf("audio: playback complete\r\n");
	// replace the above line with this bottom one for repeat
	// dma_perform((uint32_t)pcm_buffer, DMA_WM_I2S_TX, pcm_buffer_size, 0, &controller, &channel);
}

int audiohw_transfers_done()
{
	return transfersDone;
}

void audiohw_pause()
{
	if(dma_controller == -1)
		return;

	dma_pause(dma_controller, dma_channel);
}

void audiohw_resume()
{
	if(dma_controller == -1)
		return;

	dma_resume(dma_controller, dma_channel);
}

uint32_t audiohw_get_position()
{
	if(dma_controller == -1)
		return 0;

	return dma_srcpos(dma_controller, dma_channel);
}

uint32_t audiohw_get_total()
{
	if(dma_controller == -1)
		return 0;

	return pcm_buffer_size;
}

void audiohw_play_pcm(const void* addr_in, uint32_t size, int use_speaker)
{
	pcm_buffer = addr_in;
	pcm_buffer_size = size;

	uint32_t i2sController;
	uint32_t dma;

	switch_hp_speakers(use_speaker);

	i2sController = WM_I2S;
	dma = DMA_WM_I2S_TX;

	SET_REG(i2sController + I2S_TXCON,
			(1 << 24) |  /* undocumented */
			(1 << 20) |  /* undocumented */
			(0 << 16) |  /* burst length */
			(0 << 15) |  /* 0 = falling edge */
			(0 << 13) |  /* 0 = basic I2S format */
			(0 << 12) |  /* 0 = MSB first */
			(0 << 11) |  /* 0 = left channel for low polarity */
			(3 << 8) |   /* MCLK divider */
			(0 << 5) |   /* 0 = 16-bit */
			(0 << 3) |   /* bit clock per frame */
			(1 << 0));    /* channel index */

	int controller = 0;
	int channel = 0;

	transfersDone = 0;
	stopTransfers = 0;
	
	CleanCPUDataCache();

	dma_request(DMA_MEMORY, 2, 1, dma, 2, 1, &controller, &channel, iis_transfer_done);

	dma_perform((uint32_t)pcm_buffer, dma, pcm_buffer_size, 0, &controller, &channel);

	dma_controller = controller;
	dma_channel = channel;

	SET_REG(i2sController + I2S_CLKCON, (1 << 0)); /* 1 = power on */
	SET_REG(i2sController + I2S_TXCOM, 
			(0 << 3) |   /* 1 = transmit mode on */
			(1 << 2) |   /* 1 = I2S interface enable */
			(1 << 1) |   /* 1 = DMA request enable */
			(0 << 0));    /* 0 = LRCK on */

	CleanAndInvalidateCPUDataCache();

}

const struct sound_settings_info audiohw_settings[] = {
    [SOUND_VOLUME]        = {"dB", 0,  1, -73,   6, -25},
    [SOUND_BASS]          = {"dB", 0,  1, -12,  12,   0},
    [SOUND_TREBLE]        = {"dB", 0,  1, -12,  12,   0},
    [SOUND_BALANCE]       = {"%",  0,  1,-100, 100,   0},
    [SOUND_CHANNELS]      = {"",   0,  1,   0,   5,   0},
    [SOUND_STEREO_WIDTH]  = {"%",  0,  5,   0, 250, 100},
    [SOUND_BASS_CUTOFF]   = {"",   0,  1,   1,   4,   1},
    [SOUND_TREBLE_CUTOFF] = {"",   0,  1,   1,   4,   1},
};

#define RESET		0x0
#define PWRMGMT1	0x1
#define PWRMGMT2	0x2
#define PWRMGMT3	0x3
#define AINTFACE1	0x4
#define AINTFACE2	0x5
#define CLOCKING1	0x6
#define CLOCKING2	0x7
#define AINTFACE3	0x8
#define AINTFACE4	0x9
#define DACCTRL		0xA
#define LDACVOL		0xB
#define RDACVOL		0xC
#define SIDETONE	0xD
#define ADCCTRL		0xE
#define LADCVOL		0xF
#define RADCVOL		0x10
#define GPIOCTRL1	0x12
#define GPIO12		0x13
#define GPIO34		0x14
#define GPIO56		0x15
#define GPI78		0x16
#define GPIOCTRL2	0x17
#define LIN12PGAVOL	0x18
#define LIN34PGAVOL	0x19
#define RIN12PGAVOL	0x1A
#define RIN34PGAVOL	0x1B
#define LOUTVOL		0x1C
#define ROUTVOL		0x1D
#define LINEOUTVOL	0x1E
#define OUT34VOL	0x1F
#define LOPGAVOL	0x20
#define ROPGAVOL	0x21
#define SPKATTN		0x22
#define SPKCLASSD1	0x23
#define SPKCLASSD2	0x24
#define SPKCLASSD3	0x25
#define SPKCLASSD4	0x26
#define INPUTMIX1	0x27
#define INPUTMIX2	0x28
#define INPUTMIX3	0x29
#define INPUTMIX4	0x2A
#define INPUTMIX5	0x2B
#define INPUTMIX6	0x2C
#define OUTPUTMIX1	0x2D
#define OUTPUTMIX2	0x2E
#define OUTPUTMIX3	0x2F
#define OUTPUTMIX4	0x30
#define OUTPUTMIX5	0x31
#define OUTPUTMIX6	0x32
#define OUT34MIX	0x33
#define LINEOUTMIX1	0x34
#define LINEOUTMIX2	0x35
#define SPKMIX		0x36
#define ADDCTRL		0x37
#define ANTIPOP1	0x38
#define ANTIPOP2	0x39
#define MICBIAS		0x3A
#define PLL1		0x3C
#define PLL2		0x3D
#define PLL3		0x3E

void wm8991_int(uint32_t token)
{
	if(gpio_pin_state(WMCODEC_INT_GPIO) == 1)
	{
		uint16_t status = wmcodec_read(GPIOCTRL1);
		if(status & (1 << 4))
		{
			// GPIO5 is triggered
			int polarity = wmcodec_read(GPIOCTRL2);
			int polarity_gpio5 = polarity & (1 << 4);

			if(polarity_gpio5)
			{
				// We previously sent an IRQ if GPIO5 was low, now let's send one if GPIO5 goes high.
				polarity = polarity & ~(1 << 4);
				bufferPrintf("audio: headphones removed.\r\n");
			} else
			{
				// We previously sent an IRQ if GPIO5 was high, now let's send one if GPIO5 goes low.
				polarity = polarity | (1 << 4);
				bufferPrintf("audio: headphones plugged in.\r\n");
			}
			wmcodec_write(GPIOCTRL2, polarity);
			wmcodec_write(GPIOCTRL1, status);
		}
	}
}

/* Silently enable / disable audio output */
void audiohw_preinit(void)
{
	if(wmcodec_read(RESET) != 0x8990)
	{
		bufferPrintf("wm8991: error, device id mismatch.\r\n");
		return;
	}

	bufferPrintf("wm8991: starting init.\r\n");

	audiohw_mute(1);

	// speaker enabled, VREF bias enabled, Vmid to 2 * 50kOhm (normal mode) 
	wmcodec_write(PWRMGMT1, 0x1003);

	// PLL enable, thermal sensor enable, thermal shutdown enable
	wmcodec_write(PWRMGMT2, 0xe000);

	// Left DAC enable, right DAC enable
	wmcodec_write(PWRMGMT3, 0x0003);

	// Right ADC data is output on right channel, normal i2s format
	wmcodec_write(AINTFACE1, 0x4010);

	// Right DAC outputs right channel.
	wmcodec_write(AINTFACE2, 0x4000);

	// Timeout clock disabled
	// GPIO output clock = SYSCLK
	// Class D Clock Divider = SYSCLK / 16
	// BCLK frequency = SYSCLK / 8
	wmcodec_write(CLOCKING1, 0x01ce);

	// SYSCLK is from PLL output
	// SYSCLK = PLL / 2
	wmcodec_write(CLOCKING2, 0x5000);

	// Interface 1 = master, interface 2 = slave, interface 1 selected, ADCLRC rate = BCLK / 32
	wmcodec_write(AINTFACE3, 0x8020);

	// ADCLRC/GPIO1 pin is GPIO1, DACLRC rate = BCLK / 32
	wmcodec_write(AINTFACE4, 0x8020);

	// Full DAC volume
	wmcodec_write(LDACVOL, 0xc0);
	wmcodec_write(RDACVOL, 0x1c0);

	// ADC digital high pass enable
	wmcodec_write(ADCCTRL, 0x100);

	// Full ADC volume
	wmcodec_write(LADCVOL, 0xc0);
	wmcodec_write(RADCVOL, 0x1c0);

	// GPIO 2 pull down enable, GPIO 2 is IRQ output, GPIO 1 is an input pin
	wmcodec_write(GPIO12, 0x1700);

	// GPIO 4 pull down enable and is input, GPIO 3 is input
	wmcodec_write(GPIO34, 0x1000);

	// GPIO 6 pull down enable and is input, GPIO 5 is input and has IRQ enabled
	wmcodec_write(GPIO56, 0x1040);

	// 4-wire readback mode
	wmcodec_write(GPI78, 0x0);

	// Temperature sensor inverted, gpio 3 polarity inverted
	wmcodec_write(GPIOCTRL2, 0x0804);

	// All input PGAs to 0dB and muted
	wmcodec_write(LIN12PGAVOL, 0x008b);
	wmcodec_write(LIN34PGAVOL, 0x008b);
	wmcodec_write(RIN12PGAVOL, 0x008b);
	wmcodec_write(RIN34PGAVOL, 0x008b);

	// headphones at -73 dB, zero cross enabled
	wmcodec_write(LOUTVOL, 0x00b0);
	wmcodec_write(ROUTVOL, 0x01b0);

	// Line out muted
	wmcodec_write(LINEOUTVOL, 0x0066);

	// out3 and out4 muted
	wmcodec_write(OUT34VOL, 0x0022);

	// zero cross enabled, 0 dB 
	wmcodec_write(LOPGAVOL, 0x00f9);
	wmcodec_write(ROPGAVOL, 0x01f9);

	// no speaker output attenuation
	wmcodec_write(SPKATTN, 0x0);

	// speaker amplifier in class AB mode
	wmcodec_write(SPKCLASSD1, 0x103);

	// reserved. data sheet says should be 0x55, but 0x57 on iPhone
	wmcodec_write(SPKCLASSD2, 0x57);

	// no speaker boost
	wmcodec_write(SPKCLASSD3, 0x0100);

	// speaker volume zero cross enabled, -1 dB
	wmcodec_write(SPKCLASSD4, 0x01f8);

	wmcodec_write(INPUTMIX1, 0x0);
	wmcodec_write(INPUTMIX2, 0x0);
	wmcodec_write(INPUTMIX3, 0x0);
	wmcodec_write(INPUTMIX4, 0x0);
	wmcodec_write(INPUTMIX5, 0x0);
	wmcodec_write(INPUTMIX6, 0x0);
	wmcodec_write(OUTPUTMIX1, 0x0);
	wmcodec_write(OUTPUTMIX2, 0x0);
	wmcodec_write(OUTPUTMIX3, 0x0);
	wmcodec_write(OUTPUTMIX4, 0x0);
	wmcodec_write(OUTPUTMIX5, 0x0);
	wmcodec_write(OUTPUTMIX6, 0x0);

	// analogue bias optimisation to lowest (default)
	wmcodec_write(OUT34MIX, 0x180);

	wmcodec_write(LINEOUTMIX1, 0x0);
	wmcodec_write(LINEOUTMIX2, 0x0);

	// unmute left and right DAC to SPKMIX
	wmcodec_write(SPKMIX, 0x3);

	wmcodec_write(ADDCTRL, 0x0);
	wmcodec_write(ANTIPOP1, 0x0);

	// Enables the VGS / R current generator and the analogue input and output bias
	wmcodec_write(ANTIPOP2, 0x8);

	wmcodec_write(MICBIAS, 0x0);

	// PLL fractional mode, N = 7
	wmcodec_write(PLL1, 0x0087);

	// K = 0x85FC = 34300
	wmcodec_write(PLL2, 0x0085);
	wmcodec_write(PLL3, 0x00fc);

	// R = 7.523376465

	gpio_pin_use_as_input(WMCODEC_INT_GPIO);
	gpio_register_interrupt(WMCODEC_INT, TRUE, TRUE, FALSE, wm8991_int, 0);
	gpio_interrupt_enable(WMCODEC_INT);

	bufferPrintf("wm8991: init complete.\r\n");

}

void audiohw_mute(int mute)
{
    if (mute)
    {
        /* Set DACMU = 1 to soft-mute the audio DACs. */
    	wmcodec_write(DACCTRL, 0x4);
    } else {
        /* Set DACMU = 0 to soft-un-mute the audio DACs. */
    	wmcodec_write(DACCTRL, 0x0);
    }
}

void audiohw_postinit(void)
{
    audiohw_mute(0);
}

void audiohw_set_headphone_vol(int vol_l, int vol_r)
{
	vol_l = ((vol_l * 0x50) / 63) + 0x2F;
	vol_r = ((vol_r * 0x50) / 63) + 0x2F;

	wmcodec_write(LOUTVOL, 0x080 | vol_l);
	wmcodec_write(ROUTVOL, 0x180 | vol_r);
}

void audiohw_set_speaker_vol(int vol)
{
	// -73 dB to 6 dB
	wmcodec_write(SPKCLASSD4, 0x180 | (((vol * 0x50) / 100) + 0x2F));
}

static void switch_hp_speakers(int use_speakers)
{
	if(use_speakers)
		wmcodec_write(PWRMGMT1, 0x1003);	// speaker enable, LOUT and ROUT disable
	else
		wmcodec_write(PWRMGMT1, 0x0303);	// speaker disable, LOUT and ROUT enable

	if(use_speakers)
		wmcodec_write(PWRMGMT3, 0x3);		// LOMIX and ROMIX disable
	else
		wmcodec_write(PWRMGMT3, 0x33);		// LOMIX and ROMIX enable

	if(use_speakers)
	{
		wmcodec_write(LOPGAVOL, 0x00f9);	// LOPGA and ROPGA volume to 0dB
		wmcodec_write(ROPGAVOL, 0x01f9);
	} else
	{
		wmcodec_write(LOPGAVOL, 0x00b0);	// LOPGA and ROPGA volume to just above mute
		wmcodec_write(ROPGAVOL, 0x01b0);
	}

	if(use_speakers)
		wmcodec_write(SPKATTN, 0x0);		// No attenuation of speaker volume
	else
		wmcodec_write(SPKATTN, 0x3);		// Mute speaker


	if(use_speakers)
	{
		wmcodec_write(OUTPUTMIX1, 0x0);		// mute LOMIX and ROMIX
		wmcodec_write(OUTPUTMIX2, 0x0);
	} else
	{
		wmcodec_write(OUTPUTMIX1, 0x1);		// unmute LOMIX and ROMIX
		wmcodec_write(OUTPUTMIX2, 0x1);
	}

	if(use_speakers)
		wmcodec_write(SPKMIX, 0x3);		// DAC to SPKMIX
	else
		wmcodec_write(SPKMIX, 0x0);		// No DAC to SPKMIX

	// unknown
	if(use_speakers)
		wmcodec_write(ADDCTRL, 0x0);
	else
		wmcodec_write(ADDCTRL, 0x10);

}

void audiohw_switch_normal_call(int in_call)
{
	// the basic idea is to route LIN4 to OUT3, RIN4 to OUT4 and LIN1 and LIN2 to ROP and RON.
	// one route connects microphone to baseband, the other connects the baseband with the headset speaker.
	if(in_call)
	{
		// speaker disable, out3 and out4 enable. Bit 5 enable
		wmcodec_write(PWRMGMT1, 0x0c23);

		// PLL disable, LIN12 input PGA enable
		wmcodec_write(PWRMGMT2, 0x6040);

		// enable RON and ROP, disable left DAC and right DAC
		wmcodec_write(PWRMGMT3, 0x0c00);
	} else
	{
		wmcodec_write(PWRMGMT1, 0x1003);
		wmcodec_write(PWRMGMT2, 0xe000);
		wmcodec_write(PWRMGMT3, 0x0003);
	}

	if(in_call)
		wmcodec_write(DACCTRL, 0x5);	// dac mute and dacr_datinv
	else
		wmcodec_write(DACCTRL, 0x0);

	if(in_call)
		wmcodec_write(LIN12PGAVOL, 0x114);	// LI12MUTE off, (update volume), LIN12 volume i crease
	else
		wmcodec_write(LIN12PGAVOL, 0x8b);

	if(in_call)
	{
		// RON unmute, ROP unmute
		wmcodec_write(LINEOUTVOL, 0x60);

		// OUT3 unmute, OUT4 unmute
		wmcodec_write(OUT34VOL, 0x0);
	} else
	{
		wmcodec_write(LINEOUTVOL, 0x0066);
		wmcodec_write(OUT34VOL, 0x0022);
	}

	if(in_call)
		wmcodec_write(SPKATTN, 0x3);	// mute speakers
	else
		wmcodec_write(SPKATTN, 0x0);

	if(in_call)
		wmcodec_write(INPUTMIX2, 0x0030);	// LIN1 and LIN2 connected to PGA.
	else
		wmcodec_write(INPUTMIX2, 0x0000);

	if(in_call)
		wmcodec_write(OUT34MIX, 0x01b3);	// LIN4/RXN pin to OUT3MIX unmute, LOPGA to OUT3MIX umute, RIN4/RXP Pin to OUT4MIX unmute, ROPGA to OUT4MIX unmute
	else
		wmcodec_write(OUT34MIX, 0x180);

	if(in_call)
		wmcodec_write(LINEOUTMIX2, 0x14);	// ROP output to RONMIX unmute, LIN12 PGA to ROPMIX unmute
	else
		wmcodec_write(LINEOUTMIX2, 0x0);

	if(in_call)
		wmcodec_write(SPKMIX, 0x0);		// disable spekaers
	else
		wmcodec_write(SPKMIX, 0x3);

	if(in_call)
		wmcodec_write(MICBIAS, 0x40);		// MICBIAS short circuit current detect threshold to 120 microamps.
	else
		wmcodec_write(MICBIAS, 0x0);

}
