
#include "main.h"
#include "WaveWindow.h"

//#include <commctrl.h>
//#include <commdlg.h>
//#include <stdio.h>
#include <tchar.h>
#include <atlbase.h>
//#include <queue>

#include "resource.h"
#include "Section.h"

using namespace std;

HRESULT PlayMovieInWindow( LPTSTR szFile );


#define WM_OPENCLIP	(WM_USER + 1)

const int MAX_FILES = 1000;

HWND			m_hwndInOut = 0;
HANDLE		m_mutex;
char			MUTEX_NAME[] = "tal_mutex_mpb";
//char			INSTALL_DIR[] = "C:\\Program Files\\TheAudioLooper";
char			INSTALL_DIR[] = ".";

HMENU			ghMenu=0;
TCHAR			g_szFileName[MAX_PATH]= "";
BOOL			g_bAudioOnly=FALSE, g_bFullscreen=FALSE;
LONG			g_lVolume=VOLUME_FULL;
DWORD			g_dwGraphRegister=0;
PLAYSTATE	g_psCurrent=Stopped;
double		g_PlaybackRate=1.0;

queue<char*>	m_playList;
Section			m_section;
bool				m_thumbtrack = false;

int			g_tap = 0;
const int	MAX_TAPS = 10000;
DWORD			g_taps[MAX_TAPS];

IGraphBuilder *pGB = NULL;
IMediaControl *pMC = NULL;
IMediaEventEx *pME = NULL;
IVideoWindow  *pVW = NULL;
IBasicAudio   *pBA = NULL;
IBasicVideo   *pBV = NULL;
IMediaSeeking *pMS = NULL;
IMediaPosition *pMP = NULL;
IVideoFrameStep *pFS = NULL;

const LONGLONG MEDIA_TIME = 100000;
const LONGLONG SCROLL_TIME = 10000000;
LONGLONG m_currentPos = 0;
LONGLONG m_duration;
GUID m_timeFormat;
UINT IDT_TIMER1 = 1;
UINT IDT_TIMER2 = 2;
bool	m_loop = true;
bool	m_gap = false;


void writePrefsFile()
{
	char path[MAX_PATH];
	strcpy( path, INSTALL_DIR );
	strcat( path, "\\TAL.prefs" );

	FILE* file = fopen( path, "wb" );
	if (!file) return;
	
	fwrite( &m_loop, sizeof m_loop, 1, file );

	fclose( file );
}

void readPrefsFile()
{
	char path[MAX_PATH];
	strcpy( path, INSTALL_DIR );
	strcat( path, "\\TAL.prefs" );

	FILE* file = fopen( path, "rb" );
	if (!file) 
	{
		return;
	}

	fread( &m_loop, sizeof m_loop, 1, file );

	fclose( file );
}

int initPrefs()
{
	WIN32_FIND_DATA fd;

	HANDLE h = FindFirstFile( INSTALL_DIR, &fd );
	if (h == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	readPrefsFile();

	return 0;
}

void OpenClip( char* file )
{
	HRESULT hr;

	if (file[0] == L'\0') return;

	lstrcpy( g_szFileName, file );

	// Reset status variables
	g_psCurrent = Stopped;
	g_lVolume = VOLUME_FULL;

	// Start playing the media file
	hr = PlayMovieInWindow( file );

	// If we couldn't play the clip, clean up
	if (FAILED(hr)) CloseClip();
}

void CloseClip()
{
	DestroyWindow( m_hwndInOut );
	KillTimer( m_hwndInOut, IDT_TIMER1 );

	HRESULT hr;

	// Stop media playback
	if(pMC) hr = pMC->Stop();

	// Clear global flags
	g_psCurrent = Stopped;
	g_bAudioOnly = TRUE;
	g_bFullscreen = FALSE;

	// Free DirectShow interfaces
	CloseInterfaces();

	// Clear file name to allow selection of new file with open dialog
	g_szFileName[0] = L'\0';

	// No current media state
	g_psCurrent = Init;

	// Reset the player window
	RECT rect;
	GetClientRect(g_hwnd, &rect);
	InvalidateRect(g_hwnd, &rect, TRUE);

	UpdateMainTitle();
	InitPlayerWindow();
}

void addToPlayList( char* file )
{
	m_playList.push( file );

	if (m_hwndInOut)
	{
//		HWND hwndList = GetDlgItem( m_hwndInOut, IDC_LIST_PLAYLIST ); 

//		SendMessage( hwndList, LB_ADDSTRING, 0, (LPARAM)cmdLine );
//			SendMessage(hwndList, LB_SETITEMDATA, i, (LPARAM) i); 
	}
}

void playListNext()
{
	CloseClip();

	if (!m_playList.empty())
	{
		char* pc = m_playList.front();
		m_playList.pop();
		g_psCurrent = Init;
		OpenClip( pc );
		delete[] pc;
	}
}

/*
void processCmdLine( char* cmdLine )
{
	if (cmdLine[0] != '\0')
	{
		CloseClip();

		if (*cmdLine == '"')  
		{
			cmdLine++;
			char *q = cmdLine;
			while (*q != '"') q++;
			*q = 0;
		}

		g_psCurrent = Init;
		OpenClip( cmdLine );
	}
}
*/

HRESULT setupTimeline()
{
	HRESULT hr;

	JIF( pMS->GetDuration( &m_duration ) );
	m_section.setDuration( m_duration );
	m_section.setLength( m_duration );

	JIF( pMS->GetTimeFormat( &m_timeFormat ) );
	if (m_timeFormat != TIME_FORMAT_MEDIA_TIME) return ERROR_BAD_FORMAT;

	return hr;
}

HRESULT Rewind()
{
	HRESULT hr;

	__int64 start = m_section.getStart();
	__int64 stop = m_section.getStop();

	hr = pMS->SetPositions(	&start, 
									AM_SEEKING_AbsolutePositioning,
									&stop, 
									AM_SEEKING_AbsolutePositioning 	);

	if (!m_loop) PauseClip();

	return hr;
}

HRESULT PlayMovieInWindow(LPTSTR szFile)
{
	writeLog( "PlayMovieInWindow()\n" );

	USES_CONVERSION;
	WCHAR wFile[MAX_PATH];
	HRESULT hr;

	// Clear open dialog remnants before calling RenderFile()
	UpdateWindow( g_hwnd );

	// Convert filename to wide character string
	wcscpy(wFile, T2W(szFile));

	// Get the interface for DirectShow's GraphBuilder
	JIF(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
	IID_IGraphBuilder, (void **)&pGB));

	if(SUCCEEDED(hr))
	{
		// Have the graph builder construct its the appropriate graph automatically
		JIF(pGB->RenderFile(wFile, NULL));

		// QueryInterface for DirectShow interfaces
		JIF(pGB->QueryInterface(IID_IMediaControl, (void **)&pMC));
		JIF(pGB->QueryInterface(IID_IMediaEventEx, (void **)&pME));
		JIF(pGB->QueryInterface(IID_IMediaSeeking, (void **)&pMS));
		JIF(pGB->QueryInterface(IID_IMediaPosition, (void **)&pMP));

		// Query for video interfaces, which may not be relevant for audio files
		JIF(pGB->QueryInterface(IID_IVideoWindow, (void **)&pVW));
		JIF(pGB->QueryInterface(IID_IBasicVideo, (void **)&pBV));

		// Query for audio interfaces, which may not be relevant for video-only files
		JIF(pGB->QueryInterface(IID_IBasicAudio, (void **)&pBA));

		// Is this an audio-only file (no video component)?
		CheckVisibility();

		// Have the graph signal event via window callbacks for performance
		JIF(pME->SetNotifyWindow((OAHWND)g_hwnd, WM_GRAPHNOTIFY, 0));

		if (!g_bAudioOnly)
		{
			JIF(pVW->put_Owner((OAHWND)g_hwnd));
			JIF(pVW->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN));

			JIF(InitVideoWindow(1, 1));
			GetFrameStepInterface();
		}
		else
		{
			JIF(InitPlayerWindow());
		}

		// Let's get ready to rumble!
		CheckSizeMenu(ID_FILE_SIZE_NORMAL);
		ShowWindow(g_hwnd, SW_SHOWNORMAL);
		UpdateWindow(g_hwnd);
		//SetForegroundWindow(g_hwnd);
		//SetFocus(g_hwnd);
		g_bFullscreen = FALSE;
		g_PlaybackRate = 1.0;
		UpdateMainTitle();

		#ifdef REGISTER_FILTERGRAPH
			hr = AddGraphToRot(pGB, &g_dwGraphRegister);
			if (FAILED(hr))
			{
				Msg(TEXT("Failed to register filter graph with ROT!  hr=0x%x"), hr);
				g_dwGraphRegister = 0;
			}
		#endif

		JIF( setupTimeline() );
		Rewind();
		SendMessage( g_hwnd, WM_COMMAND, ID_CONTROL_INOUT, NULL );

		// Run the graph to play the media file
		JIF(pMC->Run());
		g_psCurrent=Running;

		SetFocus(g_hwnd);
	}

	return hr;
}


HRESULT InitVideoWindow(int nMultiplier, int nDivider)
{
    LONG lHeight, lWidth;
    HRESULT hr = S_OK;
    RECT rect;

    if (!pBV)
        return S_OK;

    // Read the default video size
    hr = pBV->GetVideoSize(&lWidth, &lHeight);
    if (hr == E_NOINTERFACE)
        return S_OK;

    EnablePlaybackMenu(TRUE);

    // Account for requests of normal, half, or double size
    lWidth  = lWidth  * nMultiplier / nDivider;
    lHeight = lHeight * nMultiplier / nDivider;

    SetWindowPos(g_hwnd, NULL, 0, 0, lWidth, lHeight,
                 SWP_NOMOVE | SWP_NOOWNERZORDER);

    int nTitleHeight  = GetSystemMetrics(SM_CYCAPTION);
    int nBorderWidth  = GetSystemMetrics(SM_CXBORDER);
    int nBorderHeight = GetSystemMetrics(SM_CYBORDER);

    // Account for size of title bar and borders for exact match
    // of window client area to default video size
    SetWindowPos(g_hwnd, NULL, 0, 0, lWidth + 2*nBorderWidth,
            lHeight + nTitleHeight + 2*nBorderHeight,
            SWP_NOMOVE | SWP_NOOWNERZORDER);

    GetClientRect(g_hwnd, &rect);
    JIF(pVW->SetWindowPosition(rect.left, rect.top, rect.right, rect.bottom));

    return hr;
}


HRESULT InitPlayerWindow(void)
{
    // Reset to a default size for audio and after closing a clip
    SetWindowPos(g_hwnd, NULL, 0, 0,
                 DEFAULT_AUDIO_WIDTH,
                 DEFAULT_AUDIO_HEIGHT,
                 SWP_NOMOVE | SWP_NOOWNERZORDER);

    // Check the 'full size' menu item
    CheckSizeMenu(ID_FILE_SIZE_NORMAL);
    EnablePlaybackMenu(FALSE);

    return S_OK;
}


void MoveVideoWindow(void)
{
    HRESULT hr;

    // Track the movement of the container window and resize as needed
    if(pVW)
    {
        RECT client;

        GetClientRect(g_hwnd, &client);
        hr = pVW->SetWindowPosition(client.left, client.top,
                                    client.right, client.bottom);
    }
}


void CheckVisibility(void)
{
    long lVisible;
    HRESULT hr;

    if ((!pVW) || (!pBV))
    {
        // Audio-only files have no video interfaces.  This might also
        // be a file whose video component uses an unknown video codec.
        g_bAudioOnly = TRUE;
        return;
    }
    else
    {
        // Clear the global flag
        g_bAudioOnly = FALSE;
    }

    hr = pVW->get_Visible(&lVisible);
    if (FAILED(hr))
    {
        // If this is an audio-only clip, get_Visible() won't work.
        //
        // Also, if this video is encoded with an unsupported codec,
        // we won't see any video, although the audio will work if it is
        // of a supported format.
        //
        if (hr == E_NOINTERFACE)
        {
            g_bAudioOnly = TRUE;
        }
        else
        {
            Msg(TEXT("Failed(%08lx) in pVW->get_Visible()!\r\n"), hr);
        }
    }
}

void PauseClip(void)
{
	if (!pMC) return;

	// Toggle play/pause behavior
	if((g_psCurrent == Paused) || (g_psCurrent == Stopped))
	{
		if (SUCCEEDED(pMC->Run()))
		g_psCurrent = Running;
		
		if (m_hwndInOut)
		{
			SetDlgItemText(	m_hwndInOut,
									IDC_BUTTON_PLAY,
									"&Pause"	);
		}
	}
	else
	{
		if (SUCCEEDED(pMC->Pause()))
		g_psCurrent = Paused;
		
		if (m_hwndInOut)
		{
			SetDlgItemText(	m_hwndInOut,
									IDC_BUTTON_PLAY,
									"&Play"	);
		}
	}

	UpdateMainTitle();
}


void StopClip(void)
{
    HRESULT hr;

    if ((!pMC) || (!pMS))
        return;

    // Stop and reset postion to beginning
    if((g_psCurrent == Paused) || (g_psCurrent == Running))
    {
        hr = pMC->Stop();
        g_psCurrent = Stopped;

        // Seek to the beginning
		  hr = Rewind();

        // Display the first frame to indicate the reset condition
        hr = pMC->Pause();
    }

    UpdateMainTitle();
}

void OpenFileDialog()
{
	const int nMaxFile = MAX_PATH * 1000;
	TCHAR szFilename[nMaxFile] = "";

	UpdateMainTitle();

	OPENFILENAME ofn={0};

	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = g_hwnd;
	ofn.lpstrFilter       = NULL;
	ofn.lpstrFilter       = FILE_FILTER_TEXT;
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex      = 1;
	ofn.lpstrFile         = szFilename;
	ofn.nMaxFile          = nMaxFile;
	ofn.lpstrTitle        = TEXT("Open Media File...\0");
	ofn.lpstrFileTitle    = NULL;
	ofn.lpstrDefExt       = TEXT("*\0");
	ofn.Flags             = OFN_FILEMUSTEXIST | OFN_READONLY | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	// Remember the path of the first selected file
/*	static BOOL bSetInitialDir = FALSE;

	if (bSetInitialDir == FALSE)
	{
		ofn.lpstrInitialDir = DEFAULT_MEDIA_PATH;
		bSetInitialDir = TRUE;
	}
	else
	{
		ofn.lpstrInitialDir = NULL;
	}
*/

	ofn.lpstrInitialDir = "";

	if (!GetOpenFileName((LPOPENFILENAME)&ofn))
	{
		DWORD dwDlgErr = CommDlgExtendedError();

		// Don't show output if user cancelled the selection (no dlg error)
		if (dwDlgErr)
		{
			Msg(TEXT("GetOpenFileName Failed! Error=0x%x\r\n"), GetLastError());
		}
		return;
	}

	if (CreateFile( szFilename, 0, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL ) != INVALID_HANDLE_VALUE)
	{
		TCHAR* file = new TCHAR[MAX_PATH];
		lstrcpy( file, szFilename );
		addToPlayList( file );
	}
	else
	{
		int i = ofn.nFileOffset;
		int k = 0;
		TCHAR tmp[MAX_PATH] = "";
		TCHAR* file = new TCHAR[MAX_PATH];
		lstrcpy( file, ofn.lpstrFile );
		lstrcat( file, "\\" );

		while (1)
		{
			tmp[k++] = szFilename[i];

			if (szFilename[i] == 0)
			{
				lstrcat( file, tmp );
				addToPlayList( file );
				k = 0;
				lstrcpy( file, ofn.lpstrFile );
				lstrcat( file, "\\" );
				if (szFilename[i+1] == 0) break;
			}
			else
			{
			}

			i++;
		}
	}

	playListNext();
}


void CloseInterfaces(void)
{
    HRESULT hr;

    // Relinquish ownership (IMPORTANT!) after hiding video window
    if(pVW)
    {
        hr = pVW->put_Visible(OAFALSE);
        hr = pVW->put_Owner(NULL);
    }

    // Disable event callbacks
    if (pME)
        hr = pME->SetNotifyWindow((OAHWND)NULL, 0, 0);

#ifdef REGISTER_FILTERGRAPH
    if (g_dwGraphRegister)
    {
        RemoveGraphFromRot(g_dwGraphRegister);
        g_dwGraphRegister = 0;
    }
#endif

    // Release and zero DirectShow interfaces
    SAFE_RELEASE(pME);
    SAFE_RELEASE(pMS);
    SAFE_RELEASE(pMP);
    SAFE_RELEASE(pMC);
    SAFE_RELEASE(pBA);
    SAFE_RELEASE(pBV);
    SAFE_RELEASE(pVW);
    SAFE_RELEASE(pFS);
    SAFE_RELEASE(pGB);
}


#ifdef REGISTER_FILTERGRAPH

HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
    IMoniker * pMoniker;
    IRunningObjectTable *pROT;
    if (FAILED(GetRunningObjectTable(0, &pROT))) 
    {
        return E_FAIL;
    }

    WCHAR wsz[128];
    wsprintfW(wsz, L"FilterGraph %08x pid %08x", (DWORD_PTR)pUnkGraph, 
              GetCurrentProcessId());

    HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) 
    {
        hr = pROT->Register(0, pUnkGraph, pMoniker, pdwRegister);
        pMoniker->Release();
    }

    pROT->Release();
    return hr;
}

void RemoveGraphFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) 
    {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
}

#endif


void Msg(TCHAR *szFormat, ...)
{
    TCHAR szBuffer[512];  // Large buffer for very long filenames (like HTTP)

    // Format the input string
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vstprintf(szBuffer, szFormat, pArgs);
    va_end(pArgs);

    // Display a message box with the formatted string
    MessageBox(NULL, szBuffer, TEXT("TheAudioLooper"), MB_OK);
}


HRESULT ToggleMute(void)
{
    HRESULT hr=S_OK;

    if ((!pGB) || (!pBA))
        return S_OK;

    // Read current volume
    hr = pBA->get_Volume(&g_lVolume);
    if (hr == E_NOTIMPL)
    {
        // Fail quietly if this is a video-only media file
        return S_OK;
    }
    else if (FAILED(hr))
    {
        Msg(TEXT("Failed to read audio volume!  hr=0x%x\r\n"), hr);
        return hr;
    }

    // Switch volume levels
    if (g_lVolume == VOLUME_FULL)
        g_lVolume = VOLUME_SILENCE;
    else
        g_lVolume = VOLUME_FULL;

    // Set new volume
    JIF(pBA->put_Volume(g_lVolume));

    UpdateMainTitle();
    return hr;
}


void UpdateMainTitle(void)
{
    TCHAR szTitle[MAX_PATH]={0}, szFile[MAX_PATH]={0};

    // If no file is loaded, just show the application title
    if (g_szFileName[0] == L'\0')
    {
        wsprintf(szTitle, TEXT("%s"), APPLICATIONNAME);
    }

    // Otherwise, show useful information
    else
    {
        // Get file name without full path
        GetFilename(g_szFileName, szFile);

        char szPlaybackRate[16];
        if (g_PlaybackRate == 1.0)
            szPlaybackRate[0] = '\0';
        else
            sprintf(szPlaybackRate, "(Rate:%2.2f)", g_PlaybackRate);

        TCHAR szRate[20];

        lstrcpy(szRate, szPlaybackRate);

        // Update the window title to show filename and play state
        wsprintf(szTitle, TEXT("%s [%s] %s%s%s\0\0"),
                szFile,
                g_bAudioOnly ? TEXT("Audio\0") : TEXT("Video\0"),
                (g_lVolume == VOLUME_SILENCE) ? TEXT("(Muted)\0") : TEXT("\0"),
                (g_psCurrent == Paused) ? TEXT("(Paused)\0") : TEXT("\0"),
                szRate);
    }

    SetWindowText(g_hwnd, szTitle);
}


void GetFilename(TCHAR *pszFull, TCHAR *pszFile)
{
    int nLength;
    TCHAR szPath[MAX_PATH]={0};
    BOOL bSetFilename=FALSE;

    // Strip path and return just the file's name
    _tcscpy(szPath, pszFull);
    nLength = (int) _tcslen(szPath);

    for (int i=nLength-1; i>=0; i--)
    {
        if ((szPath[i] == '\\') || (szPath[i] == '/'))
        {
            szPath[i] = '\0';
            lstrcpy(pszFile, &szPath[i+1]);
            bSetFilename = TRUE;
            break;
        }
    }

    // If there was no path given (just a file name), then
    // just copy the full path to the target path.
    if (!bSetFilename)
        _tcscpy(pszFile, pszFull);
}


HRESULT ToggleFullScreen(void)
{
    HRESULT hr=S_OK;
    LONG lMode;
    static HWND hDrain=0;

    // Don't bother with full-screen for audio-only files
    if ((g_bAudioOnly) || (!pVW))
        return S_OK;

    // Read current state
    JIF(pVW->get_FullScreenMode(&lMode));

    if (lMode == OAFALSE)
    {
        // Save current message drain
        LIF(pVW->get_MessageDrain((OAHWND *) &hDrain));

        // Set message drain to application main window
        LIF(pVW->put_MessageDrain((OAHWND) g_hwnd));

        // Switch to full-screen mode
        lMode = OATRUE;
        JIF(pVW->put_FullScreenMode(lMode));
        g_bFullscreen = TRUE;
    }
    else
    {
        // Switch back to windowed mode
        lMode = OAFALSE;
        JIF(pVW->put_FullScreenMode(lMode));

        // Undo change of message drain
        LIF(pVW->put_MessageDrain((OAHWND) hDrain));

        // Reset video window
        LIF(pVW->SetWindowForeground(-1));

        // Reclaim keyboard focus for player application
        UpdateWindow(g_hwnd);
        SetForegroundWindow(g_hwnd);
        SetFocus(g_hwnd);
        g_bFullscreen = FALSE;
    }

    return hr;
}


//
// Some video renderers support stepping media frame by frame with the
// IVideoFrameStep interface.  See the interface documentation for more
// details on frame stepping.
//
BOOL GetFrameStepInterface(void)
{
    HRESULT hr;
    IVideoFrameStep *pFSTest = NULL;

    // Get the frame step interface, if supported
    hr = pGB->QueryInterface(__uuidof(IVideoFrameStep), (PVOID *)&pFSTest);
    if (FAILED(hr))
        return FALSE;

    // Check if this decoder can step
    hr = pFSTest->CanStep(0L, NULL);

    if (hr == S_OK)
    {
        pFS = pFSTest;  // Save interface to global variable for later use
        return TRUE;
    }
    else
    {
        pFSTest->Release();
        return FALSE;
    }
}


HRESULT StepOneFrame(void)
{
    HRESULT hr=S_OK;

    // If the Frame Stepping interface exists, use it to step one frame
    if (pFS)
    {
        // The graph must be paused for frame stepping to work
        if (g_psCurrent != State_Paused)
            PauseClip();

        // Step the requested number of frames, if supported
        hr = pFS->Step(1, NULL);
    }

    return hr;
}

HRESULT StepFrames(int nFramesToStep)
{
    HRESULT hr=S_OK;

    // If the Frame Stepping interface exists, use it to step frames
    if (pFS)
    {
        // The renderer may not support frame stepping for more than one
        // frame at a time, so check for support.  S_OK indicates that the
        // renderer can step nFramesToStep successfully.
        if ((hr = pFS->CanStep(nFramesToStep, NULL)) == S_OK)
        {
            // The graph must be paused for frame stepping to work
            if (g_psCurrent != State_Paused)
                PauseClip();

            // Step the requested number of frames, if supported
            hr = pFS->Step(nFramesToStep, NULL);
        }
    }

    return hr;
}


HRESULT ModifyRate(double dRateAdjust)
{
	HRESULT hr=S_OK;
	double dRate;

	// If the IMediaPosition interface exists, use it to set rate
	if ((pMP) && (dRateAdjust != 0))
	{
		if ((hr = pMP->get_Rate(&dRate)) == S_OK)
		{
			// Add current rate to adjustment value
			double dNewRate = dRate + dRateAdjust;
			hr = pMP->put_Rate(dNewRate);

			// Save global rate
			if (SUCCEEDED(hr))
			{
				g_PlaybackRate = dNewRate;
				UpdateMainTitle();
			}
		}
	}

	return hr;
}


HRESULT SetRate(double dRate)
{
    HRESULT hr=S_OK;

    // If the IMediaPosition interface exists, use it to set rate
    if (pMP)
    {
	    hr = pMP->put_Rate(dRate);

        // Save global rate
        if (SUCCEEDED(hr))
        {
            g_PlaybackRate = dRate;
            UpdateMainTitle();
        }
    }

    return hr;
}


HRESULT HandleGraphEvent(void)
{
	LONG evCode, evParam1, evParam2;
	HRESULT hr=S_OK;

	// Make sure that we don't access the media event interface
	// after it has already been released.
	if (!pME) return S_OK;

	// Process all queued events
	while (SUCCEEDED(pME->GetEvent(&evCode, (LONG_PTR *) &evParam1, (LONG_PTR *) &evParam2, 0)))
	{
		// Free memory associated with callback, since we're not using it
		hr = pME->FreeEventParams(evCode, evParam1, evParam2);

		// If this is the end of the clip, reset to beginning
		if (EC_COMPLETE == evCode)
		{
			if (m_loop)
			{
				LONGLONG pos=0;

				// Reset to first frame of movie
				hr = Rewind();
				if (FAILED(hr))
				{
					if (FAILED(hr = pMC->Stop()))
					{
						Msg(TEXT("Failed(0x%08lx) to stop media clip!\r\n"), hr);
						break;
					}

					if (FAILED(hr = pMC->Run()))
					{
						Msg(TEXT("Failed(0x%08lx) to reset media clip!\r\n"), hr);
						break;
					}
				}
			}
			else
			{
				playListNext();
			}
		}
	}

	return hr;
}


void CheckSizeMenu(WPARAM wParam)
{
    WPARAM nItems[4] = {ID_FILE_SIZE_HALF,    ID_FILE_SIZE_DOUBLE,
                        ID_FILE_SIZE_NORMAL,  ID_FILE_SIZE_THREEQUARTER};

    // Set/clear checkboxes that indicate the size of the video clip
    for (int i=0; i<4; i++)
    {
        // Check the selected item
        CheckMenuItem(ghMenu, (UINT) nItems[i],
                     (UINT) (wParam == nItems[i]) ? MF_CHECKED : MF_UNCHECKED);
    }
}


void EnablePlaybackMenu(BOOL bEnable)
{
    WPARAM nItems[14] = {ID_FILE_PAUSE,       ID_FILE_STOP,
                        ID_FILE_MUTE,         ID_SINGLE_STEP,
                        ID_FILE_SIZE_HALF,    ID_FILE_SIZE_DOUBLE,
                        ID_FILE_SIZE_NORMAL,  ID_FILE_SIZE_THREEQUARTER,
                        ID_FILE_FULLSCREEN,   ID_RATE_INCREASE,
                        ID_RATE_DECREASE,     ID_RATE_NORMAL,
                        ID_RATE_HALF,         ID_RATE_DOUBLE};

    // Set/clear checkboxes that indicate the size of the video clip
    for (int i=0; i<14; i++)
    {
        // Check the selected item
        EnableMenuItem(ghMenu, (UINT) nItems[i],
                     (UINT) (bEnable) ? MF_ENABLED : MF_GRAYED);
    }
}


LRESULT CALLBACK AboutDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
    	    return TRUE;

        case WM_COMMAND:
	        if (wParam == IDOK)
	        {
                EndDialog(hWnd, TRUE);
	            return TRUE;
	        }
	        break;
    }
    return FALSE;
}

void updateInOutCurrent( HWND hdlg )
{
	char text[ 256 ];

	pMS->GetCurrentPosition( &m_currentPos );
	sprintf( text, "%f", m_currentPos / 10000000.0 );
	SetDlgItemText( hdlg, IDC_STATIC_CURRENT, text );
	
	sprintf( text, "%d %%", (int)(g_PlaybackRate * 100.0) );
	SetDlgItemText( hdlg, IDC_STATIC_SPEED, text );
	
	sprintf( text, "%f", m_section.getSecondsPerBeat() );
	SetDlgItemText( hdlg, IDC_STATIC_TEMPO, text );
	
	sprintf( text, "%f", m_section.getBeatsPerMinute() );
	SetDlgItemText( hdlg, IDC_STATIC_BPM, text );

	sprintf( text, "%f", m_section.getLengthInBeats() );
	SetDlgItemText( hdlg, IDC_STATIC_LENGTHINBEATS, text );
}

void updateInOut( HWND hdlg )
{
	char text[ 256 ];

//	sprintf( text, "%f", m_section.getStartInSeconds() );
//	SetDlgItemText( hdlg, IDC_EDIT1, text );

//	sprintf( text, "%f", m_section.getStopInSeconds() );
//	SetDlgItemText( hdlg, IDC_EDIT_STOP, text );

//	sprintf( text, "%f", m_section.getLengthInSeconds() );
//	SetDlgItemText( hdlg, IDC_EDIT2, text );

	sprintf( text, "%f", m_section.getBeatsPerMinute() );
	SetDlgItemText( hdlg, IDC_EDIT_TEMPO, text );

	updateInOutCurrent( hdlg );
}

LRESULT CALLBACK InOutDlgProc( HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	if (message == WM_INITDIALOG)
	{
		updateInOutCurrent( hdlg );

		// slider
		SendDlgItemMessage(	hdlg,
									IDC_SCROLLBAR1,
									SBM_SETRANGE,
									(WPARAM)0,
									(LPARAM)(m_duration / SCROLL_TIME)	);

		if (m_loop) CheckDlgButton( hdlg, IDC_CHECK_LOOP, BST_CHECKED );
		else CheckDlgButton( hdlg, IDC_CHECK_LOOP, BST_UNCHECKED );

		if (m_gap) CheckDlgButton( hdlg, IDC_CHECK_GAP, BST_CHECKED );
		else CheckDlgButton( hdlg, IDC_CHECK_GAP, BST_UNCHECKED );

		updateInOut( hdlg );
//		Rewind();

		SetTimer( hdlg, IDT_TIMER1, 100, (TIMERPROC) NULL );

		// Get the owner window and dialog box rectangles. 

		RECT rc, rcDlg, rcOwner; 

		GetWindowRect( GetParent( hdlg ), &rcOwner ); 
		GetWindowRect( hdlg, &rcDlg ); 
		CopyRect( &rc, &rcOwner ); 

		 // Offset the owner and dialog box rectangles so that 
		 // right and bottom values represent the width and 
		 // height, and then offset the owner again to discard 
		 // space taken up by the dialog box. 

		OffsetRect( &rcDlg, -rcDlg.left, -rcDlg.top ); 
		OffsetRect( &rc, -rc.left, -rc.top ); 
		OffsetRect( &rc, -rcDlg.right, -rcDlg.bottom ); 

		 // The new position is the sum of half the remaining 
		 // space and the owner's original position. 

		SetWindowPos(	hdlg, 
							HWND_TOP, 
//							rcOwner.left + (rc.right / 2), 
//							rcOwner.top + (rc.bottom / 2), 
							rcOwner.right, 
							rcOwner.top, 
							0, 0,          // ignores size arguments 
							SWP_NOSIZE	); 

		return true;
	}
	else if (message == WM_TIMER)
	{
		updateInOutCurrent( hdlg );

		if (!m_thumbtrack)
		{
			SendDlgItemMessage(	hdlg,
										IDC_SCROLLBAR1,
										SBM_SETPOS,
										(WPARAM)(m_currentPos / SCROLL_TIME),
										(LPARAM)TRUE	);
		}
	}
	else if (message == WM_HSCROLL) 
	{
		int nScrollCode = (int) LOWORD(wParam);
		short int pos = (short int) HIWORD(wParam);

		if (nScrollCode == SB_THUMBPOSITION)
		{
			//LONG pos = SendDlgItemMessage( hdlg, IDC_SCROLLBAR1, SBM_GETPOS, 0, 0 );
			m_currentPos = pos * SCROLL_TIME;
			pMS->SetPositions(	&m_currentPos, 
										AM_SEEKING_AbsolutePositioning,
										&m_duration, 
										AM_SEEKING_AbsolutePositioning 	);

			m_thumbtrack = false;
		}
		else if (nScrollCode == SB_THUMBTRACK)
		{
			m_thumbtrack = true;
		}

		return true;
	}
	else if (message == WM_COMMAND)
	{
		if (LOWORD(wParam) == IDOK)
		{
			return true;
		}
		else if (LOWORD(wParam) == IDCANCEL) 
		{
			DestroyWindow( m_hwndInOut );
			KillTimer( hdlg, IDT_TIMER1 );
			m_hwndInOut = 0;
			return true;
		}
		else if (LOWORD(wParam) == IDC_CHECK_LOOP) 
		{
			if (IsDlgButtonChecked( hdlg, IDC_CHECK_LOOP )) m_loop = true;
			else m_loop = false;

			return true;		
		}
		else if (LOWORD(wParam) == IDC_CHECK_GAP) 
		{
			if (IsDlgButtonChecked( hdlg, IDC_CHECK_GAP )) m_gap = true;
			else m_gap = false;

			return true;		
		}
		else if (LOWORD(wParam) == IDC_BUTTON_SETSTART) 
		{
			__int64 current;
			pMS->GetCurrentPosition( &current );
			m_section.setStart( current );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_SETSTOP) 
		{
			__int64 current;
			pMS->GetCurrentPosition( &current );
			m_section.setStop( current );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BEGINNING) 
		{
			m_section.setStart( 0 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_END) 
		{
			m_section.setStop( m_duration );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STARTLEFT) 
		{
			m_section.moveStartInSeconds( MOVE_LEFT, 1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STARTRIGHT) 
		{
			m_section.moveStartInSeconds( MOVE_RIGHT, 1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STOPLEFT) 
		{
			m_section.moveStopInSeconds( MOVE_LEFT, 1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STOPRIGHT) 
		{
			m_section.moveStopInSeconds( MOVE_RIGHT, 1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STARTLEFT2) 
		{
			m_section.moveStartInSeconds( MOVE_LEFT, .01 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STARTRIGHT2) 
		{
			m_section.moveStartInSeconds( MOVE_RIGHT, .01 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STOPLEFT2)
		{
			m_section.moveStopInSeconds( MOVE_LEFT, .01 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STOPRIGHT2) 
		{
			m_section.moveStopInSeconds( MOVE_RIGHT, .01 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STARTLEFT3) 
		{
			m_section.moveStartInSeconds( MOVE_LEFT, .1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STARTRIGHT3) 
		{
			m_section.moveStartInSeconds( MOVE_RIGHT, .1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STOPLEFT3)
		{
			m_section.moveStopInSeconds( MOVE_LEFT, .1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_STOPRIGHT3) 
		{
			m_section.moveStopInSeconds( MOVE_RIGHT, .1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHLEFT) 
		{
			m_section.moveStartInSeconds( MOVE_LEFT, 1 );
			m_section.moveStopInSeconds( MOVE_LEFT, 1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHRIGHT) 
		{
			m_section.moveStartInSeconds( MOVE_RIGHT, 1 );
			m_section.moveStopInSeconds( MOVE_RIGHT, 1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHLEFT2) 
		{
			m_section.moveStartInSeconds( MOVE_LEFT, .01 );
			m_section.moveStopInSeconds( MOVE_LEFT, .01 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHRIGHT2) 
		{
			m_section.moveStartInSeconds( MOVE_RIGHT, .01 );
			m_section.moveStopInSeconds( MOVE_RIGHT, .01 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHLEFT3) 
		{
			m_section.moveStartInSeconds( MOVE_LEFT, .1 );
			m_section.moveStopInSeconds( MOVE_LEFT, .1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHRIGHT3) 
		{
			m_section.moveStartInSeconds( MOVE_RIGHT, .1 );
			m_section.moveStopInSeconds( MOVE_RIGHT, .1 );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHLEFT_LENGTH) 
		{
			double length = m_section.getLengthInSeconds();
			m_section.moveStartInSeconds( MOVE_LEFT, length );
			m_section.moveStopInSeconds( MOVE_LEFT, length );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_BOTHRIGHT_LENGTH) 
		{
			double length = m_section.getLengthInSeconds();
			m_section.moveStopInSeconds( MOVE_RIGHT, length );
			m_section.moveStartInSeconds( MOVE_RIGHT, length );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_HIT) 
		{
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_REWIND_ALL) 
		{
			m_section.setStart( 0 );
			m_section.setLength( m_duration );
			updateInOut( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_REWIND) 
		{
			m_section.setStart( 0 );
			m_section.setLength( m_duration );
			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			m_currentPos -= 100 * MEDIA_TIME;
			pMS->SetPositions(	&m_currentPos, 
										AM_SEEKING_AbsolutePositioning,
										&m_duration, 
										AM_SEEKING_AbsolutePositioning 	);

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_FORWARD) 
		{
			m_section.setStart( 0 );
			m_section.setLength( m_duration );
			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			m_currentPos += 100 * MEDIA_TIME;
			pMS->SetPositions(	&m_currentPos, 
										AM_SEEKING_AbsolutePositioning,
										&m_duration, 
										AM_SEEKING_AbsolutePositioning 	);

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_PLAY) 
		{
			PauseClip();
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_SLOWER) 
		{
			ModifyRate( -0.25 );
			updateInOut( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_FASTER) 
		{
			ModifyRate( 0.25 );
			updateInOut( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_SLOWER2) 
		{
			ModifyRate( -0.01 );
			updateInOut( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_FASTER2) 
		{
			ModifyRate( 0.01 );
			updateInOut( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_FIRSTTAP) 
		{
			EnableWindow( GetDlgItem( m_hwndInOut, IDC_BUTTON_FIRSTTAP ), FALSE );
			EnableWindow( GetDlgItem( m_hwndInOut, IDC_BUTTON_TAP ), TRUE );
			EnableWindow( GetDlgItem( m_hwndInOut, IDC_BUTTON_LASTTAP ), TRUE );
			g_tap = 0;
			g_taps[g_tap++] = GetTickCount();

			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_TAP) 
		{
			if (g_tap < MAX_TAPS - 1)
			{
				g_taps[g_tap++] = GetTickCount();
			}

			m_section.setSecondsPerBeat( 
				(g_taps[g_tap - 1] - g_taps[0]) / (g_tap - 1) / 1000.0f );

			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_LASTTAP) 
		{
			if (g_tap < MAX_TAPS - 1)
			{
				g_taps[g_tap++] = GetTickCount();
			}

			m_section.setSecondsPerBeat( 
				(g_taps[g_tap - 1] - g_taps[0]) / (g_tap - 1) / 1000.0f );

			EnableWindow( GetDlgItem( m_hwndInOut, IDC_BUTTON_FIRSTTAP ), TRUE );
			EnableWindow( GetDlgItem( m_hwndInOut, IDC_BUTTON_TAP ), FALSE );
			EnableWindow( GetDlgItem( m_hwndInOut, IDC_BUTTON_LASTTAP ), FALSE );

			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_TEMPO_SET) 
		{
			char text[ 256 ];
			float bpm = 0;
			GetDlgItemText( hdlg, IDC_EDIT_TEMPO, text, 256 );
			sscanf( text, "%f", &bpm );
			if (bpm > 10 && bpm < 1000)
				m_section.setBeatsPerMinute( bpm );
			updateInOut( hdlg );

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_LENGTHLEFT) 
		{
			double length = m_section.getLengthInBeats();
			m_section.setLengthInBeats( roundToInt( length ) - 1 );
			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			Rewind();

			return true;
		}
		else if (LOWORD(wParam) == IDC_BUTTON_LENGTHRIGHT) 
		{
			double length = m_section.getLengthInBeats();
			m_section.setLengthInBeats( roundToInt( length ) + 1 );
			updateInOut( hdlg );
//			updateInOutCurrent( hdlg );
			Rewind();

			return true;
		}

		return false;
	}

	return false;
}


LRESULT CALLBACK WndMainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CREATE)
	{
//		CreateWindow(	CHILDCLASSNAME,
//							NULL,
//							WS_CHILD | WS_VISIBLE,
//							200, 200, 500, 200,
//							hWnd,
//							0,
//							g_hInstance,
//							NULL	);

		return 0;
	}
	else if (message == WM_DESTROY)
	{
		PostQuitMessage( 0 );
		return 0;
	}
	else if (message == WM_MOVE || message == WM_SIZE)
	{
		if ((hWnd == g_hwnd) && (!g_bAudioOnly))
		MoveVideoWindow();
		return 0;
	}
//	else if (message == WM_PAINT)
//	{
//		return 0;
//	}
	else if (message == WM_GETMINMAXINFO)
	{
		LPMINMAXINFO lpmm = (LPMINMAXINFO) lParam;
		lpmm->ptMinTrackSize.x = MINIMUM_VIDEO_WIDTH;
		lpmm->ptMinTrackSize.y = MINIMUM_VIDEO_HEIGHT;
		return 0;
	}
	else if (message == WM_GRAPHNOTIFY)
	{
		HandleGraphEvent();
		return 0;
	}
	else if (message == WM_CLOSE)
	{
		SendMessage(g_hwnd, WM_COMMAND, ID_FILE_EXIT, 0);
		return 0;
	}
	else if (message == WM_OPENCLIP)
	{
		char* cmdLine = new char[MAX_PATH];
		GlobalGetAtomName( wParam, cmdLine, MAX_PATH );
		sprintf( g_logBuf, "WM_OPENCLIP: %s\n", cmdLine );
		writeLog( g_logBuf );
		addToPlayList( cmdLine );
		GlobalDeleteAtom( wParam );
		return 0;
	}
	else if (message == WM_KEYDOWN)
	{
		switch(toupper((int) wParam))
		{
			// Frame stepping
			case VK_SPACE:
			case '1':
				StepOneFrame();
				return 0;

			// Frame stepping (multiple frames)
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				StepFrames((int) wParam - '0');
				return 0;

			case VK_LEFT:       // Reduce playback speed by 25%
				ModifyRate(-0.25);
				return 0;

			case VK_RIGHT:      // Increase playback speed by 25%
				ModifyRate(0.25);
				return 0;

			case VK_DOWN:       // Set playback speed to normal
				SetRate(1.0);
				return 0;

			case 'P':
				PauseClip();
				return 0;

			case 'S':
				StopClip();
				return 0;

			case 'M':
				ToggleMute();
				return 0;

			case 'F':
			case VK_RETURN:
				ToggleFullScreen();
				return 0;

			case 'H':
				InitVideoWindow(1,2);
				CheckSizeMenu(wParam);
				return 0;

			case 'N':
				InitVideoWindow(1,1);
				CheckSizeMenu(wParam);
				return 0;

			case 'D':
				InitVideoWindow(2,1);
				CheckSizeMenu(wParam);
				return 0;

			case 'T':
				InitVideoWindow(3,4);
				CheckSizeMenu(wParam);
				return 0;

			case VK_ESCAPE:
				if (g_bFullscreen)
					ToggleFullScreen();
				else
					CloseClip();
				return 0;

			case VK_F12:
			case 'Q':
			case 'X':
				CloseClip();
				return 0;
		}
	}
	else if (message == WM_COMMAND)
	{
		switch(wParam)
		{ // Menus
			case ID_FILE_OPENCLIP:
				// If we have ANY file open, close it and shut down DShow
				if (g_psCurrent != Init) CloseClip();

				// Open the new clip
				OpenFileDialog();
				return 0;

			case ID_FILE_EXIT:
				CloseClip();
				PostQuitMessage(0);
				return 0;

			case ID_FILE_PAUSE:
				PauseClip();
				return 0;

			case ID_FILE_STOP:
				StopClip();
				return 0;

			case ID_FILE_CLOSE:
				CloseClip();
				return 0;

			case ID_FILE_MUTE:
				ToggleMute();
				return 0;

			case ID_FILE_FULLSCREEN:
				ToggleFullScreen();
				return 0;

			case ID_HELP_ABOUT:
				DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX),
				g_hwnd,  (DLGPROC) AboutDlgProc);
				return 0;

			case ID_FILE_SIZE_HALF:
				InitVideoWindow(1,2);
				CheckSizeMenu(wParam);
				return 0;

			case ID_FILE_SIZE_NORMAL:
				InitVideoWindow(1,1);
				CheckSizeMenu(wParam);
				return 0;

			case ID_FILE_SIZE_DOUBLE:
				InitVideoWindow(2,1);
				CheckSizeMenu(wParam);
				return 0;

			case ID_FILE_SIZE_THREEQUARTER:
				InitVideoWindow(3,4);
				CheckSizeMenu(wParam);
				return 0;

			case ID_SINGLE_STEP:
				StepOneFrame();
				return 0;

			case ID_RATE_DECREASE:     // Reduce playback speed by 25%
				ModifyRate(-0.25);
				return 0;

			case ID_RATE_INCREASE:     // Increase playback speed by 25%
				ModifyRate(0.25);
				return 0;

			case ID_RATE_NORMAL:       // Set playback speed to normal
				SetRate(1.0);
				return 0;

			case ID_RATE_HALF:         // Set playback speed to 1/2 normal
				SetRate(0.5);
				return 0;

			case ID_RATE_DOUBLE:       // Set playback speed to 2x normal
				SetRate(2.0);
				return 0;

			case ID_CONTROL_INOUT:
				if (!IsWindow( m_hwndInOut ))
				{
					m_hwndInOut = CreateDialog(	g_hInstance, 
															MAKEINTRESOURCE(IDD_INOUT),
															g_hwnd,  
															(DLGPROC) InOutDlgProc	);
															ShowWindow( m_hwndInOut, SW_SHOW );
				}
				return 0;
			}
	} // Menus


	// Pass this message to the video window for notification of system changes
	if (pVW) pVW->NotifyOwnerMessage((LONG_PTR) hWnd, message, wParam, lParam);

	return DefWindowProc( hWnd, message, wParam, lParam );
}

int PASCAL WinMain(HINSTANCE hInstC, HINSTANCE hInstP, LPSTR lpCmdLine, int nCmdShow)
{
	m_mutex = CreateMutex( NULL, TRUE, MUTEX_NAME );
	if (GetLastError() == ERROR_ALREADY_EXISTS) 
	{
		HWND prevWnd = FindWindowEx(	NULL, NULL, CLASSNAME, NULL );
		SetForegroundWindow( prevWnd );
		ShowWindow( prevWnd, SW_SHOW );
		ATOM atom = GlobalAddAtom( lpCmdLine );
		SendMessage( prevWnd, WM_OPENCLIP, atom, 0 );
		exit( 1 );
	}

	MSG msg={0};
	WNDCLASS wc;
	USES_CONVERSION;

	initPrefs();

	mpb_init( hInstC );
	sprintf( g_logBuf, "cmd line: %s\n", lpCmdLine );
	writeLog( g_logBuf );

	// Initialize COM
	if(FAILED(CoInitialize(NULL)))
	{
		Msg( TEXT( "CoInitialize Failed!\r\n" ) );
		mpb_cleanUp();
		exit( 1 );
	}
	writeLog( "Initialized COM\n" );

	// Register the window class
	writeLog( "registering main window class\n" );
	ZeroMemory(&wc, sizeof wc);
	wc.lpfnWndProc = WndMainProc;
	wc.hInstance = hInstC;
	wc.lpszClassName = CLASSNAME;
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU);
	wc.hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = LoadIcon(hInstC, MAKEINTRESOURCE(IDI_MAINICON));
	if(!RegisterClass(&wc))
	{
		Msg(TEXT("RegisterClass Failed! Error=0x%x\r\n"), GetLastError());
		CoUninitialize();
		mpb_cleanUp();
		exit(1);
	}

//	initWaveWindow();

	// Create the main window.  The WS_CLIPCHILDREN style is required.
	writeLog( "Creating main window\n" );
	g_hwnd = CreateWindow(CLASSNAME, APPLICATIONNAME,
		WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, g_hInstance, 0);

	if (g_hwnd)
	{
		ShowWindow( g_hwnd, SW_SHOWNORMAL );
		UpdateWindow( g_hwnd );

		// Save menu handle for later use
		ghMenu = GetMenu(g_hwnd);

		char* cmdLine = new char[MAX_PATH];
		strcpy( cmdLine, lpCmdLine );
		addToPlayList( cmdLine );
		playListNext();

//		UINT t = SetTimer( g_hwnd, IDT_TIMER2, 100, (TIMERPROC) NULL );

		// Main message loop
		while(GetMessage(&msg,NULL,0,0))
		{
			if (msg.message == WM_TIMER)
			{
				int x = 0;
			}

			if (!IsWindow( m_hwndInOut ) || !IsDialogMessage( m_hwndInOut, &msg )) 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	else
	{
		Msg(TEXT("Failed to create the main window! Error=0x%x\r\n"), GetLastError());
	}

	// Finished with COM
	CoUninitialize();

	writePrefsFile();
	mpb_cleanUp();
	return (int) msg.wParam;
}

