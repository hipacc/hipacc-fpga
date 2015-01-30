# HIPAcc Vivado #

## Install ##

This fork of HIPAcc is compiled and installed the same way as HIPAcc itself.
See [HIPAcc documentation](http://hipacc-lang.org/install.html) for detailed
information.

## Run ##

Make sure that the binary `vivado_hls` can be found in your `PATH` and run the
following commands in order to build provided test cases:

```bash
cd $(CMAKE_INSTALL_PREFIX)/tests/vivado
make vivado TEST_CASE=tests/laplace_rgba
make vivado TEST_CASE=tests/harris_corner
make vivado TEST_CASE=tests/optical_flow
make vivado TEST_CASE=tests/block_matching
```

