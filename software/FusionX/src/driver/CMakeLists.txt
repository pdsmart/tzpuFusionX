set(kerneldir "/srv/dvlp/Projects/tzpu/FusionX/software/linux/kernel" CACHE STRING "Path to the kernel build directory")
if("${kerneldir}" STREQUAL "")
  execute_process(COMMAND uname -r OUTPUT_VARIABLE uname_r
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(kerneldir "/lib/modules/${uname_r}/build")
endif()
include(GNUInstallDirs)
include_directories(${kerneldir}/include)
find_file(kernel_makefile NAMES Makefile
                          PATHS ${kerneldir} NO_DEFAULT_PATH)
if(NOT kernel_makefile)
  message(FATAL_ERROR "There is no Makefile in kerneldir!")
endif()
add_library(mymodule-lib STATIC z80drv.c)
get_target_property(module_sources mymodule-lib SOURCES)
list(APPEND module_sources z80drv.c)

#find_package(Z80 REQUIRED Shared)
add_subdirectory(Z80)
target_link_libraries(mymodule-lib Z80)

# the name of the target operating system
set(CMAKE_SYSTEM_NAME Linux)

# which compilers to use for C and C++
set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

string(REPLACE ";" " " module_sources_string "${module_sources}")
configure_file(Kbuild.in Kbuild @ONLY)

foreach(src ${module_sources})
  configure_file(${src} ${src} COPYONLY)
endforeach()

set(module_cmd ${CMAKE_MAKE_PROGRAM} -C ${kerneldir} ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- M=${CMAKE_CURRENT_BINARY_DIR} modules)
add_custom_command(OUTPUT z80drv.ko
  COMMAND ${module_cmd} modules
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${module_sources} ${CMAKE_CURRENT_BINARY_DIR}/Kbuild
  VERBATIM)
add_custom_target(module DEPENDS z80drv.ko)
add_custom_target(module-clean COMMAND ${module_cmd} clean)
