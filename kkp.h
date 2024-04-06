#pragma once
#include <string>
#include <vector>

class KKP
{
public:

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
        int sourcePos = 0;
        double cumulativePackedSize = 0;
        int cumulativeUnpackedSize = 0;
        int originalSymbolID = -1;

        bool selected = false;

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

    KKPSymbol root;

    std::vector<KKPFile> files;
    std::vector<KKPByteData> bytes;
    std::vector<KKPSymbol> sortableSymbols;

    void Load(const std::string& fileName);
    void LoadSym(const std::string& fileName);
    void AddSymbol(const KKPSymbol& symbol);
    void Sort(int sortColumn, bool descending);
};

extern KKP kkp;

void OpenKKP();
void OpenSYM();
