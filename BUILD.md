# Building FGDBuilder

| | Windows | Linux |
|---|---|---|
| Compiler | MinGW-w64 (MSYS2) | GCC with C++17 |
| Qt | Qt 6 (Core + Gui + Widgets) | Qt 6 (Core + Gui + Widgets) |
| Build tools | qmake | qmake |

---

## Windows (MSYS2 / MinGW-w64)

```bash
qmake fgdbuilder.pro
mingw32-make
```

---

## Linux

```bash
qmake fgdbuilder.pro
make
```
