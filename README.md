# BtrBlocks - Efficient Columnar Compression for Data Lakes

[Paper](https://bit.ly/btrblocks)

## Components

- `include/`: public headers when using this as a library
- `btrblocks/`: the compression library, schemes, utilities, ...
- `test/`: rudimentary tests for the library
- `tools/`: various conversion and measurement tools
- `benchmarks/`: comparisons to other compression formats, S3 throughput benchmarks, ...

Refer to the README files in each folder for more information.

## Usage

After [building](#building) the library, you can use it by including the [`btrblocks.h`](btrblocks/btrblocks.h) header.

## Building

``` sh
mkdir build 
cd build
cmake ..
```

Then, depending on your usecase, build only the library or any of the tools:
- build everything: `make`
- build the library: `make btrblocks`
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
