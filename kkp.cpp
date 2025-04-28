
#define __STDC_WANT_LIB_EXT1__ /* fopen_s, fread_s */

#include <math.h>
#include <stdio.h>
#include <cstring>
#include <algorithm>

#include "kkp.h"

#include "platform.h"

KKP kkp;

void OpenKKP()
{
  static const file_dialog_filter flt = { "KKrunchy pack info files", "*.kkp" };
  std::string r = platform_open_file_dialog( "Open KKP", 1, &flt, "Data" );

  if ( !r.empty() )
    kkp.Load( r );
}

void OpenSYM()
{
  static const file_dialog_filter flt = { "Symbol map files", "*.sym" };
  std::string r = platform_open_file_dialog( "Open SYM", 1, &flt, "Data" );

  if ( !r.empty() )
    kkp.LoadSym( r );
}

std::string ReadASCIIZ( FILE* f )
{
  std::string result;

  int ch;
  while ( ( ch = fgetc( f ) ) != EOF && ch != '\0' )
    result.push_back( (char)ch );

  return result;
}

std::vector<std::string> Explode( std::string s, const std::string& delim )
{
  std::vector<std::string> result;
  size_t pos = 0;

  while ( ( pos = s.find( delim ) ) != std::string::npos )
  {
    result.emplace_back( s.substr( 0, pos ) );
    s.erase( 0, pos + delim.length() );
  }

  result.emplace_back( s );
  return result;
}


void KKP::AddSymbol( const KKPSymbol& symbol )
{
  auto nameSpaces = Explode( symbol.name, "::" );

  auto* currNode = &root;

  for ( auto& nameSpace : nameSpaces )
  {
    currNode->cumulativePackedSize += symbol.packedSize;
    currNode->cumulativeUnpackedSize += symbol.unpackedSize;
    if ( symbol.sourcePos )
      currNode->sourcePos = std::min( currNode->sourcePos, symbol.sourcePos );

    bool found = false;
    for ( auto& c : currNode->children )
    {
      if ( c.name == nameSpace )
      {
        found = true;
        currNode = &c;
        break;
      }
    }

    if ( !found )
    {
      KKPSymbol newSymbol;
      newSymbol.name = nameSpace;
      currNode->children.emplace_back( newSymbol );
      currNode = &currNode->children.back();
    }
  }

  auto strippedName = currNode->name;
  *currNode = symbol;
  currNode->name = strippedName;
}

void SortNode( KKP::KKPSymbol& symbol, int sortColumn, bool descending )
{
  switch ( sortColumn )
  {
  case 0: // name
    std::sort( symbol.children.begin(), symbol.children.begin() + symbol.children.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.name < b.name : a.name > b.name;
               } );
    break;
  case 1: // offset
    std::sort( symbol.children.begin(), symbol.children.begin() + symbol.children.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.sourcePos < b.sourcePos : a.sourcePos > b.sourcePos;
               } );
    break;
  case 2: // unpacked
    std::sort( symbol.children.begin(), symbol.children.begin() + symbol.children.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.cumulativeUnpackedSize < b.cumulativeUnpackedSize : a.cumulativeUnpackedSize > b.cumulativeUnpackedSize;
               } );
    break;
  case 3: // packed
    std::sort( symbol.children.begin(), symbol.children.begin() + symbol.children.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.cumulativePackedSize < b.cumulativePackedSize : a.cumulativePackedSize > b.cumulativePackedSize;
               } );
    break;
  case 4: // ratio
    std::sort( symbol.children.begin(), symbol.children.begin() + symbol.children.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 double ratioA = 0;
                 if ( a.cumulativeUnpackedSize )
                   ratioA = a.cumulativePackedSize / a.cumulativeUnpackedSize;
                 if ( isnan( ratioA ) || isinf( ratioA ) )
                   ratioA = 0;
                 double ratioB = 0;
                 if ( b.cumulativeUnpackedSize )
                   ratioB = b.cumulativePackedSize / b.cumulativeUnpackedSize;
                 if ( isnan( ratioB ) || isinf( ratioB ) )
                   ratioB = 0;
                 return descending ? ratioA < ratioB : ratioA > ratioB;
               } );
    break;
  }

  for ( auto& child : symbol.children )
    SortNode( child, sortColumn, descending );
}

void KKP::Sort( int sortColumn, bool descending )
{
  SortNode( root, sortColumn, descending );

  switch ( sortColumn )
  {
  case 0: // name
    std::sort( sortableSymbols.begin(), sortableSymbols.begin() + sortableSymbols.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.name < b.name : a.name > b.name;
               } );
    break;
  case 1: // offset
    std::sort( sortableSymbols.begin(), sortableSymbols.begin() + sortableSymbols.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.sourcePos < b.sourcePos : a.sourcePos > b.sourcePos;
               } );
    break;
  case 2: // unpacked
    std::sort( sortableSymbols.begin(), sortableSymbols.begin() + sortableSymbols.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.cumulativeUnpackedSize < b.cumulativeUnpackedSize : a.cumulativeUnpackedSize > b.cumulativeUnpackedSize;
               } );
    break;
  case 3: // packed
    std::sort( sortableSymbols.begin(), sortableSymbols.begin() + sortableSymbols.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 return descending ? a.cumulativePackedSize < b.cumulativePackedSize : a.cumulativePackedSize > b.cumulativePackedSize;
               } );
    break;
  case 4: // ratio
    std::sort( sortableSymbols.begin(), sortableSymbols.begin() + sortableSymbols.size(), [ descending ]( const KKP::KKPSymbol& a, const KKP::KKPSymbol& b ) -> bool
               {
                 double ratioA = 0;
                 if ( a.cumulativeUnpackedSize )
                   ratioA = a.cumulativePackedSize / a.cumulativeUnpackedSize;
                 if ( isnan( ratioA ) || isinf( ratioA ) )
                   ratioA = 0;
                 double ratioB = 0;
                 if ( b.cumulativeUnpackedSize )
                   ratioB = b.cumulativePackedSize / b.cumulativeUnpackedSize;
                 if ( isnan( ratioB ) || isinf( ratioB ) )
                   ratioB = 0;
                 return descending ? ratioA < ratioB : ratioA > ratioB;
               } );
    break;
  }
}

void BuildSymbolList( std::vector<KKP::KKPSymbol>& target, const KKP::KKPSymbol& root, const std::string name )
{
  for ( auto& child : root.children )
  {
    target.emplace_back( child );
    target.back().name = name.length() ? name + "::" + target.back().name : target.back().name;
    BuildSymbolList( target, child, target.back().name );
  }
}

bool KKP::isX64 = false;


void KKP::Check64Bit( int sourceSize )
{
  IMAGE_DOS_HEADER dosHeader;

  if ( sourceSize >= sizeof( dosHeader ) )
  {
    for ( int x = 0; x < sizeof( dosHeader ); x++ )
      ( (unsigned char*)&dosHeader )[ x ] = bytes[ x ].data;

    if ( dosHeader.e_magic == IMAGE_DOS_SIGNATURE )
    {
      IMAGE_NT_HEADERS peHeader;
      if ( (unsigned int)sourceSize >= sizeof( peHeader ) + dosHeader.e_lfanew )
      {
        for ( int x = 0; x < sizeof( peHeader ); x++ )
          ( (unsigned char*)&peHeader )[ x ] = bytes[ x + dosHeader.e_lfanew ].data;

        if ( peHeader.Signature == IMAGE_NT_SIGNATURE )
        {
          if ( peHeader.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 )
          {
            isX64 = true;
            return;
          }
        }

      }
    }
  }

  Elf32_Ehdr ehdr;
  if ( sourceSize >= sizeof( ehdr ) )
  {
    for ( int x = 0; x < sizeof( ehdr ); ++x )
      ( (uint8_t*)&ehdr )[ x ] = bytes[ x ].data;

    if ( !memcmp( ehdr.e_ident, ELFMAG, SELFMAG ) )
    {
      if ( ehdr.e_machine == EM_X86_64 )
      {
        isX64 = true;
        return;
      }
    }
  }
}

void KKP::Load( const std::string& fileName )
{
  isX64 = false;
  root = KKPSymbol();
  files.clear();
  bytes.clear();

  FILE* reader = nullptr;
  if ( fopen_s( &reader, fileName.data(), "rb" ) )
    return;

  int sourceSize = 0;
  uint8_t fourCC[ 4 ] = { 0 };
  int fileCount = 0;

  if ( !fread_s( fourCC, 4, 1, 4, reader ) )
    goto closeFile;

  if ( memcmp( fourCC, "KK64", 4 ) )
    goto closeFile;

  if ( !fread_s( &sourceSize, 4, 4, 1, reader ) )
    goto closeFile;

  if ( !fread_s( &fileCount, 4, 4, 1, reader ) )
    goto closeFile;

  for ( int x = 0; x < fileCount; x++ )
  {
    KKPFile f;
    f.name = ReadASCIIZ( reader );

    if ( !fread_s( &f.packedSize, 4, 4, 1, reader ) )
      goto closeFile;

    if ( !fread_s( &f.size, 4, 4, 1, reader ) )
      goto closeFile;

    files.emplace_back( f );
  }

  {
    int symbolCount = 0;
    if ( !fread_s( &symbolCount, 4, 4, 1, reader ) )
      goto closeFile;

    for ( int x = 0; x < symbolCount; x++ )
    {
      KKPSymbol s;
      s.name = "Code::" + ReadASCIIZ( reader );

      if ( !fread_s( &s.packedSize, 8, 8, 1, reader ) )
        goto closeFile;

      s.cumulativePackedSize = s.packedSize;

      if ( !fread_s( &s.unpackedSize, 4, 4, 1, reader ) )
        goto closeFile;

      s.cumulativeUnpackedSize = s.unpackedSize;

      if ( !fread_s( &s.isCode, 1, 1, 1, reader ) )
        goto closeFile;

      if ( !fread_s( &s.fileID, 4, 4, 1, reader ) )
        goto closeFile;

      if ( !fread_s( &s.sourcePos, 4, 4, 1, reader ) )
        goto closeFile;

      s.originalSymbolID = x;

      AddSymbol( s );
    }
  }

  sortableSymbols.clear();
  BuildSymbolList( sortableSymbols, root, "" );

  bytes.reserve( sourceSize );
  bytes.resize( sourceSize );

  if ( fread_s( bytes.data(), sourceSize * sizeof( KKPByteData ), sizeof( KKPByteData ), sourceSize, reader ) != sourceSize )
    goto closeFile;

  Check64Bit( sourceSize );

closeFile:
  fclose( reader );
}

void KKP::LoadSym( const std::string& fileName )
{
  FILE* reader = nullptr;
  if ( fopen_s( &reader, fileName.data(), "rb" ) )
    return;

  bool found = false;
  int symbolStart = 0;
  int symbolSize = 0;

  uint8_t fourCC[ 4 ] = { 0 };
  if ( !fread_s( fourCC, 4, 1, 4, reader ) )
    goto closeFile;

  if ( memcmp( fourCC, "PHXP", 4 ) )
    goto closeFile;

  {
    std::string symbolName = ReadASCIIZ( reader );
    std::vector<KKP::KKPSymbol> newSymbols;

    for ( auto& symbol : sortableSymbols )
    {
      if ( symbol.name == "Code::" + symbolName )
      {
        found = true;
        symbolStart = symbol.sourcePos;
        symbolSize = symbol.unpackedSize;
        break;
      }
    }

    if ( !found )
      goto closeFile;

    int dataSize = 0;
    if ( !fread_s( &dataSize, 4, 4, 1, reader ) )
      goto closeFile;

    if ( dataSize != symbolSize )
      goto closeFile;

    int symbolCount = 0;
    if ( !fread_s( &symbolCount, 4, 4, 1, reader ) )
      goto closeFile;

    int maxSymbolID = 0;
    for ( auto& symbol : kkp.sortableSymbols )
      maxSymbolID = std::max( maxSymbolID, symbol.originalSymbolID );

    for ( int x = 0; x < symbolCount; x++ )
    {
      KKP::KKPSymbol symbol;
      symbol.name = symbolName + "::" + ReadASCIIZ( reader );
      symbol.isCode = false;
      symbol.fileID = -1;
      symbol.sourcePos = -1;
      symbol.originalSymbolID = x + maxSymbolID + 1;
      newSymbols.emplace_back( symbol );
    }

    for ( int x = symbolStart; x < symbolStart + dataSize; x++ )
    {
      unsigned short symSymbol;

      if ( !fread_s( &symSymbol, 2, 2, 1, reader ) )
        goto closeFile;

      KKP::KKPSymbol& symbol = newSymbols[ symSymbol ];

      kkp.bytes[ x ].symbol = (short)symbol.originalSymbolID;

      if ( symbol.sourcePos == -1 ) // unsigned-signed comparison???? -pcy
        symbol.sourcePos = x;

      symbol.unpackedSize++;
      symbol.packedSize += kkp.bytes[ x ].packed;
      symbol.cumulativePackedSize = symbol.packedSize;
      symbol.cumulativeUnpackedSize = symbol.unpackedSize;
    }

    for ( auto& symbol : newSymbols )
      AddSymbol( symbol );

    sortableSymbols.clear();
    BuildSymbolList( sortableSymbols, root, "" );
  }

closeFile:
  fclose( reader );
}
