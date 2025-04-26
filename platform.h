#pragma once

#include <stdint.h>
#include <string>

struct file_dialog_filter
{
  const char* name;
  const char* pattern;
};

std::string platform_open_file_dialog(const char* title, int nfilters, const file_dialog_filter* filters, const char* def_out);


#ifndef _WIN32
#include <elf.h>

typedef struct _IMAGE_DOS_HEADER
{
  uint16_t e_magic;
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  uint32_t e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
  uint16_t Machine;
  uint16_t NumberOfSections;
  uint32_t TimeDateStamp;
  uint32_t PointerToSymbolTable;
  uint32_t NumberOfSymbols;
  uint16_t SizeOfOptionalHeader;
  uint16_t Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_NT_HEADERS {
  uint32_t                Signature;
  IMAGE_FILE_HEADER       FileHeader;
  // optional header normally here, but not used, so eh.
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

#define IMAGE_FILE_MACHINE_UNKNOWN	0x0
#define IMAGE_FILE_MACHINE_ALPHA	0x184
#define IMAGE_FILE_MACHINE_ALPHA64	0x284
#define IMAGE_FILE_MACHINE_AM33	0x1d3
#define IMAGE_FILE_MACHINE_AMD64	0x8664
#define IMAGE_FILE_MACHINE_ARM	0x1c0
#define IMAGE_FILE_MACHINE_ARM64	0xaa64
#define IMAGE_FILE_MACHINE_ARMNT	0x1c4
#define IMAGE_FILE_MACHINE_AXP64	0x284
#define IMAGE_FILE_MACHINE_EBC	0xebc
#define IMAGE_FILE_MACHINE_I386	0x14c
#define IMAGE_FILE_MACHINE_IA64	0x200
#define IMAGE_FILE_MACHINE_LOONGARCH32	0x6232
#define IMAGE_FILE_MACHINE_LOONGARCH64	0x6264
#define IMAGE_FILE_MACHINE_M32R	0x9041
#define IMAGE_FILE_MACHINE_MIPS16	0x266
#define IMAGE_FILE_MACHINE_MIPSFPU	0x366
#define IMAGE_FILE_MACHINE_MIPSFPU16	0x466
#define IMAGE_FILE_MACHINE_POWERPC	0x1f0
#define IMAGE_FILE_MACHINE_POWERPCFP	0x1f1
#define IMAGE_FILE_MACHINE_R4000	0x166
#define IMAGE_FILE_MACHINE_RISCV32	0x5032
#define IMAGE_FILE_MACHINE_RISCV64	0x5064
#define IMAGE_FILE_MACHINE_RISCV128	0x5128
#define IMAGE_FILE_MACHINE_SH3	0x1a2
#define IMAGE_FILE_MACHINE_SH3DSP	0x1a3
#define IMAGE_FILE_MACHINE_SH4	0x1a6
#define IMAGE_FILE_MACHINE_SH5	0x1a8
#define IMAGE_FILE_MACHINE_THUMB	0x1c2
#define IMAGE_FILE_MACHINE_WCEMIPSV2	0x169

#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550

#else

#include <winnt.h> /* IMAGE_DOS_HEADER, IMAGE_PE_HEADER */

/* The ELF file header.  This appears at the start of every ELF file.  */

#define EI_NIDENT (16)

typedef struct
{
  unsigned char e_ident[EI_NIDENT];     /* Magic number and other info */
  uint16_t    e_type;                 /* Object file type */
  uint16_t    e_machine;              /* Architecture */
  uint32_t    e_version;              /* Object file version */
} Elf32_Ehdr;

#define EI_MAG0         0               /* File identification byte 0 index */
#define ELFMAG0         0x7f            /* Magic number byte 0 */

#define EI_MAG1         1               /* File identification byte 1 index */
#define ELFMAG1         'E'             /* Magic number byte 1 */

#define EI_MAG2         2               /* File identification byte 2 index */
#define ELFMAG2         'L'             /* Magic number byte 2 */

#define EI_MAG3         3               /* File identification byte 3 index */
#define ELFMAG3         'F'             /* Magic number byte 3 */

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define ELFMAG          "\177ELF"
#define SELFMAG         4

#define EI_CLASS        4               /* File class byte index */
#define ELFCLASSNONE    0               /* Invalid class */
#define ELFCLASS32      1               /* 32-bit objects */
#define ELFCLASS64      2               /* 64-bit objects */
#define ELFCLASSNUM     3

#define EI_DATA         5               /* Data encoding byte index */
#define ELFDATANONE     0               /* Invalid data encoding */
#define ELFDATA2LSB     1               /* 2's complement, little endian */
#define ELFDATA2MSB     2               /* 2's complement, big endian */
#define ELFDATANUM      3

#define EI_VERSION      6               /* File version byte index */

/* Legal values for e_version (version).  */

#define EV_NONE         0               /* Invalid ELF version */
#define EV_CURRENT      1               /* Current version */
#define EV_NUM          2


#define EM_NONE          0      /* No machine */
#define EM_SPARC         2      /* SUN SPARC */
#define EM_386           3      /* Intel 80386 */
#define EM_68K           4      /* Motorola m68k family */
#define EM_IAMCU         6      /* Intel MCU */
#define EM_MIPS          8      /* MIPS R3000 big-endian */
#define EM_MIPS_RS3_LE  10      /* MIPS R3000 little-endian */
#define EM_SPARC32PLUS  18      /* Sun's "v8plus" */
#define EM_PPC          20      /* PowerPC */
#define EM_PPC64        21      /* PowerPC 64-bit */
#define EM_ARM          40      /* ARM */
#define EM_FAKE_ALPHA   41      /* Digital Alpha */
#define EM_SH           42      /* Hitachi SH */
#define EM_SPARCV9      43      /* SPARC v9 64-bit */
#define EM_IA_64        50      /* Intel Merced */
#define EM_MIPS_X       51      /* Stanford MIPS-X */
#define EM_COLDFIRE     52      /* Motorola Coldfire */
#define EM_X86_64       62      /* AMD x86-64 architecture */
#define EM_AVR          83      /* Atmel AVR 8-bit microcontroller */
#define EM_V850         87      /* NEC v850 */
#define EM_XTENSA       94      /* Tensilica Xtensa Architecture */
#define EM_VIDEOCORE    95      /* Alphamosaic VideoCore */
#define EM_MSP430       105     /* Texas Instruments msp430 */
#define EM_BLACKFIN     106     /* Analog Devices Blackfin DSP */
#define EM_ALTERA_NIOS2 113     /* Altera Nios II */
#define EM_SHARC        133     /* Analog Devices SHARC family */
#define EM_LATTICEMICO32 138    /* RISC for Lattice FPGA */
#define EM_8051         165     /* Intel 8051 and variants */
#define EM_NDS32        167     /* Andes Tech. compact code emb. RISC */
#define EM_AARCH64      183     /* ARM AARCH64 */
#define EM_MICROBLAZE   189     /* Xilinx MicroBlaze */
#define EM_RISCV        243     /* RISC-V */
#define EM_CSKY         252     /* C-SKY */
#define EM_LOONGARCH    258     /* LoongArch */
#define EM_NUM          259


#endif

#ifndef __STDC_LIB_EXT1__
#define _stricmp strcasecmp
#define sprintf_s snprintf

int fopen_s(FILE **streamptr,
    const char *filename, const char *mode);
size_t fread_s(void *buffer, size_t bufferSize, size_t elementSize,
    size_t count, FILE *stream);
#endif

