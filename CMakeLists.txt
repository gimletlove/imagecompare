cmake_minimum_required(VERSION 3.21)

project(image_compare VERSION 1.4.1 LANGUAGES CXX)

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

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)

add_subdirectory(third_party/ssim EXCLUDE_FROM_ALL)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

add_executable(imagecompare
    src/app/main.cpp
    src/core/WorkspaceModel.cpp
    src/core/ImageSource.cpp
    src/core/ImageComparison.cpp
    src/app/ApplicationController.cpp
)
target_compile_options(imagecompare PRIVATE $<$<PLATFORM_ID:Linux>:-fPIC>)
target_compile_definitions(imagecompare PRIVATE IMAGECOMPARE_VERSION="${PROJECT_VERSION}")
if(WIN32)
    configure_file(packaging/windows/imagecompare.rc.in imagecompare.rc @ONLY)
    target_sources(imagecompare PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/imagecompare.rc")
    set_target_properties(imagecompare PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
qt_add_resources(imagecompare image_compare_app_resources
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/assets
    FILES
        packaging/assets/imagecompare.svg
)
qt_add_qml_module(imagecompare
    URI ImageCompare
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        src/ui/ImageItem.h
        src/ui/ImageItem.cpp
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/components/ComparisonPaneItem.qml
        src/ui/qml/components/ComparisonWorkspace.qml
        src/ui/qml/components/PaneGridInteractionController.qml
        src/ui/qml/components/PanesGrid.qml
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
        Qt6::Widgets
        rmgr-ssim
)

if(IMAGECOMPARE_FLATPAK)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    find_package(KF6CoreAddons REQUIRED)
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
    install(FILES third_party/ssim/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/imagecompare
        RENAME LICENSE-rmgr-ssim.txt
    )
endif()

set(CPACK_PACKAGE_NAME "imagecompare")
set(CPACK_PACKAGE_VENDOR "gimletlove")
set(CPACK_PACKAGE_CONTACT "gimletlove <gimletlove@users.noreply.github.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gimletlove/imagecompare")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Image comparison and visual diff tool")
set(CPACK_PACKAGE_DESCRIPTION
    "Image Compare lets you inspect two or more images side by side, in a stacked view, or with a structural-similarity heatmap. Zoom and pan stay synchronized across images, with relative synchronization across different resolutions. It also supports grid layouts, multiple image formats, faithful color-profile rendering, and image loading through Qt 6."
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtqml-workerscript, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window"
)
set(CPACK_DEBIAN_PACKAGE_SUGGESTS
    "qt6-svg-plugins, qt6-image-formats-plugins, kimageformat6-plugins"
)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_SUGGESTS
    "qt6-qtsvg, qt6-qtimageformats, kf6-kimageformats"
)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
