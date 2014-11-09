void daccontrol_update(UINT8 ChipID, UINT32 samples);
UINT8 device_start_daccontrol(UINT8 ChipID);
void device_stop_daccontrol(UINT8 ChipID);
void device_reset_daccontrol(UINT8 ChipID);
void daccontrol_setup_chip(UINT8 ChipID, UINT8 ChType, UINT8 ChNum, UINT16 Command);
void daccontrol_set_data(UINT8 ChipID, UINT8* Data, UINT32 DataLen, UINT8 StepSize, UINT8 StepBase);
void daccontrol_refresh_data(UINT8 ChipID, UINT8* Data, UINT32 DataLen);
void daccontrol_set_frequency(UINT8 ChipID, UINT32 Frequency);
void daccontrol_start(UINT8 ChipID, UINT32 DataPos, UINT8 LenMode, UINT32 Length);
void daccontrol_stop(UINT8 ChipID);

#define DCTRL_LMODE_IGNORE	0x00
#define DCTRL_LMODE_CMDS	0x01
#define DCTRL_LMODE_MSEC	0x02
#define DCTRL_LMODE_TOEND	0x03
#define DCTRL_LMODE_BYTES	0x0F
