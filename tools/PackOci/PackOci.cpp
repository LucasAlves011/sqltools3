// Empacota as DLLs do Oracle Instant Client em um unico arquivo comprimido (LZMS),
// embutido como recurso no SQLTools.exe (ver SQLTools/OCI8/EmbeddedOci.cpp).
//
// Uso:   PackOci <saida.bin> <arquivo1> [arquivo2 ...]
// Build: cl /EHsc /O2 PackOci.cpp /link Cabinet.lib
//
// Formato (little endian):
//   "OCIB" | uint32 versao=1 | uint32 n
//   n x { uint16 tamNome | nome (ASCII) | uint64 tamOriginal | uint64 tamComprimido | dados }
#include <windows.h>
#include <compressapi.h>
#include <cstdio>
#include <string>
#include <vector>

static bool readFile (const char* path, std::vector<char>& data)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long long size = _ftelli64(f);
    fseek(f, 0, SEEK_SET);
    data.resize((size_t)size);
    bool ok = size == 0 || fread(data.data(), 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    return ok;
}

int main (int argc, char** argv)
{
    if (argc < 3) { fprintf(stderr, "uso: PackOci <saida.bin> <arquivos...>\n"); return 2; }

    FILE* out = fopen(argv[1], "wb");
    if (!out) { fprintf(stderr, "nao foi possivel criar %s\n", argv[1]); return 1; }

    unsigned n = (unsigned)(argc - 2);
    unsigned version = 1;
    fwrite("OCIB", 1, 4, out);
    fwrite(&version, 4, 1, out);
    fwrite(&n, 4, 1, out);

    COMPRESSOR_HANDLE comp = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &comp))
    {
        fprintf(stderr, "CreateCompressor falhou: %lu\n", GetLastError());
        return 1;
    }

    for (int i = 2; i < argc; ++i)
    {
        std::vector<char> raw;
        if (!readFile(argv[i], raw)) { fprintf(stderr, "erro lendo %s\n", argv[i]); return 1; }

        SIZE_T need = 0;
        Compress(comp, raw.data(), raw.size(), nullptr, 0, &need);
        std::vector<char> packed(need);
        SIZE_T packedSize = 0;
        if (!Compress(comp, raw.data(), raw.size(), packed.data(), packed.size(), &packedSize))
        {
            fprintf(stderr, "Compress falhou em %s: %lu\n", argv[i], GetLastError());
            return 1;
        }

        std::string name = argv[i];
        size_t slash = name.find_last_of("\\/");
        if (slash != std::string::npos) name = name.substr(slash + 1);

        unsigned short nameLen = (unsigned short)name.size();
        unsigned long long rawSize = raw.size(), compSize = packedSize;
        fwrite(&nameLen, 2, 1, out);
        fwrite(name.data(), 1, nameLen, out);
        fwrite(&rawSize, 8, 1, out);
        fwrite(&compSize, 8, 1, out);
        fwrite(packed.data(), 1, packedSize, out);

        printf("%-24s %12llu -> %12llu\n", name.c_str(), rawSize, compSize);
    }

    CloseCompressor(comp);
    fclose(out);
    return 0;
}
