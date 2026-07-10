cmake_minimum_required(VERSION 3.21)

project(image_compare VERSION 1.1.4 LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
add_compile_definitions(QT_NO_KEYWORDS)

option(IMAGECOMPARE_FLATPAK "Enable Flatpak-specific portal integration" OFF)

set(IMAGECOMPARE_QT_COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
if(IMAGECOMPARE_FLATPAK)
    list(APPEND IMAGECOMPARE_QT_COMPONENTS DBus)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS ${IMAGECOMPARE_QT_COMPONENTS})
if(IMAGECOMPARE_FLATPAK)
    find_package(KF6CoreAddons REQUIRED)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPSCPP REQUIRED IMPORTED_TARGET vips-cpp)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

add_executable(imagecompare
    src/app/main.cpp
    src/core/WorkspaceDocument.cpp
    src/core/ViewableImageEntry.cpp
    src/core/ViewableImageListModel.cpp
    src/core/ImageSource.cpp
    src/core/ComparisonJobQueue.cpp
    src/app/ApplicationController.cpp
    src/core/ViewState.cpp
    src/core/TilePyramid.cpp
    src/app/WindowDropFilter.cpp
)
if(WIN32)
    set_target_properties(imagecompare PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
qt_add_resources(imagecompare image_compare_app_resources
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/assets
    FILES
        packaging/assets/imagecompare.svg
)
set(qml_component_files
    src/ui/qml/components/ComparisonPaneItem.qml
    src/ui/qml/components/ComparisonWorkspace.qml
    src/ui/qml/components/PaneGridInteractionController.qml
    src/ui/qml/components/PanesGrid.qml
)
foreach(qml_component_file IN LISTS qml_component_files)
    get_filename_component(qml_component_name "${qml_component_file}" NAME)
    set_source_files_properties("${qml_component_file}"
        PROPERTIES QT_RESOURCE_ALIAS "components/${qml_component_name}"
    )
endforeach()
set_source_files_properties(
    src/ui/qml/Main.qml
    PROPERTIES QT_RESOURCE_ALIAS Main.qml
)
qt_add_qml_module(imagecompare
    URI ImageCompare
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        src/ui/TiledImageItem.h
        src/ui/TiledImageItem.cpp
    QML_FILES
        src/ui/qml/Main.qml
        ${qml_component_files}
    DEPENDENCIES
        QtQml
        QtQuick
)
target_include_directories(imagecompare
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)
target_link_libraries(imagecompare
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        PkgConfig::VIPSCPP
)

if(IMAGECOMPARE_FLATPAK)
    target_compile_definitions(imagecompare PRIVATE IMAGECOMPARE_FLATPAK=1)
    target_link_libraries(imagecompare PRIVATE KF6::CoreAddons Qt6::DBus)
endif()

install(TARGETS imagecompare
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/io.github.gimletlove.imagecompare.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES packaging/linux/io.github.gimletlove.imagecompare.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )
    install(FILES packaging/assets/imagecompare.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME io.github.gimletlove.imagecompare.svg
    )
    install(FILES LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/imagecompare
    )
endif()

set(CPACK_PACKAGE_NAME "imagecompare")
set(CPACK_PACKAGE_VENDOR "gimletlove")
set(CPACK_PACKAGE_CONTACT "gimletlove <gimletlove@users.noreply.github.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gimletlove/imagecompare")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Image comparison and visual diff tool")
set(CPACK_PACKAGE_DESCRIPTION
    "Image Compare lets you inspect two or more images side by side, in a stacked view, or with a heatmap of perceptual differences. It supports synchronized zoom and pan, grid layouts, multiple image formats, faithful color-profile rendering, and fast image loading with Qt6 and libvips."
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtqml-workerscript, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window"
)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
