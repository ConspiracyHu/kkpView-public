#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "kkp.h"
#include <unordered_map>
#include "profont.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D( HWND hWnd );
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// crinkler color scheme
/*
ImU32 heatMap[]
{
    0x80ffffff, // 0.0
    0xff80ff80, // 0.1
    0xff00ff00, // 0.5
    0xff00c000, // 1.0
    0xff008040, // 2.0
    0xff006090, // 3.0
    0xff0020f0, // 5.0
    0xff0000a0, // 7.0
    0xff000000, // 9.0
    0xff900000, // 12.0
    0xffff0000, // 1000000.0
};

ImU32 GetRatioColor(double ratio)
{
    if (ratio == 0 || isinf(ratio) || isnan(ratio))
        return 0xff808080;

    double bits = ratio * 8;
    if (bits < 0.1)
        return heatMap[1];
    if (bits < 0.5)
        return heatMap[2];
    if (bits < 1)
        return heatMap[3];
    if (bits < 2)
        return heatMap[4];
    if (bits < 3)
        return heatMap[5];
    if (bits < 5)
        return heatMap[6];
    if (bits < 7)
        return heatMap[7];
    if (bits < 9)
        return heatMap[8];
    if (bits < 12)
        return heatMap[9];
    if (bits <= 16)
        return heatMap[10];
    return heatMap[10];
}
*/

ImU32 GetCompressionColorGradient( int t )
{
  float tval = max( 0, min( 1, ( t / 9.0f ) ) );

  tval = powf( tval, 1.5f );

  int count = 5;
  // https://www.color-hex.com/color-palette/27541
  ImU32 colors[] = { ImU32( 0xff8f5624 ), // CColor( 0xff1d4877 ),
                      ImU32( 0xff679c1a ), // CColor( 0xff1b8a5a ),
                      ImU32( 0xff21b0fb ),
                      ImU32( 0xff3888f6 ),
                      ImU32( 0xff323eee ) };

  int start = (int)( tval * ( count - 1 ) );
  float interpolation = tval * ( count - 1 ) - start;
  if ( start == count - 1 )
    return colors[ count - 1 ];

  ImU32 result;
  ImU32 colA = colors[ start ];
  ImU32 colB = colors[ start + 1 ];

  for ( int x = 0; x < 4; x++ )
  {
    unsigned char a = ( (unsigned char*)&colA )[ x ];
    unsigned char b = ( (unsigned char*)&colB )[ x ];
    ( (unsigned char*)&result )[ x ] = (unsigned char)( ( b - a ) * interpolation + a );
  }

  return result;
}

ImU32 GetRatioColor( double ratio )
{
  if ( ratio == 0 || isinf( ratio ) || isnan( ratio ) )
    return 0xff808080;

  double bits = ratio * 8;
  if ( bits < 0.1 )
    return GetCompressionColorGradient( 0 );// colors[ 0 ];
  if ( bits < 0.5 )
    return GetCompressionColorGradient( 1 ); // colors[ 1 ];
  if ( bits < 1 )
    return GetCompressionColorGradient( 2 ); // colors[ 2 ];
  if ( bits < 2 )
    return GetCompressionColorGradient( 3 ); // colors[ 3 ];
  if ( bits < 3 )
    return GetCompressionColorGradient( 4 ); // colors[ 4 ];
  if ( bits < 5 )
    return GetCompressionColorGradient( 5 ); // colors[ 5 ];
  if ( bits < 7 )
    return GetCompressionColorGradient( 6 ); // colors[ 6 ];
  if ( bits < 9 )
    return GetCompressionColorGradient( 7 ); // colors[ 7 ];
  if ( bits < 12 )
    return GetCompressionColorGradient( 8 ); // colors[ 8 ];
  if ( bits <= 16 )
    return GetCompressionColorGradient( 9 ); // colors[ 9 ];
  return 0xfffd00fd;
}

struct SourceLine
{
  int index = 0;
  double ratio = 0;
  std::string text;
};

int openedSource = -1;
bool sourceChanged = false;
int selectedSourceLine = 0;
std::vector<SourceLine> sourceCode;
std::string sourcePdbRoot;
std::string sourceLocalRoot;

bool ReadLine( FILE* f, std::string& line )
{
  int ch;
  line = "";

  while ( ( ch = fgetc( f ) ) != EOF && ch != '\n' )
    line += (char)ch;

  if ( !line.empty() || ch != EOF )
    return true;

  return false;
}

void OpenSourceFile( int fileIndex, bool force = false )
{
  if ( fileIndex == openedSource && !force )
  {
    return;
  }

  openedSource = fileIndex;
  sourceCode.clear();

  if ( fileIndex == 0 ) // <no source>
  {
    return;
  }

  if ( fileIndex < 0 || !kkp.files[ fileIndex ].name.size() )
  {
    SourceLine line = { 0, 0, "No source code associated with symbol" };
    sourceCode.emplace_back( line );
    return;
  }

  FILE* f = nullptr;
  if ( kkp.files.size() <= fileIndex )
  {
    SourceLine line = { 0, 1.1, kkp.files[ fileIndex ].name.data() };
    sourceCode.emplace_back( line );
    return;
  }

  std::string filepath = kkp.files[ fileIndex ].name;
  if ( fopen_s( &f, filepath.data(), "rt" ) )
  {
    if ( filepath.find( sourcePdbRoot, 0 ) == 0 )
    {
      filepath = sourceLocalRoot + filepath.substr( sourcePdbRoot.length() );
      if ( fopen_s( &f, filepath.data(), "rt" ) )
      {
        SourceLine line = { 0, 1.1, kkp.files[ fileIndex ].name.data() };
        sourceCode.emplace_back( line );
        return;
      }
    }
  }

  struct LineSizeInfo
  {
    double packed = 0;
    int unpacked = 0;
  };

  std::unordered_map<int, LineSizeInfo> lineSizes;
  for ( auto& byte : kkp.bytes )
  {
    if ( byte.file == fileIndex )
    {
      lineSizes[ byte.line ].packed += byte.packed;
      lineSizes[ byte.line ].unpacked++;
    }
  }

  std::string line;
  int lineCount = 1;
  while ( ReadLine( f, line ) )
  {
    SourceLine srcLine;
    auto lineData = lineSizes.find( lineCount );

    srcLine.ratio = 0;
    if ( lineData != lineSizes.end() )
      srcLine.ratio = lineData->second.packed / lineData->second.unpacked;
    srcLine.index = lineCount++;
    srcLine.text = line;

    sourceCode.emplace_back( srcLine );
  }

  fclose( f );
}

enum class HexHighlightMode
{
  None,
  Symbol,
  Line
};

HexHighlightMode hexHighlightMode = HexHighlightMode::None;

int hexViewPositionChanged = false;
int targetHexViewPosition = 0;
int hexHighlightSymbol = -1;
int hexHighlightSource = -1;
int hexHighlightLine = -1;
int hexMouseSymbol = -1;
bool symbolSelectionChanged = false;
int newlySelectedSymbolID = -1;

void DrawHighlight( ImVec2 topLeft, ImVec2 bottomRight, bool top, bool left, bool bottom, bool right )
{
  ImU32 borderColor = 0xffb0b0b0;

  ImGui::GetWindowDrawList()->AddRectFilled( topLeft, bottomRight, 0xff505050 );
  if ( top )
    ImGui::GetWindowDrawList()->AddRectFilled( topLeft, ImVec2( bottomRight.x, topLeft.y + 1 ), borderColor );
  if ( left )
    ImGui::GetWindowDrawList()->AddRectFilled( topLeft, ImVec2( topLeft.x + 1, bottomRight.y ), borderColor );
  if ( bottom )
    ImGui::GetWindowDrawList()->AddRectFilled( ImVec2( topLeft.x, bottomRight.y - 1 ), bottomRight, borderColor );
  if ( right )
    ImGui::GetWindowDrawList()->AddRectFilled( ImVec2( bottomRight.x - 1, topLeft.y ), bottomRight, borderColor );
}

void DrawHexView()
{
  bool mouseSymbolFound = false;

  float hexCharWidth = ImGui::CalcTextSize( "FF" ).x + ImGui::GetStyle().ItemSpacing.x / 2;
  float lineHeight = ImGui::GetTextLineHeightWithSpacing();
  float asciiCharWidth = ImGui::CalcTextSize( "M" ).x;
  float groupSpacing = ImGui::CalcTextSize( " " ).x;
  float windowWidth = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize( "00000000" ).x - ImGui::GetStyle().ScrollbarSize - ImGui::GetStyle().ItemSpacing.x * 2;

  int bytesPerGroup = 8;

  float groupWidth = hexCharWidth * bytesPerGroup + groupSpacing + asciiCharWidth * bytesPerGroup;

  int groupsPerRow = (int)max( 1, windowWidth / groupWidth );
  int bytesPerRow = bytesPerGroup * groupsPerRow;

  ImVec2 mousePos = ImGui::GetIO().MousePos;
  auto minRect = ImGui::GetWindowContentRegionMin();
  auto maxRect = ImGui::GetWindowContentRegionMax();
  bool mouseOver = mousePos.x >= minRect.x && mousePos.y >= minRect.y && mousePos.x < maxRect.x && mousePos.y < maxRect.y;

  if ( ImGui::BeginTable( "source", 3, ImGuiTableFlags_ScrollY ) )
  {
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize( "00000000" ).x );
    ImGui::TableSetupColumn( "Hex", ImGuiTableColumnFlags_WidthFixed, ( groupWidth - asciiCharWidth * bytesPerGroup ) * groupsPerRow - groupSpacing );
    ImGui::TableSetupColumn( "ASCII", ImGuiTableColumnFlags_WidthFixed, bytesPerRow * asciiCharWidth );

    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin( (int)( ceil( kkp.bytes.size() / (double)bytesPerRow ) ) );

    if ( hexViewPositionChanged )
    {
      int row = targetHexViewPosition / bytesPerRow;
      ImGui::SetScrollY( ImGui::GetTextLineHeightWithSpacing() * row - ImGui::GetWindowHeight() / 2 );
      hexViewPositionChanged = false;
    }

    std::string asciiDisplay;

    while ( clipper.Step() )
    {
      int startByte = (int)min( clipper.DisplayStart * bytesPerRow, kkp.bytes.size() );
      int endByte = (int)min( clipper.DisplayEnd * bytesPerRow, kkp.bytes.size() );

      for ( int i = startByte; i < endByte; i++ )
      {
        if ( ( i + 1 ) % bytesPerRow == 0 || i == kkp.bytes.size() - 1 )
        {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex( 0 );
          ImGui::Text( "%08x", i - ( i % bytesPerRow ) ); // Offset
          ImGui::TableSetColumnIndex( 1 );
          ImVec2 pos = ImGui::GetCursorScreenPos();

          int start = (int)( i / bytesPerRow ) * bytesPerRow;

          for ( int x = 0; x < bytesPerRow; x++ )
          {
            int bytePos = start + x;

            if ( bytePos >= kkp.bytes.size() )
              continue;

            auto& byte = kkp.bytes[ bytePos ];

            float width = hexCharWidth;
            if ( x % bytesPerGroup == bytesPerGroup - 1 )
              width += groupSpacing;

            if ( mousePos.x >= pos.x && mousePos.y >= pos.y && mousePos.x < pos.x + width && mousePos.y < pos.y + lineHeight )
            {
              int symbolIndex = -1;
              for ( int y = 0; y < kkp.sortableSymbols.size(); y++ )
              {
                if ( kkp.sortableSymbols[ y ].originalSymbolID == byte.symbol && byte.symbol >= 0 )
                {
                  symbolIndex = y;
                  break;
                }
              }

              ImGui::BeginTooltip();
              ImGui::Text( "%08x %s", bytePos, symbolIndex >= 0 && kkp.sortableSymbols[ symbolIndex ].name.size() ? kkp.sortableSymbols[ symbolIndex ].name.data() : "<no symbol>" );
              ImGui::EndTooltip();

              hexMouseSymbol = byte.symbol;
              mouseSymbolFound = true;

              if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
              {
                ImGui::SetWindowFocus();
                for ( auto& sym : kkp.sortableSymbols )
                  sym.selected = sym.originalSymbolID == byte.symbol && byte.symbol >= 0;
                OpenSourceFile( byte.file );
                selectedSourceLine = byte.line;
                sourceChanged = true;
                hexHighlightMode = HexHighlightMode::Symbol;
                hexHighlightSymbol = byte.symbol;
                symbolSelectionChanged = true;
                newlySelectedSymbolID = byte.symbol;
              }
            }

            if ( hexHighlightMode == HexHighlightMode::Symbol && byte.symbol == hexHighlightSymbol )
            {
              bool top = bytePos < bytesPerRow || kkp.bytes[ bytePos - bytesPerRow ].symbol != hexHighlightSymbol;
              bool left = !( bytePos % bytesPerRow ) || ( bytePos && kkp.bytes[ bytePos - 1 ].symbol != hexHighlightSymbol );
              int bottomTarget = bytePos + bytesPerRow + 1;
              bool bottom = ( bytePos > kkp.bytes.size() - bytesPerRow - 2 ) || ( bottomTarget < kkp.bytes.size() && kkp.bytes[ bottomTarget ].symbol != hexHighlightSymbol );
              bool right = ( bytePos % bytesPerRow == bytesPerRow - 1 ) || bytePos == kkp.bytes.size() - 1 || ( bytePos < kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].symbol != hexHighlightSymbol );

              float w = hexCharWidth;
              if ( right )
                w -= asciiCharWidth;
              if ( !right && x % bytesPerGroup == bytesPerGroup - 1 )
                w += groupSpacing;

              ImVec2 topLeft = ImVec2( pos.x - 2, pos.y - 1 );
              ImVec2 bottomRight = ImVec2( pos.x + w + 2, pos.y + lineHeight );
              DrawHighlight( topLeft, bottomRight, top, left, bottom, right );
            }

            if ( mouseOver && byte.symbol == hexMouseSymbol && ( hexHighlightMode != HexHighlightMode::Symbol || hexMouseSymbol != hexHighlightSymbol ) )
            {
              float w = hexCharWidth;
              if ( x % bytesPerGroup == bytesPerGroup - 1 )
                w += groupSpacing;

              if ( x % bytesPerRow == bytesPerRow - 1 )
                w -= groupSpacing + 3;

              ImVec2 topLeft = ImVec2( pos.x, pos.y );
              ImVec2 bottomRight = ImVec2( pos.x + w, pos.y + lineHeight );
              ImGui::GetWindowDrawList()->AddRectFilled( topLeft, bottomRight, 0x40ffffff );
            }

            if ( hexHighlightMode == HexHighlightMode::Line && byte.line == hexHighlightLine && byte.file == hexHighlightSource )
            {
              bool top = bytePos < bytesPerRow || kkp.bytes[ bytePos - bytesPerRow ].line != hexHighlightLine || kkp.bytes[ bytePos - bytesPerRow ].file != hexHighlightSource;
              bool left = !( bytePos % bytesPerRow ) || ( bytePos && kkp.bytes[ bytePos - 1 ].line != hexHighlightLine ) || ( bytePos && kkp.bytes[ bytePos - 1 ].file != hexHighlightSource );
              bool bottom = ( bytePos > ( kkp.bytes.size() - bytesPerRow ) ) || kkp.bytes[ bytePos + bytesPerRow + 1 ].line != hexHighlightLine || kkp.bytes[ bytePos + bytesPerRow + 1 ].file != hexHighlightSource;
              bool right = ( bytePos % bytesPerRow == bytesPerRow - 1 ) || ( bytePos == kkp.bytes.size() - 1 ) || ( bytePos < kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].line != hexHighlightLine ) || ( bytePos < kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].file != hexHighlightSource );

              float w = hexCharWidth;
              if ( right )
                w -= asciiCharWidth;
              if ( !right && x % bytesPerGroup == bytesPerGroup - 1 )
                w += groupSpacing;

              ImVec2 topLeft = ImVec2( pos.x - 2, pos.y - 1 );
              ImVec2 bottomRight = ImVec2( pos.x + w + 2, pos.y + lineHeight );
              DrawHighlight( topLeft, bottomRight, top, left, bottom, right );
            }

            char hex[ 4 ];
            sprintf_s( hex, "%02X ", byte.data );
            ImGui::GetWindowDrawList()->AddText( pos, GetRatioColor( byte.packed ), hex, hex + 3 );
            pos.x += hexCharWidth;
            if ( x % bytesPerGroup == bytesPerGroup - 1 )
              pos.x += groupSpacing;
          }

          ImGui::TableSetColumnIndex( 2 );

          pos = ImGui::GetCursorScreenPos();
          for ( int x = 0; x < bytesPerRow; x++ )
          {
            int bytePos = start + x;

            if ( bytePos >= kkp.bytes.size() )
              continue;

            auto& byte = kkp.bytes[ bytePos ];

            if ( mousePos.x >= pos.x && mousePos.y >= pos.y && mousePos.x < pos.x + asciiCharWidth && mousePos.y < pos.y + lineHeight )
            {
              int symbolIndex = -1;
              for ( int y = 0; y < kkp.sortableSymbols.size(); y++ )
              {
                if ( kkp.sortableSymbols[ y ].originalSymbolID == byte.symbol && byte.symbol >= 0 )
                {
                  symbolIndex = y;
                  break;
                }
              }

              ImGui::BeginTooltip();
              ImGui::Text( "%08x %s", bytePos, symbolIndex >= 0 && kkp.sortableSymbols[ symbolIndex ].name.size() ? kkp.sortableSymbols[ symbolIndex ].name.data() : "<no symbol>" );
              ImGui::EndTooltip();

              hexMouseSymbol = byte.symbol;
              mouseSymbolFound = true;

              if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
              {
                ImGui::SetWindowFocus();
                for ( auto& sym : kkp.sortableSymbols )
                  sym.selected = sym.originalSymbolID == byte.symbol && byte.symbol >= 0;
                OpenSourceFile( byte.file );
                selectedSourceLine = byte.line;
                sourceChanged = true;
                hexHighlightMode = HexHighlightMode::Symbol;
                hexHighlightSymbol = byte.symbol;
                symbolSelectionChanged = true;
                newlySelectedSymbolID = byte.symbol;
              }
            }

            if ( mouseOver && byte.symbol == hexMouseSymbol && ( hexHighlightMode != HexHighlightMode::Symbol || hexMouseSymbol != hexHighlightSymbol ) )
            {
              ImVec2 topLeft = ImVec2( pos.x, pos.y );
              ImVec2 bottomRight = ImVec2( pos.x + asciiCharWidth, pos.y + lineHeight );

              ImGui::GetWindowDrawList()->AddRectFilled( topLeft, bottomRight, 0x40ffffff );
            }

            if ( hexHighlightMode == HexHighlightMode::Symbol && byte.symbol == hexHighlightSymbol )
            {
              bool top = bytePos < bytesPerRow || kkp.bytes[ bytePos - bytesPerRow ].symbol != hexHighlightSymbol;
              bool left = !( bytePos % bytesPerRow ) || ( bytePos && kkp.bytes[ bytePos - 1 ].symbol != hexHighlightSymbol );
              int bottomTarget = bytePos + bytesPerRow + 1;
              bool bottom = ( bytePos > kkp.bytes.size() - bytesPerRow - 1 ) || ( bottomTarget < kkp.bytes.size() && kkp.bytes[ bottomTarget ].symbol != hexHighlightSymbol );
              bool right = ( bytePos % bytesPerRow == bytesPerRow - 1 ) || bytePos == kkp.bytes.size() - 1 || ( bytePos < kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].symbol != hexHighlightSymbol );

              float width = hexCharWidth;
              if ( right )
                width -= asciiCharWidth;
              if ( !right && x % bytesPerGroup == bytesPerGroup - 1 )
                width += groupSpacing;

              ImVec2 topLeft = ImVec2( pos.x - 1, pos.y - 1 );
              ImVec2 bottomRight = ImVec2( pos.x + asciiCharWidth, pos.y + lineHeight );

              DrawHighlight( topLeft, bottomRight, top, left, bottom, right );
            }

            char ascii = byte.data && byte.data != '\t' && byte.data != '\r' && byte.data != '\n' ? byte.data : '.';

            ImGui::GetWindowDrawList()->AddText( pos, GetRatioColor( byte.packed ), &ascii, &ascii + 1 );
            pos.x += asciiCharWidth;
          }

          asciiDisplay.clear();
        }
      }
    }

    ImGui::EndTable();
  }

  if ( !mouseSymbolFound )
    hexMouseSymbol = -2;
}

void DrawCodeView()
{
  std::string source = "Source Files";
  if ( openedSource >= 0 )
    source = kkp.files[ openedSource ].name;

  if ( ImGui::SmallButton( source.data() ) )
    ImGui::OpenPopup( "SourceCodeSelector" );

  if ( sourceCode.size() <= 1 )
  {
    ImGui::SameLine();
    if ( ImGui::SmallButton( "Browse to file" ) )
    {
      char filepath[ 256 ] = { 0 };

      OPENFILENAMEA opf = { 0 };
      opf.lpstrFilter = "C++ files\0*.cpp\0\0";
      opf.lpstrFile = filepath;
      opf.nMaxFile = 256;
      opf.nMaxFileTitle = 50;
      opf.lpstrDefExt = "cpp";
      opf.Flags = ( OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON ) & ~OFN_ALLOWMULTISELECT;
      opf.lStructSize = sizeof( OPENFILENAME );
      opf.hInstance = GetModuleHandle( 0 );

      if ( GetOpenFileNameA( &opf ) )
      {
        size_t filepathlen = strlen( filepath );
        for ( size_t i = source.length(), j = filepathlen; i >= 0 && j >= 0; i--, j-- )
        {
          if ( source[ i ] != filepath[ j ] )
          {
            sourcePdbRoot = source.substr( 0, i + 1 );
            sourceLocalRoot = std::string( filepath ).substr( 0, j + 1 );
            OpenSourceFile( openedSource, true );
            break;
          }
        }
      }
    }
  }

  if ( ImGui::BeginPopup( "SourceCodeSelector" ) )
  {
    for ( int x = 0; x < kkp.files.size(); x++ )
    {
      if ( ImGui::MenuItem( kkp.files[ x ].name.data(), nullptr, nullptr, true ) )
        OpenSourceFile( x );
    }
    ImGui::EndPopup();
  }

  if ( ImGui::BeginTable( "source", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY ) )
  {
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableSetupColumn( "#", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Ratio", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Code", ImGuiTableColumnFlags_WidthStretch );

    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin( (int)sourceCode.size() );

    if ( sourceChanged )
      ImGui::SetScrollY( ImGui::GetTextLineHeightWithSpacing() * selectedSourceLine - ImGui::GetWindowHeight() / 2 );

    while ( clipper.Step() )
    {
      for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ )
      {
        auto& line = sourceCode[ row ];

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );

        char indexText[ 10 ]{};
        sprintf_s( indexText, "%d", line.index );

        ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
        if ( ImGui::Selectable( indexText, line.index == selectedSourceLine, selectableFlags ) )
        {
        }

        if ( ImGui::IsItemFocused() && selectedSourceLine != line.index )
        {
          selectedSourceLine = line.index;
          hexHighlightMode = HexHighlightMode::Line;
          hexHighlightLine = line.index;
          hexHighlightSource = openedSource;
          hexViewPositionChanged = true;

          for ( int x = 0; x < kkp.bytes.size(); x++ )
          {
            auto& byte = kkp.bytes[ x ];
            if ( byte.line == line.index && byte.file == openedSource )
            {
              targetHexViewPosition = x;
              break;
            }
          }
        }


        auto color = GetRatioColor( line.ratio );
        ImGui::PushStyleColor( ImGuiCol_Text, color );

        /*
                    ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    if (ImGui::Selectable(symbol.name.data(), symbol.selected, selectableFlags))
                    {
                        for (auto& sym : kkp.sortableSymbols)
                            sym.selected = false;
                        symbol.selected = true;
                    }
        */

        /*
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", line.index);
        */

        ImGui::TableSetColumnIndex( 1 );
        ImGui::Text( "%g", line.ratio );

        ImGui::TableSetColumnIndex( 2 );
        ImGui::Text( "%s", line.text.data() );
        ImGui::PopStyleColor();
      }
    }

    ImGui::EndTable();
  }

  sourceChanged = false;
}

const int numberOfColumns = 5;
static bool scopes = false;
static bool foldersOnTop = true;

void SetSymbolColumns( const KKP::KKPSymbol& symbol )
{
  double ratio = symbol.cumulativePackedSize / symbol.cumulativeUnpackedSize;
  if ( isnan( ratio ) || isinf( ratio ) )
    ratio = 0;

  ImGui::TableSetColumnIndex( 1 );
  ImGui::Text( "%d", symbol.sourcePos );

  ImGui::TableSetColumnIndex( 2 );
  ImGui::Text( "%d", symbol.cumulativeUnpackedSize );

  ImGui::TableSetColumnIndex( 3 );
  ImGui::Text( "%g", symbol.cumulativePackedSize );

  ImGui::TableSetColumnIndex( 4 );
  ImGui::Text( "%g", ratio );
}

void RecursiveClearSymbolSelected( KKP::KKPSymbol& symbol )
{
  symbol.selected = false;
  for ( auto& child : symbol.children )
    RecursiveClearSymbolSelected( child );
}

void DoSymbolSelection( KKP::KKPSymbol& symbol )
{
  if ( ImGui::IsItemFocused() && !symbol.selected )
  {
    RecursiveClearSymbolSelected( kkp.root );
    for ( auto& sym : kkp.sortableSymbols )
      sym.selected = false;
    symbol.selected = true;
    OpenSourceFile( symbol.fileID );
    auto& byte = kkp.bytes[ symbol.sourcePos ];
    selectedSourceLine = byte.line;
    sourceChanged = true;

    hexViewPositionChanged = true;
    targetHexViewPosition = symbol.sourcePos;

    hexHighlightMode = HexHighlightMode::Symbol;

    if ( symbol.unpackedSize )
      hexHighlightSymbol = byte.symbol;
    else
      hexHighlightSymbol = -2;
  }
}

void AddNonFolder( KKP::KKPSymbol& symbol, const ImVec2& tableTopLeft, int& rowIndex, const ImVec2& windowSize, const float rowHeight, const float scroll )
{
  ImGui::TableNextRow();

  float rowTop = rowIndex * rowHeight;
  float rowBottom = rowTop + rowHeight;
  bool rowVisible = ( rowTop < windowSize.y + scroll ) && ( rowBottom > scroll );
  rowIndex++;

  if ( rowVisible )
  {
    double ratio = symbol.cumulativePackedSize / symbol.cumulativeUnpackedSize;
    ImGui::PushStyleColor( ImGuiCol_Text, GetRatioColor( ratio ) );
    ImGui::TableSetColumnIndex( 0 );
    ImGui::Selectable( symbol.name.data(), symbol.selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap );
    DoSymbolSelection( symbol );
    SetSymbolColumns( symbol );
    ImGui::PopStyleColor();
  }
  else
  {
    ImGui::TableSetColumnIndex( 0 );
    ImGui::Text( "" );
  }
}

void AddSymbolRecursive( KKP::KKPSymbol& node, const ImVec2& tableTopLeft, int& rowIndex, const ImVec2& windowSize, const float rowHeight, const float scroll )
{
  // folders
  for ( auto& child : node.children )
  {
    if ( !child.children.size() )
    {
      if (!foldersOnTop )
        AddNonFolder( child, tableTopLeft, rowIndex, windowSize, rowHeight, scroll );
      continue;
    }

    ImGui::TableNextRow();

    float rowTop = rowIndex * rowHeight;
    float rowBottom = rowTop + rowHeight;
    bool rowVisible = ( rowTop < windowSize.y + scroll ) && ( rowBottom > scroll );
    rowIndex++;

    ImVec2 rowStart = ImGui::GetCursorPos();

    if ( rowVisible )
    {
      double ratio = child.cumulativePackedSize / child.cumulativeUnpackedSize;
      ImGui::PushStyleColor( ImGuiCol_Text, GetRatioColor( ratio ) );
    }

    ImGui::TableSetColumnIndex( 0 );
    bool folderOpen = ImGui::TreeNodeEx( child.name.data(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ( ImGuiTreeNodeFlags_Selected * child.selected ) );

    DoSymbolSelection( child );

    if ( rowVisible )
      SetSymbolColumns( child );

    if ( folderOpen )
    {
      AddSymbolRecursive( child, tableTopLeft, rowIndex, windowSize, rowHeight, scroll );
      ImGui::TreePop();
    }

    if ( rowVisible )
      ImGui::PopStyleColor();
  }

  if ( foldersOnTop )
  {
    // non-folders
    for ( auto& child : node.children )
    {
      if ( child.children.size() )
        continue;

      AddNonFolder( child, tableTopLeft, rowIndex, windowSize, rowHeight, scroll );
    }
  }
}

void DrawSymbolList()
{
  ImGui::Checkbox( "Enable namespace scopes", &scopes );
  if ( scopes )
  {
    ImGui::SameLine();
    ImGui::Checkbox( "Folders on top", &foldersOnTop );
  }

  if ( ImGui::BeginTable( "myTable", numberOfColumns, ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY ) )
  {
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableSetupColumn( "Symbol Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Unpacked Size", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Packed Size", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Pack Ratio", ImGuiTableColumnFlags_WidthStretch );

    ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();

    if ( sortSpecs && sortSpecs->SpecsCount > 0 )
    {
      for ( int n = 0; n < sortSpecs->SpecsCount; n++ )
      {
        const ImGuiTableColumnSortSpecs* sortSpec = &sortSpecs->Specs[ n ];
        kkp.Sort( sortSpec->ColumnIndex, sortSpec->SortDirection == ImGuiSortDirection_Descending );
      }
      sortSpecs->SpecsDirty = false;
    }

    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin( (int)kkp.sortableSymbols.size() );

    if ( symbolSelectionChanged )
    {
      for ( int x = 0; x < kkp.sortableSymbols.size(); x++ )
      {
        if ( kkp.sortableSymbols[ x ].originalSymbolID == newlySelectedSymbolID )
        {
          ImGui::SetScrollY( ImGui::GetTextLineHeightWithSpacing() * x - ImGui::GetWindowHeight() / 2 );
          break;
        }
      }
      symbolSelectionChanged = false;
    }


    if ( !scopes )
    {
      while ( clipper.Step() )
      {
        for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ )
        {
          auto& symbol = kkp.sortableSymbols[ row ];

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex( 0 );

          double ratio = symbol.cumulativePackedSize / symbol.cumulativeUnpackedSize;

          ImGui::PushStyleColor( ImGuiCol_Text, GetRatioColor( ratio ) );

          ImGui::Selectable( symbol.name.data(), symbol.selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap );
          DoSymbolSelection( symbol );

          SetSymbolColumns( symbol );

          ImGui::PopStyleColor();
        }
      }
    }
    else
    {
      ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );

      auto pos = ImGui::GetCursorScreenPos();
      ImVec2 windowSize = ImGui::GetWindowSize();
      auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
      float scroll = ImGui::GetScrollY();

      int index = 0;
      AddSymbolRecursive( kkp.root, pos, index, windowSize, rowHeight, scroll );

      ImGui::PopStyleVar();
    }

    ImGui::EndTable();
  }
}

INT WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow )
{
    //ImGui_ImplWin32_EnableDpiAwareness();
  WNDCLASSEXW wc = { sizeof( wc ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
  ::RegisterClassExW( &wc );
  HWND hwnd = ::CreateWindowW( wc.lpszClassName, L"Conspiracy KKP Analyzer", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr );

  if ( !CreateDeviceD3D( hwnd ) )
  {
    CleanupDeviceD3D();
    ::UnregisterClassW( wc.lpszClassName, wc.hInstance );
    return 1;
  }

  ::ShowWindow( hwnd, SW_SHOWMAXIMIZED );
  ::UpdateWindow( hwnd );

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  ImGui_ImplWin32_Init( hwnd );
  ImGui_ImplDX11_Init( g_pd3dDevice, g_pd3dDeviceContext );

  auto profont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF( profont_compressed_data, profont_compressed_size, 11.0f );
  //io.Fonts->AddFontFromFileTTF("ProfontPixelated.ttf", 11.0f);

  ImVec4 clear_color = ImVec4( 0, 0, 0, 1.00f );

  int rtWidth = 0;
  int rtHeight = 0;

  ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

  bool done = false;
  while ( !done )
  {
    MSG msg;
    while ( ::PeekMessage( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
    {
      ::TranslateMessage( &msg );
      ::DispatchMessage( &msg );
      if ( msg.message == WM_QUIT )
        done = true;
    }
    if ( done )
      break;

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
    ImGui::NewFrame();
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    ImGui::BeginMainMenuBar();
    ImVec2 menuSize = ImGui::GetWindowSize();

    if ( ImGui::BeginMenu( "File", true ) )
    {
      if ( ImGui::MenuItem( "Open kkp file", nullptr, nullptr, true ) )
      {
        OpenKKP();
        openedSource = -1;
        sourceCode.clear();
      }
      if ( ImGui::MenuItem( "Open sym file", nullptr, nullptr, true ) )
        OpenSYM();
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();

    static float f = 0.0f;
    static int counter = 0;

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::SetNextWindowPos( ImVec2( 0, menuSize.y ) );
    ImGui::SetNextWindowSize( ImVec2( (float)rtWidth, (float)( rtHeight - menuSize.y ) ) );

    ImGui::Begin( "kkpView", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar );

    ImVec2 windowSize = ImGui::GetContentRegionAvail();

    const float hSplitterPos = 0.5f;
    const float vSplitterPos = 0.5f;
    const float verticalSplitPixelPos = windowSize.x * vSplitterPos;
    const float horizontalSplitPixelPos = ( windowSize.y * hSplitterPos );

    ImGui::BeginChild( "left side", ImVec2( verticalSplitPixelPos, -1 ), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX );
    {
      // Upper part of the left area
      ImGui::BeginChild( "Hex View", ImVec2( -1, horizontalSplitPixelPos ), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY );
      DrawHexView();
      ImGui::EndChild();

      // Lower part of the left area
      ImGui::BeginChild( "Code View", ImVec2( -1, -1 ), ImGuiChildFlags_None );
      DrawCodeView();
      ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild( "Symbol List", ImVec2( -1, -1 ), ImGuiChildFlags_None );
    DrawSymbolList();
    ImGui::EndChild();

    ImGui::End();

    ImGui::Render();
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
