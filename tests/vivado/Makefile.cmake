SIZE_X       ?= 3
SIZE_Y       ?= 3

# Configuration
HIPACC_DIR     ?= @CMAKE_INSTALL_PREFIX@
COMPILER       ?= $(HIPACC_DIR)/bin/hipacc
COMMON_INC     ?= -I@OPENCV_INCLUDE_DIR@ \
                  -I$(TEST_CASE) \
                  -I/usr/include
COMPILER_INC   ?= -std=c++11 \
                  -resource-dir `@CLANG_EXECUTABLE@ -print-file-name=` \
                  -I`@CLANG_EXECUTABLE@ -print-file-name=include` \
                  -I`@LLVM_CONFIG_EXECUTABLE@ --includedir` \
                  -I`@LLVM_CONFIG_EXECUTABLE@ --includedir`/c++/v1 \
                  -I$(HIPACC_DIR)/include/dsl \
                  $(COMMON_INC)
TEST_CASE      ?= ./tests/laplace_rgba
MYFLAGS        ?= -DWIDTH=1024 -DHEIGHT=1024 -DSIZE_X=$(SIZE_X) -DSIZE_Y=$(SIZE_Y)
NVCC_FLAGS      = -gencode=arch=compute_$(GPU_ARCH),code=\"sm_$(GPU_ARCH),compute_$(GPU_ARCH)\" \
                  -Xptxas -v @NVCC_COMP@ #-keep
OFLAGS          = -O3

CC_CC           = @CMAKE_CXX_COMPILER@ -std=c++11 -Wall -Wunused
CU_CC           = @NVCC@ $(NVCC_FLAGS) -Xcompiler -Wall -Xcompiler -Wunused
CC_LINK         = -lm -ldl -lstdc++ @TIME_LINK@
CU_LINK         = $(CC_LINK) @CUDA_LINK@
# OpenCL specific configuration
ifeq ($(HIPACC_TARGET),Midgard)
    CL_CC       = @NDK_CXX_COMPILER@ @NDK_CXX_FLAGS@ @NDK_INCLUDE_DIRS_STR@ -std=c++0x -Wall -Wunused
    CL_LINK     = $(CC_LINK) @NDK_LINK_LIBRARIES_STR@ @EMBEDDED_OPENCL_LFLAGS@
    COMMON_INC += @EMBEDDED_OPENCL_CFLAGS@
else
    CL_CC       = $(CC_CC)
    CL_LINK     = $(CC_LINK) @OPENCL_LFLAGS@
    COMMON_INC += @OPENCL_CFLAGS@
endif

# Renderscript specific configuration
RS_TARGET_API = @RS_TARGET_API@
ifge = $(shell if [ $(1) -ge $(2) ]; then echo true; else echo false; fi)


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
HIPACC_TARGET?=Fermi-20


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
	@echo 'Executing HIPAcc Compiler for C++:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-cpu $(HIPACC_OPTS) -o main.cc
	@echo 'Compiling C++ file using g++:'
	$(CC_CC) -I$(HIPACC_DIR)/include $(COMMON_INC) $(MYFLAGS) $(OFLAGS) -o main_cpu main.cc $(CC_LINK)
	@echo 'Executing C++ binary'
	./main_cpu

cuda:
	@echo 'Executing HIPAcc Compiler for CUDA:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-cuda $(HIPACC_OPTS) -o main.cu
	@echo 'Compiling CUDA file using nvcc:'
	$(CU_CC) -I$(HIPACC_DIR)/include $(COMMON_INC) $(MYFLAGS) $(OFLAGS) -o main_cuda main.cu $(CU_LINK)
	@echo 'Executing CUDA binary'
	./main_cuda

opencl-acc opencl-cpu opencl-gpu:
	@echo 'Executing HIPAcc Compiler for OpenCL:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-$@ $(HIPACC_OPTS) -o main.cc
	@echo 'Compiling OpenCL file using g++:'
	$(CL_CC) -I$(HIPACC_DIR)/include $(COMMON_INC) $(MYFLAGS) $(OFLAGS) -o main_opencl main.cc $(CL_LINK)
ifneq ($(HIPACC_TARGET),Midgard)
	@echo 'Executing OpenCL binary'
	./main_opencl
endif

filterscript renderscript:
	rm -f *.rs *.fs
	@echo 'Executing HIPAcc Compiler for $@:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-$@ $(HIPACC_OPTS) -o main.cc
	mkdir -p build_$@
ifeq ($(call ifge, $(RS_TARGET_API), 19), true) # build using ndk-build
	rm -rf build_$@/*
	mkdir -p build_$@/jni
	cp @CMAKE_CURRENT_SOURCE_DIR@/tests/Android.mk.cmake build_$@/jni/Android.mk
	cp main.cc *.$(subst renderscript,rs,$(subst filterscript,fs,$@)) build_$@
	@echo 'Compiling $@ file using llvm-rs-cc and g++:'
	export CASE_FLAGS="$(MYFLAGS)"; \
	export RS_TARGET_API=$(RS_TARGET_API); \
	export HIPACC_INCLUDE=$(HIPACC_DIR)/include; \
	cd build_$@; ndk-build -B APP_PLATFORM=android-$(RS_TARGET_API) APP_STL=stlport_static
	cp build_$@/libs/armeabi/main_renderscript ./main_$@
else
	@echo 'Generating build system current test case:'
	cd build_$@; cmake .. -DANDROID_SOURCE_DIR=@ANDROID_SOURCE_DIR@ -DTARGET_NAME=@TARGET_NAME@ -DHOST_TYPE=@HOST_TYPE@ -DNDK_TOOLCHAIN_DIR=@NDK_TOOLCHAIN_DIR@ -DRS_TARGET_API=$(RS_TARGET_API) $(MYFLAGS)
	@echo 'Compiling $@ file using llvm-rs-cc and g++:'
	cd build_$@; make
	cp build_$@/main_renderscript ./main_$@
endif

export CPLUS_INCLUDE_PATH := $(CPLUS_INCLUDE_PATH):@CMAKE_INSTALL_PREFIX@/include

vivado:
	@echo 'Executing HIPAcc Compiler for Vivado HLS:'
	$(COMPILER) $(TEST_CASE)/main.cpp $(MYFLAGS) $(COMPILER_INC) -emit-vivado $(HIPACC_OPTS) -o main.cc
	vivado_hls -f script.tcl

clean:
	rm -f main_* *.cu *.cc *.cubin *.cl *.isa *.rs *.fs *.log
	rm -rf hipacc_project
	rm -rf build_*

