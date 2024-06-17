cmake_minimum_required(VERSION 3.14)
project(example)

add_executable(calc calc.cc)
target_include_directories(calc PRIVATE ..)
target_link_libraries(calc ${add_link_deps})

add_executable(calc2 calc2.cc)
target_include_directories(calc2 PRIVATE ..)
target_link_libraries(calc2 ${add_link_deps})

add_executable(calc3 calc3.cc)
target_include_directories(calc3 PRIVATE ..)
target_link_libraries(calc3 ${add_link_deps})

add_executable(calc4 calc4.cc)
target_include_directories(calc4 PRIVATE ..)
target_link_libraries(calc4 ${add_link_deps})

add_executable(calc5 calc5.cc)
target_include_directories(calc5 PRIVATE ..)
target_link_libraries(calc5 ${add_link_deps})

add_executable(indent indent.cc)
target_include_directories(indent PRIVATE ..)
target_link_libraries(indent ${add_link_deps})

add_executable(docx docx.cc)
target_include_directories(docx PRIVATE ..)
target_link_libraries(docx ${add_link_deps})

add_executable(sequence sequence.cc)
target_include_directories(sequence PRIVATE ..)
target_link_libraries(sequence ${add_link_deps})

add_executable(enter_leave enter_leave.cc)
target_include_directories(enter_leave PRIVATE ..)
target_link_libraries(enter_leave ${add_link_deps})

add_executable(choice choice.cc)
target_include_directories(choice PRIVATE ..)
target_link_libraries(choice ${add_link_deps})
