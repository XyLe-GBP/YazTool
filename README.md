# YazTool

A command line tool to decompress and compress Nintendo archives (U8, SARC, BFRES, BRRES, etc.).

---

## Download

<s>[Release](https://github.com/XyLe-GBP/YazTool/Release)</s>

Currently, there is no release build.

You will need to build your application from the cpp source files.

---

## Description

Supports decompression and compression of Nintendo archive files.

This tool uses this arguments:
  `YazTool <OPTIONS> <FILE-PATH>`
  
The `<OPTIONS>` argument must be one of the following.
  
`0: Decompress`
  
`1: Compress`
  
The `<FILE-PATH>` argument is the full path of the file to be compressed and decompressed.
  
* Supported decompress formats:
  
  * `Yaz0-compressed file formats (*.YAZ0, *.SZS, *.SARC, *.RARC, *.BFRES, *.BRRES, etcs.)`
  
* Supported recompress formats:
  
  * `Yaz0-decompressed file formats (*.YAZ0, *.SZS, *.SARC, *.RARC, *.BFRES, *.BRRES, etcs.)`
  
an .SZS extension does not necessarily indicate which format a given SZS file originated from.

---

### About Licensing

<p>This tool is released under the MIT license.</p>
