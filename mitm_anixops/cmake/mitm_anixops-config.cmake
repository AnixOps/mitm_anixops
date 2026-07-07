include_guard(GLOBAL)

get_filename_component(_MITM_ANIXOPS_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET mitm_anixops::mitm_anixops)
	add_library(mitm_anixops::mitm_anixops SHARED IMPORTED)
	set_target_properties(
		mitm_anixops::mitm_anixops
		PROPERTIES
			IMPORTED_LOCATION "${_MITM_ANIXOPS_PREFIX}/lib/libmitm_anixops.so"
			INTERFACE_INCLUDE_DIRECTORIES "${_MITM_ANIXOPS_PREFIX}/include")
endif()

set(mitm_anixops_FOUND TRUE)
