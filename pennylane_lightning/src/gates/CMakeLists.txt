project(lightning_gates)

set(GATES_FILES GateUtil.cpp DynamicDispatcher.cpp CACHE INTERNAL "" FORCE)

add_library(lightning_gates STATIC ${GATES_FILES})
target_link_libraries(lightning_gates PRIVATE lightning_compile_options
                                              lightning_external_libs
                                              lightning_utils)
target_include_directories(lightning_gates PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET lightning_gates PROPERTY POSITION_INDEPENDENT_CODE ON)
