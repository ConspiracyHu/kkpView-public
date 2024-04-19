Universal compression ratio analyzer using the .kkp and .sym formats introduced 
in [rekkrunchy-with-analytics](https://github.com/ConspiracyHu/rekkrunchy-with-analytics).

![image](https://github.com/ConspiracyHu/kkpView-public/assets/1076235/758264fc-9a35-4498-8223-5d6a5c213503)

The purpose of this tool is to visualize the contents of a compressed executable
using a .kkp file and show the packing ratio at a per-byte resolution. The .kkp 
format supports per byte symbol and source code information. The additional .sym 
format can be used to map the contents of individual symbols inside the
executable (for embedded resources).

# Known compression tools supporting
* [Our rekkrunchy fork](https://github.com/ConspiracyHu/rekkrunchy-with-analytics)
* [BoyC's Crinkler fork](https://github.com/BoyC/Crinkler-with-kkp-export)

# File formats

## KKP file format
Used to describe a binary file with all its contents and compression statistics,
including symbol info

```
4 bytes: FOURCC: 'KK64'
4 bytes: size of described binary in bytes (Ds)
4 bytes: number of source code files (Cc)

// source code descriptors:
Cc times:
	ASCIIZ string: filename
	float: packed size for the complete file
	4 bytes: unpacked size for the complete file, in int

4 bytes: number of symbols (Sc)
// symbol data:
Sc times:
	ASCIIZ string: symbol name
	double: packed size of symbol
	4 bytes: unpacked size of symbol in bytes
	1 byte: boolean to tell if symbol is code (true if yes)
	4 bytes: source code file ID
	4 bytes: symbol position in executable

// binary compression data:
Ds times: (for each byte of the described binary)
	1 byte: original data from the binary
	2 bytes: symbol index
	double: packed size
	2 bytes: source code line
	2 bytes: source code file index
```

## SYM file format
Used to describe the contents of the a symbol inside a kkp file to enable more
precise analysis of an embedded resource

```
4 bytes: FOURCC: 'PHXP'
ASCIIZ string: symbol name
4 bytes: symbol data size (Ds)
4 bytes: symbol count (Sc)
For each symbol (Sc)
	ASCIIZ string: symbol name
For each byte in the symbol (Ds)
	2 bytes: symbol index
```

# Acknowledgements
* [ImGui](https://github.com/ocornut/imgui/) by Omar Cornut
* [Zydis](https://github.com/zyantific/zydis/) by Zyantific
