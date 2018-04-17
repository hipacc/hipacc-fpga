# Hipacc FPGA
A domain-specific language and compiler for image processing

Hipacc allows to design image processing kernels and algorithms in a domain-specific language (DSL).
From this high-level description, low-level target code for GPU accelerators is generated using source-to-source translation.
As back ends, the framework supports C/C++, CUDA, OpenCL, and Renderscript.

## Install ##
See [Hipacc documentation](http://hipacc-lang.org/install.html) and [Install notes](INSTALL) for detailed information.

## Run Vivado HLS ##

Make sure that the binary `vivado_hls` can be found in your `PATH` and run the
following commands in order to build provided test cases:

```bash
cd $(CMAKE_INSTALL_PREFIX)/tests/fpga
make vivado TEST_CASE=tests/laplace_rgba
make vivado TEST_CASE=tests/harris_corner
make vivado TEST_CASE=tests/optical_flow
make vivado TEST_CASE=tests/block_matching
```
