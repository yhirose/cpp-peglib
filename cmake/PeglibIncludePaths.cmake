include_guard(GLOBAL)

# When building via cmakelists like this, the peglib.h file is located
# simply at the top of the peglib source tree.
get_target_property(_source_folder CppPegLib SOURCE_FOLDER)

target_include_directories(
  CppPegLib

  INTERFACE

  ${_source_folder}
)

unset(_source_folder)
