set(IMAGECOMPARE_QT_COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
if(IMAGECOMPARE_FLATPAK)
    list(APPEND IMAGECOMPARE_QT_COMPONENTS DBus)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS ${IMAGECOMPARE_QT_COMPONENTS})
if(IMAGECOMPARE_FLATPAK)
    find_package(KF6CoreAddons REQUIRED)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
pkg_check_modules(VIPSCPP REQUIRED IMPORTED_TARGET vips-cpp)
