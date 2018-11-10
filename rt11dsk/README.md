## rt11dsk
Command-line utility used to work with UKNC floppy/IDE disk images.

Disk image commands:
 * `rt11dsk l <ImageFile>` -- list image contents
 * `rt11dsk e <ImageFile> <FileName>` -- extract file
 * `rt11dsk a <ImageFile> <FileName>` -- add file
 * `rt11dsk x <ImageFile>` -- extract all files
 * `rt11dsk d <ImageFile> <FileName>` -- delete file
 * `rt11dsk xu <ImageFile>` -- extract all unused space
 
Hard disk image commands:
 * `rt11dsk hl <HddImage>` -- list HDD image partitions
 * `rt11dsk hx <HddImage> <Partn> <FileName>` -- extract partition to file
 * `rt11dsk hu <HddImage> <Partn> <FileName>` -- update partition from the file
 * `rt11dsk hi <HddImage>` -- invert HDD image file
 * `rt11dsk hpl <HddImage> <Partn>` -- list partition contents
 * `rt11dsk hpe <HddImage> <Partn> <FileName>` -- extract file from the partition
 * `rt11dsk hpa <HddImage> <Partn> <FileName>` -- add file to the partition

Parameters:
 * `<ImageFile>` is UKNC disk image in .dsk or .rtd format
 * `<HddImage>` is UKNC hard disk image file name
 * `<Partn>` is hard disk image partition number, 0..23
 * `<FileName>` is a file name to read from or save to

Options:
 * `-oXXXXX` -- Set start offset to XXXXX; 0 by default
