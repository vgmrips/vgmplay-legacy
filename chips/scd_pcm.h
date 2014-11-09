struct pcm_chip_
{
	float Rate;
	int Smpl0Patch;
	int Enable;
	int Cur_Chan;
	int Bank;

	struct pcm_chan_
	{
		unsigned int ENV;		/* envelope register */
		unsigned int PAN;		/* pan register */
		unsigned int MUL_L;		/* envelope & pan product letf */
		unsigned int MUL_R;		/* envelope & pan product right */
		unsigned int St_Addr;	/* start address register */
		unsigned int Loop_Addr;	/* loop address register */
		unsigned int Addr;		/* current address register */
		unsigned int Step;		/* frequency register */
		unsigned int Step_B;	/* frequency register binaire */
		unsigned int Enable;	/* channel on/off register */
		int Data;				/* wave data */
		unsigned int Muted;
	} Channel[8];
	
	unsigned long int RAMSize;
	unsigned char* RAM;
};

//extern struct pcm_chip_ PCM_Chip;
//extern unsigned char Ram_PCM[64 * 1024];
//extern int PCM_Enable;

//int  PCM_Init(int Rate);
//void PCM_Set_Rate(int Rate);
//void PCM_Reset(void);
//void PCM_Write_Reg(unsigned int Reg, unsigned int Data);
//int  PCM_Update(int **buf, int Length);

void rf5c164_update(UINT8 ChipID, stream_sample_t **outputs, int samples);
int device_start_rf5c164(UINT8 ChipID, int clock);
void device_stop_rf5c164(UINT8 ChipID);
void device_reset_rf5c164(UINT8 ChipID);
void rf5c164_w(UINT8 ChipID, offs_t offset, UINT8 data);
void rf5c164_mem_w(UINT8 ChipID, offs_t offset, UINT8 data);
void rf5c164_write_ram(UINT8 ChipID, offs_t DataStart, offs_t DataLength, const UINT8* RAMData);

void rf5c164_set_mute_mask(UINT8 ChipID, UINT32 MuteMask);
