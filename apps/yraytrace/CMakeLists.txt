add_executable(yraytrace  yraytrace.cpp)

set_target_properties(yraytrace PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(yraytrace  PRIVATE ${CMAKE_SOURCE_DIR}/libs)
target_link_libraries(yraytrace yocto yocto_raytrace)

if(YOCTO_OPENGL)
target_link_libraries(yraytrace yocto_gui)
endif(YOCTO_OPENGL)
