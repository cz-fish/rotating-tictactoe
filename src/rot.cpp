#include <windows.h>
#include <stdio.h>
#include <gl\glaux.h>		// Header File For The Glaux Library

#define TEXRES 512
#define IDI_MAIN 101

HDC			hDC=NULL;		// Private GDI Device Context
HGLRC		hRC=NULL;		// Permanent Rendering Context
HWND		hWnd=NULL;		// Holds Our Window Handle
HINSTANCE	hInstance;		// Holds The Instance Of The Application
GLuint	fontbase;			// Base Display List For The Font Set
bool done = false;
int winner=0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void reshape (int width, int height);

int xsize, ysize;
float aspect;
float fov = 60.0f;
float zNear=0.1f, zFar=50.0f;
int kurzorx = 0;
int kurzory = 0;

GLuint gridtex;
GLuint overtex;
GLuint overmasktex;
GLuint tex1, tex2, tex1mask, tex2mask;
GLuint play;
bool player = false;
char pole[8][8];
GLUquadricObj *quadratic;
char name[2][64];

struct {
	char x;
	char y;
} his[64];
int nexthistry=0;

void MakeTex ();

//----------------------------------------------------------------------------------------------
//texture
AUX_RGBImageRec *LoadBMP(char *Filename)				// Loads A Bitmap Image
{
	FILE *File=NULL;									// File Handle

	if (!Filename)										// Make Sure A Filename Was Given
	{
		return NULL;									// If Not Return NULL
	}

	File=fopen(Filename,"r");							// Check To See If The File Exists

	if (File)											// Does The File Exist?
	{
		fclose(File);									// Close The Handle
		return auxDIBImageLoad(Filename);				// Load The Bitmap And Return A Pointer
	}

	return NULL;										// If Load Failed Return NULL
}

int LoadGLTextures(char* Filename, GLuint* texture)
{
	int Status=FALSE;

	AUX_RGBImageRec *TextureImage=NULL;

	// Load The Bitmap, Check For Errors, If Bitmap's Not Found Quit
	if (TextureImage=LoadBMP(Filename))
	{
		Status=TRUE;									// Set The Status To TRUE

		glGenTextures(1, texture);					// Create Three Textures

		glBindTexture(GL_TEXTURE_2D, *texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, TextureImage->sizeX, TextureImage->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, TextureImage->data);
	}

	if (TextureImage)								// If Texture Exists
	{
		if (TextureImage->data)						// If Texture Image Exists
		{
			free(TextureImage->data);				// Free The Texture Image Memory
		}

		free(TextureImage);							// Free The Image Structure
	}

	return Status;										// Return The Status
}

//-----------------------------------------------------------------------------------------------
//font

GLvoid BuildFont(GLvoid)								// Build Our Bitmap Font
{
	HFONT	font;										// Windows Font ID
	HFONT	oldfont;									// Used For Good House Keeping

	fontbase = glGenLists(96);								// Storage For 96 Characters

	font = CreateFont(	-20,							// Height Of Font
						0,								// Width Of Font
						0,								// Angle Of Escapement
						0,								// Orientation Angle
						FW_BOLD,						// Font Weight
						FALSE,							// Italic
						FALSE,							// Underline
						FALSE,							// Strikeout
						ANSI_CHARSET,					// Character Set Identifier
						OUT_TT_PRECIS,					// Output Precision
						CLIP_DEFAULT_PRECIS,			// Clipping Precision
						ANTIALIASED_QUALITY,			// Output Quality
						FF_DONTCARE|DEFAULT_PITCH,		// Family And Pitch
						"Courier New");					// Font Name

	oldfont = (HFONT)SelectObject(hDC, font);           // Selects The Font We Want
	wglUseFontBitmaps(hDC, 32, 96, fontbase);			// Builds 96 Characters Starting At Character 32
	SelectObject(hDC, oldfont);							// Selects The Font We Want
	DeleteObject(font);									// Delete The Font
}

GLvoid KillFont(GLvoid)									// Delete The Font List
{
	glDeleteLists(fontbase, 96);						// Delete All 96 Characters
}

GLvoid glprintf(const char *fmt, ...)					// Custom GL "Print" Routine
{
	char		text[256];								// Holds Our String
	va_list		ap;										// Pointer To List Of Arguments

	if (fmt == NULL)									// If There's No Text
		return;											// Do Nothing

	va_start(ap, fmt);									// Parses The String For Variables
	    vsprintf(text, fmt, ap);						// And Converts Symbols To Actual Numbers
	va_end(ap);											// Results Are Stored In Text

	glPushAttrib(GL_LIST_BIT);							// Pushes The Display List Bits
	glListBase(fontbase - 32);								// Sets The Base Character to 32
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);	// Draws The Display List Text
	glPopAttrib();										// Pops The Display List Bits
}

//----------------------------------------------------------------------------------------------
//window
GLvoid KillGLWindow(GLvoid)								// Properly Kill The Window
{
	if (hRC)											// Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL,NULL))					// Are We Able To Release The DC And RC Contexts?
		{
			MessageBox(NULL,"Release Of DC And RC Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}

		if (!wglDeleteContext(hRC))						// Are We Able To Delete The RC?
		{
			MessageBox(NULL,"Release Rendering Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}
		hRC=NULL;										// Set RC To NULL
	}

	if (hDC && !ReleaseDC(hWnd,hDC))					// Are We Able To Release The DC
	{
		MessageBox(NULL,"Release Device Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hDC=NULL;										// Set DC To NULL
	}

	if (hWnd && !DestroyWindow(hWnd))					// Are We Able To Destroy The Window?
	{
		MessageBox(NULL,"Could Not Release hWnd.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hWnd=NULL;										// Set hWnd To NULL
	}

	if (!UnregisterClass("OpenGL",hInstance))			// Are We Able To Unregister Class
	{
		MessageBox(NULL,"Could Not Unregister Class.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hInstance=NULL;									// Set hInstance To NULL
	}
}

//------------------------------------------------------------------------------------------------
bool CreateGLWindow()
{
/*	for (int a=0;a < 256; a++)
		keys[a] = false;*/

	GLuint		PixelFormat;			// Holds The Results After Searching For A Match
	WNDCLASS	wc;						// Windows Class Structure
	DWORD		dwExStyle;				// Window Extended Style
	DWORD		dwStyle;				// Window Style
	RECT		WindowRect;				// Grabs Rectangle Upper Left / Lower Right Values
	WindowRect.left=(long)0;			// Set Left Value To 0
	WindowRect.right=(long)800;		// Set Right Value To Requested Width
	WindowRect.top=(long)0;				// Set Top Value To 0
	WindowRect.bottom=(long)600;		// Set Bottom Value To Requested Height

	hInstance			= GetModuleHandle(NULL);				// Grab An Instance For Our Window
	wc.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
	wc.lpfnWndProc		= (WNDPROC) WndProc;					// WndProc Handles Messages
	wc.cbClsExtra		= 0;									// No Extra Window Data
	wc.cbWndExtra		= 0;									// No Extra Window Data
	wc.hInstance		= hInstance;							// Set The Instance
	wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN));			// Load The Default Icon
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);			// Load The Arrow Pointer
	wc.hbrBackground	= NULL;									// No Background Required For GL
	wc.lpszMenuName		= NULL;									// We Don't Want A Menu
	wc.lpszClassName	= "OpenGL";								// Set The Class Name

	if (!RegisterClass(&wc))									// Attempt To Register The Window Class
	{
		MessageBox(NULL,"Failed To Register The Window Class.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;											// Return FALSE
	}
	
	dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;			// Window Extended Style
	dwStyle=WS_OVERLAPPEDWINDOW;							// Windows Style

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);		// Adjust Window To True Requested Size

	// Create The Window
	if (!(hWnd=CreateWindowEx(	dwExStyle,							// Extended Style For The Window
								"OpenGL",							// Class Name
								"Otoèné piškvorky",						// Window Title
								dwStyle |							// Defined Window Style
								WS_CLIPSIBLINGS |					// Required Window Style
								WS_CLIPCHILDREN,					// Required Window Style
								0, 0,								// Window Position
								WindowRect.right-WindowRect.left,	// Calculate Window Width
								WindowRect.bottom-WindowRect.top,	// Calculate Window Height
								NULL,								// No Parent Window
								NULL,								// No Menu
								hInstance,							// Instance
								NULL)))								// Dont Pass Anything To WM_CREATE
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Window Creation Error.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;								// Return FALSE
	}

	static	PIXELFORMATDESCRIPTOR pfd=				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_WINDOW |						// Format Must Support Window
		PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,							// Must Support Double Buffering
		PFD_TYPE_RGBA,								// Request An RGBA Format
		16,							// Select Our Color Depth
		0, 0, 0, 0, 0, 0,							// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,									// Accumulation Bits Ignored
		16,											// 16Bit Z-Buffer (Depth Buffer)  
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,								// Main Drawing Layer
		0,											// Reserved
		0, 0, 0										// Layer Masks Ignored
	};
	
	if (!(hDC=GetDC(hWnd)))							// Did We Get A Device Context?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Create A GL Device Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;								// Return FALSE
	}

	if (!(PixelFormat=ChoosePixelFormat(hDC,&pfd)))	// Did Windows Find A Matching Pixel Format?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Find A Suitable PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;								// Return FALSE
	}

	if(!SetPixelFormat(hDC,PixelFormat,&pfd))		// Are We Able To Set The Pixel Format?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Set The PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;								// Return FALSE
	}

	if (!(hRC=wglCreateContext(hDC)))				// Are We Able To Get A Rendering Context?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Create A GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;								// Return FALSE
	}

	if(!wglMakeCurrent(hDC,hRC))					// Try To Activate The Rendering Context
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Activate The GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;								// Return FALSE
	}

	ShowWindow(hWnd,SW_SHOW);						// Show The Window
	SetForegroundWindow(hWnd);						// Slightly Higher Priority
	SetFocus(hWnd);									// Sets Keyboard Focus To The Window
	reshape(800,600);					// Set Up Our Perspective GL Screen

	return true;									// Success
}

//----------------------------------------------------------------------------------------------
void GLinit ()
{
	glEnable(GL_CULL_FACE);
	glClearDepth(1.0f);
	glClearColor(0,0,0,1);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable (GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);

	quadratic=gluNewQuadric();
	gluQuadricNormals(quadratic, GLU_SMOOTH);
	gluQuadricTexture(quadratic, GL_TRUE);

	LoadGLTextures ("texture/grid.bmp", &gridtex);
	LoadGLTextures ("texture/marker.bmp", &overtex);
	LoadGLTextures ("texture/markermask.bmp", &overmasktex);
	LoadGLTextures ("texture/tex1.bmp", &tex1);
	LoadGLTextures ("texture/tex2.bmp", &tex2);
	LoadGLTextures ("texture/tex1mask.bmp", &tex1mask);
	LoadGLTextures ("texture/tex2mask.bmp", &tex2mask);

	UINT* pTex = new UINT [TEXRES * TEXRES * 3];
	memset ((void*)pTex, 0, TEXRES * TEXRES * 3 * sizeof (UINT));
	glGenTextures (1, &play);
	glBindTexture (GL_TEXTURE_2D, play);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, TEXRES, TEXRES, 0, GL_RGB, GL_UNSIGNED_INT, pTex);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	delete [] pTex;

	MakeTex();
}

void reshape (int width, int height)
{
    xsize = width; 
    ysize = height;
    aspect = (float)xsize/(float)ysize;
    glViewport(0, 0, xsize, ysize);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return;
}

void display ()
{
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov, aspect, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity(); 

	//1.quadratic - hraci pole
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glTranslatef(0,0,-5.0f);
	glRotatef (45.0f*kurzorx+22.5f, 0.0,1.0,0.0);
	glRotatef (90, 1.0 ,0.0,0.0);
	glTranslatef(0,0,-1.5f);
	glColor4f (1.0,1.0,1.0,1.0f);
	glBindTexture (GL_TEXTURE_2D, play);
	gluCylinder(quadratic,1.0f,1.0f,3.0f,32,32);

    glLoadIdentity(); 
	glTranslatef(0,3.0f/8.0f*kurzory,-5.0f);
	glRotatef (22.5f, 0.0 ,1.0,0.0);
	glRotatef (90, 1.0 ,0.0,0.0);
	glTranslatef(0,0,-1.5f);
	glEnable (GL_BLEND);
	//2.quadratic - maska kurzoru
	glBlendFunc(GL_DST_COLOR,GL_ZERO);
	glDisable (GL_DEPTH_TEST);
	glColor4f (1.0,1.0,1.0,0.8f);
	glBindTexture (GL_TEXTURE_2D, overmasktex);
	gluCylinder(quadratic,1.00f,1.00f,3.0f,32,32);
	//3.quadratic - kurzor
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glColor4f (1.0,1.0,1.0,0.8f);
	glBindTexture (GL_TEXTURE_2D, overtex);
	gluCylinder(quadratic,1.00f,1.00f,3.0f,32,32);

	//HUD
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, 0.1, 2.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity(); 

	glDisable (GL_TEXTURE_2D);
	glTranslatef (0,0,-1);
	glEnable (GL_BLEND);
	glColor4f (0.8f,0.7f,0.0f,1.0f);
	glRasterPos2f (-0.025f,0.7f);
	glprintf ("%d", kurzorx+1);
	for (int a=0; a< 8;a++)
	{
		if (7-a == kurzory) glColor4f (0.8f,0.7f,0.0f,1.0f);
		else glColor4f (0.0f,0.0f,0.8f,1.0f);
		glRasterPos2f (-0.35f, 0.48f - 0.06875f*a*2);
		glprintf ("%d", a+1);
	}
	glColor4f (1.0f,1.0f,1.0f,1.0f);
	glRasterPos2f (-0.77f, -0.85f);
	glprintf ("%s", name[0]);
	glRasterPos2f (0.77f-strlen (name[1])*0.03f, -0.85f);
	glprintf ("%s", name[1]);

	if (winner==1) glColor4f (0, 0.8f,0,1);
	else if (winner==2) glColor4f (0.8f, 0,0,1);
	else if (winner==3) glColor4f (0.7f, 0.7f,0.7f,1);
	if (winner==1 || winner==2)
	{
		glRasterPos2f ((strlen(name[winner-1])+8)*(-0.015f), 0.9f);
		glprintf ("Winner: %s", name[winner-1]);
	}
	else if (winner==3)
	{
		glRasterPos2f (3*(-0.015f), 0.9f);
		glprintf ("Tie");
	}

	int strt=0;
	if (nexthistry > 6) strt = nexthistry-6;
	for (a=strt; a< nexthistry; a++)
	{
		if (a % 2) glColor4f (0.8f, 0,0,1);
		else glColor4f (0,0.8f,0,1);
		glRasterPos2f (0.6f,(nexthistry-a)*0.1f);
		glprintf ("%d: [%d:%d]", a, his[a].x+1, 8-his[a].y);
	}

	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, tex1mask);
	if (player) glColor4f (1.0f,1.0f,1.0f,1.0f);
	else glColor4f (0.8f,0.7f,0.0f,1.0f);
	glBegin (GL_QUADS);
		glTexCoord2d (1.0f,1.0f);
		glVertex3f (-0.8f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,1.0f);
		glVertex3f (-0.9f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,0.0f);
		glVertex3f (-0.9f, -0.9f, 0.0f);
		glTexCoord2d (1.0f,0.0f);
		glVertex3f (-0.8f, -0.9f, 0.0f);
	glEnd();
	glBindTexture (GL_TEXTURE_2D, tex1);
	glBegin (GL_QUADS);
		glTexCoord2d (1.0f,1.0f);
		glVertex3f (-0.8f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,1.0f);
		glVertex3f (-0.9f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,0.0f);
		glVertex3f (-0.9f, -0.9f, 0.0f);
		glTexCoord2d (1.0f,0.0f);
		glVertex3f (-0.8f, -0.9f, 0.0f);
	glEnd();
	glBindTexture (GL_TEXTURE_2D, tex2mask);
	if (!player) glColor4f (1.0f,1.0f,1.0f,1.0f);
	else glColor4f (0.8f,0.7f,0.0f,1.0f);
	glBegin (GL_QUADS);
		glTexCoord2d (1.0f,1.0f);
		glVertex3f (0.9f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,1.0f);
		glVertex3f (0.8f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,0.0f);
		glVertex3f (0.8f, -0.9f, 0.0f);
		glTexCoord2d (1.0f,0.0f);
		glVertex3f (0.9f, -0.9f, 0.0f);
	glEnd();
	glBindTexture (GL_TEXTURE_2D, tex2);
	glBegin (GL_QUADS);
		glTexCoord2d (1.0f,1.0f);
		glVertex3f (0.9f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,1.0f);
		glVertex3f (0.8f, -0.8f, 0.0f);
		glTexCoord2d (0.0f,0.0f);
		glVertex3f (0.8f, -0.9f, 0.0f);
		glTexCoord2d (1.0f,0.0f);
		glVertex3f (0.9f, -0.9f, 0.0f);
	glEnd();
	glDisable (GL_BLEND);
	glEnable (GL_DEPTH_TEST);
}

void MakeTex ()
{
	glEnable (GL_TEXTURE_2D);
	glViewport (0,0, TEXRES, TEXRES);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, 0.1, 2.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity(); 
	glTranslatef (0,0,-1);
	//render old play as a base
	glDisable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glColor4f (1.0,1.0,1.0,1.0);
	glBindTexture (GL_TEXTURE_2D, gridtex);
	glBegin (GL_QUADS);
		glTexCoord2d (1.0,1.0);
		glVertex3f (1.0, 1.0, 0.0);
		glTexCoord2d (0.0,1.0);
		glVertex3f (0.0, 1.0, 0.0);
		glTexCoord2d (0.0,0.0);
		glVertex3f (0.0, 0.0, 0.0);
		glTexCoord2d (1.0,0.0);
		glVertex3f (1.0, 0.0, 0.0);
	glEnd();

	//render new symbol
	for (int a=0; a < 8; a++)
		for (int b=0; b<8; b++)
		{
			if (pole[a][b]==1 || pole[a][b]==101) glBindTexture (GL_TEXTURE_2D, tex1mask);
			else if (pole[a][b]==2 || pole[a][b]==102) glBindTexture (GL_TEXTURE_2D, tex2mask);
			else continue;
			if (pole[a][b]> 100) glColor4f (0.8f, 0.7f, 0, 1);
			else glColor4f (1,1,1,1);
			glBlendFunc(GL_DST_COLOR,GL_ZERO);
			glEnable (GL_BLEND);
			glBegin (GL_QUADS);
				glTexCoord2d (1.0,1.0);
				glVertex3f ((a+1)/8.0f+0.005f, 1-(b+1)/8.0f, 0.0);
				glTexCoord2d (1.0,0.0);
				glVertex3f ((a+1)/8.0f+0.005f, 1-(b)/8.0f, 0.0);
				glTexCoord2d (0.0,0.0);
				glVertex3f ((a)/8.0f+0.005f, 1-(b)/8.0f, 0.0);
				glTexCoord2d (0.0,1.0);
				glVertex3f ((a)/8.0f+0.005f, 1-(b+1)/8.0f, 0.0);
			glEnd();

			glBlendFunc(GL_SRC_ALPHA,GL_ONE);
			if (pole[a][b]==1) glBindTexture (GL_TEXTURE_2D, tex1);
			else if (pole[a][b]==2) glBindTexture (GL_TEXTURE_2D, tex2);
			else continue;
			glBegin (GL_QUADS);
				glTexCoord2d (1.0,1.0);
				glVertex3f ((a+1)/8.0f+0.005f, 1-(b+1)/8.0f, 0.0);
				glTexCoord2d (1.0,0.0);
				glVertex3f ((a+1)/8.0f+0.005f, 1-(b)/8.0f, 0.0);
				glTexCoord2d (0.0,0.0);
				glVertex3f ((a)/8.0f+0.005f, 1-(b)/8.0f, 0.0);
				glTexCoord2d (0.0,1.0);
				glVertex3f ((a)/8.0f+0.005f, 1-(b+1)/8.0f, 0.0);
			glEnd();
		}

	//and save to play
	glBindTexture (GL_TEXTURE_2D, play);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, TEXRES, TEXRES, 0);

	//bring everything back to normal
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);			
	glClearColor(0,0,0,1);
	glDisable (GL_BLEND);
	glEnable (GL_DEPTH_TEST);
	glViewport(0, 0, xsize, ysize);
}

void IsOver ()
{
	char sym;
	char cnt[] = {0,0,0,0,0,0,0,0};
	bool cont[] = {true, true, true, true, true, true, true, true};
	int cx, cy;
	bool finished = true;
	bool remis=true;

	if (player) sym=1; else sym=2;
	for (int a=1; a<5; a++)
	{
		cx = kurzorx-a;
		cy = kurzory-a;
		if (cx < 0) cx += 8;
		if (cy < 0) cont[0]=cont[1]=cont[2]=false;
		if (cont[0] && pole[cx][cy] == sym) cnt[0]++; else cont[0]=false;
		if (cont[1] && pole[kurzorx][cy]==sym) cnt[1]++; else cont[1]=false;
		if (cont[7] && pole[cx][kurzory]==sym) cnt[7]++; else cont[7]=false;
		cx = kurzorx+a;
		if (cx > 7) cx -=8;
		if (cont[2] && pole[cx][cy]==sym) cnt[2]++; else cont[2]=false;
		cx = kurzorx+a;
		cy = kurzory+a;
		if (cx > 7) cx -=8;
		if (cy > 7) cont[6]=cont[5]=cont[4]=false;
		if (cont[5] && pole[kurzorx][cy]==sym) cnt[5]++; else cont[5]=false;
		if (cont[3] && pole[cx][kurzory]==sym) cnt[3]++; else cont[3]=false;
		if (cont[4] && pole[cx][cy]==sym) cnt[4]++; else cont[4]=false;
		cx = kurzorx-a;
		if (cx < 0) cx += 8;
		if (cont[6] && pole[cx][cy]==sym) cnt[6]++; else cont[6]=false;
	}

	if (cnt[0]+cnt[4]+1 >= 5)
	{
		pole[kurzorx][kurzory]+=100;
		for (a=1; a< 5; a++)
		{
			cy = kurzory - a;
			if (cy >= 0)
			{
				cx = kurzorx -a;
				if (cx < 0) cx +=8;
				if (pole[cx][cy]==sym) pole[cx][cy]+=100; else break;
			}
		}
		for (a=1; a< 5; a++)
		{
			cy = kurzory + a;
			if (cy < 8)
			{
				cx = kurzorx +a;
				if (cx > 7) cx -=8;
				if (pole[cx][cy]==sym) pole[cx][cy]+=100; else break;
			}
		}
	}
	else if (cnt[1]+cnt[5]+1 >= 5)
	{
		pole[kurzorx][kurzory]+=100;
		for (a=1; a< 5; a++)
		{
			cy = kurzory - a;
			if (cy > 0 && pole[kurzorx][cy]==sym)
				pole[kurzorx][cy]+=100;
			else break;
		}
		for (a=1; a< 5; a++)
		{
			cy = kurzory + a;
			if (cy < 7 && pole[kurzorx][cy]==sym)
				pole[kurzorx][cy]+=100;
			else break;
		}
	}
	else if (cnt[2]+cnt[6]+1 >= 5)
	{
		pole[kurzorx][kurzory]+=100;
		for (a=1; a< 5; a++)
		{
			cy = kurzory - a;
			if (cy >= 0)
			{
				cx = kurzorx +a;
				if (cx > 7) cx -=8;
				if (pole[cx][cy]==sym) pole[cx][cy]+=100; else break;
			}
		}
		for (a=1; a< 5; a++)
		{
			cy = kurzory + a;
			if (cy < 8)
			{
				cx = kurzorx -a;
				if (cx < 0) cx +=8;
				if (pole[cx][cy]==sym) pole[cx][cy]+=100; else break;
			}
		}
	}
	else if (cnt[3]+cnt[7]+1 >= 5)
	{
		pole[kurzorx][kurzory]+=100;
		for (a=1; a< 5; a++)
		{
			cx = kurzorx - a;
			if (cx < 0) cx +=8;
			if (pole[cx][kurzory]==sym)
				pole[cx][kurzory]+=100;
			else break;
		}
		for (a=1; a< 5; a++)
		{
			cx = kurzorx + a;
			if (cx > 7) cx-=8;
			if (pole[cx][kurzory]==sym)
				pole[cx][kurzory]+=100;
			else break;
		}
	}
	else finished = false;
	for (a=0; a< 8;a++)
		for (int b=0; b<8; b++)
			if (pole[a][b] == 0) {remis=false; break; break;}
	if (remis) winner = 3;
	if (finished) winner=sym;
}

void Mark()
{
	if (winner) return;
	if (pole[kurzorx][kurzory]) return;
	if (!player) pole[kurzorx][kurzory]=1;
	else pole[kurzorx][kurzory]=2;
	his[nexthistry].x = kurzorx;
	his[nexthistry++].y = kurzory;
	player = !player;
	IsOver();
	MakeTex();
}

void keydown (int key)
{
	switch (key)
	{
	case VK_ESCAPE:
		done = true;
		break;
	case VK_SPACE:
		Mark();
		break;
	case VK_LEFT:
		if (++kurzorx > 7) kurzorx=0;
		break;
	case VK_RIGHT:
		if (--kurzorx < 0) kurzorx=7;
		break;
	case VK_UP:
		if (++kurzory > 7) kurzory=7;
		break;
	case VK_DOWN:
		if (--kurzory < 0) kurzory=0;
		break;
	}
}

//------------------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(	HWND	hWnd,			// Handle For This Window
							UINT	uMsg,			// Message For This Window
							WPARAM	wParam,			// Additional Message Information
							LPARAM	lParam)			// Additional Message Information
{
	switch (uMsg)									// Check For Windows Messages
	{
		case WM_SYSCOMMAND:							// Intercept System Commands
			switch (wParam)							// Check System Calls
			{
				case SC_SCREENSAVE:					// Screensaver Trying To Start?
				case SC_MONITORPOWER:				// Monitor Trying To Enter Powersave?
				return 0;							// Prevent From Happening
			}
			break;									// Exit
		case WM_CLOSE:								// Did We Receive A Close Message?
			done = true;
			PostQuitMessage(0);						// Send A Quit Message
			return 0;								// Jump Back
		case WM_KEYDOWN:							// Is A Key Being Held Down?
			keydown (wParam);
			return 0;								// Jump Back
		case WM_SIZE:								// Resize The OpenGL Window
			reshape(LOWORD(lParam),HIWORD(lParam));  // LoWord=Width, HiWord=Height
			return 0;								// Jump Back
	}
	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG		msg;									// Windows Message Structure

	memset (pole, 0, 64);	//clear pole
	memset (his, 0, 128);	//clear his
	strcpy (name[0], "player 1");
	strcpy (name[1], "player 2");

	if (!CreateGLWindow ()) return 1;

	GLinit();
	BuildFont ();

	while (!done)
	{
		if (PeekMessage(&msg,NULL,0,0,PM_REMOVE))	// Is There A Message Waiting?
		{
			if (msg.message == WM_QUIT)
				done=true;							// If So done=TRUE
			else
			{
				TranslateMessage(&msg);				// Translate The Message
				DispatchMessage(&msg);				// Dispatch The Message
			}
		}
		else										// If There Are No Messages
		{
			display();					// Draw The Scene
			SwapBuffers(hDC);				// Swap Buffers (Double Buffering)
		}
	}
	KillFont();
	KillGLWindow ();
	return 0;
}
