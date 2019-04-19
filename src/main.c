
#define COBJMACROS
#include <windows.h>
//#include <commdlg.h>
//#include <Unknwnbase.h>
#include <Unknwn.h>
#include <wincodec.h>
#include <d2d1.h>
#include <d2d1helper.h>

//#define NK_IMPLEMENTATION
//#include "nuklear.h"
//#include <vlc/vlc.h>


static const float DEFAULT_DPI = 96.f;
static const D2D1_COLOR_F D2D1_COLOR_F_WHITE = { 1.f, 1.f, 1.f, 1.f };
static const D2D1_MATRIX_3X2_F D2D1_MATRIX_3X2_F_IDENTITY = { 1.f, 0.f, 0.f, 1.f, 0.f, 0.f };

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define SAFE_RELEASE(X) if ((X)) { IUnknown_Release(((IUnknown*)(X))); X = NULL; }

#define WINDOW_WIDTH 950
#define WINDOW_HEIGHT 700
#define MIN_WINDOW_WIDTH 320
#define MIN_WINDOW_HEIGHT 240

static struct {
	unsigned int width;
	unsigned int height;
	unsigned int min_width;
	unsigned int min_height;
} tine_app;

static struct {
	ID2D1Factory *d2d_factory;
	ID2D1HwndRenderTarget *hwnd_render_target;
	ID2D1Bitmap *d2d_bitmap;
} d2d_res;

static struct {
	IWICImagingFactory *wic_imaging_factory;
	IWICFormatConverter *converted_bitmap_source;
	unsigned int image_width;
	unsigned int image_height;
} wic_res;

//int main()
//{
	//libvlc_instance_t *vlc_instance;
	//libvlc_media_player_t *media_player;
	//libvlc_media_t *media;

	//int wait_time = 1000 * 20;

	//vlc_instance = libvlc_new(0, NULL);
	////media = libvlc_media_new_location(vlc_instance, "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov");
	//media = libvlc_media_new_path(vlc_instance, "media\\BigBuckBunny_320x180_Trim.mp4");
	//media_player = libvlc_media_player_new_from_media(media);

	//libvlc_media_release(media);

	//libvlc_media_player_play(media_player);
	//
	//Sleep(wait_time);

	//libvlc_media_player_stop(media_player);

	//libvlc_media_player_release(media_player);

	//libvlc_release(vlc_instance);

	//return 0;

//}

static
HRESULT InitializeFactory(HINSTANCE hinstance)
{
	HRESULT hr = S_OK;

	// Create WIC Factory
	hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &wic_res.wic_imaging_factory);

	if (SUCCEEDED(hr)) {
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, &d2d_res.d2d_factory);
	}

	return hr;
}

static
bool LocateImageFile(HWND hwnd, LPWSTR file_name, DWORD file_name_length)
{
	file_name[0] = L'\0';
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter =   L"All Image Files\0"              L"*.bmp;*.dib;*.wdp;*.mdp;*.hdp;*.gif;*.png;*.jpg;*.jpeg;*.tif;*.ico\0"
						L"Windows Bitmap\0"               L"*.bmp;*.dib\0"
						L"High Definition Photo\0"        L"*.wdp;*.mdp;*.hdp\0"
						L"Graphics Interchange Format\0"  L"*.gif\0"
						L"Portable Network Graphics\0"    L"*.png\0"
						L"JPEG File Interchange Format\0" L"*.jpg;*.jpeg\0"
						L"Tiff File\0"                    L"*.tif\0"
						L"Icon\0"                         L"*.ico\0"
						L"All Files\0"                    L"*.*\0"
						L"\0";
	ofn.lpstrFile = file_name;
	ofn.nMaxFile = file_name_length;
	ofn.lpstrTitle = L"Open Image";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	return (GetOpenFileName(&ofn) == TRUE);
}

static
HRESULT CreateDeviceResources(HWND hwnd)
{
	HRESULT hr = S_OK;
	RECT rc;
	if (!GetClientRect(hwnd, &rc)) {
		hr = HRESULT_FROM_WIN32(GetLastError());
	}
	if (SUCCEEDED(hr)) {

		if (!d2d_res.hwnd_render_target) {
			// Set the DPI to be the default system DPI to allow direct mapping
			// between image pixels and desktop pixels in different system DPI settings
			D2D1_PIXEL_FORMAT pixel_format = { DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN };
			D2D1_RENDER_TARGET_PROPERTIES render_target_properties = {
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				pixel_format,
				DEFAULT_DPI,
				DEFAULT_DPI,
				D2D1_RENDER_TARGET_USAGE_NONE,
				D2D1_FEATURE_LEVEL_DEFAULT,
			};
			// auto render_target_properties = D2D1::RenderTargetProperties();

			D2D1_SIZE_U size = { rc.right - rc.left, rc.bottom - rc.top };

			D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_rtp = { hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY };

			
			hr = ID2D1Factory_CreateHwndRenderTarget(d2d_res.d2d_factory, &render_target_properties, &hwnd_rtp, &d2d_res.hwnd_render_target);
		}
		else {
			D2D1_SIZE_U size = { rc.right - rc.left, rc.bottom - rc.top };
			ID2D1HwndRenderTarget_Resize(d2d_res.hwnd_render_target, &size);
		}
	}
	return hr;
}

static
HRESULT CreateD2DBitmapFromFile(HWND hwnd)
{
	HRESULT hr = S_OK;
	WCHAR file_name[MAX_PATH];

	// 1. load a image file
	hr = LocateImageFile(hwnd, file_name, COUNT_OF(file_name)) ? S_OK : E_FAIL;


	if (SUCCEEDED(hr)) {
		// 2. decode the source image
		IWICBitmapDecoder *decoder = NULL;
		hr = IWICImagingFactory_CreateDecoderFromFilename(wic_res.wic_imaging_factory, file_name, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
		IWICBitmapFrameDecode *frame = NULL;
		if (SUCCEEDED(hr)) {
			hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
		}

		// 3. format convert the frame to 32bpPBGRA
		if (SUCCEEDED(hr)) {
			IWICBitmapFrameDecode_GetSize(frame, &wic_res.image_width, &wic_res.image_height);
			SAFE_RELEASE(wic_res.converted_bitmap_source);
			hr = IWICImagingFactory_CreateFormatConverter(wic_res.wic_imaging_factory, &wic_res.converted_bitmap_source);
		}

		if (SUCCEEDED(hr)) {
			hr = IWICFormatConverter_Initialize(
				wic_res.converted_bitmap_source,
				(IWICBitmapSource *)frame,                           // Input bitmap to convert
				&GUID_WICPixelFormat32bppPBGRA,   // Destination pixel format
				WICBitmapDitherTypeNone,         // Specified dither pattern
				NULL,                         // Specify a particular palette 
				0.f,                             // Alpha threshold
				WICBitmapPaletteTypeCustom       // Palette translation type
			);
		}

		// 4. create render target and D2D bitmap from IWICBitmapSource
		if (SUCCEEDED(hr)) {
			hr = CreateDeviceResources(hwnd);
		}
		if (SUCCEEDED(hr)) {
			SAFE_RELEASE(d2d_res.d2d_bitmap);
			ID2D1HwndRenderTarget_CreateBitmapFromWicBitmap(d2d_res.hwnd_render_target, (IWICBitmapSource *)wic_res.converted_bitmap_source, NULL, &d2d_res.d2d_bitmap);
		}

		SAFE_RELEASE(decoder);
		SAFE_RELEASE(frame);
	}


	return hr;
}

static
LRESULT RenderView(HWND hwnd)
{
	HRESULT hr = S_OK;
	PAINTSTRUCT ps;

	if (BeginPaint(hwnd, &ps)) {
		hr = CreateDeviceResources(hwnd);
		if (SUCCEEDED(hr) && !(ID2D1HwndRenderTarget_CheckWindowState(d2d_res.hwnd_render_target) & D2D1_WINDOW_STATE_OCCLUDED)) {
			ID2D1HwndRenderTarget_BeginDraw(d2d_res.hwnd_render_target);
			ID2D1HwndRenderTarget_SetTransform(d2d_res.hwnd_render_target, &D2D1_MATRIX_3X2_F_IDENTITY);
			ID2D1HwndRenderTarget_Clear(d2d_res.hwnd_render_target, &D2D1_COLOR_F_WHITE);
			// D2D1_SIZE_F rt_size = ID2D1HwndRenderTarget_GetSize(d2d_res.hwnd_render_target);

			
			// D2DBitmap may have been released due to device loss. 
			// If so, re-create it from the source bitmap
			if (wic_res.converted_bitmap_source && !d2d_res.d2d_bitmap) {
				ID2D1HwndRenderTarget_CreateBitmapFromWicBitmap(d2d_res.hwnd_render_target, (IWICBitmapSource *)wic_res.converted_bitmap_source, NULL, &d2d_res.d2d_bitmap);
			}
			if (d2d_res.d2d_bitmap) {
				D2D1_SIZE_F resize = { wic_res.image_width, wic_res.image_height };

				float _ratio = resize.width / resize.height;

				resize.width = resize.width < tine_app.width ? resize.width : tine_app.width;
				resize.height = resize.width / _ratio;
				resize.height = resize.height < tine_app.height ? resize.height : tine_app.height;
				resize.width = resize.height * _ratio;

				float _left = 0.5f*(tine_app.width - resize.width);
				float _top = 0.5f*(tine_app.height - resize.height);
				float _right = _left + resize.width;
				float _bottom = _top + resize.height;

				D2D_RECT_F rectangle = { _left, _top, _right, _bottom};

				ID2D1HwndRenderTarget_DrawBitmap(d2d_res.hwnd_render_target, d2d_res.d2d_bitmap, &rectangle, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
			}
			hr = ID2D1HwndRenderTarget_EndDraw(d2d_res.hwnd_render_target, NULL, NULL);

			// In case of device loss, discard D2D render target and D2DBitmap
			// They will be re-created in the next rendering pass
			if (hr == D2DERR_RECREATE_TARGET) {
				SAFE_RELEASE(d2d_res.d2d_bitmap);
				SAFE_RELEASE(d2d_res.hwnd_render_target);
				// force a re-render
				hr = InvalidateRect(hwnd, NULL, TRUE) ? S_OK : E_FAIL;
			}
		}
		EndPaint(hwnd, &ps);
	}

	return SUCCEEDED(hr) ? 0 : 1;
}

static
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_CREATE: {
			if (FAILED(CreateD2DBitmapFromFile(hwnd))) {
				MessageBox(hwnd, L"Failed to load image", L"Application Error", MB_ICONEXCLAMATION | MB_OK);
			}
			break;
		}
		case WM_SIZE: {
			D2D1_SIZE_U size = { LOWORD(lParam), HIWORD(lParam) };
			if (d2d_res.hwnd_render_target) {
				// If we couldn't resize, release the device and we'll recreate it
				// during the next render pass.
				tine_app.width = size.width;
				tine_app.height = size.height;
				if (FAILED(ID2D1HwndRenderTarget_Resize(d2d_res.hwnd_render_target, &size))) {
					SAFE_RELEASE(d2d_res.hwnd_render_target);
					SAFE_RELEASE(d2d_res.d2d_bitmap);
				}
				InvalidateRect(hwnd, NULL, FALSE);
			}
			break;
		}

		case WM_PAINT: {
			return RenderView(hwnd);
		}
		case WM_GETMINMAXINFO: {
			MINMAXINFO *min_max_info = (PMINMAXINFO)lParam;
			min_max_info->ptMinTrackSize.x = tine_app.min_width;
			min_max_info->ptMinTrackSize.y = tine_app.min_height;
			break;
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;

		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);

}

static
HRESULT InitializeWindow(HINSTANCE hInstance)
{
	HRESULT hr = S_OK;

	const wchar_t CLASS_NAME[] = L"Tine Base Window Class";
	tine_app.width = WINDOW_WIDTH;
	tine_app.height = WINDOW_HEIGHT;
	tine_app.min_width = MIN_WINDOW_WIDTH;
	tine_app.min_height = MIN_WINDOW_HEIGHT;

	WNDCLASS wc = { 0 };
	RECT rc = { 0, 0, tine_app.width, tine_app.height };
	DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	DWORD exstyle = WS_EX_APPWINDOW;


	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	AdjustWindowRectEx(&rc, style, FALSE, exstyle);

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Tine",
		style,
		CW_USEDEFAULT, CW_USEDEFAULT, tine_app.width, tine_app.height,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	hr = hwnd ? S_OK : E_FAIL;

	//ShowWindow(hwnd, nCmdShow);
	return hr;
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPreInstance, PWSTR pCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPreInstance);
	UNREFERENCED_PARAMETER(pCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr)) {
		hr = InitializeFactory(hInstance);
		if (SUCCEEDED(hr)) {
			hr = InitializeWindow(hInstance);
		}
		if (SUCCEEDED(hr)) {
			MSG msg = { 0 };
			while (GetMessage(&msg, NULL, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		CoUninitialize();
	}


	SAFE_RELEASE(wic_res.wic_imaging_factory);
	SAFE_RELEASE(d2d_res.d2d_factory);
	SAFE_RELEASE(d2d_res.hwnd_render_target);
	SAFE_RELEASE(d2d_res.d2d_bitmap);
	SAFE_RELEASE(wic_res.converted_bitmap_source);

	return 0;
}

