cmake_minimum_required(VERSION 3.10)
project(mtp-configfs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(LIBANDROID_PROPERTIES REQUIRED libandroid-properties)

include_directories(
  ${GIO_INCLUDE_DIRS}
  ${LIBANDROID_PROPERTIES_INCLUDE_DIRS}
)

set(SOURCES usb-config.c isodrive.c utils.c)

add_executable(mtp-configfs ${SOURCES})

target_link_libraries(mtp-configfs
  ${GIO_LIBRARIES}
  ${LIBANDROID_PROPERTIES_LIBRARIES}
)

install(
  TARGETS mtp-configfs
  RUNTIME DESTINATION sbin
)
