
#include <tchar.h>
#include <windows.h>
#include <d3d11.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "main.h"
#include "platform.h"


std::string platform_open_file_dialog(const char* title, int nfilters, const file_dialog_filter* filters, const char* def_out)
{
  std::string retval;
  std::string built_filter;

  if (nfilters < 0)
    return retval;

  for (int i = 0; i < nfilters; ++i)
  {
    built_filter += filters[i].title;
    built_filter += "\0";
    built_filter += filters[i].pattern;
    built_filter += "\0";
  }
  built_filter += "\0";

  char dir[ 1024 ];
  if ( !GetCurrentDirectoryA( 1024, dir ) )
    memset( dir, 0, sizeof( char ) * 1024 );

  char Filestring[ 256 ];

  OPENFILENAMEA opf;
  opf.hwndOwner = 0;
  opf.lpstrFilter = built_filter.c_str();
  opf.lpstrCustomFilter = 0;
  opf.nMaxCustFilter = 0L;
  opf.nFilterIndex = 1L;
  opf.lpstrFile = Filestring;
  opf.lpstrFile[ 0 ] = '\0';
  opf.nMaxFile = 256;
  opf.lpstrFileTitle = 0;
  opf.nMaxFileTitle = 50;
  opf.lpstrInitialDir = def_out;
  opf.lpstrTitle = title;
  opf.nFileOffset = 0;
  opf.nFileExtension = 0;
  opf.lpstrDefExt = (nfilters < 1) ? NULL : (filters[0].pattern + 2);
  opf.lpfnHook = NULL;
  opf.lCustData = 0;
  opf.Flags = ( OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON ) & ~OFN_ALLOWMULTISELECT;
  opf.lStructSize = sizeof( OPENFILENAME );

  opf.hInstance = GetModuleHandle( 0 );
  opf.pvReserved = NULL;
  opf.dwReserved = 0;
  opf.FlagsEx = 0;

  if ( GetOpenFileNameA( &opf ) )
  {
    SetCurrentDirectoryA( dir );
    retval = opf.lpstrFile;
  }

  SetCurrentDirectoryA( dir );

  return retval;
}


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


INT WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow )
{
  //ImGui_ImplWin32_EnableDpiAwareness();
  WNDCLASSEXW wc = { sizeof( wc ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
  ::RegisterClassExW( &wc );
  HWND hwnd = ::CreateWindowW( wc.lpszClassName, L"Conspiracy KKP Analyzer " VERSION, WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr );

  if ( !CreateDeviceD3D( hwnd ) )
  {
    CleanupDeviceD3D();
    ::UnregisterClassW( wc.lpszClassName, wc.hInstance );
    return 1;
  }

  ::ShowWindow( hwnd, SW_SHOWMAXIMIZED );
  ::UpdateWindow( hwnd );
  DragAcceptFiles( hwnd, TRUE );

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  ImGui_ImplWin32_Init( hwnd );
  ImGui_ImplDX11_Init( g_pd3dDevice, g_pd3dDeviceContext );

  InitStuff(__argc, __argv);

  ImVec4 clear_color = ImVec4( 0, 0, 0, 1.00f );

  int rtWidth = 0;
  int rtHeight = 0;

  bool done = false;
  while ( !done )
  {
    MSG msg;
    while ( ::PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
    {
      ::TranslateMessage( &msg );
      ::DispatchMessage( &msg );
      if ( msg.message == WM_DROPFILES )
      {
        HDROP drop = (HDROP)msg.wParam;
        int count = DragQueryFileA( drop, -1, nullptr, 0 );
        for ( int i = 0; i < count; i++ )
        {
          char sz[ MAX_PATH ] = { 0 };
          DragQueryFileA( drop, i, sz, MAX_PATH );
          LoadFile( sz );
        }
        DragFinish( drop );
      }
      if ( msg.message == WM_QUIT )
      {
        done = true;
      }
    }
    if ( done )
    {
      break;
    }

    if ( g_ResizeWidth != 0 && g_ResizeHeight != 0 )
    {
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers( 0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0 );
      rtWidth = g_ResizeWidth;
      rtHeight = g_ResizeHeight;
      g_ResizeWidth = g_ResizeHeight = 0;
      CreateRenderTarget();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    RenderFrame();

    const float clear_color_with_alpha[ 4 ] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
    g_pd3dDeviceContext->OMSetRenderTargets( 1, &g_mainRenderTargetView, nullptr );
    g_pd3dDeviceContext->ClearRenderTargetView( g_mainRenderTargetView, clear_color_with_alpha );
    ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );

    g_pSwapChain->Present( 1, 0 );
  }

  // Cleanup
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  ::DestroyWindow( hwnd );
  ::UnregisterClassW( wc.lpszClassName, wc.hInstance );

  return 0;
}


ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Helper functions

bool CreateDeviceD3D( HWND hWnd )
{
    // Setup swap chain
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory( &sd, sizeof( sd ) );
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[ 2 ] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
  HRESULT res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext );
  if ( res == DXGI_ERROR_UNSUPPORTED ) // Try high-performance WARP software driver if hardware is not available.
    res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext );
  if ( res != S_OK )
    return false;

  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D()
{
  CleanupRenderTarget();
  if ( g_pSwapChain ) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
  if ( g_pd3dDeviceContext ) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
  if ( g_pd3dDevice ) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
  ID3D11Texture2D* pBackBuffer;
  g_pSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
  g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_mainRenderTargetView );
  pBackBuffer->Release();
}

void CleanupRenderTarget()
{
  if ( g_mainRenderTargetView ) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
  if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
    return true;

  switch ( msg )
  {
  case WM_SIZE:
    if ( wParam == SIZE_MINIMIZED )
      return 0;
    g_ResizeWidth = (UINT)LOWORD( lParam ); // Queue resize
    g_ResizeHeight = (UINT)HIWORD( lParam );
    return 0;
  case WM_SYSCOMMAND:
    if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    ::PostQuitMessage( 0 );
    return 0;
  }
  return ::DefWindowProcW( hWnd, msg, wParam, lParam );
}

