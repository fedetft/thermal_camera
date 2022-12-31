
project(QTSIMULATOR)
cmake_minimum_required(VERSION 3.1)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)

set(MXGUI_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../mxgui)
set(QTSIMULATOR_DIRECTORY ${MXGUI_DIRECTORY}/_tools/qtsimulator)

# Function to handle embedding images in the executable binary
# It basically calls the pngconverter tool and generates the corresponding
# .cpp and .h file in the build directory
# Adapted from http://www.cmake.org/pipermail/cmake/2010-June/037733.html
find_program(PNGCONV_EXECUTABLE pngconverter PATHS ${MXGUI_DIRECTORY}/_tools/code_generators/build)
function(preprocess_png out_var)
  set(result)
  foreach(file ${ARGN})
    get_filename_component(basename ${file} NAME_WE)
    get_filename_component(path ${file} PATH)
    set(png "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
    set(cpp "${CMAKE_CURRENT_SOURCE_DIR}/${path}/${basename}.cpp")
    set(h   "${CMAKE_CURRENT_SOURCE_DIR}/${path}/${basename}.h")
    add_custom_command(OUTPUT ${cpp} ${h}
      COMMAND ${PNGCONV_EXECUTABLE} --in ${png} --depth 16
      DEPENDS ${png}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Preprocessing ${file}"
      VERBATIM
      )
    set_source_files_properties(${cpp} PROPERTIES GENERATED 1)
    set_source_files_properties(${h}   PROPERTIES GENERATED 1)
    list(APPEND result ${cpp} ${h})
  endforeach()
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# List here .png files to be embedded in the executable
set(FOO_IMG
    ../../images/batt0icon.png 
    ../../images/batt25icon.png
    ../../images/batt50icon.png
    ../../images/batt75icon.png
    ../../images/batt100icon.png
    ../../images/miosixlogoicon.png
    ../../images/emissivityicon.png
    ../../images/smallcelsiusicon.png
    ../../images/largecelsiusicon.png
    ../../images/pauseicon.png)
preprocess_png(IMG_OUT ${FOO_IMG})

# List here your application files
set(FOO_SRCS
    simapplication.cpp
    ../../renderer.cpp
    ../../colormap.cpp
    ../../textbox.cpp)

# These are the sources of the mxgui library and the simulator
set(LIB_SRCS
    ${MXGUI_DIRECTORY}/font.cpp
    ${MXGUI_DIRECTORY}/misc_inst.cpp
    ${MXGUI_DIRECTORY}/display.cpp
    ${MXGUI_DIRECTORY}/resourcefs.cpp
    ${MXGUI_DIRECTORY}/resource_image.cpp
    ${MXGUI_DIRECTORY}/tga_image.cpp
    ${MXGUI_DIRECTORY}/level2/input.cpp
    ${MXGUI_DIRECTORY}/level2/application.cpp
    ${MXGUI_DIRECTORY}/level2/drawing_context_proxy.cpp
    ${MXGUI_DIRECTORY}/level2/label.cpp
    ${MXGUI_DIRECTORY}/level2/simple_plot.cpp
    ${MXGUI_DIRECTORY}/drivers/display_qt.cpp
    ${MXGUI_DIRECTORY}/drivers/event_qt.cpp
    ${QTSIMULATOR_DIRECTORY}/qtbackend.cpp
    ${QTSIMULATOR_DIRECTORY}/window.cpp
    ${QTSIMULATOR_DIRECTORY}/qtsimulator.cpp)

set(FOO_HEADERS ${QTSIMULATOR_DIRECTORY}/window.h)

# ../../.. is the main project directory
include_directories(../..)
include_directories(${MXGUI_DIRECTORY})
include_directories(${QTSIMULATOR_DIRECTORY})
add_definitions(-DMXGUI_LIBRARY)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
qt5_wrap_cpp(FOO_HEADERS_MOC ${FOO_HEADERS})
add_executable(qtsimulator ${LIB_SRCS} ${FOO_SRCS} ${IMG_OUT} ${FOO_HEADERS_MOC})
target_link_libraries(qtsimulator Qt5::Widgets)
set(BOOST_LIBS thread filesystem system)
find_package(Boost COMPONENTS ${BOOST_LIBS} REQUIRED)
target_link_libraries(qtsimulator ${Boost_LIBRARIES})
target_include_directories(qtsimulator PRIVATE ${Boost_INCLUDE_DIRS})
find_package(Threads REQUIRED)
target_link_libraries(qtsimulator ${CMAKE_THREAD_LIBS_INIT})