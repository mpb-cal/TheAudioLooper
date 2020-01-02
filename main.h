
#include "../MPBLibrary/MPBLibrary.h"
#include "../MPBLibrary/MPBGlobal.h"

// Function prototypes
HRESULT InitPlayerWindow(void);
HRESULT InitVideoWindow(int nMultiplier, int nDivider);
HRESULT HandleGraphEvent(void);
HRESULT StepOneFrame(void);
HRESULT StepFrames(int nFramesToStep);
HRESULT ModifyRate(double dRateAdjust);
HRESULT SetRate(double dRate);

BOOL GetFrameStepInterface(void);
BOOL GetClipFileName(LPTSTR szName);

void PaintAudioWindow(void);
void MoveVideoWindow(void);
void CheckVisibility(void);
void CloseInterfaces(void);

void OpenClip(void);
void PauseClip(void);
void StopClip(void);
void CloseClip(void);

void UpdateMainTitle(void);
void CheckSizeMenu(WPARAM wParam);
void EnablePlaybackMenu(BOOL bEnable);
void GetFilename(TCHAR *pszFull, TCHAR *pszFile);
void Msg(TCHAR *szFormat, ...);

HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister);
void RemoveGraphFromRot(DWORD pdwRegister);

// Constants
#define VOLUME_FULL     0L
#define VOLUME_SILENCE  -10000L

// File filter for OpenFile dialog
#define FILE_FILTER_TEXT \
    TEXT("All Supported Files\0*.avi; *.qt; *.mov; *.mpg; *.mpeg; *.m1v; *.wav; *.mpa; *.mp2; *.mp3; *.au; *.aif; *.aiff; *.snd; *.mid; *.midi; *.rmi; *.jpg; *.bmp; *.gif; *.tga\0") \
    TEXT("Video Files (*.avi; *.qt; *.mov; *.mpg; *.mpeg; *.m1v)\0*.avi; *.qt; *.mov; *.mpg; *.mpeg; *.m1v\0")\
    TEXT("Audio files (*.wav; *.mpa; *.mp2; *.mp3; *.au; *.aif; *.aiff; *.snd)\0*.wav; *.mpa; *.mp2; *.mp3; *.au; *.aif; *.aiff; *.snd\0")\
    TEXT("MIDI Files (*.mid, *.midi, *.rmi)\0*.mid; *.midi; *.rmi\0") \
    TEXT("Image Files (*.jpg, *.bmp, *.gif, *.tga)\0*.jpg; *.bmp; *.gif; *.tga\0") \
    TEXT("All Files (*.*)\0*.*;\0\0")

// Begin default media search at root directory
#define DEFAULT_MEDIA_PATH  TEXT("\\\0")

// Defaults used with audio-only files
#define DEFAULT_AUDIO_WIDTH     240
#define DEFAULT_AUDIO_HEIGHT    120
#define DEFAULT_VIDEO_WIDTH     320
#define DEFAULT_VIDEO_HEIGHT    240
#define MINIMUM_VIDEO_WIDTH     200
#define MINIMUM_VIDEO_HEIGHT    120

#define APPLICATIONNAME TEXT("The Audio Looper")
#define CLASSNAME       TEXT("AudioLooperMPB")
#define CHILDCLASSNAME  TEXT("AudioLooperMPBChild")

#define WM_GRAPHNOTIFY  WM_USER+13

enum PLAYSTATE {Stopped, Paused, Running, Init};

//
// Macros
//
#define SAFE_RELEASE(x) { if (x) x->Release(); x = NULL; }

#define JIF(x) if (FAILED(hr=(x))) \
    {Msg(TEXT("FAILED(hr=0x%x) in ") TEXT(#x) TEXT("\n"), hr); return hr;}

#define LIF(x) if (FAILED(hr=(x))) \
    {Msg(TEXT("FAILED(hr=0x%x) in ") TEXT(#x) TEXT("\n"), hr);}


