/*
 * VIC20 implementation
 *
 *
 *
 *
 *
 *
 *
 */

#include <GL/glfw3.h>
#include <GL/glext.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

uint16_t PinGetPIN_AB();
uint8_t PinGetPIN_DB();
void PinSetPIN_DB(uint8_t);
void PinSetPIN_O0(uint8_t);
uint8_t PinGetPIN_SYNC();
uint8_t PinGetPIN_RW();
void PinSetPIN__IRQ(uint8_t);
void PinSetPIN__RES(uint8_t);

// Step 1. Memory

unsigned char DLo[0x2002];
unsigned char DHi[0x2002];

unsigned char CRom[0x1000];
unsigned char BRom[0x2000];
unsigned char KRom[0x2000];

unsigned char RamLo[0x400];
unsigned char RamHi[0xE00];

unsigned char CRam[0x400];
unsigned char SRam[0x200];

#define USE_CART_A0		1
#define USE_CART_20		0

int LoadRom(unsigned char* rom,unsigned int size,const char* fname)
{
	unsigned int readFileSize=0;
	FILE* inFile = fopen(fname,"rb");
	if (!inFile || size != (readFileSize = fread(rom,1,size,inFile)))
	{
		printf("Failed to open rom : %s - %d/%d",fname,readFileSize,size);
		return 1;
	}
	fclose(inFile);
	return 0;
}


int InitialiseMemory()
{
	if (LoadRom(CRom,0x1000,"roms/901460-03.ud7"))
		return 1;
	if (LoadRom(BRom,0x2000,"roms/901486-01.ue11"))
		return 1;
	if (LoadRom(KRom,0x2000,"roms/901486-07.ue12"))
		return 1;

#if 0
	if (LoadRom(DLo,0x2002,"roms/Donkey Kong-2000.prg"))
		return 1;
	if (LoadRom(DHi,0x2002,"roms/Donkey Kong-A000.prg"))
		return 1;
#else
//	if (LoadRom(DHi,0x2002,"roms/Cosmic Cruncher (1982)(Commodore).a0"))
//		return 1;
	if (LoadRom(DHi,0x2002,"roms/Omega Race (1982)(Commodore).a0"))
		return 1;
//	if (LoadRom(DHi,0x2002,"roms/Arcadia (19xx)(-).a0"))
//		return 1;
#endif
	return 0;
}

uint8_t VIAGetByte(int chipNo,int regNo);
void VIASetByte(int chipNo,int regNo,uint8_t byte);
void VIATick(int chipNo);

uint8_t GetByte6561(int regNo);
void SetByte6561(int regNo,uint8_t byte);
void Tick6561();

uint8_t GetByte(uint16_t addr)
{
	if (addr<0x0400)
	{
		return RamLo[addr];
	}
	if (addr<0x1000)
	{
		return 0xFF;
	}
	if (addr<0x1E00)
	{
		return RamHi[addr-0x1000];
	}
	if (addr<0x2000)
	{
		return SRam[addr-0x1E00];
	}
	if (addr<0x4000)
	{
#if USE_CART_20
		return DLo[2+(addr-0x2000)];
#else
		return 0xFF;
#endif
	}
	if (addr<0x8000)
	{
		return 0xFF;
	}
	if (addr<0x9000)
	{
		return CRom[addr-0x8000];
	}
	if (addr<0x9400)
	{
		if (addr>=0x9000 && addr<=0x900F)
		{
			return GetByte6561(addr&0x0F);
		}
		if (addr>=0x9110 && addr<=0x912F)
		{
			return VIAGetByte(((addr-0x10)>>4)&1,addr&0x0F);
		}
		// Various expansions
		printf("Attempt to acccess : %04X\n",addr);
		return 0xFF;
	}
	if (addr<0x9800)
	{
		return CRam[addr-0x9400]&0x0F;
	}
	if (addr<0xA000)
	{
		return 0xFF;
	}
	if (addr<0xC000)
	{
#if USE_CART_A0
		return DHi[2+(addr-0xA000)];
#else
		return 0xFF;
#endif
	}
	if (addr<0xE000)
	{
		return BRom[addr-0xC000];
	}
	return KRom[addr-0xE000];
}

void SetByte(uint16_t addr,uint8_t byte)
{
	if (addr<0x0400)
	{
		RamLo[addr]=byte;
		return;
	}
	if (addr<0x1000)
	{
		return;
	}
	if (addr<0x1E00)
	{
		RamHi[addr-0x1000]=byte;
		return;
	}
	if (addr<0x2000)
	{
		SRam[addr-0x1E00]=byte;
		return;
	}
	if (addr<0x8000)
	{
		return;
	}
	if (addr<0x9000)
	{
		return;
	}
	if (addr<0x9400)
	{
		if (addr>=0x9000 && addr<=0x900F)
		{
			SetByte6561(addr&0x0F,byte);
			return;
		}
		if (addr>=0x9110 && addr<=0x912F)
		{
			VIASetByte(((addr-0x10)>>4)&1,addr&0x0F,byte);
			return;
		}
		// Various expansions
		printf("WRITE Attempt to acccess :%02X %04X\n",byte,addr);
		return;
	}
	if (addr<0x9800)
	{
		CRam[addr-0x9400]=byte&0x0F;
		return;
	}
	if (addr<0xC000)
	{
		return;
	}
	if (addr<0xE000)
	{
		return;
	}
	return;
}


int masterClock=0;
int pixelClock=0;
int cpuClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

extern uint8_t *DIS_[256];

extern uint8_t	A;
extern uint8_t	X;
extern uint8_t	Y;
extern uint16_t	SP;
extern uint16_t	PC;
extern uint8_t	P;

struct via6522
{
	uint8_t	ORB;
	uint8_t IRB;
	uint8_t ORA;
	uint8_t IRA;
	uint8_t	DDRB;
	uint8_t DDRA;
	uint16_t	T1C;
	uint8_t T1LL;
	uint8_t T1LH;
	uint8_t	T2LL;
	uint16_t	T2C;
	uint8_t SR;
	uint8_t ACR;
	uint8_t PCR;
	uint8_t IFR;
	uint8_t IER;
	uint8_t T2TimerOff;
};

struct via6522	via[2];

void DUMP_REGISTERS()
{
	printf("--------\n");
	printf("FLAGS = N  V  -  B  D  I  Z  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			P&0x80 ? "1" : "0",
			P&0x40 ? "1" : "0",
			P&0x20 ? "1" : "0",
			P&0x10 ? "1" : "0",
			P&0x08 ? "1" : "0",
			P&0x04 ? "1" : "0",
			P&0x02 ? "1" : "0",
			P&0x01 ? "1" : "0");
	printf("A = %02X\n",A);
	printf("X = %02X\n",X);
	printf("Y = %02X\n",Y);
	printf("SP= %04X\n",SP);
	printf("VIA1 IFR/IER/ACR/PCR/T1C/T2C = %02X/%02X/%02X/%02X/%04X/%04X\n",via[0].IFR,via[0].IER,via[0].ACR,via[0].PCR,via[0].T1C,via[0].T2C);
	printf("VIA2 IFR/IER/ACR/PCR/T1C/T2C = %02X/%02X/%02X/%02X/%04X/%04X\n",via[1].IFR,via[1].IER,via[1].ACR,via[1].PCR,via[1].T1C,via[1].T2C);
	printf("--------\n");
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = GetByte(address);
	if (byte>realLength)
	{
		sprintf(temporaryBuffer,"UNKNOWN OPCODE");
		return temporaryBuffer;
	}
	else
	{
		const char* mnemonic=table[byte];
		const char* sPtr=mnemonic;
		char* dPtr=temporaryBuffer;
		int counting = 0;
		int doingDecode=0;

		if (sPtr==NULL)
		{
			sprintf(temporaryBuffer,"UNKNOWN OPCODE");
			return temporaryBuffer;
		}

		while (*sPtr)
		{
			if (!doingDecode)
			{
				if (*sPtr=='%')
				{
					doingDecode=1;
					if (*(sPtr+1)=='$')
						sPtr++;
				}
				else
				{
					*dPtr++=*sPtr;
				}
			}
			else
			{
				char *tPtr=sprintBuffer;
				int negOffs=1;
				if (*sPtr=='-')
				{
					sPtr++;
					negOffs=-1;
				}
				int offset=(*sPtr-'0')*negOffs;
				sprintf(sprintBuffer,"%02X",GetByte(address+offset));
				while (*tPtr)
				{
					*dPtr++=*tPtr++;
				}
				doingDecode=0;
				counting++;
			}
			sPtr++;
		}
		*dPtr=0;
		*count=counting;
	}
	return temporaryBuffer;
}

int Disassemble(unsigned int address,int registers)
{
	int a;
	int numBytes=0;
	const char* retVal = decodeDisasm(DIS_,address,&numBytes,255);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %04X\n",address);
		for (a=0;a<numBytes+1;a++)
		{
			printf("%02X ",GetByte(address+a));
		}
		printf("\n");
		exit(-1);
	}

	if (registers)
	{
		DUMP_REGISTERS();
	}
	printf("%04X :",address);

	for (a=0;a<numBytes+1;a++)
	{
		printf("%02X ",GetByte(address+a));
	}
	for (a=0;a<8-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);

	return numBytes+1;
}


unsigned char NEXTINT;

int dumpInstruction=0;

int g_instructionStep=0;
int g_traceStep=0;

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define HEIGHT	23*8
#define	WIDTH	22*8

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW		1
#define REGISTER_WINDOW		2

GLFWwindow windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

uint8_t CTRL_1=0;
uint8_t CTRL_2=0;
uint8_t CTRL_3=0;
uint8_t CTRL_4=0;
uint8_t CTRL_5=0;
uint8_t CTRL_6=0;
uint8_t CTRL_7=0;
uint8_t CTRL_8=0;
uint8_t CTRL_9=0;
uint8_t CTRL_10=0;
uint8_t CTRL_11=0;
uint8_t CTRL_12=0;
uint8_t CTRL_13=0;
uint8_t CTRL_14=0;
uint8_t CTRL_15=0;
uint8_t CTRL_16=0;

uint32_t	cTable[16]={
	0x00000000,0x00ffffff,0x00782922,0x0087d6dd,0x00aa5fb6,0x0055a049,0x0040318d,0x00bfce72,
	0x00aa7449,0x00eab489,0x00b86962,0x00c7ffff,0x00eaf9f6,0x0094e089,0x008071cc,0x00ffffb2,
			};

void ShowScreen(int windowNum,int w,int h)
{
	if (windowNum==MAIN_WINDOW)
	{
		int x,y,xx,yy;
		uint32_t* outputTexture = (uint32_t*)videoMemory[windowNum];
		uint16_t addr,addr1,addr2;
		uint16_t chaddr;
		uint16_t caddr;
		uint32_t paper;
		uint32_t auxcol;

		addr1=CTRL_6&0x70;
		addr2=CTRL_3&0x80;
		addr=(addr1<<6)|(addr2<<2);
		if ((CTRL_6&0x80)==0)
		{
			addr|=0x8000;
		}
		
		chaddr=CTRL_6&0x07;
		chaddr<<=10;
		if ((CTRL_6&0x08)==0)
		{
			chaddr|=0x8000;
		}

		caddr=CTRL_3&0x80;
		caddr<<=2;
		caddr|=0x9400;

		paper=(CTRL_16&0xF0)>>4;
		paper=cTable[paper];

		auxcol=(CTRL_15&0xF0)>>4;
		auxcol=cTable[auxcol];

		printf("addr : %04X\n",chaddr);

		for (y=0;y<23;y++)
		{
			for (x=0;x<22;x++)
			{
				uint16_t index;
				uint32_t col = GetByte(caddr+x+y*22);
				if (col&0x80)
					printf("MULTICOLOR\n");
				col=cTable[col&7];
				index=GetByte(addr+x+y*22);
				index<<=3;

				for (yy=0;yy<8;yy++)
				{
					uint8_t byte = GetByte((chaddr+index));
					index++;
					for (xx=7;xx>=0;xx--)
					{
						if (byte&(1<<xx))
						{
							outputTexture[(x*8+(7-xx)+(y*8+yy)*22*8)]=col;
						}
						else
						{
							outputTexture[(x*8+(7-xx)+(y*8+yy)*22*8)]=paper;
						}
					}
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(0.0f, h);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f(w, h);
	glVertex2f(1.0f, -1.0f);

	glTexCoord2f(w, 0.0f);
	glVertex2f(1.0f, 1.0f);
	glEnd();
	
	glFlush();
}

void setupGL(int windowNum,int w, int h) 
{
	videoTexture[windowNum] = windowNum+1;
	videoMemory[windowNum] = (unsigned char*)malloc(w*h*sizeof(unsigned int));
	memset(videoMemory[windowNum],0,w*h*sizeof(unsigned int));
	//Tell OpenGL how to convert from coordinates to pixel values
	glViewport(0, 0, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glClearColor(1.0f, 0.f, 1.0f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);

	//	glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, 0, NULL);

	//	glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_CACHED_APPLE);
	//	glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, w,
			h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);

	glDisable(GL_DEPTH_TEST);
}

void restoreGL(int windowNum,int w, int h) 
{
	//Tell OpenGL how to convert from coordinates to pixel values
	glViewport(0, 0, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glClearColor(1.0f, 0.f, 1.0f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glDisable(GL_DEPTH_TEST);
}

unsigned char keyArray[512*3];

int KeyDown(int key)
{
	return keyArray[key*3+1]==GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key*3+2];
}

void ClearKey(int key)
{
	keyArray[key*3+2]=0;
}

void kbHandler( GLFWwindow window, int key, int action )		/* At present ignores which window, will add per window keys later */
{
	keyArray[key*3 + 0]=keyArray[key*3+1];
	keyArray[key*3 + 1]=action;
	keyArray[key*3 + 2]|=(keyArray[key*3+0]==GLFW_RELEASE)&&(keyArray[key*3+1]==GLFW_PRESS);
}

static int doDebug=0;

int main(int argc,char**argv)
{
	double	atStart,now,remain;
	uint16_t bp;

	/// Initialize GLFW 
	glfwInit(); 

	// Open screen OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"vic",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwSetKeyCallback(kbHandler);

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	
#if 0
	PinSetRESET(1);
	PIN_BUFFER_RESET=1;
	PinSetO1(1);			// Run with reset high for a few cycles to perform a full cpu reset
	PinSetO1(0);
	PinSetO2(1);
	PinSetO2(0);
	PinSetO1(1);
	PinSetO1(0);
	PinSetO2(1);
	PinSetO2(0);
	PinSetO1(1);
	PinSetO1(0);
	PinSetO2(1);
	PinSetO2(0);
	PinSetRESET(0);			// RESET CPU
	PIN_BUFFER_RESET=0;
#endif
	PinSetPIN__IRQ(1);

	//dumpInstruction=100000;

	PC=GetByte(0xFFFC);
	PC|=GetByte(0xFFFD)<<8;

	bp = GetByte(0xc000);
	bp|=GetByte(0xC001)<<8;
	bp = GetByte(0xFFFE);
	bp|=GetByte(0xFFFF)<<8;

	printf("%04X\n",bp);

	printf("%02X != %02X\n",BRom[0xD487-0xC000],GetByte(0xD487));

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
		if (BRom[0xD487-0xC000]!=0xA2)
			exit(-1);

		PinSetPIN_O0(1);
		if (PinGetPIN_SYNC())
		{
//			if (PinGetPIN_AB()==bp)
//				doDebug=1;
		
			if ((PinGetPIN_AB()&0xE000)==0x2000)
			{
//				exit(1);
//				doDebug=1;
			}

			if (doDebug)
			{
				Disassemble(PinGetPIN_AB(),1);
				getch();
			}
		}
		if (PinGetPIN_RW())
		{
			uint16_t addr = PinGetPIN_AB();
			uint8_t  data = GetByte(addr);
			if (doDebug)
				printf("Getting : %02X @ %04X\n", data,PinGetPIN_AB());
			PinSetPIN_DB(data);
		}
		if (!PinGetPIN_RW())
		{
			if (doDebug)
				printf("Storing : %02X @ %04X\n", PinGetPIN_DB(),PinGetPIN_AB());
			SetByte(PinGetPIN_AB(),PinGetPIN_DB());
		}

		PinSetPIN_O0(0);

		Tick6561();
		VIATick(0);
		VIATick(1);

		pixelClock++;

#if 0
		if (!stopTheClock)
		{
			masterClock++;
			if ((masterClock%4)==0)
				pixelClock++;

			if ((masterClock%10)==0)
			{
								// I8080 emulation works off positive edge trigger. So we need to supply the same sort of
								// clock.
				PIN_BUFFER_O2=0;
				PIN_BUFFER_O1=1;
				PinSetO1(1);		// Execute a cpu step
				if (bTimingEnabled)
					RecordPins();
				PIN_BUFFER_O1=0;
				PinSetO1(0);
				if (bTimingEnabled)
					RecordPins();
				PIN_BUFFER_O2=1;
				PinSetO2(1);
				if (bTimingEnabled)
					RecordPins();
				PIN_BUFFER_O2=0;
				PinSetO2(0);

				if (!MEM_Handler())
				{
					stopTheClock=1;
				}
				if (bTimingEnabled)
					RecordPins();

				PinSetINT(0);		// clear interrupt state
				PIN_BUFFER_INT=0;
				cpuClock++;
			}
			if (pixelClock==30432+10161)		// Based on 19968000 Mhz master clock + mame notes
			{
				NEXTINT=0xCF;
				PinSetINT(1);
				PIN_BUFFER_INT=1;
			}
			if (pixelClock==71008+10161)
			{
				NEXTINT=0xD7;
				PinSetINT(1);
				PIN_BUFFER_INT=1;
			}
		}
#endif
		if (pixelClock>=83200/* || stopTheClock*/)
		{
			if (pixelClock>=83200)
				pixelClock=0;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
				
			glfwPollEvents();
			
			g_traceStep=0;
			
			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.02f))
			{
				now=glfwGetTime();

				remain = now-atStart;
			}
			atStart=glfwGetTime();
		}
	}
	
	return 0;

}


///////////////////
//


//	VIA 1	9110-911F			VIA 2	9120-912F
//	NMI					IRQ
//	CA1 - ~RESTORE				CA1	CASSETTE READ
// 	PA0 - SERIAL CLK(IN)			PA0	ROW INPUT
//	PA1 - SERIAL DATA(IN)			PA1	ROW INPUT
//	PA2 - JOY0				PA2	ROW INPUT
//	PA3 - JOY1				PA3	ROW INPUT
//	PA4 - JOY2				PA4	ROW INPUT
//	PA5 - LIGHT PEN				PA5	ROW INPUT
//	PA6 - CASETTE SWITCH			PA6	ROW INPUT
//	PA7 - SERIAL ATN(OUT)			PA7	ROW INPUT
//	CA2 - CASETTE MOTOR			CA2	SERIAL CLK (OUT)
//
//	CB1 - USER PORT				CB1	SERIAL SRQ(IN)
//	PB0 - USER PORT				PB0	COLUMN OUTPUT
//	PB1 - USER PORT				PB1	COLUMN OUTPUT
//	PB2 - USER PORT				PB2	COLUMN OUTPUT
//	PB3 - USER PORT				PB3	COLUMN OUTPUT
//	PB4 - USER PORT				PB4	COLUMN OUTPUT
//	PB5 - USER PORT				PB5	COLUMN OUTPUT
//	PB6 - USER PORT				PB6	COLUMN OUTPUT
//	PB7 - USER PORT				PB7	COLUMN OUTPUT  JOY3
//	CB2 - USER PORT				CB2	SERIAL DATA (OUT)
//
//
//

uint8_t VIAGetByte(int chipNo,int regNo)
{
//	printf("R VIA%d %02X\n",chipNo+1,regNo);
	switch (regNo)
	{
		case 0:			//IRB
			via[chipNo].IFR&=~0x18;
			return (via[chipNo].IRB & (~via[chipNo].DDRB)) | (via[chipNo].ORB & via[chipNo].DDRB);
		case 1:			//IRA
			via[chipNo].IFR&=~0x03;
			//FALL through intended
		case 15:
			return (via[chipNo].IRA & (~via[chipNo].DDRA));
		case 2:
			return via[chipNo].DDRB;
		case 3:
			return via[chipNo].DDRA;
		case 4:
			via[chipNo].IFR&=~0x40;
			return (via[chipNo].T1C&0xFF);
		case 5:
			return (via[chipNo].T1C>>8);
		case 6:
			return via[chipNo].T1LL;
		case 7:
			return via[chipNo].T1LH;
		case 8:
			via[chipNo].IFR&=~0x20;
			return (via[chipNo].T2C&0xFF);
		case 9:
			return (via[chipNo].T2C>>8);
		case 10:
			via[chipNo].IFR&=~0x04;
			return via[chipNo].SR;
		case 11:
			return via[chipNo].ACR;
		case 12:
			return via[chipNo].PCR;
		case 13:
			return via[chipNo].IFR;
		case 14:
			return via[chipNo].IER & 0x7F;
	}
}

void VIASetByte(int chipNo,int regNo,uint8_t byte)
{
//	printf("W VIA%d %02X,%02X\n",chipNo+1,regNo,byte);
	switch (regNo)
	{
		case 0:
			via[chipNo].IFR&=~0x18;
			via[chipNo].ORB=byte&via[chipNo].DDRB;
			break;
		case 1:
			via[chipNo].IFR&=~0x03;
			//FALL through intended
		case 15:
			via[chipNo].ORA=byte&via[chipNo].DDRA;
			break;
		case 2:
			via[chipNo].DDRB=byte;
			break;
		case 3:
			via[chipNo].DDRA=byte;
			break;
		case 4:
			via[chipNo].T1LL=byte;
			break;
		case 5:
			via[chipNo].T1LH=byte;
			via[chipNo].T1C=byte<<8;
			via[chipNo].T1C|=via[chipNo].T1LL;
			via[chipNo].IFR&=~0x40;
			break;
		case 6:
			via[chipNo].T1LL=byte;
			break;
		case 7:
			via[chipNo].T1LH=byte;
			via[chipNo].IFR&=~0x40;
			break;
		case 8:
			via[chipNo].T2LL=byte;
			break;
		case 9:
			via[chipNo].T2TimerOff=0;
			via[chipNo].T2C=byte<<8;
			via[chipNo].T2C|=via[chipNo].T2LL;
			via[chipNo].IFR&=~0x20;
			break;
		case 10:
			via[chipNo].IFR&=~0x04;
			via[chipNo].SR=byte;
			break;
		case 11:
			via[chipNo].ACR=byte;
			break;
		case 12:
			via[chipNo].PCR=byte;
			break;
		case 13:
			if (byte&0x80)
			{
				via[chipNo].IFR|=byte&0x7F;
			}
			else
			{
				via[chipNo].IFR&=~(byte&0x7F);
			}
			break;
		case 14:
			if (byte&0x80)
			{
				via[chipNo].IER|=byte&0x7F;
			}
			else
			{
				via[chipNo].IER&=~(byte&0x7F);
			}
			break;
	}
}

uint8_t CheckKeys(uint8_t scan,int key0,int key1,int key2,int key3,int key4,int key5,int key6,int key7)
{
	uint8_t keyVal=0xFF;

	if ((via[1].ORB&scan)==0)
	{
		if (KeyDown(key0))
		{
			keyVal&=~0x01;
		}
		if (KeyDown(key1))
		{
			keyVal&=~0x02;
		}
		if (KeyDown(key2))
		{
			keyVal&=~0x04;
		}
		if (KeyDown(key3))
		{
			keyVal&=~0x08;
		}
		if (KeyDown(key4))
		{
			keyVal&=~0x10;
		}
		if (KeyDown(key5))
		{
			keyVal&=~0x20;
		}
		if (KeyDown(key6))
		{
			keyVal&=~0x40;
		}
		if (KeyDown(key7))
		{
			keyVal&=~0x80;
		}
	}

	return keyVal;
}

void VIATick(int chipNo)
{
	via[chipNo].IRA=0x00;
	via[chipNo].IRB=0x00;

	if (via[chipNo].T1C)
	{
		via[chipNo].T1C--;
		if (via[chipNo].T1C==0)
		{
			via[chipNo].IFR|=0x40;
			if (via[chipNo].ACR&0x40)
			{
				via[chipNo].T1C=via[chipNo].T1LH<<8;
				via[chipNo].T1C|=via[chipNo].T1LL;
			}
//			printf("T1 Counter Underflow On VIA %d\n",chipNo);
		}
	}
	via[chipNo].T2C--;
	if ((via[chipNo].T2C==0) && (via[chipNo].T2TimerOff==0))
	{
		via[chipNo].IFR|=0x20;
		via[chipNo].T2TimerOff=1;
//		printf("T2 Counter Underflow On VIA %d\n",chipNo);
	}

	if (chipNo==1)
	{
		// TODO : some keys require that shift is held down - F2,F4,CURSOR LEFT etc
		uint8_t keyVal=0xFF;
		keyVal&=CheckKeys(0x01,'1','3','5','7','9','-','=',GLFW_KEY_BACKSPACE);
		keyVal&=CheckKeys(0x02,'`','W','R','Y','I','P',']',GLFW_KEY_ENTER);
		keyVal&=CheckKeys(0x04,GLFW_KEY_LCTRL,'A','D','G','J','L','\'',GLFW_KEY_RIGHT);
		keyVal&=CheckKeys(0x08,GLFW_KEY_TAB,GLFW_KEY_LSHIFT,'X','V','N',',','/',GLFW_KEY_DOWN);
		keyVal&=CheckKeys(0x10,GLFW_KEY_SPACE,'Z','C','B','M','.',GLFW_KEY_RSHIFT,GLFW_KEY_F1);
		keyVal&=CheckKeys(0x20,GLFW_KEY_RCTRL,'S','F','H','K',';','#',GLFW_KEY_F3);
		keyVal&=CheckKeys(0x40,'Q','E','T','U','O','[','/',GLFW_KEY_F5);
		keyVal&=CheckKeys(0x80,'2','4','6','8','0','\\',GLFW_KEY_HOME,GLFW_KEY_F7);
		via[chipNo].IRA=keyVal;

	}

	via[chipNo].IFR&=0x7F;
	if ((via[chipNo].IFR&0x7F)&(via[chipNo].IER&0x7F))
	{
		via[chipNo].IFR|=0x80;
		if (chipNo==1)
		{
			PinSetPIN__IRQ(0);
		}
	}
	else
	{
		via[chipNo].IFR|=0x00;
		if (chipNo==1)
		{
			PinSetPIN__IRQ(1);
		}
	}
}


////////////6561////////////////////

uint16_t RASTER_CNT;

uint8_t GetByte6561(int regNo)
{
	switch(regNo)
	{
		case 0:
			return CTRL_1;
		case 1:
			return CTRL_2;
		case 2:
			return CTRL_3;
		case 3:
			return CTRL_4;
		case 4:
			return CTRL_5;
		case 5:
			return CTRL_6;
		case 6:
			return CTRL_7;
		case 7:
			return CTRL_8;
		case 8:
			return CTRL_9;
		case 9:
			return CTRL_10;
		case 10:
			return CTRL_11;
		case 11:
			return CTRL_12;
		case 12:
			return CTRL_13;
		case 13:
			return CTRL_14;
		case 14:
			return CTRL_15;
		case 15:
			return CTRL_16;
	}
}

void SetByte6561(int regNo,uint8_t byte)
{
	printf("W 6561 %02X,%02X\n",regNo,byte);
	switch(regNo)
	{
		case 0:
			CTRL_1=byte;
			break;
		case 1:
			CTRL_2=byte;
			break;
		case 2:
			CTRL_3=byte;
			break;
		case 3:
			CTRL_4|=byte&0x7F;
			break;
		case 4:
			break;
		case 5:
			CTRL_6=byte;
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		case 10:
			CTRL_11=byte;
			break;
		case 11:
			CTRL_12=byte;
			break;
		case 12:
			CTRL_13=byte;
			break;
		case 13:
			CTRL_14=byte;
			break;
		case 14:
			CTRL_15=byte;
			break;
		case 15:
			CTRL_16=byte;
			break;
	}
}

void Tick6561()
{
	RASTER_CNT++;
	CTRL_5=(RASTER_CNT>>1)&0xFF;
	CTRL_4&=0x7F;
	CTRL_4|=(RASTER_CNT&0x01)<<7;
}
