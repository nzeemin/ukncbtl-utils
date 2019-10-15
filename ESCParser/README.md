ESCParser — command-line utility, ESC/P printer emulator.
The current implementation is very close to **Robotron CM 6329.01M** printer. Also the utility uses the font from the printer ROM to "print" symbols.

ESCParser can produce several output formats:
  * PostScript — with multi-page support. Use GSView + Ghostscript to view the output and convert it to other formats.
  * SVG — no multi-page support. You can view the result in any modern web browser.
  * PDF — with multi-page support, zlib is used to compress the blobs. Use Adobe Acrobat Reader or any modern browser to view the result.

Usage examples:
```
  ESCParser -ps printer.log > DOC.ps
  ESCParser -svg printer.log > DOC.svg
  ESCParser -pdf printer.log > DOC.pdf
```
Test sample with ESCParser produces the following result (converted to PNG):

![](https://github.com/nzeemin/ukncbtl-utils/blob/master/ESCParser/ESCParser.png)

#### See Also

* [https://github.com/nzeemin/robotron-dotmatrix-font] — Robotron dot matrix printer font as a web font
* [shokre/node-escprinter](https://github.com/shokre/node-escprinter) — "ESC/P2 printer command emulator with SVG output.", ESCParser NodeJS conversion by shokre.
* [epsonps](https://github.com/christopherkobayashi/TI99Utilities/tree/master/printer_listener/epsonps)
