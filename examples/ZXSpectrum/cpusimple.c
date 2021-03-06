/*
 * ZXSpectrum
 *
 *  Uses Z80 step core built in EDL
 *  Rest is C for now
 */

#include <stdio.h>
#include <stdint.h>

void STEP(void);
void RESET(void);
void INTERRUPT(uint8_t);

extern uint16_t	PC;
extern uint8_t CYCLES;
int Disassemble(unsigned int address,int registers);

void CPU_RESET()
{
	RESET();
}

int CPU_STEP(int intClocks,int doDebug)
{
	if (doDebug)
	{
		Disassemble(PC,1);
	}
	if (intClocks)
	{
		INTERRUPT(0xFF);
	
		if (CYCLES==0)
		{
			STEP();
		}
	}
	else
	{
		STEP();
	}

	return CYCLES;
}
