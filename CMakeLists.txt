project(arif)
cmake_minimum_required(VERSION 3.9)

include(FindPkgConfig)
include(CheckSymbolExists)

macro(set_prefixed varname string)
  set(${varname})
  foreach(name ${ARGN})
    set( ${varname} ${${varname}} ${string}${name} )
  endforeach(name)
endmacro(set_prefixed)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS
             Core Gui Widgets Network Svg Concurrent PrintSupport LinguistTools)
set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::Concurrent Qt5::PrintSupport)

find_package(OpenCV REQUIRED)
pkg_check_modules(PKGCONFS REQUIRED qarv-3 libswscale libavcodec libavutil tclap)
pkg_check_modules(GIO REQUIRED gio-2.0)
find_package(Boost COMPONENTS system REQUIRED)

string(REPLACE ";" " " PKGCONFS_CFLAGS "${PKGCONFS_CFLAGS}")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall ${PKGCONFS_CFLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")

include_directories(${GIO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
qt5_wrap_ui(arif_UI
  ${arif_UI_pre}
)
qt5_wrap_cpp(arif_MOC_MOCD
  ${arif_MOC}
)
qt5_add_resources(arif_QRC res/icons/icons.qrc)
add_executable(arif
  ${arif_SRC}
  ${arif_MOC_MOCD}
  ${arif_UI}
  ${arif_QRC}
)
target_link_libraries(arif
  ${QT_LIBRARIES}
  ${SWSCALE_LIB}
  ${QARV_LIB}
  ${PKGCONFS_LDFLAGS}
  ${OpenCV_LIBS}
  ${Boost_LIBRARIES}
)

install(TARGETS arif
  RUNTIME DESTINATION bin/
  LIBRARY DESTINATION lib/)
install(FILES res/arif.desktop
  DESTINATION share/applications/)
install(FILES res/icons/arif.svgz
  RENAME application-x-arif.svgz
  DESTINATION share/icons/)
