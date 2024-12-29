# gwiond

[Language Server](https://langserver.org/) implementation for the [Gwion](https://github.com/Gwion/Gwion) programming language.

## Features

* [x] Diagnostics
* [x] Hover information
* [x] Code completion
* [x] Go to definition/declaration/implementation
* [x] Go to type definition
* [x] Formatting
* [x] Folding
* [x] References
* [x] Renaming
* [-] Signatures
* [x] Selection range
* [ ] Inlay hints
* [ ] Inline values (needs DAP?)
* [ ] Call tree
* [ ] Type tree
* [ ] Highlight
* [x] Symbols
* [ ] Semantic tokens

## Prerequisites

Runtime dependencies:

* cJSON

Compile time dependencies:

* C standard compiler
* cJSON
* pkgconf
* make

## Installation

Compile using GNU make utility
```bash
make
```

## Notes

Based on [minic-lsp](https://github.com/BojanStipic/minic-lsp)
