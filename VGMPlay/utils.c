// This file is part of the VGMPlay program 
// Licensed under the GNU General Public License version 2 (or later)

#include "utils.h"

INT8 sign(double Value)
{
	if (Value > 0.0)
		return 1;
	else if (Value < 0.0)
		return -1;
	else
		return 0;
}

long int Round(double Value)
{
	// Alternative:	(fabs(Value) + 0.5) * sign(Value);
	return (long int)(Value + 0.5 * sign(Value));
}

double RoundSpecial(double Value, double RoundTo)
{
	return (long int)(Value / RoundTo + 0.5 * sign(Value)) * RoundTo;
}

void PrintTime(UINT32 SamplePos, UINT32 SmplRate)
{
	float TimeSec;
	UINT16 TimeMin;
	UINT16 TimeHours;
	
	TimeSec = (float) RoundSpecial(SamplePos / (double)SmplRate, 0.01);
	TimeMin = (UINT16) TimeSec / 60;
	TimeSec -= TimeMin * 60;
	TimeHours = TimeMin / 60;
	TimeMin %= 60;

	if (TimeHours > 0)
    printf("%hu:", TimeHours);

	printf("%02hu:%05.2f", TimeMin, TimeSec);
	return;
}
