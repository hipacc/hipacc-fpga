# Hipacc FPGA
A domain-specific language and compiler for image processing

Hipacc allows to design image processing kernels and algorithms in a domain-specific language (DSL).
From this high-level description, low-level target code for GPU accelerators is generated using source-to-source translation.
As back ends, the framework supports C/C++, CUDA, OpenCL, and Renderscript.

# Install
See [Hipacc documentation](http://hipacc-lang.org/install.html) and [Install notes](INSTALL.md) for detailed information.

# Run Samples

Make sure that the binary `vivado_hls` can be found in your `PATH` and follow
the instructions [here](INSTALL.md#run-samples).
