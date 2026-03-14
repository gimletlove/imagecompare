find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)
find_package(KF6CoreAddons REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips)
pkg_check_modules(VIPSCPP REQUIRED IMPORTED_TARGET vips-cpp)
