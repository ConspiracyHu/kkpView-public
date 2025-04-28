project('kkpview', 'c', 'cpp')

srcs = [
  'kkp.cpp',
  'main.cpp',
  'ImGui/imgui.cpp',
  'ImGui/imgui_demo.cpp',
  'ImGui/imgui_draw.cpp',
  'ImGui/imgui_tables.cpp',
  'ImGui/imgui_widgets.cpp',

  'zydis/src/Disassembler.c',
  'zydis/src/Encoder.c',
  'zydis/src/EncoderData.c',
  'zydis/src/FormatterATT.c',
  'zydis/src/FormatterBase.c',
  'zydis/src/FormatterBuffer.c',
  'zydis/src/FormatterIntel.c',
  'zydis/src/MetaInfo.c',
  'zydis/src/Mnemonic.c',
  'zydis/src/Register.c',
  'zydis/src/Segment.c',
  'zydis/src/SharedData.c',
  'zydis/src/String.c',
  'zydis/src/Utils.c',
  'zydis/src/Zydis.c',
  'zydis/src/Decoder.c',
  'zydis/src/DecoderData.c',
  'zydis/src/Formatter.c',
]
deps = []
incdirs = include_directories('./', './ImGui/', './ImGui/backends/',
  './zydis/dependencies/zycore/include/', './zydis/include/', './zydis/src/')


sdl3_dep = dependency('sdl3')

if build_machine.system() == 'windows'
  srcs += [
    'platform/dx11.cpp',
    'ImGui/backends/imgui_impl_opengl3.cpp',
    'ImGui/backends/imgui_impl_sdl3.cpp'
  ]

  executable('kkpview_win32', srcs,
    include_directories: incdirs,
    win_subsystem: 'windows',
    dependencies: deps)
endif

if sdl3_dep.found()
  gl_dep = dependency('gl')

  srcs += [
    'platform/sdl3.cpp',
    'ImGui/backends/imgui_impl_opengl3.cpp',
    'ImGui/backends/imgui_impl_sdl3.cpp'
  ]
  deps += [sdl3_dep, gl_dep]

  executable('kkpview_sdl3', srcs,
    include_directories: incdirs,
    win_subsystem: 'windows',
    dependencies: deps)
endif

