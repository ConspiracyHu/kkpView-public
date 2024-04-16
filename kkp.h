#pragma once
#include <string>
#include <vector>

class KKP
{
public:

  static bool isX64;

#pragma pack(push)
#pragma pack(1)
  struct KKPFile
  {
    std::string name;
    int size = 0;
    float packedSize = 0;
  };

  struct KKPSymbol
  {
    std::string name;
    int unpackedSize = 0;
    double packedSize = 0;
    bool isCode = false;
    int fileID = 0;
    unsigned int sourcePos = 0xffffffff;
    double cumulativePackedSize = 0;
    int cumulativeUnpackedSize = 0;
    int originalSymbolID = -1;

    bool selected = false;
    bool onHotPath = false; // used to force open the folder view when a symbol is selected

    std::vector<KKPSymbol> children;
  };

  struct KKPByteData
  {
    unsigned char data = 0;
    short symbol = 0;
    double packed = 0;
    short line = 0;
    short file = 0;
  };

#pragma pack(pop)

  KKPSymbol root;

  std::vector<KKPFile> files;
  std::vector<KKPByteData> bytes;
  std::vector<KKPSymbol> sortableSymbols;

  void Load( const std::string& fileName );
  void LoadSym( const std::string& fileName );
  void AddSymbol( const KKPSymbol& symbol );
  void Sort( int sortColumn, bool descending );
};

extern KKP kkp;

void OpenKKP();
void OpenSYM();
