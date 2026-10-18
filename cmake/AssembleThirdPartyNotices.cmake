# Assembles THIRD-PARTY-NOTICES.txt at install time: the credits list, the licence of
# PDF4QT / Andre PDF, the licence texts of 3rdparty_licenses/ and, when the build used
# vcpkg, the copyright file of every installed vcpkg port (so the exact texts of the
# distributed libraries are carried, whatever versions the baseline pins).
#   NOTICES_SOURCE_DIR - source tree
#   NOTICES_VCPKG_SHARE_DIR - <vcpkg_installed>/<triplet>/share, may be empty
#   NOTICES_OUTPUT - file to write

foreach(NOTICES_VARIABLE NOTICES_SOURCE_DIR NOTICES_OUTPUT)
    if(NOT DEFINED ${NOTICES_VARIABLE})
        message(FATAL_ERROR "${NOTICES_VARIABLE} is not defined.")
    endif()
endforeach()

set(NOTICES_RULE "\n\n================================================================================\n")

file(READ "${NOTICES_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" NOTICES_TEXT)
file(READ "${NOTICES_SOURCE_DIR}/LICENSE" NOTICES_PART)
string(APPEND NOTICES_TEXT "${NOTICES_RULE}PDF4QT and Andre PDF (LICENSE)${NOTICES_RULE}${NOTICES_PART}")

file(GLOB NOTICES_FILES "${NOTICES_SOURCE_DIR}/3rdparty_licenses/*")
list(SORT NOTICES_FILES)
foreach(NOTICES_FILE IN LISTS NOTICES_FILES)
    get_filename_component(NOTICES_NAME "${NOTICES_FILE}" NAME)
    file(READ "${NOTICES_FILE}" NOTICES_PART)
    string(APPEND NOTICES_TEXT "${NOTICES_RULE}${NOTICES_NAME}${NOTICES_RULE}${NOTICES_PART}")
endforeach()

if(NOTICES_VCPKG_SHARE_DIR AND IS_DIRECTORY "${NOTICES_VCPKG_SHARE_DIR}")
    file(GLOB NOTICES_FILES "${NOTICES_VCPKG_SHARE_DIR}/*/copyright")
    list(SORT NOTICES_FILES)
    foreach(NOTICES_FILE IN LISTS NOTICES_FILES)
        get_filename_component(NOTICES_PORT_DIR "${NOTICES_FILE}" DIRECTORY)
        get_filename_component(NOTICES_PORT "${NOTICES_PORT_DIR}" NAME)
        # Build helper ports are not distributed
        if(NOTICES_PORT MATCHES "^vcpkg-")
            continue()
        endif()
        file(READ "${NOTICES_FILE}" NOTICES_PART)
        string(APPEND NOTICES_TEXT "${NOTICES_RULE}${NOTICES_PORT} (vcpkg copyright file)${NOTICES_RULE}${NOTICES_PART}")
    endforeach()
endif()

string(REPLACE "\r\n" "\n" NOTICES_TEXT "${NOTICES_TEXT}")
file(WRITE "${NOTICES_OUTPUT}" "${NOTICES_TEXT}")
message(STATUS "Third-party notices: ${NOTICES_OUTPUT}")
