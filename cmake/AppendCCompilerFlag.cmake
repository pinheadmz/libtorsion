# AppendCCompilerFlag.cmake - checked c flags appending
# Copyright (c) 2020, Christopher Jeffrey (MIT License).
# https://github.com/chjj

if(DEFINED __APPEND_C_COMPILER_FLAG__)
  return()
endif()

set(__APPEND_C_COMPILER_FLAG__ 1)

include(CheckCCompilerFlag)

function(append_c_compiler_flag list)
  set(flags ${ARGV})
  list(REMOVE_AT flags 0)

  foreach(flag IN LISTS flags)
    string(TOUPPER "CMAKE_HAVE_C_FLAG${flag}" name)
    string(REGEX REPLACE "[^A-Z0-9]" "_" name "${name}")

    check_c_compiler_flag(${flag} ${name})

    if(${name})
      list(APPEND ${list} ${flag})
    endif()
  endforeach()

  set(${list} ${${list}} PARENT_SCOPE)
endfunction()
