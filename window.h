#include <windows.h>
#include <d3d11.h>

extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern UINT g_ResizeWidth, g_ResizeHeight;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

bool CreateDeviceD3D( HWND hWnd );
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
