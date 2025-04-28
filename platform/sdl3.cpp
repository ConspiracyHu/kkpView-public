
#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mutex.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include "main.h"
#include "platform.h"



static SDL_Window* thewindow;
static int rtWidth, rtHeight;


struct filedialog_userdata
{
  std::string* pstr;
  SDL_Semaphore* sem;
};


static void SDLCALL open_file_cb(void* userdata, const char* const* filelist, int filter)
{
  filedialog_userdata* ud = (filedialog_userdata*)userdata;

  if (*filelist)
  {
    *ud->pstr = *filelist;
  }
  SDL_SignalSemaphore(ud->sem);
}

static bool do_event_loop()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT)
      return true;
    else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(thewindow))
      return true;
    else if (event.type == SDL_EVENT_DROP_FILE && event.drop.windowID == SDL_GetWindowID(thewindow))
      LoadFile(event.drop.data);
    else if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == SDL_GetWindowID(thewindow))
    {
      rtWidth  = event.window.data1;
      rtHeight = event.window.data2;
    }
  }

  return false;
}

std::string platform_open_file_dialog(const char* title, int nfilters, const file_dialog_filter* filters, const char* def_out)
{
  std::string retval;
  filedialog_userdata ud;

  if (nfilters < 0)
    return retval;

  ud.pstr = &retval;
  ud.sem = SDL_CreateSemaphore(0);

  SDL_DialogFileFilter* sdlflt = new SDL_DialogFileFilter[nfilters];
  for (int i = 0; i < nfilters; ++i)
  {
    sdlflt[i].name = filters[i].name;
    sdlflt[i].pattern = strcmp(filters[i].pattern, "*") ? (filters[i].pattern + 2) : "*";
  }
  SDL_ShowOpenFileDialog(open_file_cb, &ud, thewindow, sdlflt, nfilters, def_out, true);

  while (!SDL_TryWaitSemaphore(ud.sem))
  {
    if (do_event_loop())
    {
      ImGui_ImplOpenGL3_Shutdown();
      ImGui_ImplSDL3_Shutdown();
      ImGui::DestroyContext();

      SDL_DestroyWindow(thewindow);
      SDL_Quit();
      exit(0);
    }
  }

  SDL_DestroySemaphore(ud.sem);
  delete[] sdlflt;

  return retval;
}

int main(int argc, char** argv)
{
  if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
  {
    printf("Usage: %s [<file.kkp>]\n", argv[0]);
    return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
  {
      printf("Error: SDL_Init(): %s\n", SDL_GetError());
      return 1;
  }

#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char* glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  // GL 3.2 Core + GLSL 150
  const char* glsl_version = "#version 150";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  //SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
  rtWidth = 1280; rtHeight = 720;

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
  SDL_Window* window = SDL_CreateWindow("Conspiracy KKP Analyzer", rtWidth, rtHeight, window_flags);
  if (window == nullptr)
  {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return 1;
  }
  thewindow = window;
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1); // Enable vsync
  SDL_ShowWindow(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  InitStuff(argc, argv);

  ImVec4 clear_color = ImVec4(0, 0, 0, 1.00f);

  while (true)
  {
    if (do_event_loop())
      break;

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    RenderFrame(rtWidth, rtHeight);

    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DestroyContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

int fopen_s(FILE **streamptr,
    const char *filename, const char *mode)
{
  FILE* r = fopen(filename, mode);
  if (r)
  {
    *streamptr = r;
    return 0;
  }

  return -EIO;
}

size_t fread_s(void *buffer, size_t bufferSize, size_t elementSize,
    size_t count, FILE *stream)
{
  size_t bytes;

  if (__builtin_umull_overflow(elementSize, count, &bytes))
  {
    errno = -EINVAL;
    return 0;
  }

  if (bytes > bufferSize)
  {
    size_t diff = bytes - bufferSize;
    count -= (diff + elementSize - 1) / elementSize;
  }

  return fread(buffer, elementSize, count, stream);
}

