# Synthetic Qt6GuiPrivate package config.
#
# Debian's qt6-base-dev / qt6-base-private-dev ship QtGui's private headers
# but, unlike Arch's qt6-base or Fedora's qt6-qtbase-private-devel, don't
# ship a Qt6GuiPrivateConfig.cmake for them, so plain
# find_package(Qt6 COMPONENTS GuiPrivate) fails there with "Qt6GuiPrivate
# not found". This file is added to CMAKE_PREFIX_PATH (see plugin/CMakeLists.txt)
# as a fallback searched only after the real system paths, so a genuine
# distro-provided Qt6GuiPrivateConfig.cmake always takes precedence over it.
#
# It provides only the include-path side effect this project needs; there is
# no separate library for a module's private headers on any platform.

if(NOT TARGET Qt6::Gui)
    find_package(Qt6Gui REQUIRED CONFIG)
endif()

if(NOT TARGET Qt6::GuiPrivate)
    # A module's private headers live in a version-numbered subdirectory next
    # to its public ones (<prefix>/<Version>/<Module>/private/*.h) -- true for
    # every Qt module, on every distro layout, when the private headers are
    # installed at all.
    get_target_property(_qt6gui_include_dirs Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
    set(_qt6gui_private_dir "")
    foreach(_dir ${_qt6gui_include_dirs})
        if(EXISTS "${_dir}/${Qt6Gui_VERSION}/QtGui/private")
            set(_qt6gui_private_dir "${_dir}/${Qt6Gui_VERSION}")
            break()
        endif()
    endforeach()

    if(NOT _qt6gui_private_dir)
        set(Qt6GuiPrivate_FOUND FALSE)
        set(Qt6GuiPrivate_NOT_FOUND_MESSAGE
            "Could not locate QtGui private headers (QtGui/<version>/QtGui/private) under any of Qt6::Gui's include directories: ${_qt6gui_include_dirs}. Install your distro's QtGui private-headers package (e.g. qt6-base-private-dev on Debian).")
        return()
    endif()

    add_library(Qt6::GuiPrivate INTERFACE IMPORTED)
    set_target_properties(Qt6::GuiPrivate PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_qt6gui_private_dir};${_qt6gui_private_dir}/QtGui"
        INTERFACE_LINK_LIBRARIES "Qt6::Gui"
    )
endif()

set(Qt6GuiPrivate_FOUND TRUE)
