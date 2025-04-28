#define __STDC_WANT_LIB_EXT1__ /* fopen_s, fread_s, etc. */

#include <unordered_map>
#include <math.h>

#include "imgui.h"
#include "kkp.h"
#include "profont.h"
#include "version.h"
#include "main.h"
#include "platform.h"

#define ZYDIS_STATIC_BUILD
#include "zydis/include/Zydis/Zydis.h"


bool darkColors = true;

// crinkler color scheme
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

ImU32 GetCrinklerRatioColor( double ratio )
{
  if ( ratio == 0 || isinf( ratio ) || isnan( ratio ) )
    return 0xff808080;

  double bits = ratio * 8;
  if ( bits < 0.1 )
    return heatMap[ 1 ];
  if ( bits < 0.5 )
    return heatMap[ 2 ];
  if ( bits < 1 )
    return heatMap[ 3 ];
  if ( bits < 2 )
    return heatMap[ 4 ];
  if ( bits < 3 )
    return heatMap[ 5 ];
  if ( bits < 5 )
    return heatMap[ 6 ];
  if ( bits < 7 )
    return heatMap[ 7 ];
  if ( bits < 9 )
    return heatMap[ 8 ];
  if ( bits < 12 )
    return heatMap[ 9 ];
  if ( bits <= 16 )
    return heatMap[ 10 ];
  return heatMap[ 10 ];
}

ImU32 GetCompressionColorGradient( int t )
{
  float tval = std::max( 0.0f, std::min( 1.0f, ( t / 9.0f ) ) );

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
  if ( !darkColors )
    return GetCrinklerRatioColor( ratio );

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
  int unpackedSize = 0;
  double packedSize = 0;
  double ratio = 0;
  std::string text;
};

struct DisassemblyLine
{
  int address = 0;
  int sourceLine = 0;
  int unpackedSize = 0;
  double packedSize = 0;
  double ratio = 0;
  std::string text;
};

int openedSource = -1;
bool sourceChanged = false;
int selectedSourceLine = 0;
bool selectedSourceLineChanged = false;
std::vector<SourceLine> sourceCode;
std::vector<DisassemblyLine> disassembly;
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
    SourceLine line = { 0, 0, 0, 0, "No source code associated with symbol" };
    sourceCode.emplace_back( line );
    return;
  }

  FILE* f = nullptr;
  if ( (int)kkp.files.size() <= fileIndex )
  {
    SourceLine line = { 0, 0, 0, 1.1, kkp.files[ fileIndex ].name.data() };
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
        SourceLine line = { 0, 0, 0, 1.1, kkp.files[ fileIndex ].name.data() };
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
    {
      srcLine.unpackedSize += lineData->second.unpacked;
      srcLine.packedSize += lineData->second.packed;
      srcLine.ratio = lineData->second.packed / lineData->second.unpacked;
    }
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

void RecursiveClearSymbolSelected( KKP::KKPSymbol& symbol )
{
  symbol.selected = false;
  for ( auto& child : symbol.children )
    RecursiveClearSymbolSelected( child );
}

void RecursiveClearSymbolHotPath( KKP::KKPSymbol& symbol )
{
  symbol.onHotPath = false;
  for ( auto& child : symbol.children )
    RecursiveClearSymbolHotPath( child );
}

bool SetSymbolHotPath( KKP::KKPSymbol& symbol, int symbolID )
{
  if ( symbol.originalSymbolID == symbolID )
  {
    symbol.onHotPath = true;
    return true;
  }

  for ( auto& sym : symbol.children )
  {
    if ( SetSymbolHotPath( sym, symbolID ) )
    {
      sym.onHotPath = true;
      return true;
    }
  }

  return false;
}

void SelectByte( KKP::KKPByteData& byte )
{
  RecursiveClearSymbolSelected( kkp.root );
  for ( auto& sym : kkp.sortableSymbols )
    sym.selected = byte.symbol == sym.originalSymbolID && byte.symbol >= 0;

  RecursiveClearSymbolHotPath( kkp.root );
  SetSymbolHotPath( kkp.root, byte.symbol );

  OpenSourceFile( byte.file );
  selectedSourceLine = byte.line;
  selectedSourceLineChanged = true;
  sourceChanged = true;
  hexHighlightMode = HexHighlightMode::Symbol;
  hexHighlightSymbol = byte.symbol;
  symbolSelectionChanged = true;
  newlySelectedSymbolID = byte.symbol;

  disassembly.clear();

  for ( auto& symbol : kkp.sortableSymbols )
  {
    if ( symbol.originalSymbolID == byte.symbol && symbol.unpackedSize > 0 && symbol.isCode )
    {
      unsigned char* data = new unsigned char[ symbol.unpackedSize ];
      memset( data, 0, symbol.unpackedSize );
      for ( int x = 0; x < symbol.unpackedSize; x++ )
        data[ x ] = kkp.bytes[ symbol.sourcePos + x ].data;

      ZyanU64 address = symbol.sourcePos;

      ZyanUSize offset = 0;
      ZydisDisassembledInstruction instruction;
      while ( ZYAN_SUCCESS( ZydisDisassembleIntel( KKP::isX64 ? ZYDIS_MACHINE_MODE_LONG_64 : ZYDIS_MACHINE_MODE_LONG_COMPAT_32, address, data + offset, symbol.unpackedSize - offset, &instruction ) ) )
      {
        DisassemblyLine line;
        line.address = (int)address;
        line.unpackedSize = instruction.info.length;
        line.text = instruction.text;
        line.sourceLine = kkp.bytes[ symbol.sourcePos + offset ].line;

        for ( int x = 0; x < instruction.info.length; x++ )
          line.packedSize += kkp.bytes[ symbol.sourcePos + offset + x ].packed;
        line.ratio = line.packedSize / line.unpackedSize;

        offset += instruction.info.length;
        address += instruction.info.length;
        disassembly.emplace_back( line );
      }

      delete[] data;
      break;
    }
  }
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

  int groupsPerRow = (int)std::max( 1.0f, windowWidth / groupWidth );
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
      int startByte = (int)std::min( clipper.DisplayStart * bytesPerRow, (int)kkp.bytes.size() );
      int endByte = (int)std::min( clipper.DisplayEnd * bytesPerRow, (int)kkp.bytes.size() );

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

            if ( bytePos >= (int)kkp.bytes.size() )
              continue;

            auto& byte = kkp.bytes[ bytePos ];

            float width = hexCharWidth;
            if ( x % bytesPerGroup == bytesPerGroup - 1 )
              width += groupSpacing;

            if ( mousePos.x >= pos.x && mousePos.y >= pos.y && mousePos.x < pos.x + width && mousePos.y < pos.y + lineHeight )
            {
              int symbolIndex = -1;
              for ( int y = 0; y < (int)kkp.sortableSymbols.size(); y++ )
              {
                if ( kkp.sortableSymbols[ y ].originalSymbolID == byte.symbol && byte.symbol >= 0 )
                {
                  symbolIndex = y;
                  break;
                }
              }

              ImGui::BeginTooltip();
              ImGui::Text( "%08x (%g) %s", bytePos, byte.packed, symbolIndex >= 0 && kkp.sortableSymbols[ symbolIndex ].name.size() ? kkp.sortableSymbols[ symbolIndex ].name.data() : "<no symbol>" );
              ImGui::EndTooltip();

              hexMouseSymbol = byte.symbol;
              mouseSymbolFound = true;

              if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
              {
                ImGui::SetWindowFocus();
                for ( auto& sym : kkp.sortableSymbols )
                  sym.selected = sym.originalSymbolID == byte.symbol && byte.symbol >= 0;
                SelectByte( byte );
              }
            }

            if ( hexHighlightMode == HexHighlightMode::Symbol && byte.symbol == hexHighlightSymbol )
            {
              bool top = bytePos < bytesPerRow || kkp.bytes[ bytePos - bytesPerRow ].symbol != hexHighlightSymbol;
              bool left = !( bytePos % bytesPerRow ) || ( bytePos && kkp.bytes[ bytePos - 1 ].symbol != hexHighlightSymbol );
              int bottomTarget = bytePos + bytesPerRow + 1;
              bool bottom = ( bytePos > (int)kkp.bytes.size() - bytesPerRow - 2 ) || ( bottomTarget < (int)kkp.bytes.size() && kkp.bytes[ bottomTarget ].symbol != hexHighlightSymbol );
              bool right = ( bytePos % bytesPerRow == bytesPerRow - 1 ) || bytePos == kkp.bytes.size() - 1 || ( bytePos < (int)kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].symbol != hexHighlightSymbol );

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
              bool bottom = ( bytePos > ( (int)kkp.bytes.size() - bytesPerRow ) ) || kkp.bytes[ bytePos + bytesPerRow + 1 ].line != hexHighlightLine || kkp.bytes[ bytePos + bytesPerRow + 1 ].file != hexHighlightSource;
              bool right = ( bytePos % bytesPerRow == bytesPerRow - 1 ) || ( bytePos == kkp.bytes.size() - 1 ) || ( bytePos < (int)kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].line != hexHighlightLine ) || ( bytePos < (int)kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].file != hexHighlightSource );

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
            sprintf_s( hex, 4, "%02X ", byte.data );
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

            if ( bytePos >= (int)kkp.bytes.size() )
              continue;

            auto& byte = kkp.bytes[ bytePos ];

            if ( mousePos.x >= pos.x && mousePos.y >= pos.y && mousePos.x < pos.x + asciiCharWidth && mousePos.y < pos.y + lineHeight )
            {
              int symbolIndex = -1;
              for ( int y = 0; y < (int)kkp.sortableSymbols.size(); y++ )
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
                SelectByte( byte );
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
              bool bottom = ( bytePos > (int)kkp.bytes.size() - bytesPerRow - 1 ) || ( bottomTarget < (int)kkp.bytes.size() && kkp.bytes[ bottomTarget ].symbol != hexHighlightSymbol );
              bool right = ( bytePos % bytesPerRow == bytesPerRow - 1 ) || bytePos == kkp.bytes.size() - 1 || ( bytePos < (int)kkp.bytes.size() - 1 && kkp.bytes[ bytePos + 1 ].symbol != hexHighlightSymbol );

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
  if ( openedSource >= 0 && kkp.files.size() > openedSource && !kkp.files[ openedSource ].name.empty() )
    source = kkp.files[ openedSource ].name;

  if ( ImGui::SmallButton( source.data() ) )
    ImGui::OpenPopup( "SourceCodeSelector" );

  if ( sourceCode.size() <= 1 )
  {
    ImGui::SameLine();
    if ( ImGui::SmallButton( "Browse to file" ) )
    {
      file_dialog_filter flt[ 2 ] = {
        { "C++ files", "*.cpp" },
        { "C files", "*.c" },
      };
      std::string filepath = platform_open_file_dialog( "Choose C/C++ source to read...", 2, flt, NULL );

      if ( !filepath.empty() )
      {
        size_t filepathlen = filepath.size();
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
    for ( int x = 0; x < (int)kkp.files.size(); x++ )
    {
      if ( ImGui::MenuItem( kkp.files[ x ].name.data(), nullptr, nullptr, true ) )
        OpenSourceFile( x );
    }
    ImGui::EndPopup();
  }

  if ( ImGui::BeginTable( "source", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY ) )
  {
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableSetupColumn( "#", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Unpacked", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Packed", ImGuiTableColumnFlags_WidthStretch );
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
        sprintf_s( indexText, 10, "%d", line.index );

        ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
        if ( ImGui::Selectable( indexText, line.index == selectedSourceLine, selectableFlags ) )
        {
        }

        if ( ImGui::IsItemFocused() && selectedSourceLine != line.index )
        {
          for ( int x = 0; x < (int)kkp.bytes.size(); x++ )
          {
            auto& byte = kkp.bytes[ x ];
            if ( byte.line == line.index && byte.file == openedSource )
            {
              SelectByte( kkp.bytes[ x ] );
              break;
            }
          }

          selectedSourceLine = line.index;
          selectedSourceLineChanged = true;
          hexHighlightMode = HexHighlightMode::Line;
          hexHighlightLine = line.index;
          hexHighlightSource = openedSource;
          hexViewPositionChanged = true;
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
        ImGui::Text( "%d", line.unpackedSize );

        ImGui::TableSetColumnIndex( 2 );
        ImGui::Text( "%g", line.packedSize );

        ImGui::TableSetColumnIndex( 3 );
        ImGui::Text( "%g", line.ratio );

        ImGui::TableSetColumnIndex( 4 );
        ImGui::Text( "%s", line.text.data() );
        ImGui::PopStyleColor();
      }
    }

    ImGui::EndTable();
  }

  sourceChanged = false;
}

void DrawDisassemblyView()
{
  if ( ImGui::BeginTable( "disassembly", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY ) )
  {
    ImGui::TableSetupScrollFreeze( 0, 1 );
    ImGui::TableSetupColumn( "#", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Unpacked", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Packed", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Ratio", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "Code", ImGuiTableColumnFlags_WidthStretch );

    ImGui::TableHeadersRow();

    if ( openedSource < 0 || openedSource >= kkp.files.size() || !kkp.files[ openedSource ].name.size() )
    {
      ImGui::EndTable();
      return;
    }

    ImGuiListClipper clipper;
    clipper.Begin( (int)disassembly.size() );

    if ( selectedSourceLineChanged )
    {
      for ( int x = 0; x < (int)disassembly.size(); x++ )
      {
        if ( disassembly[ x ].sourceLine == selectedSourceLine )
        {
          ImGui::SetScrollY( ImGui::GetTextLineHeightWithSpacing() * x - ImGui::GetWindowHeight() / 2 );
          break;
        }
      }
      selectedSourceLineChanged = false;
    }

    while ( clipper.Step() )
    {
      for ( int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ )
      {
        auto& line = disassembly[ row ];

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );

        char indexText[ 10 ]{};
        sprintf_s( indexText, 10, "0x%x", line.address );

        ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
        if ( ImGui::Selectable( indexText, line.sourceLine == selectedSourceLine, selectableFlags ) )
        {
        }

        if ( ImGui::IsItemFocused() && selectedSourceLine != line.address )
        {
          selectedSourceLine = line.address;
          hexHighlightMode = HexHighlightMode::Line;
          hexHighlightLine = line.address;
          hexHighlightSource = openedSource;
          hexViewPositionChanged = true;

          for ( int x = 0; x < (int)kkp.bytes.size(); x++ )
          {
            auto& byte = kkp.bytes[ x ];
            if ( byte.line == line.address && byte.file == openedSource )
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
        ImGui::Text( "%d", line.unpackedSize );

        ImGui::TableSetColumnIndex( 2 );
        ImGui::Text( "%g", line.packedSize );

        ImGui::TableSetColumnIndex( 3 );
        ImGui::Text( "%g", line.ratio );

        ImGui::TableSetColumnIndex( 4 );
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

void ProcessSymbolClick( KKP::KKPSymbol& symbol )
{
  if ( ImGui::IsItemFocused() && !symbol.selected )
  {
    if ( !symbol.children.size() )
    {
      auto& byte = kkp.bytes[ std::max( size_t( 0 ), std::min( static_cast<size_t>( kkp.bytes.size() ) - 1, static_cast<size_t>( symbol.sourcePos ) ) ) ];

      SelectByte( byte );

      symbol.selected = true;
      symbolSelectionChanged = false;
      hexViewPositionChanged = true;
      targetHexViewPosition = symbol.sourcePos;
      if ( symbol.unpackedSize )
        hexHighlightSymbol = byte.symbol;
      else
        hexHighlightSymbol = -2;
    }
    else
    {
      RecursiveClearSymbolSelected( kkp.root );
      symbol.selected = true;
    }
  }
}

void AddNonFolder( KKP::KKPSymbol& symbol, const ImVec2& tableTopLeft, int& rowIndex, const ImVec2& windowSize, const float rowHeight, const float scroll )
{
  if ( symbolSelectionChanged && symbol.originalSymbolID == newlySelectedSymbolID )
  {
    symbol.selected = true;
    ImGui::SetScrollHereY();
    symbolSelectionChanged = false;
  }

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
    ProcessSymbolClick( symbol );
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
      if ( !foldersOnTop )
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

    if ( child.onHotPath )
    {
      ImGui::SetNextItemOpen( true );
      child.onHotPath = false;
    }

    bool folderOpen = ImGui::TreeNodeEx( child.name.data(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected * child.selected );

    ProcessSymbolClick( child );

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
      if ( !scopes )
      {
        for ( int x = 0; x < (int)kkp.sortableSymbols.size(); x++ )
        {
          if ( kkp.sortableSymbols[ x ].originalSymbolID == newlySelectedSymbolID )
          {
            ImGui::SetScrollY( ImGui::GetTextLineHeightWithSpacing() * x - ImGui::GetWindowHeight() / 2 );
            break;
          }
        }
        symbolSelectionChanged = false;
      }
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
          ProcessSymbolClick( symbol );

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

void LoadFile( const char* filePath )
{
  const char* ext = strrchr( filePath, '.' );
  if ( !ext )
  {
    return;
  }

  if ( _stricmp( ext, ".kkp" ) == 0 )
  {
    kkp.Load( filePath );
  }
  else if ( _stricmp( ext, ".sym" ) == 0 )
  {
    kkp.LoadSym( filePath );
  }
}

void InitStuff( int argc, char** argv )
{
  auto profont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF( profont_compressed_data, profont_compressed_size, 11.0f );
  //io.Fonts->AddFontFromFileTTF("ProfontPixelated.ttf", 11.0f);

  for ( int i = 1; i < argc; i++ )
  {
    LoadFile( argv[ i ] );
  }

  ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
}

void RenderFrame( int rtWidth, int rtHeight )
{
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

    if ( ImGui::MenuItem( "Toggle color scheme", nullptr, nullptr, true ) )
    {
      darkColors = !darkColors;
      if ( darkColors )
        ImGui::StyleColorsDark();
      else
        ImGui::StyleColorsLight();
    }

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
  const float codeSplitterPos = 0.75f;

  ImGui::BeginChild( "left side", ImVec2( verticalSplitPixelPos, -1 ), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX );
  {
    // Upper part of the left area
    ImGui::BeginChild( "Hex View", ImVec2( -1, horizontalSplitPixelPos ), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY );
    DrawHexView();
    ImGui::EndChild();

    // Lower part of the left area
    ImGui::BeginChild( "Code View", ImVec2( -1, -1 ), ImGuiChildFlags_None );
    {
      windowSize = ImGui::GetContentRegionAvail();
      const float codeSplitPixelPos = windowSize.x * codeSplitterPos;
      ImGui::BeginChild( "left side", ImVec2( codeSplitPixelPos, -1 ), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX );
      {
        DrawCodeView();
        ImGui::EndChild();
        ImGui::SameLine();
        DrawDisassemblyView();
      }
      ImGui::EndChild();
    }
  }
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild( "Symbol List", ImVec2( -1, -1 ), ImGuiChildFlags_None );
  DrawSymbolList();
  ImGui::EndChild();

  ImGui::End();

  ImGui::Render();
}
