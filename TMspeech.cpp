// Tom's E-Mail Client, started on 9/21/98
// By Tom Grandgent (tgrand@canvaslink.com)

// Speech functions

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define SPEECH

#ifdef SPEECH

#include <initguid.h>
#include <mbctype.h>
#include <spchwrap.h>

PCVoiceText pCVTxt = NULL;
int COMInitialized = 0;
int speechInitialized = 0;

#endif

int init_tts_engine(void)
{
#ifdef SPEECH
	if (speechInitialized)
		return 1;

	if (!COMInitialized)
	{
		CoInitialize(NULL);
		COMInitialized = 1;
	}

    pCVTxt = new CVoiceText;
    if (!pCVTxt) 
        return 1;
        
    HRESULT hRes;
    //hRes = pCVTxt->Init(L"TMIDI");
    hRes = pCVTxt->Init();
    if (hRes)
        return 1;

	speechInitialized = 1;
#endif

	return 0;
}

void speak(char *text, ...)
{
#ifdef SPEECH
	va_list ap;
	char s[4096];
	static UINT uCodePage = 0;
	WCHAR ws[4096];

	if (speechInitialized)
	{
		if (!uCodePage)
			uCodePage = _getmbcp();

		va_start(ap, text);
		vsprintf(s, text, ap);
		va_end(ap);

		HRESULT hRes;
		MultiByteToWideChar(uCodePage, 0, s, -1, ws, 4096);
		hRes = pCVTxt->Speak(ws);
	}
#endif
}

void stop_speaking(void)
{
#ifdef SPEECH
	pCVTxt->StopSpeaking();
#endif
}

int shutdown_tts_engine(void)
{
#ifdef SPEECH
	if (pCVTxt)
        delete pCVTxt;
	if (COMInitialized)
	{
		CoUninitialize();
		COMInitialized = 0;
	}
	speechInitialized = 0;
#endif

	return 0;
}
