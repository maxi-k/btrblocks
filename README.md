# BtrBlocks - Efficient Columnar Compression for Data Lakes

[![Paper](https://img.shields.io/badge/paper-SIGMOD%202023-green)](https://bit.ly/btrblocks)
[![Build](https://github.com/maxi-k/btrblocks/actions/workflows/test.yml/badge.svg?event=push)](https://github.com/maxi-k/btrblocks/actions/workflows/test.yml)

- [Paper](https://bit.ly/btrblocks) (two-column version)
- [Video](https://dl.acm.org/doi/10.1145/3589263) (SIGMOD 2023 presentation)

## Usage

After [building](#building) the library, follow one of the [examples](./tools/examples) to get started.

## Components

- `btrblocks/`: the compression library, schemes, utilities, ...
- `btrfiles/`: helper library for binary files and yaml schema information
- `tools/`: various conversion, measurement and benchmarking tools
- `test/`: rudimentary tests for the library

![Dependency Graph](./doc/dependencies.svg)

## Building

``` sh
mkdir build 
cd build
cmake ..
```

Then, depending on your usecase, build only the library or any of the tools:
- build everything: `make`
- install static library and headers on your system: `sudo make install`
- build the compression library only: `make btrblocks`
- build the tests `make tester`
- build the in-memory decompression speed benchmark: `make decompression_speed`
- ...

For a list of all valid targets, run `make help`.

## Contributors

Adnan Alhomssi
David Sauerwein
Maximilian Kuschewski

## License

MIT - See [License File](LICENSE)
