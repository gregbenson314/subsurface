message(STATUS "processing version.cmake")

if(DEFINED ENV{CANONICALVERSION})
       set(CANONICAL_VERSION_STRING $ENV{CANONICALVERSION})
else()
       execute_process(
               COMMAND bash ${CMAKE_TOP_SRC_DIR}/scripts/get-version.sh
               WORKING_DIRECTORY ${CMAKE_TOP_SRC_DIR}
               OUTPUT_VARIABLE CANONICAL_VERSION_STRING
               OUTPUT_STRIP_TRAILING_WHITESPACE
       )
endif()

if(DEFINED ENV{CANONICALVERSION_4})
       set(CANONICAL_VERSION_STRING_4 $ENV{CANONICALVERSION_4})
else()
       execute_process(
               COMMAND bash ${CMAKE_TOP_SRC_DIR}/scripts/get-version.sh 4
               WORKING_DIRECTORY ${CMAKE_TOP_SRC_DIR}
               OUTPUT_VARIABLE CANONICAL_VERSION_STRING_4
               OUTPUT_STRIP_TRAILING_WHITESPACE
       )
endif()

configure_file(${SRC} ${DST} @ONLY)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	execute_process(
		COMMAND cat ${CMAKE_TOP_SRC_DIR}/../packaging/windows/smtk-import.nsi.in
		COMMAND sed -e "s/VERSIONTOKEN/${CANONICAL_VERSION_STRING}/"
		COMMAND sed -e "s/PRODVTOKEN/${CANONICAL_VERSION_STRING_4}/"
		OUTPUT_FILE ${CMAKE_BINARY_DIR}/staging/smtk-import.nsi
	)
endif()
