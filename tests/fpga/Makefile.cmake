SIZE_X ?= 3
SIZE_Y ?= 3

# Configuration
HIPACC_DIR     ?= @CMAKE_INSTALL_PREFIX@
COMPILER       ?= $(HIPACC_DIR)/bin/hipacc
COMMON_INC     ?= -I@OpenCV_INCLUDE_DIRS@ \
                  -I$(TEST_CASE)
COMPILER_INC   ?= -std=c++11 \
                  -I`@llvm-config@ --includedir` \
                  -I`@llvm-config@ --includedir`/c++/v1 \
                  -I`@clang@ -print-file-name=include` \
                  -I$(HIPACC_DIR)/include/dsl \
                  $(COMMON_INC)
TEST_CASE      ?= ./tests/laplace_rgba
MYFLAGS        ?= -DWIDTH=1024 -DHEIGHT=1024 -DSIZE_X=$(SIZE_X) -DSIZE_Y=$(SIZE_Y)
NVCC_FLAGS      = -gencode=arch=compute_$(GPU_ARCH),code=\"sm_$(GPU_ARCH),compute_$(GPU_ARCH)\" -res-usage #-keep
OFLAGS          = -O3

CC_CC           = @CMAKE_CXX_COMPILER@ -std=c++11 @THREADS_ARG@ -Wall -Wunused
CL_CC           = $(CC_CC) -I@OpenCL_INCLUDE_DIRS@
CU_CC           = @NVCC@ -std=c++11 @NVCC_COMP@ -Xcompiler -Wall -Xcompiler -Wunused -I@CUDA_INCLUDE_DIRS@ $(NVCC_FLAGS)
CC_LINK         = -lm -ldl -lstdc++ @THREADS_LINK@ @RT_LIBRARIES@ @OpenCV_LIBRARIES@
CL_LINK         = $(CC_LINK) @OpenCL_LIBRARIES@
CU_LINK         = $(CC_LINK) @NVCC_LINK@
ALTERA_RUN      = LD_LIBRARY_PATH=$$LD_LIBRARAY_PATH$$(aocl link-config | sed 's/\ /\n/g' | grep "^\-L" | sed 's/-L/:/g' | tr -d '\n')
ALTERA_CXX     := arm-linux-gnueabihf-g++


# Source-to-source compiler configuration
# use local memory -> set HIPACC_LMEM to off|on
# use texture memory -> set HIPACC_TEX to off|Linear1D|Linear2D|Array2D|Ldg
# vectorize code (experimental, doesn't work) -> set HIPACC_VEC to off|on
# pad images to a multiple of n bytes -> set HIPACC_PAD to n
# map n output pixels to one thread -> set HIPACC_PPT to n
# use specific configuration for kernels -> set HIPACC_CONFIG to nxm
# generate code that explores configuration -> set HIPACC_EXPLORE to off|on
# generate code that times kernel execution -> set HIPACC_TIMING to off|on
HIPACC_LMEM?=off
HIPACC_TEX?=off
HIPACC_VEC?=off
HIPACC_PPT?=1
HIPACC_CONFIG?=128x1
HIPACC_EXPLORE?=off
HIPACC_TIMING?=off
HIPACC_TARGET?=Kepler-30


HIPACC_OPTS=-target $(HIPACC_TARGET)
ifdef HIPACC_PAD
    HIPACC_OPTS+= -emit-padding $(HIPACC_PAD)
endif
ifdef HIPACC_LMEM
    HIPACC_OPTS+= -use-local $(HIPACC_LMEM)
endif
ifdef HIPACC_TEX
    HIPACC_OPTS+= -use-textures $(HIPACC_TEX)
endif
ifdef HIPACC_PPT
    HIPACC_OPTS+= -pixels-per-thread $(HIPACC_PPT)
endif
ifdef HIPACC_VEC
    HIPACC_OPTS+= -vectorize $(HIPACC_VEC)
endif
ifdef HIPACC_CONFIG
    HIPACC_OPTS+= -use-config $(HIPACC_CONFIG)
endif
ifeq ($(HIPACC_EXPLORE),on)
    HIPACC_OPTS+= -explore-config
endif
ifeq ($(HIPACC_TIMING),on)
    HIPACC_OPTS+= -time-kernels
endif
ifdef HIPACC_TARGET_II
    HIPACC_OPTS+= -target-II $(HIPACC_TARGET_II)
endif

# set target GPU architecture to the compute capability encoded in target
GPU_ARCH := $(shell echo $(HIPACC_TARGET) |cut -f2 -d-)


all:
run:
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC)

cpu:
	@echo 'Executing Hipacc Compiler for C++:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-cpu $(HIPACC_OPTS) -o main.cc
	@echo 'Compiling C++ file using c++:'
	$(CC_CC) -I$(HIPACC_DIR)/include $(COMMON_INC) $(MYFLAGS) $(OFLAGS) -o main_cpu main.cc $(CC_LINK)
	@echo 'Executing C++ binary'
	./main_cpu

cuda:
	@echo 'Executing Hipacc Compiler for CUDA:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-cuda $(HIPACC_OPTS) -o main.cu
	@echo 'Compiling CUDA file using nvcc:'
	$(CU_CC) -I$(HIPACC_DIR)/include $(COMMON_INC) $(MYFLAGS) $(OFLAGS) -o main_cuda main.cu $(CU_LINK)
	@echo 'Executing CUDA binary'
	./main_cuda

opencl-acc opencl-cpu opencl-gpu:
	@echo 'Executing Hipacc Compiler for OpenCL:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-$@ $(HIPACC_OPTS) -o main.cc
	@echo 'Compiling OpenCL file using c++:'
	$(CL_CC) -I$(HIPACC_DIR)/include $(COMMON_INC) $(MYFLAGS) $(OFLAGS) -o main_opencl main.cc $(CL_LINK)
	@echo 'Executing OpenCL binary'
	./main_opencl

opencl-fpga:
	@echo 'Executing Hipacc Compiler for OpenCL:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-$@ $(HIPACC_OPTS) -o main.cc

altera-emulate: opencl-fpga
	@echo 'Compiling Host Code for Altera Emulation'
	g++ -DALTERACL $(MYFLAGS) -std=c++11 -fPIC -I$(HIPACC_DIR)/include $(shell aocl compile-config) -Wl,--no-as-needed $(shell aocl link-config) -lstdc++ -static-libstdc++ -o main_altera_em main.cc
	echo 'Generate aocx for Altera Emulation'
	aoc $(MYFLAGS) -I$(HIPACC_DIR)/include -v -march=emulator hipacc_run.cl
	@echo 'Emulate Generated Binaries'
	CL_CONTEXT_EMULATOR_DEVICE_ALTERA=s5_ref $(ALTERA_RUN) ./main_altera_em

altera-compile: opencl-fpga
	@echo 'Compiling Host Code for Target Architecture'
	$(ECHO)$(ALTERA_CXX) -DALTERACL $(MYFLAGS) -std=c++11 -fPIC -I$(HIPACC_DIR)/include $(shell aocl compile-config) main.cc $(shell aocl link-config) -static-libstdc++ -o ./main_altera_syn
	@echo 'Generate aocx for Target Altera Device'
	aoc $(MYFLAGS) -I$(HIPACC_DIR)/include -v --report hipacc_run.cl

filterscript renderscript:
	rm -f *.rs *.fs
	@echo 'Executing Hipacc Compiler for $@:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-$@ $(HIPACC_OPTS) -o main.cc
	rm -rf build_$@/*
	mkdir -p build_$@
	mkdir -p build_$@/jni
	cp Android.mk build_$@/jni/Android.mk
	cp Application.mk build_$@/jni/Application.mk
	cp main.cc *.$(subst renderscript,rs,$(subst filterscript,fs,$@)) build_$@ || true
	@echo 'Compiling $@ file using ndk-build:'
	export CASE_FLAGS="$(MYFLAGS)"; \
	export HIPACC_INCLUDE=$(HIPACC_DIR)/include; \
	cd build_$@; @Renderscript_ndk_build_EXECUTABLE@ -B
	cp build_$@/libs/armeabi-v7a/main_renderscript ./main_$@
	adb shell mkdir -p /data/local/tmp
	adb push main_$@ /data/local/tmp
	adb shell /data/local/tmp/main_$@

vivado:
	@echo 'Executing HIPAcc Compiler for Vivado HLS:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-vivado $(HIPACC_OPTS) -o main.cc
	export CPLUS_INCLUDE_PATH=$(CPLUS_INCLUDE_PATH):$(HIPACC_DIR)/include; \
	  vivado_hls -f script.tcl

clean:
	rm -f main_* *.cu *.cc *.cubin *.cl *.isa *.rs *.fs *.aoco *.aocx *.log
	rm -rf hipacc_project
	rm -rf build_* bin_*

