cmake_minimum_required(VERSION 3.21)

project(image_compare VERSION 1.0.3 LANGUAGES CXX)

include(GNUInstallDirs)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
add_compile_definitions(QT_NO_KEYWORDS)

include(cmake/Dependencies.cmake)

add_library(image_compare_core
    src/core/WorkspaceDocument.cpp
    src/core/ViewableImageEntry.cpp
    src/core/ViewableImageListModel.cpp
    src/core/SourceIdentity.cpp
    src/core/ImageSource.cpp
    src/core/ImageRepository.cpp
    src/core/ComparisonService.cpp
    src/core/ComparisonJobQueue.cpp
    src/core/ViewState.cpp
    src/core/TilePyramid.cpp
    src/app/ApplicationController.cpp
    src/app/WindowDropFilter.cpp
    src/ui/TiledImageItem.cpp
    src/ui/TileTextureCache.cpp
)
qt_add_resources(image_compare_core image_compare_qml_resources
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/qml
    FILES
        src/ui/qml/Main.qml
        src/ui/qml/components/AppToolbar.qml
        src/ui/qml/components/ComparisonPaneItem.qml
        src/ui/qml/components/ComparisonWorkspace.qml
        src/ui/qml/components/PaneHeader.qml
        src/ui/qml/components/PaneGridInteractionController.qml
        src/ui/qml/components/PanesGrid.qml
)
qt_add_resources(image_compare_core image_compare_app_resources
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/assets
    FILES
        packaging/assets/imagecompare.svg
)
target_include_directories(image_compare_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(image_compare_core
    PUBLIC
        KF6::CoreAddons
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Widgets
        PkgConfig::VIPS
        PkgConfig::VIPSCPP
)

add_executable(imagecompare
    src/app/main.cpp
)
target_link_libraries(imagecompare
    PRIVATE
        image_compare_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

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
endif()
