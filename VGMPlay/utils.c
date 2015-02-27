/*
 *  This file is part of VGMPlay <https://github.com/vgmrips/vgmplay>
 *
 *  (c)2015 Felipe C. da S. Sanches <juca@members.fsf.org>
 *  (c)2015 Valley Bell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

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
