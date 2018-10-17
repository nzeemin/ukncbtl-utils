ESCParser -- command-line utility, ESC/P printer emulator.
The current implementation is very close to **Robotron CM 6329.01M** printer. Also the utility uses the font from the printer ROM to "print" symbols.

ESCParser can produce two output formats:
  * PostScript -- with multi-page support. You can use GSView + Ghostscript to view the output and convert it to other formats.
  * SVG -- no multi-page support. You can view the result in any modern web browser.

Usage examples:
```
  ESCParser -ps printer.log > DOC.ps
  ESCParser -svg printer.log > DOC.svg
```
Test sample with ESCParser produces the following result (converted to PNG):

![](https://github.com/nzeemin/ukncbtl-utils/blob/master/ESCParser/ESCParser.png)

##### See Also

* [**shokre/node-escprinter**](https://github.com/shokre/node-escprinter) -- NodeJS version by shokre.
