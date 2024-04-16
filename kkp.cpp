#include "kkp.h"
#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <algorithm>

KKP kkp;

void OpenKKP()
{
  char dir[ 1024 ];
  if ( !GetCurrentDirectoryA( 1024, dir ) )
    memset( dir, 0, sizeof( char ) * 1024 );

  char Filestring[ 256 ];

  OPENFILENAMEA opf;
  opf.hwndOwner = 0;
  opf.lpstrFilter = "KKrunchy pack info files\0*.kkp\0\0";
  opf.lpstrCustomFilter = 0;
  opf.nMaxCustFilter = 0L;
  opf.nFilterIndex = 1L;
  opf.lpstrFile = Filestring;
  opf.lpstrFile[ 0 ] = '\0';
  opf.nMaxFile = 256;
  opf.lpstrFileTitle = 0;
  opf.nMaxFileTitle = 50;
  opf.lpstrInitialDir = "Data";
  opf.lpstrTitle = "Open KKP";
  opf.nFileOffset = 0;
  opf.nFileExtension = 0;
  opf.lpstrDefExt = "kkp";
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
    kkp.Load( std::string( opf.lpstrFile ) );
  }

  SetCurrentDirectoryA( dir );
}

void OpenSYM()
{
  char dir[ 1024 ];
  if ( !GetCurrentDirectoryA( 1024, dir ) )
    memset( dir, 0, sizeof( char ) * 1024 );

  char Filestring[ 256 ];

  OPENFILENAMEA opf;
  opf.hwndOwner = 0;
  opf.lpstrFilter = "Symbol map files\0*.sym\0\0";
  opf.lpstrCustomFilter = 0;
  opf.nMaxCustFilter = 0L;
  opf.nFilterIndex = 1L;
  opf.lpstrFile = Filestring;
  opf.lpstrFile[ 0 ] = '\0';
  opf.nMaxFile = 256;
  opf.lpstrFileTitle = 0;
  opf.nMaxFileTitle = 50;
  opf.lpstrInitialDir = "Data";
  opf.lpstrTitle = "Open SYM";
  opf.nFileOffset = 0;
  opf.nFileExtension = 0;
  opf.lpstrDefExt = "sym";
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
    kkp.LoadSym( std::string( opf.lpstrFile ) );
  }

  SetCurrentDirectoryA( dir );
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
      currNode->sourcePos = min( currNode->sourcePos, symbol.sourcePos );

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
  SortNode(root, sortColumn, descending);

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
  unsigned int fourCC = 0;
  int fileCount = 0;

  if ( !fread_s( &fourCC, 4, 4, 1, reader ) )
    goto closeFile;

  if ( fourCC != '46KK' )
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

      if ( s.name == "Code::<arch: x64>" )
        isX64 = true;
      else
        AddSymbol( s );
    }
  }

  sortableSymbols.clear();
  BuildSymbolList( sortableSymbols, root, "" );

  bytes.reserve( sourceSize );
  bytes.resize( sourceSize );

  if ( fread_s( bytes.data(), sourceSize * sizeof( KKPByteData ), sizeof( KKPByteData ), sourceSize, reader ) != sourceSize )
    goto closeFile;

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

  unsigned int fourCC = 0;
  if ( !fread_s( &fourCC, 4, 4, 1, reader ) )
    goto closeFile;

  if ( fourCC != 'PXHP' )
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
      maxSymbolID = max( maxSymbolID, symbol.originalSymbolID );

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

      if ( symbol.sourcePos == -1 )
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
