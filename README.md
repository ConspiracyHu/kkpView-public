Universal compression ratio analyzer using the .kkp and .sym formats introduced in [rekkrunchy-with-analytics](https://github.com/ConspiracyHu/rekkrunchy-with-analytics).

![image](https://github.com/ConspiracyHu/kkpView-public/assets/1076235/e84ba62f-6bec-4721-8fca-1e2c636a7734)

The purpose of this tool is to visualize the contents of a compressed executable using a .kkp file and show the packing ratio at a per-byte resolution. The kkp format supports per byte symbol and source code information. The additional sym format can be used to map the contents of individual symbols inside the executable (for embedded resources).

File format descriptions follow.
## KKP file format
Used to describe a binary file with all its contents and compression statistics, including symbol info

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
	4 bytes: source code line ID

// binary compression data:
Ds times: (for each byte of the described binary)
	1 byte: original data from the binary
	2 bytes: symbol index
	double: packed size
	2 bytes: source code line
	2 bytes: source code file index
```

## SYM file format
Used to describe the contents of the a symbol inside a kkp file to enable more precise analysis of an embedded resource

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
