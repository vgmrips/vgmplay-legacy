/***************************************************************************

  2608intf.c

  The YM2608 emulator supports up to 2 chips.
  Each chip has the following connections:
  - Status Read / Control Write A
  - Port Read / Data Write A
  - Control Write B
  - Data Write B

***************************************************************************/

#include "mamedef.h"
//#include "sndintrf.h"
//#include "streams.h"
#include <memory.h>	// for memset
#include <malloc.h>	// for free
#include "2608intf.h"
//#include "fm.h"

#define NULL	((void *)0)

typedef struct _ym2608_state ym2608_state;
struct _ym2608_state
{
	//sound_stream *	stream;
	//emu_timer *	timer[2];
	void *			chip;
	void *			psg;
	ym2608_interface intf;
	//const device_config *device;
};

#define CHTYPE_YM2608	0x21


extern UINT8 CHIP_SAMPLING_MODE;
extern INT32 CHIP_SAMPLE_RATE;
#define MAX_CHIPS	0x02
static ym2608_state YM2608Data[MAX_CHIPS];

#define SOUND_YM2608

/*INLINE ym2608_state *get_safe_token(const device_config *device)
{
	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type == SOUND);
	assert(sound_get_type(device) == SOUND_YM2608);
	return (ym2608_state *)device->token;
}*/



static void psg_set_clock(void *param, int clock)
{
	ym2608_state *info = (ym2608_state *)param;
	if (info->psg != NULL)
		ay8910_set_clock_ym(info->psg, clock);
}

static void psg_write(void *param, int address, int data)
{
	ym2608_state *info = (ym2608_state *)param;
	if (info->psg != NULL)
		ay8910_write_ym(info->psg, address, data);
}

static int psg_read(void *param)
{
	ym2608_state *info = (ym2608_state *)param;
	if (info->psg != NULL)
		return ay8910_read_ym(info->psg);
	else
		return 0x00;
}

static void psg_reset(void *param)
{
	ym2608_state *info = (ym2608_state *)param;
	if (info->psg != NULL)
		ay8910_reset_ym(info->psg);
}

static const ssg_callbacks psgintf =
{
	psg_set_clock,
	psg_write,
	psg_read,
	psg_reset
};


/* IRQ Handler */
static void IRQHandler(void *param,int irq)
{
	ym2608_state *info = (ym2608_state *)param;
	//if(info->intf->handler) info->intf->handler(info->device, irq);
	if(info->intf.handler) info->intf.handler(irq);
}

/* Timer overflow callback from timer.c */
/*static TIMER_CALLBACK( timer_callback_2608_0 )
{
	ym2608_state *info = (ym2608_state *)ptr;
	ym2608_timer_over(info->chip,0);
}

static TIMER_CALLBACK( timer_callback_2608_1 )
{
	ym2608_state *info = (ym2608_state *)ptr;
	ym2608_timer_over(info->chip,1);
}*/

static void timer_handler(void *param,int c,int count,int clock)
{
	ym2608_state *info = (ym2608_state *)param;
	if( count == 0 )
	{	/* Reset FM Timer */
		//timer_enable(info->timer[c], 0);
	}
	else
	{	/* Start FM Timer */
		//attotime period = attotime_mul(ATTOTIME_IN_HZ(clock), count);
		//if (!timer_enable(info->timer[c], 1))
		//	timer_adjust_oneshot(info->timer[c], period, 0);
	}
}

/* update request from fm.c */
void ym2608_update_request(void *param)
{
	ym2608_state *info = (ym2608_state *)param;
	//stream_update(info->stream);
	
	ym2608_update_one(info->chip, DUMMYBUF, 0);
	if (info->psg != NULL)
		ay8910_update_one(info->psg, DUMMYBUF, 0);
}

//static STREAM_UPDATE( ym2608_stream_update )
void ym2608_stream_update(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	//ym2608_state *info = (ym2608_state *)param;
	ym2608_state *info = &YM2608Data[ChipID];
	ym2608_update_one(info->chip, outputs, samples);
}

void ym2608_stream_update_ay(UINT8 ChipID, stream_sample_t **outputs, int samples)
{
	//ym2608_state *info = (ym2608_state *)param;
	ym2608_state *info = &YM2608Data[ChipID];
	
	memset(outputs[0], 0x00, samples * sizeof(stream_sample_t));
	memset(outputs[1], 0x00, samples * sizeof(stream_sample_t));
	if (info->psg != NULL)
		ay8910_update_one(info->psg, outputs, samples);
}


//static STATE_POSTLOAD( ym2608_intf_postload )
static void ym2608_intf_postload(UINT8 ChipID)
{
	//ym2608_state *info = (ym2608_state *)param;
	ym2608_state *info = &YM2608Data[ChipID];
	ym2608_postload(info->chip);
}


//static DEVICE_START( ym2608 )
int device_start_ym2608(UINT8 ChipID, int clock, unsigned char AYDisable, unsigned char AYFlags,
						int* AYrate)
{
	static const ym2608_interface generic_2608 =
	{
		{
			AY8910_LEGACY_OUTPUT | AY8910_SINGLE_OUTPUT,
			AY8910_DEFAULT_LOADS
			//DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL
		},
		NULL
	};
	//const ym2608_interface *intf = device->static_config ? (const ym2608_interface *)device->static_config : &generic_2608;
	ym2608_interface *intf;
	int rate;
	//void *pcmbufa;
	//int  pcmsizea;

	//ym2608_state *info = get_safe_token(device);
	ym2608_state *info;

	if (ChipID >= MAX_CHIPS)
		return 0;
	
	info = &YM2608Data[ChipID];
	rate = clock/72;
	if ((CHIP_SAMPLING_MODE == 0x01 && rate < CHIP_SAMPLE_RATE) ||
		CHIP_SAMPLING_MODE == 0x02)
		rate = CHIP_SAMPLE_RATE;
	info->intf = generic_2608;
	intf = &info->intf;
	if (AYFlags)
		intf->ay8910_intf.flags = AYFlags;
	//info->device = device;

	/* FIXME: Force to use simgle output */
	//info->psg = ay8910_start_ym(NULL, SOUND_YM2608, clock, &intf->ay8910_intf);
	if (! AYDisable)
	{
		info->psg = ay8910_start_ym(NULL, CHTYPE_YM2608, clock, &intf->ay8910_intf);
		*AYrate = clock / 32;
	}
	else
	{
		info->psg = NULL;
		*AYrate = 0;
	}
	//assert_always(info->psg != NULL, "Error creating YM2608/AY8910 chip");

	/* Timer Handler set */
	//info->timer[0] = timer_alloc(device->machine, timer_callback_2608_0, info);
	//info->timer[1] = timer_alloc(device->machine, timer_callback_2608_1, info);

	/* stream system initialize */
	//info->stream = stream_create(device,0,2,rate,info,ym2608_stream_update);
	/* setup adpcm buffers */
	//pcmbufa  = device->region;
	//pcmsizea = device->regionbytes;

	/* initialize YM2608 */
	//info->chip = ym2608_init(info,device,device->clock,rate,
	//	           pcmbufa,pcmsizea,
	//	           timer_handler,IRQHandler,&psgintf);
	info->chip = ym2608_init(info,clock,rate,
		           timer_handler,IRQHandler,&psgintf);
	//assert_always(info->chip != NULL, "Error creating YM2608 chip");

	//state_save_register_postload(device->machine, ym2608_intf_postload, info);
	
	return rate;
}

//static DEVICE_STOP( ym2608 )
void device_stop_ym2608(UINT8 ChipID)
{
	//ym2608_state *info = get_safe_token(device);
	ym2608_state *info = &YM2608Data[ChipID];
	ym2608_shutdown(info->chip);
	if (info->psg != NULL)
	{
		ay8910_stop_ym(info->psg);
		free(info->psg);
	}
}

//static DEVICE_RESET( ym2608 )
void device_reset_ym2608(UINT8 ChipID)
{
	//ym2608_state *info = get_safe_token(device);
	ym2608_state *info = &YM2608Data[ChipID];
	ym2608_reset_chip(info->chip);
	if (info->psg != NULL)
		ay8910_reset_ym(info->psg);
}


//READ8_DEVICE_HANDLER( ym2608_r )
UINT8 ym2608_r(UINT8 ChipID, offs_t offset)
{
	//ym2608_state *info = get_safe_token(device);
	ym2608_state *info = &YM2608Data[ChipID];
	return ym2608_read(info->chip, offset & 3);
}

//WRITE8_DEVICE_HANDLER( ym2608_w )
void ym2608_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	//ym2608_state *info = get_safe_token(device);
	ym2608_state *info = &YM2608Data[ChipID];
	ym2608_write(info->chip, offset & 3, data);
}

//READ8_DEVICE_HANDLER( ym2608_read_port_r )
UINT8 ym2608_read_port_r(UINT8 ChipID, offs_t offset)
{
	return ym2608_r(ChipID, 1);
}
//READ8_DEVICE_HANDLER( ym2608_status_port_a_r )
UINT8 ym2608_status_port_a_r(UINT8 ChipID, offs_t offset)
{
	return ym2608_r(ChipID, 0);
}
//READ8_DEVICE_HANDLER( ym2608_status_port_b_r )
UINT8 ym2608_status_port_b_r(UINT8 ChipID, offs_t offset)
{
	return ym2608_r(ChipID, 2);
}

//WRITE8_DEVICE_HANDLER( ym2608_control_port_a_w )
void ym2608_control_port_a_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2608_w(ChipID, 0, data);
}
//WRITE8_DEVICE_HANDLER( ym2608_control_port_b_w )
void ym2608_control_port_b_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2608_w(ChipID, 2, data);
}
//WRITE8_DEVICE_HANDLER( ym2608_data_port_a_w )
void ym2608_data_port_a_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2608_w(ChipID, 1, data);
}
//WRITE8_DEVICE_HANDLER( ym2608_data_port_b_w )
void ym2608_data_port_b_w(UINT8 ChipID, offs_t offset, UINT8 data)
{
	ym2608_w(ChipID, 3, data);
}


void ym2608_write_data_pcmrom(UINT8 ChipID, UINT8 rom_id, offs_t ROMSize, offs_t DataStart,
							  offs_t DataLength, const UINT8* ROMData)
{
	ym2608_state* info = &YM2608Data[ChipID];
	ym2608_write_pcmrom(info->chip, rom_id, ROMSize, DataStart, DataLength, ROMData);
}

void ym2608_set_mute_mask(UINT8 ChipID, UINT32 MuteMaskFM, UINT32 MuteMaskAY)
{
	ym2608_state* info = &YM2608Data[ChipID];
	ym2608_set_mutemask(info->chip, MuteMaskFM);
	if (info->psg != NULL)
		ay8910_set_mute_mask_ym(info->psg, MuteMaskAY);
}


/**************************************************************************
 * Generic get_info
 **************************************************************************/

/*DEVICE_GET_INFO( ym2608 )
{
	switch (state)
	{
		// --- the following bits of info are returned as 64-bit signed integers ---
		case DEVINFO_INT_TOKEN_BYTES:					info->i = sizeof(ym2608_state);				break;

		// --- the following bits of info are returned as pointers to data or functions ---
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( ym2608 );				break;
		case DEVINFO_FCT_STOP:							info->stop = DEVICE_STOP_NAME( ym2608 );				break;
		case DEVINFO_FCT_RESET:							info->reset = DEVICE_RESET_NAME( ym2608 );				break;

		// --- the following bits of info are returned as NULL-terminated strings ---
		case DEVINFO_STR_NAME:							strcpy(info->s, "YM2608");							break;
		case DEVINFO_STR_FAMILY:					strcpy(info->s, "Yamaha FM");						break;
		case DEVINFO_STR_VERSION:					strcpy(info->s, "1.0");								break;
		case DEVINFO_STR_SOURCE_FILE:						strcpy(info->s, __FILE__);							break;
		case DEVINFO_STR_CREDITS:					strcpy(info->s, "Copyright Nicola Salmoria and the MAME Team"); break;
	}
}*/
