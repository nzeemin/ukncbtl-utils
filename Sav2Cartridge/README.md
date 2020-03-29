## Sav2Cart

Command line utility used to convert executable RT-11 SAV file into UKNC ROM cartridge image.

UKNC catridge is 24 KB = 24576 bytes, so that's "natural" limit for a file to put into the cartridge.

If the SAV file is too large for the cartridge, the utility attempts to use RLE / LZSS / LZ4 compression.

Usage:
```
Sav2Cart [options] <inputfile.SAV> <outputfile.BIN>
Options:
    -none - try to fit non-compressed
    -rle  - try RLE compression
    -lzss - try LZSS compression
    -lz4  - try LZ4 compression
    (no compression options) - try all on-by-one until fit
```
NOTE: '-' character used as an option sign under Linux/Mac, '/' character under Windows.

Example:
```
> Sav2Cart.exe HWYENC.SAV HWYENC.BIN
Input file: HWYENC.SAV
Input file size 47616. bytes
SAV Start       001000  0200    512
SAV Stack       134674  b9bc  47548
SAV Top         134672  b9ba  47546
SAV image size  133674  b7bc  47036
Input file is too big for cartridge: 47616. bytes, max 24576. bytes
RLE input size 47036. bytes
RLE output size 33070. bytes (70.31 %)
RLE encoded size too big: 33070. bytes, max 24064. bytes
LZSS input size 47036. bytes
LZSS output size 23507. bytes (49.98 %)
LZSS decode check done, decoded size 47036. bytes
Output file: HWYENC.BIN
Done.
```
