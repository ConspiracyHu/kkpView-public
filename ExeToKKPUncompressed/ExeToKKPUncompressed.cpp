#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#define _NO_CVCONST_H
#include <ImageHlp.h>

#pragma comment(lib,"dbghelp.lib")

#pragma pack(push)
#pragma pack(1)
struct KKPByteData
{
  unsigned char data = 0;
  short symbol = 0;
  double packed = 0;
  short line = 0;
  short file = 0;
};
#pragma pack(pop)

struct SymbolInfo
{
  std::string name;
  int fileID{};
  ULONG64 VA{};
  ULONG64 size{};
  double packedSize{};
  unsigned int sourcePos{};
  bool isCode = false;
};

std::vector<SymbolInfo> symbols;
std::vector<std::string> sourceFiles;
std::vector<IMAGE_SECTION_HEADER> exeSections;
DWORD64 imageBase = 0;

unsigned int VirtualToPhysical( ULONG64 virtualAddress, const std::vector<IMAGE_SECTION_HEADER>& sections )
{
  bool found = false;
  unsigned int physicalOffset = 0;

  for ( const auto& section : sections )
  {
    if ( section.VirtualAddress <= virtualAddress - imageBase &&
         virtualAddress - imageBase < section.VirtualAddress + section.Misc.VirtualSize )
    {
      found = true;
      physicalOffset = (unsigned int)( ( virtualAddress - imageBase - section.VirtualAddress ) + section.PointerToRawData );
      break;
    }
  }

  if ( !found )
  {
    printf( "Virtual address not found in any section!\n" );
    return 0;
  }

  return physicalOffset;
}

bool LoadPEHeaders( BYTE* peContent, std::vector<IMAGE_SECTION_HEADER>& sections )
{
  IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>( peContent );
  if ( dosHeader->e_magic != IMAGE_DOS_SIGNATURE )
  {
    printf( "Not a valid PE file!\n" );
    return false;
  }

  auto ntHeaders = *reinterpret_cast<IMAGE_NT_HEADERS*>( peContent + dosHeader->e_lfanew );
  if ( ntHeaders.Signature != IMAGE_NT_SIGNATURE )
  {
    printf( "Not a valid PE file!\n" );
    return false;
  }

  ULONG sectionOffset = dosHeader->e_lfanew + sizeof( DWORD ) + sizeof( IMAGE_FILE_HEADER ) + ntHeaders.FileHeader.SizeOfOptionalHeader;
  IMAGE_SECTION_HEADER* sectionHeaders = reinterpret_cast<IMAGE_SECTION_HEADER*>( peContent + sectionOffset );
  sections.resize( ntHeaders.FileHeader.NumberOfSections );
  for ( int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i )
    sections[ i ] = sectionHeaders[ i ];

  return true;
}

BOOL CALLBACK EnumSymbols(SYMBOL_INFO* info, ULONG size, void* param)
{
  SymbolInfo sym;
  sym.name = info->Name;
  sym.fileID = 0;
  sym.VA = info->Address;
  sym.size = info->Size;
  sym.isCode = false;

/*
  if ( sym.size == 0 )
  {
    ULONG64 typeLength;
    if ( SymGetTypeInfo( GetCurrentProcess(), info->ModBase, info->TypeIndex, TI_GET_LENGTH, &typeLength ) )
      sym.size = typeLength;
  }

  if ( sym.size == 0 && strstr( info->Name, "_real" ) == info->Name )
    sym.size = 4; // float value

  if ( sym.size == 0 && strstr( info->Name, "_imp__" ) == info->Name )
    sym.size = 4; // import

  if ( sym.size == 0 && strstr( info->Name, "_xmm" ) == info->Name )
    sym.size = 16; // xmm

  if ( sym.size == 0 && strstr( info->Name, "_IMPORT_DESCRIPTOR_" ) == info->Name )
    sym.size = sizeof( _IMAGE_IMPORT_DESCRIPTOR );

  if ( sym.size == 0 && strstr( info->Name, "_NULL_THUNK_DATA" ) )
    sym.size = 4; // ??

  if ( sym.size == 0 && strstr( info->Name, "_GUID_" ) == info->Name )
    sym.size = sizeof( GUID ); // guid
*/

  if ( sym.size == 0 )
  {
    auto proc = GetCurrentProcess();

    do
    {
      sym.size++;

      char symbolBuffer[ sizeof( SYMBOL_INFO ) + MAX_SYM_NAME * sizeof( TCHAR ) ];

      PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
      pSymbol->SizeOfStruct = sizeof( SYMBOL_INFO );
      pSymbol->MaxNameLen = MAX_SYM_NAME;

      if ( !SymFromAddr( proc, info->Address + sym.size, nullptr, pSymbol ) )
        break;

      if ( std::string( pSymbol->Name ) != sym.name )
        break;

    } while ( 1 );    
  }

  sym.packedSize = (int)sym.size;
  sym.sourcePos = VirtualToPhysical( sym.VA, exeSections );

  sym.fileID = -1;

  IMAGEHLP_LINE64 LineInfo;
  LineInfo.SizeOfStruct = sizeof( IMAGEHLP_LINE64 );
  symbols.emplace_back( sym );

  return TRUE;
}

int main( int argc, char** argv)
{
  printf( "ExeToKKPUncompressed - a tool to extract symbol and line info from an executable through the use of a PDB\n" );

  if ( argc < 3 )
  {
    printf( "Usage: ExeToKKPUncompressed <input> <output>\n" );
  }

  // open exe
  FILE* f = nullptr;
  if ( fopen_s( &f, argv[ 1 ], "rb" ) )
  {
    printf( "Failed to open executable.\n" );
    return 0;
  }

  // get filesize
  fseek( f, 0, SEEK_END );
  int fileSize = ftell( f );
  fseek( f, 0, SEEK_SET );

  // read raw exe data
  unsigned char* fileData = new unsigned char[ fileSize ];
  if ( fread( fileData, 1, fileSize, f ) != fileSize )
  {
    printf( "Failed to read executable.\n" );
    return 0;
  }
  fclose( f );

  if ( !LoadPEHeaders( fileData, exeSections ) )
  {
    printf( "Failed to parse executable.\n" );
    return 0;
  }

  // initial setup of kkp data with raw file image
  KKPByteData* kkpData = new KKPByteData[ fileSize ];
  memset( kkpData, 0, sizeof( KKPByteData ) * fileSize );
  for ( int x = 0; x < fileSize; x++ )
  {
    kkpData[ x ].data = fileData[ x ];
    kkpData[ x ].packed = 1; // no compression here
  }
  delete[] fileData;

  SymInitialize( GetCurrentProcess(), nullptr, false );
  SymSetOptions( SYMOPT_LOAD_LINES | SYMOPT_UNDNAME );
  imageBase = SymLoadModule64( GetCurrentProcess(), NULL, argv[ 1 ], NULL, 0, 0 );

  if ( !imageBase )
  {
    printf( "Failed to load pdb for executable.\n" );
    return 0;
  }

  sourceFiles.emplace_back( "<no source>" );

  SymbolInfo emptySymbol;
  emptySymbol.name = "<no symbol>";
  symbols.emplace_back( emptySymbol );

  printf( "Enumerating symbols...\n" );

  if ( !SymEnumSymbols( GetCurrentProcess(), imageBase, "*!*", EnumSymbols, nullptr ) )
  {
    printf( "Failed to enumerate symbols.\n" );
    return 0;
  }

  std::sort( symbols.begin(), symbols.begin() + symbols.size(), []( const SymbolInfo& a, const SymbolInfo& b ) -> bool
             {
               return a.sourcePos < b.sourcePos;
             } );

  printf( "Building map...\n" );

  for ( int x = 0; x < (int)symbols.size(); x++ )
  {
    auto& sym = symbols[ x ];

    if ( (int)sym.sourcePos >= fileSize || (int)sym.sourcePos + sym.size >= fileSize )
      continue;

    for ( int y = 0; y < sym.size; y++ )
    {
      IMAGEHLP_LINE64 lineInfo;
      lineInfo.SizeOfStruct = sizeof( IMAGEHLP_LINE64 );

      DWORD lineDisplacement = 0;

      int fileIdx = -1;
      if ( SymGetLineFromAddr64( GetCurrentProcess(), sym.VA + y, &lineDisplacement, &lineInfo ) )
      {
        if ( lineInfo.FileName )
        {
          for ( int x = 0; x < (int)sourceFiles.size(); x++ )
          {
            if ( sourceFiles[ x ] == std::string( lineInfo.FileName ) )
            {
              fileIdx = x;
              break;
            }
          }

          if ( fileIdx < 0 )
          {
            fileIdx = (int)sourceFiles.size();
            sourceFiles.emplace_back( std::string( lineInfo.FileName ) );
          }

          sym.fileID = fileIdx;
          kkpData[ sym.sourcePos + y ].file = fileIdx;
          kkpData[ sym.sourcePos + y ].line = (int)lineInfo.LineNumber;
        }
      }

      kkpData[ sym.sourcePos + y ].symbol = x;
    }
  }

  for ( auto& sym : symbols )
  {
    sym.size = 0;
    sym.packedSize = 0;
  }

  for ( int x = 0; x < fileSize; x++ )
  {
    symbols[ kkpData[ x ].symbol ].size++;
    symbols[ kkpData[ x ].symbol ].packedSize++;
  }

  if ( fopen_s( &f, argv[ 2 ], "wb" ) )
  {
    printf( "Failed to open output for writing.\n" );
    return 0;
  }

  fwrite( "KK64", 1, 4, f );
  fwrite( &fileSize, 4, 1, f );

  int sourceCount = (int)sourceFiles.size();
  fwrite( &sourceCount, 4, 1, f );

  for ( auto& file : sourceFiles )
  {
    fwrite( file.data(), 1, file.size() + 1, f );
    int packedSize = 0;
    fwrite( &packedSize, 4, 1, f );
    fwrite( &packedSize, 4, 1, f );
  }

  int symbolCount = (int)symbols.size();
  fwrite( &symbolCount, 4, 1, f );

  for ( auto& symbol : symbols )
  {
    fwrite( symbol.name.data(), 1, symbol.name.size() + 1, f );
    fwrite( &symbol.packedSize, 8, 1, f );
    fwrite( &symbol.size, 4, 1, f );
    fwrite( &symbol.isCode, 1, 1, f );
    fwrite( &symbol.fileID, 4, 1, f );
    fwrite( &symbol.sourcePos, 4, 1, f );
  }

  fwrite( kkpData, sizeof( KKPByteData ), fileSize, f );
  fclose( f );

  return 0;
}
