# BtrBlocks - Efficient Columnar Compression for Data Lakes

[![Paper](https://img.shields.io/badge/paper-SIGMOD%202023-green)](https://bit.ly/btrblocks)

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
