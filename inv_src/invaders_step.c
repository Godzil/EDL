/*
 * Space invaders implementation
 *
 *  Uses 8080 step core built in EDL
 *  Rest is C for now
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

void STEP(void);
void RESET(void);
void INTERRUPT(void);

// Step 1. Memory

//0000 - 07FF	- invaders.h
//0800 - 0FFF	- invaders.g
//1000 - 17FF	- invaders.f
//1800 - 1FFF	- invaders.e

unsigned char Rom[0x2000];	// rom
unsigned char Ram[0x2000];

unsigned char RamCmp[0x2000];

int LoadRom(unsigned int address,unsigned int size,const char* fname)
{
	unsigned int readFileSize=0;
	FILE* inFile = fopen(fname,"rb");
	if (!inFile || size != (readFileSize = fread(&Rom[address],1,size,inFile)))
	{
		printf("Failed to open rom : %s - %d/%d",fname,readFileSize,size);
		return 1;
	}
	fclose(inFile);
	return 0;
}


int InitialiseMemory()
{
	if (LoadRom(0x0000,0x0800,"roms/invaders/invaders.h"))
		return 1;
	if (LoadRom(0x0800,0x0800,"roms/invaders/invaders.g"))
		return 1;
	if (LoadRom(0x1000,0x0800,"roms/invaders/invaders.f"))
		return 1;
	if (LoadRom(0x1800,0x0800,"roms/invaders/invaders.e"))
		return 1;

	// Blank video and ram for now - real hardware probably doesn't do this though!
	memset(&Ram[0],0,256*32);
	memset(&RamCmp[0],0,256*32);
	return 0;
}

void SaveScreen()
{
	int x,y,go=0;
	unsigned char zero=0;
	unsigned char all=255;
	FILE* screen=NULL;
	char filename[256];
	static int magic=0;
	
	for (y=32;y<256;y++)
	{
		for (x=0;x<32;x++)
		{
			if (Ram[y*32+x]!=RamCmp[y*32+x])
			{
				go=1;
			}
			RamCmp[y*32+x]=Ram[y*32+x];
		}
	}

	if (!go)
		return;

	sprintf(filename,"screens/out%05d.raw",magic++);
	screen = fopen(filename,"wb");
	for (y=32;y<256;y++)
	{
		for (x=0;x<32;x++)
		{
			unsigned char mask=0x01;
			while (mask)
			{
				if (Ram[y*32 + x]&mask)
				{
					fwrite(&all,1,1,screen);
				}
				else
				{
					fwrite(&zero,1,1,screen);
				}
				mask<<=1;
			}
		}
	}

	fclose(screen);
}
	
int masterClock=0;
int pixelClock=0;
int cpuClock=0;

int KeyDown(int key);
int CheckKey(int key);
void ClearKey(int key);

unsigned char NEXTINT;

unsigned int PORT4_LATCH;
unsigned int PORT2_LATCH;

unsigned char HandleIOPortRead(unsigned char port)
{
	unsigned char res=0;
	switch (port)
	{
	case 1:		// -|Player1Right|Player1Left|Player1Fire|-|1PlayerButton|2PlayerButton|Coin
		if (KeyDown('0'))
		{
			res|=0x01;
		}
		if (KeyDown('1'))
		{
			res|=0x04;
		}
		if (KeyDown(' '))
		{
			res|=0x10;
		}
		if (KeyDown(GLFW_KEY_LEFT))
		{
			res|=0x20;
		}
		if (KeyDown(GLFW_KEY_RIGHT))
		{
			res|=0x40;
		}
		return res;
	case 2:		// ShowCoinInfo|Player2Right|Player2Left|Player2Fire|-|Easy|NumShips(last 2 bits)
		return 0;
	case 3:
		return (PORT4_LATCH << PORT2_LATCH)>>8;
	default:
		printf("Reading from unhandled IO Port %02X\n",port);
		break;
	}
		
	return 0x00;
}

void HandleIOPortWrite(unsigned char port,unsigned char data)
{
	switch (port)
	{
	case 2:
		PORT2_LATCH=data;
		break;
	case 3:				// Sound effects
		break;
	case 4:
		PORT4_LATCH>>=8;
		PORT4_LATCH&=0x000000FF;
		PORT4_LATCH|=data<<8;
		break;
	case 5:				// Sound effects
		break;
	case 6:				// Watchdog
		break;
	default:
		printf("Writing to unhandled IO Port %02X <- %02X\n",port,data);
		break;
	}

}

#define REGISTER_WIDTH	256
#define	REGISTER_HEIGHT	256

#define TIMING_WIDTH	680
#define TIMING_HEIGHT	400

#define HEIGHT	256
#define	WIDTH	256

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW		1
#define REGISTER_WINDOW		2

GLFWwindow windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
	if (windowNum==MAIN_WINDOW)
	{
		int x,y,b;
		unsigned char* outputTexture = videoMemory[windowNum];
		for (y=32;y<256;y++)
		{
			for (x=0;x<32;x++)
			{
				unsigned char mask=0x01;
				for (b=0;b<8;b++)
				{
					if (Ram[y*32 + x]&mask)
					{
						outputTexture[(x*8+b+y*256)*4+0]=0xFF;
						outputTexture[(x*8+b+y*256)*4+1]=0xFF;
						outputTexture[(x*8+b+y*256)*4+2]=0xFF;
					}
					else
					{
						outputTexture[(x*8+b+y*256)*4+0]=0x0;
						outputTexture[(x*8+b+y*256)*4+1]=0x0;
						outputTexture[(x*8+b+y*256)*4+2]=0x0;
					}
					mask<<=1;
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	if (windowNum==MAIN_WINDOW)		// Invaders needs rotating
	{
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f((-1.0f * 0) - (1.0f*1.f), (1.0f*0) + (-1.f*1.0f));

		glTexCoord2f(0.0f, h);
		glVertex2f((-1.0f * 0) - (-1.0f*1.f), (-1.0f*0) + (-1.f*1.0f));

		glTexCoord2f(w, h);
		glVertex2f((1.0f * 0) - (-1.0f*1.f), (-1.0f*0) + (1.f*1.0f));

		glTexCoord2f(w, 0.0f);
		glVertex2f((1.0f * 0) - (1.0f*1.f), (1.0f*0) + (1.f*1.0f));
	}
	else
	{
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(-1.0f,1.0f);

		glTexCoord2f(0.0f, h);
		glVertex2f(-1.0f, -1.0f);

		glTexCoord2f(w, h);
		glVertex2f(1.0f, -1.0f);

		glTexCoord2f(w, 0.0f);
		glVertex2f(1.0f, 1.0f);
	}
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

int mouseWheelDelta=0;

void mwHandler( GLFWwindow window,int posx,int posy)
{
	mouseWheelDelta=posy;
}

int wcHandler( GLFWwindow window )
{
	return 1;
}

void Disassemble(unsigned int address);

extern unsigned short PC;
extern unsigned char CYCLES;

int main(int argc,char**argv)
{
	double	atStart,now,remain;

	/// Initialize GLFW 
	glfwInit(); 

	// Open invaders OpenGL window 
	if( !(windows[MAIN_WINDOW]=glfwOpenWindow( WIDTH, HEIGHT, GLFW_WINDOWED,"invaders",NULL)) ) 
	{ 
		glfwTerminate(); 
		return 1; 
	} 

	glfwSetWindowPos(windows[MAIN_WINDOW],300,300);
	
	glfwMakeContextCurrent(windows[MAIN_WINDOW]);
	setupGL(MAIN_WINDOW,WIDTH,HEIGHT);

	glfwSwapInterval(0);			// Disable VSYNC

	glfwSetKeyCallback(kbHandler);
	glfwSetScrollCallback(mwHandler);
	glfwSetWindowCloseCallback(wcHandler);

	atStart=glfwGetTime();
	//////////////////

	if (InitialiseMemory())
		return -1;
	
	RESET();

	while (!glfwGetKey(windows[MAIN_WINDOW],GLFW_KEY_ESC))
	{
//		Disassemble(PC);

		STEP();

		masterClock+=CYCLES;
		while (masterClock>=4)
		{	
			pixelClock+=1;

			if (pixelClock==30432+10161)		// Based on 19968000 Mhz master clock + mame notes
			{
				NEXTINT=0xCF;
				INTERRUPT();
			}
			if (pixelClock==71008+10161)
			{
				NEXTINT=0xD7;
				INTERRUPT();
			}
			masterClock-=4;
		}
		if (pixelClock>=83200)
		{
			pixelClock=0;

            		glfwMakeContextCurrent(windows[MAIN_WINDOW]);
			ShowScreen(MAIN_WINDOW,WIDTH,HEIGHT);
			glfwSwapBuffers();
				
			glfwPollEvents();
			
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

unsigned char GetByte(unsigned short addr)
{
	if (addr&0x2000)
		return Ram[addr&0x1FFF];
	
	return Rom[addr&0x1FFF];
}

unsigned char GetStackByte(unsigned short addr)
{
	return GetByte(addr);
}

void SetByte(unsigned short addr,unsigned char byte)
{
	if (addr&0x2000)
	{
		Ram[addr&0x1FFF]=byte;
	}
}

void SetStackByte(unsigned short addr,unsigned char byte)
{
	SetByte(addr,byte);
}

unsigned char GetPort(unsigned short addr)
{
	return HandleIOPortRead(addr&0xFF);
}

void SetPort(unsigned short addr,unsigned char byte)
{
	HandleIOPortWrite(addr&0xFF,byte);
}

unsigned char GetInterruptVector()
{
	return NEXTINT;
}

