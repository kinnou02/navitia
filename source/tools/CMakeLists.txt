add_executable(dumpsn dumpsn.cpp)
target_link_libraries(dumpsn data)

add_executable(merge merge_sp_lyon.cpp)
target_link_libraries(merge data)
