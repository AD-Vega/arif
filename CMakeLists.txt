project(arif)
cmake_minimum_required(VERSION 2.8)
find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL QtNetwork QtSVG)
include(${QT_USE_FILE})
include(FindPkgConfig)

macro(set_prefixed varname string)
  set(${varname})
  foreach(name ${ARGN})
    set( ${varname} ${${varname}} ${string}${name} )
  endforeach(name)
endmacro(set_prefixed)

find_path(QWT_INCLUDE qwt_plot.h PATH_SUFFIXES qwt6 qwt)
find_package(OpenCV REQUIRED)
pkg_check_modules(PKGCONFS REQUIRED qarv-1 libswscale libavcodec libavutil tclap)
find_library(QWT_LIB NAMES qwt6 qwt)
find_package(Boost COMPONENTS system REQUIRED)

string(REPLACE ";" " " PKGCONFS_CFLAGS "${PKGCONFS_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall ${PKGCONFS_CFLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src ${QWT_INCLUDE})

set_prefixed(arif_videosources_SRC videosources/
  interfaces.cpp
  rawvideo.cpp
  images.cpp
  aravis.cpp
  qarvvideo.cpp
)
set_prefixed(arif_videosources_MOC videosources/
  interfaces.h
  rawvideo.h
  images.h
  aravis.h
  qarvvideo.h
)
set_prefixed(arif_UI_pre src/
  arifmainwindow.ui
)
set_prefixed(arif_SRC src/
  ${arif_videosources_SRC}
  main.cpp
  glvideowidget.cpp
  arifmainwindow.cpp
  foreman.cpp
  processing.cpp
  sourceselectionwindow.cpp
  qcustomplot.cpp
  plotwidgets.cpp
)
set_prefixed(arif_MOC src/
  ${arif_videosources_MOC}
  glvideowidget.h
  arifmainwindow.h
  foreman.h
  sourceselectionwindow.h
  qcustomplot.h
  plotwidgets.h
)
qt4_wrap_ui(arif_UI
  ${arif_UI_pre}
)
qt4_wrap_cpp(arif_MOC_MOCD
  ${arif_MOC}
)
add_executable(arif
  ${arif_SRC}
  ${arif_MOC_MOCD}
  ${arif_UI}
)
target_link_libraries(arif
  ${QT_LIBRARIES}
  ${SWSCALE_LIB}
  ${QARV_LIB}
  ${QWT_LIB}
  ${PKGCONFS_LDFLAGS}
  ${OpenCV_LIBS}
  ${Boost_LIBRARIES}
)
