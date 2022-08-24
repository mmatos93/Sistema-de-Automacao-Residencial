# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/neves/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/neves/projeto-se-automacao/build/bootloader"
  "C:/Users/neves/projeto-se-automacao/build/bootloader-prefix"
  "C:/Users/neves/projeto-se-automacao/build/bootloader-prefix/tmp"
  "C:/Users/neves/projeto-se-automacao/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/neves/projeto-se-automacao/build/bootloader-prefix/src"
  "C:/Users/neves/projeto-se-automacao/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/neves/projeto-se-automacao/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
