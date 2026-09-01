# SPDX-License-Identifier: BSD-2-Clause
#
# The packaging module (the architecture review's G): CPack over
# the install() surface -- the release workflow's hand-staged
# tarballs shipped lib/ and include/ only, leaving every tool
# out of the artifacts, and carried the notices as YAML
# folklore. Identity comes from version.h (the SSOT -- no third
# version source); the workflow selects generators and keeps the
# artifact naming per platform via CPACK_PACKAGE_FILE_NAME.

file(READ "${CMAKE_SOURCE_DIR}/include/retrace/version.h" _retrace_vh)
string(REGEX MATCH "#define RETRACE_VERSION_STRING \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
	_retrace_v "${_retrace_vh}")
if(NOT CMAKE_MATCH_1)
	message(FATAL_ERROR "packaging: no RETRACE_VERSION_STRING in version.h")
endif()

set(CPACK_PACKAGE_NAME "retrace")
set(CPACK_PACKAGE_VERSION "${CMAKE_MATCH_1}")
set(CPACK_PACKAGE_VENDOR "Ribose")
set(CPACK_PACKAGE_CONTACT "open.source@ribose.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
	"userspace libc-call interceptor, kernel-truth grader, and supervisor")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.adoc")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.adoc")
set(CPACK_STRIP_FILES OFF)
set(CPACK_PACKAGE_CHECKSUM SHA256)

# default file name: the workflow overrides per platform so the
# release asset names stay byte-identical to the hand-staged era
set(CPACK_PACKAGE_FILE_NAME
	"retrace-${CPACK_PACKAGE_VERSION}")

# system packages (the roadmap item): the Linux legs also build
# these; dpkg-deb/rpm validate at package time, not install time
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ribose <open.source@ribose.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_RPM_PACKAGE_LICENSE "BSD-2-Clause")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")

include(CPack)
