# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/matthewkweon/esp/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/Users/matthewkweon/esp/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader"
  "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix"
  "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix/tmp"
  "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix/src"
  "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/matthewkweon/Documents/GitHub/02-Team3-Kweon-Connell-Xia-Yu/quest-3/code/udp_server/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
