cmake_minimum_required(VERSION 3.15)
project(mitm_anixops_cmake_smoke C)

find_package(mitm_anixops CONFIG REQUIRED)

add_executable(mitm_anixops_cmake_smoke ../pkg_config_smoke.c)
target_link_libraries(mitm_anixops_cmake_smoke PRIVATE mitm_anixops::mitm_anixops)
