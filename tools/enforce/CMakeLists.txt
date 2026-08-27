# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-enforce (TODO.beyond-libc/01): the generic kernel-
# enforcement installer. Linux-only; POSIX stubs keep the tree
# building elsewhere (the tool simply is not installed).

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

add_executable(retrace_enforce
	main.c enforce_spec.c landlock_apply.c seccomp_apply.c
	sandbox_apply.c artifact_audit.c appcontainer_apply.c
	${_parson_source})
set_target_properties(retrace_enforce PROPERTIES
	OUTPUT_NAME retrace-enforce)

target_include_directories(retrace_enforce PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/config/json
	${CMAKE_SOURCE_DIR}/test/property/parson_stub)

if(NOT RETRACE_PLATFORM_LINUX)
	target_compile_definitions(retrace_enforce PRIVATE
		RETRACE_ENFORCE_NO_KERNEL)
endif()

# Audited artifacts (01 P2): SHA-256 digests when OpenSSL is
# linked; FNV-1a-64 fallback otherwise (the record says which).
if(RETRACE_PLATFORM_WINDOWS)
	target_link_libraries(retrace_enforce PRIVATE userenv)
endif()

if(OpenSSL_FOUND)
	target_compile_definitions(retrace_enforce PRIVATE
		RETRACE_HAVE_OPENSSL=1)
	target_link_libraries(retrace_enforce PRIVATE OpenSSL::Crypto)
	target_include_directories(retrace_enforce PRIVATE
		${OPENSSL_INCLUDE_DIR})
endif()

install(TARGETS retrace_enforce
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
