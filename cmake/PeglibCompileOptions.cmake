include_guard(GLOBAL)

if(MSVC)
  target_compile_options(
    CppPegLib

    INTERFACE

    /Zc:__cplusplus
    /utf-8
    /D_CRT_SECURE_NO_DEPRECATE
    /W3
  )
else()
  target_compile_options(
    CppPegLib

    INTERFACE

    -Wall
    -Wextra
  )
endif()
