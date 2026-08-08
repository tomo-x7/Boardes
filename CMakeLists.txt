cmake_minimum_required(VERSION 3.16)

project(Boardes VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Qt6.10.2 で開発しているが、パッチバージョンを固定すると他バージョンでビルドできず脆いので
# EXACT は付けない (6.10 系であれば動く)。
# Svg は画像エクスポート (QSvgGenerator) のためだけに使う。CI 側は Qt インストール時に
# modules: qtsvg の指定が必要 (Qt6 では addon モジュール扱いのため既定では入らない)。
find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets Svg Test)

# ------------------------------------------------------------------------
# boardes_core: main.cpp を除く全ロジック。Boardes 本体と BoardesTests の両方が
# これをリンクすることで、テストが実アプリと同じコードを検証できるようにする。
# ------------------------------------------------------------------------
set(BOARDES_CORE_SOURCES
	src/core/util.h
	src/core/units.h
	src/core/ids.h
	src/core/geometry.h

	src/model/part.h
	src/model/part.cpp
	src/model/board.h
	src/model/board.cpp
	src/model/placement.h
	src/model/wire.h
	src/model/wire.cpp
	src/model/library.h
	src/model/library.cpp
	src/model/librarymanager.h
	src/model/librarymanager.cpp
	src/model/document.h
	src/model/document.cpp
	src/model/netlist.h
	src/model/netlist.cpp
	src/model/drc.h
	src/model/drc.cpp
	src/model/stats.h
	src/model/stats.cpp

	src/io/jsonutil.h
	src/io/loadresult.h
	src/io/zipio.h
	src/io/zipio.cpp
	src/io/partio.h
	src/io/partio.cpp
	src/io/boardio.h
	src/io/boardio.cpp
	src/io/libraryio.h
	src/io/libraryio.cpp
	src/io/documentio.h
	src/io/documentio.cpp
	src/io/shiftjis.h
	src/io/shiftjis.cpp
	src/io/passimport.h
	src/io/passimport.cpp
	src/io/imageexport.h
	src/io/imageexport.cpp

	src/vendor/miniz.h
	src/vendor/miniz.c
	src/vendor/shiftjis_table.h

	src/render/artworkcache.h
	src/render/artworkcache.cpp
	src/render/boardscene.h
	src/render/boardscene.cpp
	src/render/boardview.h
	src/render/boardview.cpp
	src/render/items/boarditem.h
	src/render/items/boarditem.cpp
	src/render/items/placementitem.h
	src/render/items/placementitem.cpp
	src/render/items/wireitem.h
	src/render/items/wireitem.cpp
	src/render/items/labelitem.h
	src/render/items/labelitem.cpp
	src/render/items/overlayitem.h
	src/render/items/overlayitem.cpp

	src/commands/placementcommands.h
	src/commands/placementcommands.cpp
	src/commands/wirecommands.h
	src/commands/wirecommands.cpp
	src/commands/boardcommands.h
	src/commands/boardcommands.cpp

	src/ui/about.h
	src/ui/about.cpp
	src/ui/about.ui
	src/ui/helphint.h
	src/ui/zoombar.h
	src/ui/zoombar.cpp
	src/ui/viewlink.h
	src/ui/viewlink.cpp
	src/ui/partselector.h
	src/ui/partselector.cpp
	src/ui/objectlistpanel.h
	src/ui/objectlistpanel.cpp
	src/ui/drcpanel.h
	src/ui/drcpanel.cpp
	src/ui/statspanel.h
	src/ui/statspanel.cpp
	src/ui/boardeditordialog.h
	src/ui/boardeditordialog.cpp
	src/ui/parteditordialog.h
	src/ui/parteditordialog.cpp
	src/ui/licensepickerwidget.h
	src/ui/licensepickerwidget.cpp
	src/ui/librarymetadatadialog.h
	src/ui/librarymetadatadialog.cpp
	src/ui/duplicatelibrarydialog.h
	src/ui/duplicatelibrarydialog.cpp
	src/ui/librarymanagerdialog.h
	src/ui/librarymanagerdialog.cpp
	src/ui/partpickerdialog.h
	src/ui/partpickerdialog.cpp
	src/ui/exportpackagedialog.h
	src/ui/exportpackagedialog.cpp
	src/ui/exportimageoptionsdialog.h
	src/ui/exportimageoptionsdialog.cpp
	src/ui/actionregistry.h
	src/ui/actionregistry.cpp
	src/ui/toolbarlayout.h
	src/ui/toolbarlayout.cpp
	src/ui/toolbarcustomizedialog.h
	src/ui/toolbarcustomizedialog.cpp
	src/ui/input/inputgesture.h
	src/ui/input/inputgesture.cpp
	src/ui/input/commandregistry.h
	src/ui/input/commandregistry.cpp
	src/ui/input/keymap.h
	src/ui/input/keymap.cpp
	src/ui/input/keymapdialog.h
	src/ui/input/keymapdialog.cpp
	src/ui/input/gesturecapturedialog.h
	src/ui/input/gesturecapturedialog.cpp
	src/ui/tools/toolcontext.h
	src/ui/tools/toolcontext.cpp
	src/ui/tools/tool.h
	src/ui/tools/toolmanager.h
	src/ui/tools/toolmanager.cpp
	src/ui/tools/snapengine.h
	src/ui/tools/selecttool.h
	src/ui/tools/selecttool.cpp
	src/ui/tools/placeparttool.h
	src/ui/tools/placeparttool.cpp
	src/ui/tools/wiretool.h
	src/ui/tools/wiretool.cpp
	src/ui/tools/drafttool.h
	src/ui/tools/drafttool.cpp
	src/ui/mainwindow.h
	src/ui/mainwindow.cpp
	src/ui/mainwindow.ui
)

add_library(boardes_core STATIC ${BOARDES_CORE_SOURCES})
target_include_directories(boardes_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(boardes_core PUBLIC Qt6::Widgets Qt6::Svg)

# vendor/miniz.c は第三者の C ソース。プロジェクトの警告設定を適用しない。
set_source_files_properties(src/vendor/miniz.c PROPERTIES COMPILE_OPTIONS "")

# ------------------------------------------------------------------------
# Boardes 本体
# ------------------------------------------------------------------------
qt_add_executable(Boardes
	MANUAL_FINALIZATION
	src/main.cpp
)
target_link_libraries(Boardes PRIVATE boardes_core)

set_target_properties(Boardes PROPERTIES
	MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
	MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
	MACOSX_BUNDLE TRUE
	WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Boardes
	BUNDLE DESTINATION .
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(Boardes)

# ------------------------------------------------------------------------
# テスト
# ------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
