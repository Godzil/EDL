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

#include <GLFW/glfw3.h>
#include <GL/glext.h>

#include <al.h>
#include <alc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void AudioKill();
void AudioInitialise();
void UpdateAudio();
void _AudioAddData(int channel,int16_t dacValue);

uint16_t MAIN_PinGetAB();
uint8_t MAIN_PinGetDB();
void MAIN_PinSetDB(uint8_t);
void MAIN_PinSetO0(uint8_t);
uint8_t MAIN_PinGetRW();
uint8_t MAIN_PinGetO2();
void MAIN_PinSet_RES(uint8_t);

void TIA_PinSetOSC(uint8_t);
void TIA_PinSetO2(uint8_t);
uint8_t TIA_PinGetO0();

extern uint8_t TIA_CClock;

void RIOT_PinSetO2(uint8_t);

extern uint8_t RIOT_RAM[128];


uint8_t MAIN_PinGetSYNC();	// just for debugging

// Step 1. Memory

//unsigned char Ram[0x80];		Implemented inside m6532 RIOT
unsigned char Rom[0x1000];

int LoadRom(unsigned char* rom,const char* fname)
{
	unsigned int size;
	unsigned int readFileSize=0;
	FILE* inFile = fopen(fname,"rb");

	if (!inFile)
	{
		printf("Failed to open rom : %s\n",fname);
		return 1;
	}
	fseek(inFile,0,SEEK_END);
	size=ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	if (size>4096)
	{
		printf("Rom requires mapping.. no mapper support\n");
		return 1;
	}
	if (size!=2048 && size!=4096)
	{
		printf("Irregular rom size... \n");
		return 1;
	}

	if (size==2048)	// Load Twice as mirror - first time to upper half
	{
		readFileSize=fread(rom+2048,1,size,inFile);
		fseek(inFile,0,SEEK_SET);
	}

	readFileSize=fread(rom,1,size,inFile);

	fclose(inFile);
	return 0;
}


int InitialiseMemory()
{
#if 0
	if (LoadRom(Rom,2048,"roms/volymN.bin"))
		return 1;
	if (LoadRom(&Rom[2048],2048,"roms/volymN.bin"))		// mirror
		return 1;
#else
//	if (LoadRom(Rom,4096,"roms/inv-plus.bin"))
//		return 1;

#endif

	return 0;
}

int doDebug=0;

// MMAP - apparantly
// ROM - A12 = 1
// TIA - A12 = 0 & A7 = 0
// RAM - A12 = 0 & A7 = 1 & A9 = 0		A9 = RS signal to RIOT
// RIOT - A12 = 0 & A7 = 1 & A9 = 1

//	12 11 10 9 8 7 6 5 4 3 2 1 0
//	1  0  0  1 0 1 0 0 0 0 0 0 0
	
//	0x1280

uint8_t GetByteDebugger(uint16_t addr)
{
	if (addr&0x1000)
	{
		return Rom[addr&0xFFF];
	}
	if ((addr&0x1080)==0x00)
	{
		return 0;
	}
	if ((addr&0x1280)==0x80)
	{
		return RIOT_RAM[addr-0x80];
	}
	if ((addr&0x1280)==0x280)
	{
		return 0;
	}
	return 0;
}

uint8_t GetByte(uint16_t addr)
{
	return Rom[addr&0xFFF];
}

void SetByte(uint16_t addr,uint8_t byte)
{
}


int masterClock=0;
int pixelClock=0;
int cpuClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

extern uint8_t *MAIN_DIS_[256];

extern uint8_t	MAIN_A;
extern uint8_t	MAIN_X;
extern uint8_t	MAIN_Y;
extern uint16_t	MAIN_SP;
extern uint16_t	MAIN_PC;
extern uint8_t	MAIN_P;

void DUMP_REGISTERS()
{
	printf("--------CCLOCK : %d\n",TIA_CClock);
	printf("FLAGS = N  V  -  B  D  I  Z  C\n");
	printf("        %s  %s  %s  %s  %s  %s  %s  %s\n",
			MAIN_P&0x80 ? "1" : "0",
			MAIN_P&0x40 ? "1" : "0",
			MAIN_P&0x20 ? "1" : "0",
			MAIN_P&0x10 ? "1" : "0",
			MAIN_P&0x08 ? "1" : "0",
			MAIN_P&0x04 ? "1" : "0",
			MAIN_P&0x02 ? "1" : "0",
			MAIN_P&0x01 ? "1" : "0");
	printf("A = %02X\n",MAIN_A);
	printf("X = %02X\n",MAIN_X);
	printf("Y = %02X\n",MAIN_Y);
	printf("SP= %04X\n",MAIN_SP);
	printf("--------\n");
}

const char* decodeDisasm(uint8_t *table[256],unsigned int address,int *count,int realLength)
{
	static char temporaryBuffer[2048];
	char sprintBuffer[256];

	uint8_t byte = GetByteDebugger(address);
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
				sprintf(sprintBuffer,"%02X",GetByteDebugger(address+offset));
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
	const char* retVal = decodeDisasm(MAIN_DIS_,address,&numBytes,255);

	if (strcmp(retVal,"UNKNOWN OPCODE")==0)
	{
		printf("UNKNOWN AT : %04X\n",address);
		for (a=0;a<numBytes+1;a++)
		{
			printf("%02X ",GetByteDebugger(address+a));
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
		printf("%02X ",GetByteDebugger(address+a));
	}
	for (a=0;a<8-(numBytes+1);a++)
	{
		printf("   ");
	}
	printf("%s\n",retVal);

	return numBytes+1;
}

int dumpInstruction=0;

int g_instructionStep=0;
int g_traceStep=0;

#define HEIGHT	(262)
#define	WIDTH	(228)

#define MAX_WINDOWS		(1)

#define MAIN_WINDOW		0

GLFWwindow* windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
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

struct KeyArray
{
	unsigned char lastState;
	unsigned char curState;
	unsigned char debounced;
	GLFWwindow*    window;
};

struct KeyArray keyArray[512];

int KeyDown(int key)
{
	return keyArray[key].curState==GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key].debounced;
}

int CheckKeyWindow(int key,GLFWwindow* window)
{
	return keyArray[key].debounced && (keyArray[key].window==window);
}

void ClearKey(int key)
{
	keyArray[key].debounced=0;
}

void kbHandler( GLFWwindow* window, int key, int scan, int action, int mod )
{
	keyArray[key].lastState=keyArray[key].curState;
	if (action==GLFW_RELEASE)
	{
		keyArray[key].curState=GLFW_RELEASE;
	}
	else
	{
		keyArray[key].curState=GLFW_PRESS;
	}
	keyArray[key].debounced|=(keyArray[key].lastState==GLFW_RELEASE)&&(keyArray[key].curState==GLFW_PRESS);
	keyArray[key].window=window;
}

void sizeHandler(GLFWwindow* window,int xs,int ys)
{
	glfwMakeContextCurrent(window);
	glViewport(0, 0, xs, ys);
}

void UpdateHardware()
{
	// For now assume a joystick?
	static uint8_t switches=0x07;
	uint8_t joyMove=0xFF;
	uint8_t joyFire=0x03;

	if (KeyDown(GLFW_KEY_UP))
	{
		joyMove&=0xEF;
	}
	if (KeyDown(GLFW_KEY_DOWN))
	{
		joyMove&=0xDF;
	}
	if (KeyDown(GLFW_KEY_LEFT))
	{
		joyMove&=0xBF;
	}
	if (KeyDown(GLFW_KEY_RIGHT))
	{
		joyMove&=0x7F;
	}
	if (KeyDown(GLFW_KEY_SPACE))
	{
		joyFire&=0x02;
	}
	if (CheckKey(GLFW_KEY_F1))
	{
		ClearKey(GLFW_KEY_F1);
		switches^=0x80;
	}
	if (CheckKey(GLFW_KEY_F2))
	{
		ClearKey(GLFW_KEY_F2);
		switches^=0x40;
	}
	if (CheckKey(GLFW_KEY_F5))
	{
		ClearKey(GLFW_KEY_F5);
		switches^=0x08;
	}
	if (CheckKey(GLFW_KEY_F6))
	{
		ClearKey(GLFW_KEY_F6);
		switches^=0x04;
	}
	if (CheckKey(GLFW_KEY_F7))
	{
		ClearKey(GLFW_KEY_F7);
		switches^=0x02;
	}
	if (CheckKey(GLFW_KEY_F8))
	{
		ClearKey(GLFW_KEY_F8);
		switches^=0x01;
	}


	RIOT_PinSetPB(switches);		// d7 (p0 difficulty 0 easy 1 hard) d6 (p1 difficulty) d3 (b/w 0 colour 1) d2 (game select pressed 0 released 1) d0 (game reset pressed 0 released 1)
	RIOT_PinSetPA(joyMove);			// Joystick (Right, Left, Down, Up | Right, Left, Down, Up) 0 pressed 1 released
	TIA_PinSetDI(0xF);
	TIA_PinSetLI(joyFire);			// Joystick (Fire | Fire) 0 pressed 1 released
}
		
int stopTheClock=1;

void DoCPU()
{
	uint16_t addr;

	static uint16_t lastPC;
	static uint8_t lastO0=0;
	uint8_t curO0=TIA_PinGetO0();

	MAIN_PinSetRDY(TIA_PinGet_RDY());
	MAIN_PinSetO0(curO0);
	addr=MAIN_PinGetAB(); 
	RIOT_PinSet_RS((addr&0x200)>>9);
	RIOT_PinSet_CS2((addr&0x1000)>>12);
	RIOT_PinSetCS1((addr&0x80)>>7);
	TIA_PinSet_CS0((addr&0x1000)>>12);
	TIA_PinSetCS1(1);
	TIA_PinSet_CS2(0);
	TIA_PinSet_CS3((addr&0x80)>>7);
	TIA_PinSetRW(MAIN_PinGetRW());
	TIA_PinSetAB(addr&0x3F);
	RIOT_PinSetRW(MAIN_PinGetRW());
	RIOT_PinSetAD(addr&0x7F);
	TIA_PinSetO2(MAIN_PinGetO2());
	RIOT_PinSetO2(MAIN_PinGetO2());

	if (MAIN_PinGetRW())
	{
		// Chip Select
		switch(addr&0x1280)
		{
			case 0x0000:
			case 0x0200:
				//printf("TIA ACCESS @%04X\n",addr);
				MAIN_PinSetDB((MAIN_PinGetDB()&0x3F)|(TIA_PinGetDB67()<<6));
				break;
			case 0x0080:
			case 0x0280:
				//printf("RIOT ACCESS @%04X,%02X\n",addr,RIOT_PinGetDB());
				MAIN_PinSetDB(RIOT_PinGetDB());
				break;
			case 0x1000:
			case 0x1080:
			case 0x1200:
			case 0x1280:
				//printf("ROM ACCESS @%04X\n",addr);
				MAIN_PinSetDB(GetByte(addr));
				break;
		}
	}
	if (!MAIN_PinGetRW())
	{
		// Chip Select
		switch(addr&0x1280)
		{
			case 0x0000:
			case 0x0200:
				//printf("TIA WRITE @%04X,0\n",addr);
				TIA_PinSetDB67(MAIN_PinGetDB()>>6);
				TIA_PinSetDB05(MAIN_PinGetDB()&0x3F);
				break;
			case 0x0080:
			case 0x0280:
				//printf("RIOT WRITE @%04X,%02X\n",addr,MAIN_PinGetDB());
				RIOT_PinSetDB(MAIN_PinGetDB());
				break;
			case 0x1000:
			case 0x1080:
			case 0x1200:
			case 0x1280:
				printf("ROM WRITE @%04X\n",addr);
				break;
		}
	}

	if (lastO0==0 && curO0==1)
	{
		if ((TIA_PinGet_RDY()&1)&&(MAIN_PinGetSYNC()&1))
		{
			lastPC=addr;

			if (doDebug)
			{
				Disassemble(addr,1);
				//getch();
			}
			g_instructionStep=0;
		}

	}		
	lastO0=curO0;
}

uint32_t quickPalette[8*16]={
			 0x00000000,0x00222222,0x00444444,0x00666666,0x00888888,0x00AAAAAA,0x00CCCCCC,0x00EEEEEE 
			,0x000A1800,0x002C3A00,0x004E5C00,0x00707E00,0x0092A000,0x00B4C213,0x00D6E435,0x00F8FF57 
			,0x00300000,0x00522200,0x00744400,0x00966600,0x00B8880A,0x00DAAA2C,0x00FCCC4E,0x00FFEE70 
			,0x004B0000,0x006D0A00,0x008F2C00,0x00B14E1C,0x00D3703E,0x00F59260,0x00FFB482,0x00FFD6A4 
			,0x00550000,0x0077001D,0x00991A3F,0x00BB3C61,0x00DD5E83,0x00FF80A5,0x00FFA2C7,0x00FFC4E9
			,0x004D0040,0x006F0062,0x00911084,0x00B332A6,0x00D554C8,0x00F776EA,0x00FF98FF,0x00FFBAFF
			,0x00350078,0x0057009A,0x007912BC,0x009B34DE,0x00BD56FF,0x00DF78FF,0x00FF9AFF,0x00FFBCFF
			,0x00100096,0x003200B8,0x00541FDA,0x007641FC,0x009863FF,0x00BA85FF,0x00DCA7FF,0x00FEC9FF
			,0x00000093,0x000A12B5,0x002C34D7,0x004E56F9,0x007078FF,0x00929AFF,0x00B4BCFF,0x00D6DEFF
			,0x0000086F,0x00002A91,0x000A4CB3,0x002C6ED5,0x004E90F7,0x0070B2FF,0x0092D4FF,0x00B4F6FF
			,0x00001F34,0x00004156,0x00006378,0x0016859A,0x0038A7BC,0x005AC9DE,0x007CEBFF,0x009EFFFF
			,0x00002F00,0x0000510F,0x00007331,0x00119553,0x0033B775,0x0055D997,0x0077FBB9,0x0099FFDB
			,0x00003500,0x00005700,0x00007900,0x001F9B11,0x0041BD33,0x0063DF55,0x0085FF77,0x00A7FF99
			,0x00002F00,0x00005100,0x001B7300,0x003D9500,0x005FB703,0x0081D925,0x00A3FB47,0x00C5FF69
			,0x00001F00,0x001F4100,0x00416300,0x00638500,0x0085A700,0x00A7C912,0x00C9EB34,0x00EBFF56
			,0x00240800,0x00462A00,0x00684C00,0x008A6E00,0x00AC9000,0x00CEB220,0x00F0D442,0x00FFF664
			};



// If sync lasts at least 300 clocks, wait for sync end, and thats the start of first line. If sync<300 assume HSYNC -- crude but will work for now
void DummyNTSCTV()
{
	static int curScan=0;		// (we have 228 pixels of screen space for a line, 262 lines for a display
	static int curPos=0;
	static int syncCounter=0;

	if (TIA_PinGetSYNC()&1)
	{
		++syncCounter;
	}
	else
	{
		if (syncCounter)
		{
			if (syncCounter<300)
			{
				curScan++;
				curPos=0;
			}
			if (syncCounter>=300)
			{
				curScan=0;
				curPos=0;
            			glfwMakeContextCurrent(windows[MAIN_WINDOW]);
				ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
				glfwSwapBuffers(windows[MAIN_WINDOW]);
				memset(videoMemory,WIDTH*HEIGHT*4,0);
			}
		}
		syncCounter=0;

//#enforced blank
		if ((curScan>37) && (curScan<262) && (curPos<228))
		{
			uint32_t* outputTexture = (uint32_t*)(videoMemory[MAIN_WINDOW]);
	
			uint32_t col = quickPalette[(((TIA_PinGetLUM2()&1)<<2)|((TIA_PinGetLUM1()&1)<<1)|((TIA_PinGetLUM0()&1)<<0))|((TIA_PinGetCOL()&0xF)<<3)];

			//printf("%02X / %02X / %02X / %08X\n",TIA_PinGetLUM2()&1,TIA_PinGetLUM1()&1,TIA_PinGetLUM0()&1,col);

			outputTexture[curPos+curScan*228]=col;

		}
		curPos++;
	}

}


// NTSC
// Main Clock = 3.58Mhz
// CPU Clock = 1.19Mhz		(Main/3)

// TIA has 228 clocks per line	* 262 * 60 = ~3.58
//
// 
int main(int argc,char**argv)
{
	int w,h;
	double	atStart,now,remain;
	uint16_t bp;

	if (LoadRom(Rom,argv[1]))
	{
		return 1;
	}

	/// Initialize GLFW 
	glfwInit(); 

	// Open screen OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwCreateWindow( WIDTH, HEIGHT, "2600",NULL,NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwGetWindowSize(windows[MAIN_WINDOW],&w,&h);

	printf("width : %d (%d) , height : %d (%d)\n", w,WIDTH,h,HEIGHT);
	glfwSetKeyCallback(windows[MAIN_WINDOW],kbHandler);
	glfwSetWindowSizeCallback(windows[MAIN_WINDOW],sizeHandler);

	atStart=glfwGetTime();
	//////////////////
	AudioInitialise();

	if (InitialiseMemory())
		return -1;

	//dumpInstruction=100000;

	MAIN_PinSet_RES(0);		// Trip reset
	MAIN_PinSet_RES(1);
	
	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESCAPE))
	{
		static uint8_t lastPb0=0xff;
		
		if ((!stopTheClock) || g_traceStep || g_instructionStep)
		{
			TIA_PinSetOSC(1);

			DoCPU();

			TIA_PinSetOSC(0);
			
			DoCPU();

			DummyNTSCTV();

			pixelClock++;

			UpdateHardware();


		}


		if (pixelClock>=(228*262) || stopTheClock)
		{
			static int normalSpeed=1;

			if (pixelClock>=(228*262))
			{
				pixelClock-=228*262;
			}
			else
			{
            			glfwMakeContextCurrent(windows[MAIN_WINDOW]);
				ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
				glfwSwapBuffers(windows[MAIN_WINDOW]);
			}

				
			glfwPollEvents();
			
			if (CheckKey(GLFW_KEY_INSERT))
			{
				ClearKey(GLFW_KEY_INSERT);
				normalSpeed^=1;
			}
			if (CheckKey(GLFW_KEY_DELETE))
			{
				ClearKey(GLFW_KEY_DELETE);
				stopTheClock^=1;
			}
			g_traceStep=0;
			if (KeyDown(GLFW_KEY_KP_DIVIDE))
			{
				g_traceStep=1;
			}
			if (CheckKey(GLFW_KEY_KP_MULTIPLY))
			{
				ClearKey(GLFW_KEY_KP_MULTIPLY);
				g_traceStep=1;
			}
			if (CheckKey(GLFW_KEY_KP_SUBTRACT))
			{
				ClearKey(GLFW_KEY_KP_SUBTRACT);
				g_instructionStep=1;
			}
			
			now=glfwGetTime();

			remain = now-atStart;

			while ((remain<0.02f) && normalSpeed && !stopTheClock)
			{
				now=glfwGetTime();

				remain = now-atStart;
			}
			atStart=glfwGetTime();
		}
	}

	AudioKill();

	return 0;

}

//////////////////////// NOISES //////////////////////////

#define NUMBUFFERS            (2)				/* living dangerously*/

ALuint		  uiBuffers[NUMBUFFERS];
ALuint		  uiSource;

ALboolean ALFWInitOpenAL()
{
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	ALboolean bReturn = AL_FALSE;
	
	pDevice = alcOpenDevice(NULL);				/* Request default device*/
	if (pDevice)
	{
		pContext = alcCreateContext(pDevice, NULL);
		if (pContext)
		{
			printf("\nOpened %s Device\n", alcGetString(pDevice, ALC_DEVICE_SPECIFIER));
			alcMakeContextCurrent(pContext);
			bReturn = AL_TRUE;
		}
		else
		{
			alcCloseDevice(pDevice);
		}
	}

	return bReturn;
}

ALboolean ALFWShutdownOpenAL()
{
	ALCcontext *pContext;
	ALCdevice *pDevice;

	pContext = alcGetCurrentContext();
	pDevice = alcGetContextsDevice(pContext);
	
	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);

	return AL_TRUE;
}

#if 0/*USE_8BIT_OUTPUT*/

#define AL_FORMAT						(AL_FORMAT_MONO8)
#define BUFFER_FORMAT				U8
#define BUFFER_FORMAT_SIZE	(1)
#define BUFFER_FORMAT_SHIFT	(8)

#else

#define AL_FORMAT						(AL_FORMAT_MONO16)
#define BUFFER_FORMAT				int16_t
#define BUFFER_FORMAT_SIZE	(2)
#define BUFFER_FORMAT_SHIFT	(0)

#endif

int curPlayBuffer=0;

#define BUFFER_LEN		(44100/50)

BUFFER_FORMAT audioBuffer[BUFFER_LEN];
int amountAdded=0;

void AudioInitialise()
{
	int a=0;
	for (a=0;a<BUFFER_LEN;a++)
	{
		audioBuffer[a]=0;
	}

	ALFWInitOpenAL();

  /* Generate some AL Buffers for streaming */
	alGenBuffers( NUMBUFFERS, uiBuffers );

	/* Generate a Source to playback the Buffers */
  alGenSources( 1, &uiSource );

	for (a=0;a<NUMBUFFERS;a++)
	{
		alBufferData(uiBuffers[a], AL_FORMAT, audioBuffer, BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
		alSourceQueueBuffers(uiSource, 1, &uiBuffers[a]);
	}

	alSourcePlay(uiSource);
}


void AudioKill()
{
	ALFWShutdownOpenAL();
}

int16_t currentDAC[4] = {0,0,0,0};

void _AudioAddData(int channel,int16_t dacValue)
{
	currentDAC[channel]=dacValue;
}

uint32_t tickCnt=0;
uint32_t tickRate=((22152*4096)/(44100/50));

/* audio ticked at same clock as everything else... so need a step down */
void UpdateAudio()
{
	tickCnt+=1*4096;
	
	if (tickCnt>=tickRate*50)
	{
		tickCnt-=tickRate;

		if (amountAdded!=BUFFER_LEN)
		{
			int32_t res=0;

			res+=currentDAC[0];
			res+=currentDAC[1];
			res+=currentDAC[2];
			res+=currentDAC[3];

			audioBuffer[amountAdded]=res>>BUFFER_FORMAT_SHIFT;
			amountAdded++;
		}
	}

	if (amountAdded==BUFFER_LEN)
	{
		/* 1 second has passed by */

		ALint processed;
		ALint state;
		alGetSourcei(uiSource, AL_SOURCE_STATE, &state);
		alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &processed);
		if (processed>0)
		{
			ALuint buffer;

			amountAdded=0;
			alSourceUnqueueBuffers(uiSource,1, &buffer);
			alBufferData(buffer, AL_FORMAT, audioBuffer, BUFFER_LEN*BUFFER_FORMAT_SIZE, 44100);
			alSourceQueueBuffers(uiSource, 1, &buffer);
		}

		if (state!=AL_PLAYING)
		{
			alSourcePlay(uiSource);
		}
	}
}


