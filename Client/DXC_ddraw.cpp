// DXC_ddraw.cpp: implementation of the DXC_ddraw class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <objbase.h>
#include <vector>
#include "DXC_ddraw.h"
#include <string>
#include "GlobalDef.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image_write.h"

extern HWND G_hEditWnd;
extern HWND G_hWnd;

#define CHANGE16BPP				16
#define CHANGE32BPP				32

constexpr int RESOLUTION_X = 800;
constexpr int RESOLUTION_Y = 600;

extern long    G_lTransG100[64][64], G_lTransRB100[64][64];
extern long    G_lTransG70[64][64], G_lTransRB70[64][64];
extern long    G_lTransG50[64][64], G_lTransRB50[64][64];
extern long    G_lTransG25[64][64], G_lTransRB25[64][64];
extern long    G_lTransG2[64][64], G_lTransRB2[64][64];

struct DDSelectCtx { HMONITOR target; GUID guid; bool has; };
static BOOL WINAPI DDEnumCb(GUID* lpGUID, LPSTR, LPSTR, LPVOID p, HMONITOR hmon)
{
	DDSelectCtx* c = (DDSelectCtx*)p;
	if (hmon == c->target) {
		if (lpGUID) { c->guid = *lpGUID; c->has = true; }
		else { c->has = false; }
		return false;
	}
	return true;
}

static void DDDbg(const char* tag, HRESULT hr) {
	char buf[128]; wsprintf(buf, "%s hr=0x%08lX\r\n", tag, (unsigned long)hr);
	OutputDebugString(buf);
}

static void SizeWindowForClient(HWND hWnd, int clientW, int clientH, const RECT& workArea)
{
	DWORD style = GetWindowLong(hWnd, GWL_STYLE);
	DWORD ex = GetWindowLong(hWnd, GWL_EXSTYLE);
	RECT wr = { 0, 0, clientW, clientH };

	HMODULE u32 = GetModuleHandleA("user32.dll");
	typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
	typedef BOOL(WINAPI* PFN_AdjustForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);
	PFN_GetDpiForWindow pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(u32, "GetDpiForWindow");
	PFN_AdjustForDpi     pAdjustForDpi = (PFN_AdjustForDpi)GetProcAddress(u32, "AdjustWindowRectExForDpi");

	if (pGetDpiForWindow && pAdjustForDpi) {
		UINT dpi = pGetDpiForWindow(hWnd);
		pAdjustForDpi(&wr, style, false, ex, dpi);
	}
	else {
		AdjustWindowRectEx(&wr, style, false, ex);
	}

	int winW = wr.right - wr.left;
	int winH = wr.bottom - wr.top;
	int cx = (workArea.left + workArea.right) / 2;
	int cy = (workArea.top + workArea.bottom) / 2;
	SetWindowPos(hWnd, HWND_TOP, cx - winW / 2, cy - winH / 2, winW, winH, SWP_SHOWWINDOW);
}

bool DXC_ddraw::SelectAdapterForWindow(HWND hWnd)
{
	DDSelectCtx ctx = {};
	ctx.target = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	ctx.has = false;
	DirectDrawEnumerateExA(DDEnumCb, &ctx,
		DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES | DDENUM_NONDISPLAYDEVICES);
	m_hasGuid = ctx.has;
	if (m_hasGuid) m_ddGuid = ctx.guid;
	return m_hasGuid;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DXC_ddraw::DXC_ddraw()
{
	m_lpFrontB4 = NULL;
	m_lpDD4 = NULL;
	m_lpPDBGS = NULL;
	m_lpBackB4flip = NULL;
	m_cPixelFormat = 0;
	m_init = false;
#ifdef DEF_WINDOWED_MODE	
	m_bFullMode = false;
#else
	m_bFullMode = true;
#endif
	res_x = 0;
	res_y = 0;
	res_x_mid = 0;
	res_y_mid = 0;
	m_hWnd = NULL;
	m_hasGuid = false;
	ZeroMemory(&m_ddGuid, sizeof(m_ddGuid));
}

DXC_ddraw::~DXC_ddraw()
{
	if (m_hFontInUse != NULL) DeleteObject(m_hFontInUse);
	if (m_lpBackB4flip != NULL) m_lpBackB4flip->Release();
	if (m_lpBackB4 != NULL) m_lpBackB4->Release();
	if (m_lpFrontB4 != NULL) m_lpFrontB4->Release();
	if (m_bFullMode == true)
	{
		if (m_lpDD4 != NULL) m_lpDD4->RestoreDisplayMode();
	}
	if (m_lpDD4 != NULL) m_lpDD4->Release();
}

bool DXC_ddraw::bInit(HWND hWnd)
{
	HRESULT        ddVal;
	DDSURFACEDESC2 ddsd;
	int            iS, iD;

	m_hWnd = hWnd;

	res_x = RESOLUTION_X;
	res_y = RESOLUTION_Y;
	res_x_mid = res_x / 2;
	res_y_mid = res_y / 2;

	SetRect(&m_rcClipArea, 0, 0, res_x, res_y);

	// Create DD on the adapter that owns this window's monitor (if available)
	SelectAdapterForWindow(hWnd);
	GUID* pGuid = m_hasGuid ? &m_ddGuid : NULL;

	ddVal = DirectDrawCreateEx(pGuid, (VOID**)&m_lpDD4, IID_IDirectDraw7, NULL);
	if (ddVal != DD_OK) return false;

	if (m_bFullMode)
	{
		DDSCAPS2 ddscaps;
		ddVal = m_lpDD4->SetCooperativeLevel(hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
		DDDbg("SetCooperativeLevel", ddVal);
		if (ddVal != DD_OK) return false;

		ChangeBPP(CHANGE32BPP);
		ddVal = m_lpDD4->SetDisplayMode(res_x, res_y, 16, 0, 0);
		DDDbg("SetDisplayMode", ddVal);
		if (ddVal != DD_OK) return false;

		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		ddsd.dwBackBufferCount = 1;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;

		ddVal = m_lpDD4->CreateSurface(&ddsd, &m_lpFrontB4, NULL);
		if (ddVal != DD_OK) return false;

		ZeroMemory(&ddscaps, sizeof(ddscaps));
		ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
		ddVal = m_lpFrontB4->GetAttachedSurface(&ddscaps, &m_lpBackB4flip);
		if (ddVal != DD_OK) return false;

		SetRect(&m_rcFlipping, 0, 0, res_x, res_y);
	}
	else
	{
		// Windowed: cooperative level and DPI-aware client sizing to 800x600 on the window's monitor
		MONITORINFOEX mi;
		mi.cbSize = sizeof(mi);
		HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		GetMonitorInfo(mon, &mi);
		RECT wa = mi.rcWork;

		ddVal = m_lpDD4->SetCooperativeLevel(hWnd, DDSCL_NORMAL | DDSCL_MULTITHREADED);
		DDDbg("SetCooperativeLevel", ddVal);
		if (ddVal != DD_OK) return false;

		DWORD style = GetWindowLong(hWnd, GWL_STYLE);
		DWORD ex = GetWindowLong(hWnd, GWL_EXSTYLE);
		RECT wr = { 0, 0, res_x, res_y };

		HMODULE u32 = GetModuleHandleA("user32.dll");
		typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
		typedef BOOL(WINAPI* PFN_AdjustForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);
		PFN_GetDpiForWindow pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(u32, "GetDpiForWindow");
		PFN_AdjustForDpi     pAdjustForDpi = (PFN_AdjustForDpi)GetProcAddress(u32, "AdjustWindowRectExForDpi");
		if (pGetDpiForWindow && pAdjustForDpi) {
			UINT dpi = pGetDpiForWindow(hWnd);
			pAdjustForDpi(&wr, style, false, ex, dpi);
		}
		else {
			AdjustWindowRectEx(&wr, style, false, ex);
		}
		int winW = wr.right - wr.left;
		int winH = wr.bottom - wr.top;
		int cx = (wa.left + wa.right) / 2;
		int cy = (wa.top + wa.bottom) / 2;
		SetWindowPos(hWnd, HWND_TOP, cx - winW / 2, cy - winH / 2, winW, winH, SWP_SHOWWINDOW);

		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

		ddVal = m_lpDD4->CreateSurface(&ddsd, &m_lpFrontB4, NULL);
		if (ddVal != DD_OK) return false;

		SetRect(&m_rcFlipping, 0, 0, 0, 0); // use clipper
		InitFlipToGDI(hWnd);
	}

	// Offscreen back buffers (explicit 16-bit 565 in pCreateOffScreenSurface)
	m_lpBackB4 = pCreateOffScreenSurface(res_x, res_y);
	if (m_lpBackB4 == NULL) return false;

	m_lpPDBGS = pCreateOffScreenSurface(res_x + 32, res_y + 32);
	if (m_lpPDBGS == NULL) return false;

	ddsd.dwSize = sizeof(ddsd);
	if (m_lpBackB4->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK) return false;
	m_pBackB4Addr = (WORD*)ddsd.lpSurface;
	m_sBackB4Pitch = (short)(ddsd.lPitch >> 1);
	m_lpBackB4->Unlock(NULL);

	_TestPixelFormat();
	for (iS = 0; iS < 64; iS++)
		for (iD = 0; iD < 64; iD++) {
			m_lTransRB100[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 1.0f);
			m_lTransG100[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 1.0f);
			m_lTransRB70[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.7f);
			m_lTransG70[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 0.7f);
			m_lTransRB50[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.5f);
			m_lTransG50[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 0.5f);
			m_lTransRB25[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.25f);
			m_lTransG25[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 0.25f);
			m_lTransRB2[iD][iS] = _CalcMaxValue(iS, iD, 'R', 2, 1.0f);
			m_lTransG2[iD][iS] = _CalcMaxValue(iS, iD, 'G', 2, 1.0f);
			m_lFadeRB[iD][iS] = _CalcMinValue(iS, iD, 'R');
			m_lFadeG[iD][iS] = _CalcMinValue(iS, iD, 'G');
			G_lTransRB100[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 1.0f);
			G_lTransG100[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 1.0f);
			G_lTransRB70[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.7f);
			G_lTransG70[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 0.7f);
			G_lTransRB50[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.5f);
			G_lTransG50[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 0.5f);
			G_lTransRB25[iD][iS] = _CalcMaxValue(iS, iD, 'R', 1, 0.25f);
			G_lTransG25[iD][iS] = _CalcMaxValue(iS, iD, 'G', 1, 0.25f);
			G_lTransRB2[iD][iS] = _CalcMaxValue(iS, iD, 'R', 2, 1.0f);
			G_lTransG2[iD][iS] = _CalcMaxValue(iS, iD, 'G', 2, 1.0f);
		}

	m_hFontInUse = CreateFont(16, 0, 0, 0, FW_NORMAL, false, false, false, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, VARIABLE_PITCH, "Tahoma");
	m_hDC = NULL;
	m_init = true;
	return true;
}

HRESULT DXC_ddraw::iFlip()
{
	if (!m_init)
		return DD_OK;

	HRESULT ddVal;

	if (m_bFullMode)
	{
#ifdef DEF_USING_WIN_IME
		ddVal = m_lpFrontB4->Blt(NULL, m_lpBackB4, NULL, DDBLT_WAIT, NULL);
		if (G_hEditWnd != NULL) {
			if (ddVal != DDERR_SURFACELOST) m_lpDD4->FlipToGDISurface();
		}
#else
		ddVal = m_lpBackB4flip->BltFast(0, 0, m_lpBackB4, &m_rcFlipping, DDBLTFAST_NOCOLORKEY);
		ddVal = m_lpFrontB4->Flip(m_lpBackB4flip, DDFLIP_WAIT);
#endif
	}
	else
	{
		RECT rcClient; POINT pt = { 0,0 };
		GetClientRect(m_hWnd, &rcClient);
		ClientToScreen(m_hWnd, &pt);

		// dest == client top-left with exact src size (800x600), so no stretch
		RECT dst = { pt.x, pt.y, pt.x + res_x, pt.y + res_y };

		ddVal = m_lpFrontB4->Blt(&dst, m_lpBackB4, NULL, DDBLT_WAIT, NULL);
	}

	if (ddVal == DDERR_SURFACELOST) {
		DDSURFACEDESC2 ddsd2; ddsd2.dwSize = sizeof(ddsd2);
		m_lpFrontB4->Restore();
		m_lpBackB4->Restore();
		if (m_lpBackB4->Lock(NULL, &ddsd2, DDLOCK_WAIT, NULL) != DD_OK) return DDERR_SURFACELOST;
		m_pBackB4Addr = (WORD*)ddsd2.lpSurface;
		m_lpBackB4->Unlock(NULL);
		return DDERR_SURFACELOST;
	}
	return DD_OK;
}

void DXC_ddraw::ChangeDisplayMode(HWND hWnd)
{
	HRESULT        ddVal;
	DDSURFACEDESC2 ddsd;

	if (!m_init) return;

	m_hWnd = hWnd;

	// Release current surfaces
	if (m_lpBackB4flip) { m_lpBackB4flip->Release(); m_lpBackB4flip = NULL; }
	if (m_lpBackB4) { m_lpBackB4->Release();     m_lpBackB4 = NULL; }
	if (m_lpPDBGS) { m_lpPDBGS->Release();      m_lpPDBGS = NULL; }
	if (m_lpFrontB4) { m_lpFrontB4->Release();    m_lpFrontB4 = NULL; }

	// If we were fullscreen, restore the desktop mode
	if (m_bFullMode == true && m_lpDD4) m_lpDD4->RestoreDisplayMode();

	res_x = RESOLUTION_X;
	res_y = RESOLUTION_Y;
	res_x_mid = RESOLUTION_X / 2;
	res_y_mid = RESOLUTION_Y / 2;
	SetRect(&m_rcClipArea, 0, 0, res_x, res_y);

	if (m_bFullMode == false)
	{
		// Windowed
		MONITORINFOEX mi;
		mi.cbSize = sizeof(mi);
		HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		GetMonitorInfo(mon, &mi);
		RECT wa = mi.rcWork;

		ddVal = m_lpDD4->SetCooperativeLevel(hWnd, DDSCL_NORMAL | DDSCL_MULTITHREADED);
		if (ddVal != DD_OK) return;

		DWORD style = GetWindowLong(hWnd, GWL_STYLE);
		DWORD ex = GetWindowLong(hWnd, GWL_EXSTYLE);
		RECT wr = { 0, 0, res_x, res_y };

		HMODULE u32 = GetModuleHandleA("user32.dll");
		typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
		typedef BOOL(WINAPI* PFN_AdjustForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);
		PFN_GetDpiForWindow pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(u32, "GetDpiForWindow");
		PFN_AdjustForDpi     pAdjustForDpi = (PFN_AdjustForDpi)GetProcAddress(u32, "AdjustWindowRectExForDpi");
		if (pGetDpiForWindow && pAdjustForDpi) {
			UINT dpi = pGetDpiForWindow(hWnd);
			pAdjustForDpi(&wr, style, false, ex, dpi);
		}
		else {
			AdjustWindowRectEx(&wr, style, false, ex);
		}
		int winW = wr.right - wr.left;
		int winH = wr.bottom - wr.top;
		int cx = (wa.left + wa.right) / 2;
		int cy = (wa.top + wa.bottom) / 2;
		SetWindowPos(hWnd, HWND_TOP, cx - winW / 2, cy - winH / 2, winW, winH, SWP_SHOWWINDOW);

		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

		ddVal = m_lpDD4->CreateSurface(&ddsd, &m_lpFrontB4, NULL);
		if (ddVal != DD_OK) return;

		SetRect(&m_rcFlipping, 0, 0, 0, 0);
		InitFlipToGDI(hWnd);
	}
	else
	{
		// Fullscreen
		DDSCAPS2 ddscaps;

		ddVal = m_lpDD4->SetCooperativeLevel(hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
		if (ddVal != DD_OK) return;
		ChangeBPP(CHANGE32BPP);
		ddVal = m_lpDD4->SetDisplayMode(res_x, res_y, 16, 0, 0);
		if (ddVal != DD_OK) return;

		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		ddsd.dwBackBufferCount = 1;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;

		ddVal = m_lpDD4->CreateSurface(&ddsd, &m_lpFrontB4, NULL);
		if (ddVal != DD_OK) return;

		ZeroMemory(&ddscaps, sizeof(ddscaps));
		ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
		ddVal = m_lpFrontB4->GetAttachedSurface(&ddscaps, &m_lpBackB4flip);
		if (ddVal != DD_OK) return;

		SetRect(&m_rcFlipping, 0, 0, res_x, res_y);
	}

	// Recreate back buffers
	m_lpBackB4 = pCreateOffScreenSurface(res_x, res_y);
	if (!m_lpBackB4) return;

	m_lpPDBGS = pCreateOffScreenSurface(res_x + 32, res_y + 32);
	if (!m_lpPDBGS) return;

	DDSURFACEDESC2 lockd;
	ZeroMemory(&lockd, sizeof(lockd));
	lockd.dwSize = sizeof(lockd);
	if (m_lpBackB4->Lock(NULL, &lockd, DDLOCK_WAIT, NULL) != DD_OK) return;
	m_pBackB4Addr = (WORD*)lockd.lpSurface;
	m_sBackB4Pitch = (short)(lockd.lPitch >> 1);
	m_lpBackB4->Unlock(NULL);
}

IDirectDrawSurface7* DXC_ddraw::pCreateOffScreenSurface(WORD wSzX, WORD wSzY)
{
	DDSURFACEDESC2 ddsd; IDirectDrawSurface7* pdds4;

	ZeroMemory(&ddsd, sizeof(ddsd));
	if ((wSzX % 4) != 0) wSzX += 4 - (wSzX % 4);
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	ddsd.dwWidth = (DWORD)wSzX;
	ddsd.dwHeight = (DWORD)wSzY;

	// Explicit 16-bit 5:6:5 format so all your draw code and color keys stay correct
	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
	ddsd.ddpfPixelFormat.dwRBitMask = 0xF800;
	ddsd.ddpfPixelFormat.dwGBitMask = 0x07E0;
	ddsd.ddpfPixelFormat.dwBBitMask = 0x001F;

	if (m_lpDD4->CreateSurface(&ddsd, &pdds4, NULL) != DD_OK) return NULL;
	return pdds4;
}

HRESULT DXC_ddraw::iSetColorKey(IDirectDrawSurface7* pdds4, COLORREF rgb)
{
	DDCOLORKEY ddck;

	ddck.dwColorSpaceLowValue = _dwColorMatch(pdds4, rgb);
	ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
	return pdds4->SetColorKey(DDCKEY_SRCBLT, &ddck);
}

HRESULT DXC_ddraw::iSetColorKey(IDirectDrawSurface7* pdds4, WORD wColorKey)
{
	DDCOLORKEY ddck;

	ddck.dwColorSpaceLowValue = _dwColorMatch(pdds4, wColorKey);
	ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
	return pdds4->SetColorKey(DDCKEY_SRCBLT, &ddck);
}

DWORD DXC_ddraw::_dwColorMatch(IDirectDrawSurface7* pdds4, COLORREF rgb)
{
	COLORREF rgbT;
	HDC hdc;
	DWORD dw = CLR_INVALID;
	DDSURFACEDESC2 ddsd2;
	HRESULT hres;

	if (rgb != CLR_INVALID && pdds4->GetDC(&hdc) == DD_OK)
	{
		rgbT = GetPixel(hdc, 0, 0);
		SetPixel(hdc, 0, 0, rgb);
		pdds4->ReleaseDC(hdc);
	}

	ddsd2.dwSize = sizeof(ddsd2);
	while ((hres = pdds4->Lock(NULL, &ddsd2, 0, NULL)) == DDERR_WASSTILLDRAWING);

	if (hres == DD_OK)
	{
		dw = *(DWORD*)ddsd2.lpSurface;
		dw &= (1 << ddsd2.ddpfPixelFormat.dwRGBBitCount) - 1;
		pdds4->Unlock(NULL);
	}

	if (rgb != CLR_INVALID && pdds4->GetDC(&hdc) == DD_OK)
	{
		SetPixel(hdc, 0, 0, rgbT);
		pdds4->ReleaseDC(hdc);
	}

	return dw;
}

DWORD DXC_ddraw::_dwColorMatch(IDirectDrawSurface7* pdds4, WORD wColorKey)
{
	DWORD dw = CLR_INVALID, * dwp;
	DDSURFACEDESC2 ddsd2;
	HRESULT hres;

	ddsd2.dwSize = sizeof(ddsd2);
	while ((hres = pdds4->Lock(NULL, &ddsd2, 0, NULL)) == DDERR_WASSTILLDRAWING);

	if (hres == DD_OK)
	{
		dwp = (DWORD*)ddsd2.lpSurface;
		*dwp = (DWORD)wColorKey;
		dw = *(DWORD*)ddsd2.lpSurface;
		dw &= (1 << ddsd2.ddpfPixelFormat.dwRGBBitCount) - 1;
		pdds4->Unlock(NULL);
	}

	return dw;
}

void DXC_ddraw::TextOut(int x, int y, char* cStr, COLORREF rgb)
{
	SetTextColor(m_hDC, rgb);
	::TextOut(m_hDC, x, y, cStr, strlen(cStr));
}

SIZE DXC_ddraw::MeasureText(const std::string& text)
{
	SIZE size = { 0, 0 };
	_GetBackBufferDC();
	GetTextExtentPoint32A(m_hDC, text.c_str(), (int)text.length(), &size);
	_ReleaseBackBufferDC();
	return size;
}

void DXC_ddraw::_TestPixelFormat()
{
	DDSURFACEDESC2 ddSurfaceDesc2;
	HRESULT       hResult;

	ZeroMemory(&ddSurfaceDesc2, sizeof(DDSURFACEDESC2));
	ddSurfaceDesc2.dwSize = sizeof(ddSurfaceDesc2);
	ddSurfaceDesc2.dwFlags = DDSD_PIXELFORMAT;
	hResult = m_lpBackB4->GetSurfaceDesc(&ddSurfaceDesc2);

	if (hResult == DD_OK)
	{
		if (ddSurfaceDesc2.ddpfPixelFormat.dwRBitMask == 0x0000F800) {
			m_cPixelFormat = 1;
			// RGB 5:6:5
		}
		if (ddSurfaceDesc2.ddpfPixelFormat.dwRBitMask == 0x00007C00) {
			m_cPixelFormat = 2;
			// RGB 5:5:5 
		}
		if (ddSurfaceDesc2.ddpfPixelFormat.dwRBitMask == 0x0000001F) {
			m_cPixelFormat = 3;
			// BGR 5:6:5 
		}
	}
}

long DXC_ddraw::_CalcMaxValue(int iS, int iD, char cMode, char cMethod, double dAlpha)
{
	long Sum;
	double dTmp;

	switch (cMethod) {
	case 1:
		dTmp = (double)iS;
		dTmp = dTmp * dAlpha;
		iS = (int)dTmp;
		Sum = (iS)+(iD);
		if (Sum < iD) Sum = iD;
		break;

	case 2:
		Sum = (iS + iD) / 2;
		break;
	}

	switch (cMode) {
	case 'G':
		switch (m_cPixelFormat) {
		case 1:
			if (Sum >= 64) Sum = 63; //v1.3
			break;
		case 2:
			if (Sum >= 32) Sum = 31;
			break;
		}
		break;

	default:
		if (Sum >= 32) Sum = 31;
		break;
	}

	return Sum;
}

long DXC_ddraw::_CalcMinValue(int iS, int iD, char cMode)
{
	long Sum;

	Sum = iD - iS;
	if (Sum < 0) Sum = 0;

	switch (cMode) {
	case 'G':
		switch (m_cPixelFormat) {
		case 1:
			if (Sum >= 64) Sum = 63;
			break;
		case 2:
			if (Sum >= 32) Sum = 31;
			break;
		}
		break;

	default:
		if (Sum >= 32) Sum = 31;
		break;
	}

	return Sum;
}

void DXC_ddraw::ClearBackB4()
{
	DDSURFACEDESC2 ddsd2;
	ddsd2.dwSize = sizeof(ddsd2);
	if (m_lpBackB4->Lock(NULL, &ddsd2, DDLOCK_WAIT, NULL) != DD_OK) return;
	memset((char*)ddsd2.lpSurface, 0, ddsd2.lPitch * res_y);
	m_lpBackB4->Unlock(NULL);
}

void DXC_ddraw::DrawShadowBox(short sX, short sY, short dX, short dY, int iType)
{
	WORD* pDst, wValue;
	int ix, iy;

	pDst = (WORD*)m_pBackB4Addr + sX + ((sY)*m_sBackB4Pitch);

	if (iType == 0) {
		switch (m_cPixelFormat) {
		case 1:
			for (iy = 0; iy <= (dY - sY); iy++) {
				for (ix = 0; ix <= (dX - sX); ix++)
					pDst[ix] = (pDst[ix] & 0xf7de) >> 1;

				pDst += m_sBackB4Pitch;
			}
			break;

		case 2:
			for (iy = 0; iy <= (dY - sY); iy++) {
				for (ix = 0; ix <= (dX - sX); ix++)
					pDst[ix] = (pDst[ix] & 0x7bde) >> 1;

				pDst += m_sBackB4Pitch;
			}
			break;
		}
	}
	else
	{
		switch (iType) {
		case 1:
			if (m_cPixelFormat == 1)
				wValue = 0x38e7;
			else wValue = 0x1ce7;
			break;

		case 2:
			if (m_cPixelFormat == 1)
				wValue = 0x1863;
			else wValue = 0xc63;
			break;
		}

		for (iy = 0; iy <= (dY - sY); iy++) {
			for (ix = 0; ix <= (dX - sX); ix++)
				pDst[ix] = wValue;

			pDst += m_sBackB4Pitch;
		}
	}
}

void DXC_ddraw::PutPixel(short sX, short sY, WORD wR, WORD wG, WORD wB)
{
	WORD* pDst;

	if ((sX < 0) || (sY < 0) || (sX > 799) || (sY > 599)) return;

	pDst = (WORD*)m_pBackB4Addr + sX + ((sY)*m_sBackB4Pitch);

	switch (m_cPixelFormat) {
	case 1:
		*pDst = (WORD)(((wR >> 3) << 11) | ((wG >> 2) << 5) | (wB >> 3));
		break;
	case 2:
		*pDst = (WORD)(((wR >> 3) << 10) | ((wG >> 3) << 5) | (wB >> 3));
		break;
	}
}

void DXC_ddraw::_GetBackBufferDC()
{
	m_lpBackB4->GetDC(&m_hDC);
	SelectObject(m_hDC, m_hFontInUse);
	SetBkMode(m_hDC, TRANSPARENT);
	SetBkColor(m_hDC, RGB(0, 0, 0));
}

void DXC_ddraw::_ReleaseBackBufferDC()
{
	m_lpBackB4->ReleaseDC(m_hDC);
}

void DXC_ddraw::DrawText(LPRECT pRect, const char* pString, COLORREF rgb)
{
	SetTextColor(m_hDC, rgb);
	::DrawText(m_hDC, pString, strlen(pString), pRect, DT_CENTER | DT_NOCLIP | DT_WORDBREAK | DT_NOPREFIX);
}

HRESULT DXC_ddraw::InitFlipToGDI(HWND hWnd)
{
	LPDIRECTDRAWCLIPPER pClipper;
	HRESULT hr;
	DDCAPS ddcaps;

	ZeroMemory(&ddcaps, sizeof(ddcaps));
	ddcaps.dwSize = sizeof(ddcaps);
	m_lpDD4->GetCaps(&ddcaps, NULL);

	if ((ddcaps.dwCaps2 & DDCAPS2_CANRENDERWINDOWED) == 0)
	{
		return E_FAIL;
	}

	// Create a clipper when using GDI to draw on the primary surface 
	if (FAILED(hr = m_lpDD4->CreateClipper(0, &pClipper, NULL)))
		return hr;

	pClipper->SetHWnd(0, hWnd);

	if (FAILED(hr = m_lpFrontB4->SetClipper(pClipper))) return hr;

	if (pClipper)
	{
		pClipper->Release();
		pClipper = NULL;
	}
	return S_OK;
}

void DXC_ddraw::ColorTransferRGB(COLORREF fcolor, int* iR, int* iG, int* iB)
{
	WORD wR, wG, wB;

	switch (m_cPixelFormat)
	{
	case 1:
		// R
		wR = (WORD)((fcolor & 0x000000f8) >> 3);
		// G
		wG = (WORD)((fcolor & 0x0000fc00) >> 10);
		// B
		wB = (WORD)((fcolor & 0x00f80000) >> 19);
		*iR = (int)wR;
		*iG = (int)wG;
		*iB = (int)wB;
		break;
	case 2:
		// R
		wR = (WORD)((fcolor & 0x000000f8) >> 3);
		// G
		wG = (WORD)((fcolor & 0x0000f800) >> 11);
		// B
		wB = (WORD)((fcolor & 0x00f80000) >> 19);
		*iR = (int)wR;
		*iG = (int)wG;
		*iB = (int)wB;
		break;
	}
}

//---------------------------------------------------------------------------

bool DXC_ddraw::Screenshot(LPCTSTR FileName, LPDIRECTDRAWSURFACE7 lpDDS)
{
	return true;
	//if (!FileName || !lpDDS) return false;

	//DDSURFACEDESC2 ddsd{};
	//ddsd.dwSize = sizeof(ddsd);

	//// Lock surface to get raw pixel data
	//if (FAILED(lpDDS->Lock(NULL, &ddsd, DDLOCK_READONLY | DDLOCK_WAIT, NULL)))
	//	return false;

	//int Width = ddsd.dwWidth;
	//int Height = ddsd.dwHeight;

	//// Allocate 24-bit RGB buffer
	//std::vector<unsigned char> pixels(Width * Height * 3);

	//// Convert 16-bit 565 -> 24-bit RGB
	//WORD* src = (WORD*)ddsd.lpSurface;
	//unsigned char* dst = pixels.data();

	//for (int y = 0; y < Height; ++y)
	//{
	//	WORD* row = (WORD*)((BYTE*)ddsd.lpSurface + y * ddsd.lPitch);
	//	for (int x = 0; x < Width; ++x)
	//	{
	//		WORD c = row[x];

	//		// 565 format: RRRRRGGGGGGBBBBB
	//		unsigned char r = (unsigned char)(((c >> 11) & 0x1F) * 255 / 31);
	//		unsigned char g = (unsigned char)(((c >> 5) & 0x3F) * 255 / 63);
	//		unsigned char b = (unsigned char)((c & 0x1F) * 255 / 31);

	//		*dst++ = r;
	//		*dst++ = g;
	//		*dst++ = b;
	//	}
	//}

	//lpDDS->Unlock(NULL);

	//// Write JPEG
	//bool success = (stbi_write_jpg(FileName, Width, Height, 3, pixels.data(), 100) != 0);

	//return success;
}

//---------------------------------------------------------------------------
void DXC_ddraw::ChangeBPP(int8_t bpp) {
	//return;
	DEVMODE d = { 0 };
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &d);

	d.dmBitsPerPel = bpp;
	ChangeDisplaySettings(&d, 0);
}

void DXC_ddraw::DrawItemShadowBox(short sX, short sY, short dX, short dY, int iType)
{
	WORD* pDst, wValue;
	int ix, iy;

	if (sX < 0)
		sX = 0;

	if (sY <= 0)
		sY = 0;

	if (dX >= 799)
		dX = dX - (dX - 799);

	if (dY >= 599)
		dY = dY - (dY - 599);

	int countx = dX - sX;
	int county = dY - sY;

	for (int a = 0; a < countx; a++)
	{
		PutPixel(sX + (a), sY, 152, 123, 54);
		PutPixel(sX + (a), sY - 1, 152, 123, 54);
		PutPixel(sX + (a), dY, 152, 123, 54);
		PutPixel(sX + (a), dY - 1, 152, 123, 54);
	}

	for (int b = 0; b < county; b++)
	{
		PutPixel(sX, sY + (b), 152, 123, 54);
		PutPixel(sX + 1, sY + (b), 152, 123, 54);
		PutPixel(dX, sY + (b), 152, 123, 54);
		PutPixel(dX + 1, sY + (b), 152, 123, 54);
	}

	pDst = (WORD*)m_pBackB4Addr + sX + ((sY)*m_sBackB4Pitch);

	if (iType == 0) {
		switch (m_cPixelFormat) {
		case 1:
			for (iy = 0; iy <= (dY - sY); iy++) {

				for (ix = 0; ix <= (dX - sX); ix++)
					pDst[ix] = (pDst[ix] & 0xf7de) >> 1;

				pDst += m_sBackB4Pitch;
			}
			break;

		case 2:
			for (iy = 0; iy <= (dY - sY); iy++) {

				for (ix = 0; ix <= (dX - sX); ix++)
					pDst[ix] = (pDst[ix] & 0x7bde) >> 1;

				pDst += m_sBackB4Pitch;
			}
			break;
		}
	}
	else
	{
		switch (iType) {
		case 1:
			if (m_cPixelFormat == 1)
				wValue = 0x38e7;
			else wValue = 0x1ce7;
			break;

		case 2:
			if (m_cPixelFormat == 1)
				wValue = 0x1863;
			else wValue = 0xc63;
			break;
		}

		for (iy = 0; iy <= (dY - sY); iy++) {

			for (ix = 0; ix <= (dX - sX); ix++)
				pDst[ix] = wValue;

			pDst += m_sBackB4Pitch;
		}
	}
}