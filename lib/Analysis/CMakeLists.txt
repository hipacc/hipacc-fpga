set(KernelStatistics_SOURCES KernelStatistics.cpp)
set(Polly_SOURCES Polly.cpp)
set(HostDataDeps_SOURCES HostDataDeps.cpp)

add_library(hipaccKernelStatistics ${KernelStatistics_SOURCES})
if(USE_POLLY)
    add_library(hipaccPolly ${Polly_SOURCES})
endif()
add_library(hipaccHostDataDeps ${HostDataDeps_SOURCES})
