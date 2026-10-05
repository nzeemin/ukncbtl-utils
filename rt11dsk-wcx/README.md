## rt11dsk-wcx
Total Commander packer plugin (WCX) for RT-11 floppy disk images (`.dsk`).

 * Browse, view and copy files out of the image (F5, F3)
 * Copy files into the image (F5 into the opened image), replace existing files, delete files (F8)
 * File names must fit RT-11 6.3 format, characters `A-Z 0-9 $`

Build: open `rt11dsk-wcx-VS2013.sln`; Win32 builds `rt11dsk.wcx`, x64 builds `rt11dsk.wcx64`.
Install: put `rt11dsk.wcx`, `rt11dsk.wcx64` and `pluginst.inf` into a zip and open it in Total Commander.

Limitations: no creating new images; catalog segment split is not implemented (adding a file
fails when the segment is full); hard disk images are not supported yet.
