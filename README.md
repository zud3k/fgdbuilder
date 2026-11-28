# FGDBuilder

A small GUI tool for creating and editing FGD files for Source engine games.
Written in C++ with Qt 6.

## Screenshots

<p>
  <img src="https://i.ibb.co/FqxMq5WK/fgdbuilder-RVF9-Puo-AOa.png" width="639" alt="FGDBuilder main window"/>
</p>

## What it does
 
- Opens and saves FGD files. After an import it shows a report of what was kept as raw text, skipped, or worth a warning.
- Supports all class types, including `@ExtendClass`. Helpers the editor has no field for are kept as text and written back unchanged.
- Loads `@include` files, so inherited keyvalues, inputs and outputs show up in the editor and in the validator.
- Checks a project against the limits from the Valve Developer Community FGD page (Project > Check for Problems).
- Multi selection, search through properties and descriptions, editing a class as raw FGD text, undo and redo.

## Command line
 
The same executable works without a window:
 
```bash
fgdbuilder --export project.fgdbp output.fgd
fgdbuilder --check some.fgd
```
 
Both take a project (`.fgdbp`) or an FGD file. `@include` files are looked up in the folder of the input file. Add `-v` to also print info messages.
 
Exit codes:
 
- `0` no errors
- `1` validation or syntax errors found (`--export` still writes the file)
- `2` the file could not be read, or the arguments are wrong

## Building

You need Qt 6 and qmake, see [BUILD.md](BUILD.md).

## License

Copyright © 2026 zud3k  
FGDBuilder is licensed under the GNU General Public License v3.0 (GPLv3).  
See the LICENSE file for details.
