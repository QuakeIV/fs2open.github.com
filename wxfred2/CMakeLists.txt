
include(source_groups.cmake)

SET(PNG_OUT_FILES)
FOREACH(file ${file_root_res_pngs})
	FILE(RELATIVE_PATH FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR} ${file})

	SET(OUTPUT "${GENERATED_SOURCE_DIR}/wxfred2/${FILE_NAME}")

	# For some reason this is needed...
	GET_FILENAME_COMPONENT(DIRECTORY_PATH ${OUTPUT} PATH)
	FILE(MAKE_DIRECTORY ${DIRECTORY_PATH})

	get_filename_component(FILENAME ${file} NAME)

	STRING(REPLACE "-" "_" FILENAME ${FILENAME})
	STRING(REPLACE "." "_" FILENAME ${FILENAME})
	STRING(REPLACE "#" "_" FILENAME ${FILENAME})

	ADD_CUSTOM_COMMAND(
		OUTPUT ${OUTPUT}.cpp
		COMMAND embedfile -wx "${file}" "${OUTPUT}" "${FILENAME}"
		DEPENDS ${file}
		COMMENT "Generating png header for ${file}"
		)

	LIST(APPEND PNG_OUT_FILES ${OUTPUT}.cpp)
ENDFOREACH(file ${file_root_res_pngs})

SOURCE_GROUP("Generated Files\\PNG Files" FILES ${PNG_OUT_FILES})

IF(UNIX)
	SET(wxWidgets_USE_DEBUG OFF)
	SET(wxWidgets_USE_UNICODE ON)
	SET(wxWidgets_USE_UNIVERSAL OFF)
	SET(wxWidgets_USE_STATIC OFF)
ELSEIF(WIN32)
	set(wxWidgets_CONFIGURATION "mswu" CACHE STRING "Release WxWidgets configuration" FORCE)
ENDIF(UNIX)

find_package(wxWidgets COMPONENTS gl core base REQUIRED)

include(${wxWidgets_USE_FILE})

add_executable(wxfred2 ${EXE_GUI_TYPE} ${file_root} ${PNG_OUT_FILES})

# SDL 2
find_package(SDL2 REQUIRED)
target_link_libraries(wxfred2 PUBLIC SDL2::SDL2)

# set compile warning options
target_compile_options(wxfred2 PRIVATE -Wno-write-strings) # TODO: ISO C++ forbids converting a string constant to ‘char*’, who cares?
target_compile_options(wxfred2 PRIVATE -Wno-reorder) # TODO: "‘frmFRED2::fredName’ will be initialized after", this is some weird C++ object ism
target_compile_options(wxfred2 PRIVATE -Wno-deprecated-declarations) # TODO: "warning: ‘void wxSplitterWindow::SetSashSize(int)’ is deprecated"
target_compile_options(wxfred2 PRIVATE -Wno-unused-variable) # TODO: "warning: ‘fred_app_xpm’ defined but not used [-Wunused-variable]" why is a cpp with static shit being included?

set_target_properties(wxfred2
	PROPERTIES
	COMPILE_DEFINITIONS_RELEASE "NDEBUG"
	COMPILE_DEFINITIONS_DEBUG "_DEBUG"
	DEBUG_POSTFIX "-DEBUG"
)

SET_TARGET_PROPERTIES(wxfred2 PROPERTIES OUTPUT_NAME "wxfred2_${FSO_BINARY_SUFFIX}")

add_definitions(
	-DFRED
)

INCLUDE_DIRECTORIES(${CODE_HEADERS})
INCLUDE_DIRECTORIES(${WXFRED_HEADERS})
INCLUDE_DIRECTORIES(${wxWidgets_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${GENERATED_SOURCE_DIR})
INCLUDE_DIRECTORIES(${GENERATED_SOURCE_DIR}/wxfred2)
INCLUDE_DIRECTORIES(${GENERATED_SOURCE_DIR}/code)

INCLUDE_DIRECTORIES(${COMPILER_INCLUDES})
INCLUDE_DIRECTORIES(${PLATFORM_INCLUDES})

TARGET_LINK_LIBRARIES(wxfred2 PUBLIC ${wxWidgets_LIBRARIES})
TARGET_LINK_LIBRARIES(wxfred2 PUBLIC ${SDL_LIBS})

if(UNIX)
	add_definitions(
		-DSCP_UNIX
		-DLUA_USE_LINUX
		-std=c++11
	)
elseif(MSVC)
	IF(NOT MSVC_USE_RUNTIME_DLL)
		STRING(REGEX REPLACE "/MT" "/MD" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
		STRING(REGEX REPLACE "/MT" "/MD" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

		STRING(REGEX REPLACE "/MT" "/MD" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
		STRING(REGEX REPLACE "/MT" "/MD" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	ENDIF(NOT MSVC_USE_RUNTIME_DLL)


	add_definitions(
		/DWINVER=0x0501
		/D_WIN32_WINNT=0x0501
		/D_WINDOWS
		/DWIN32
	)

	TARGET_LINK_LIBRARIES(wxfred2
		odbc32.lib
		odbccp32.lib
		comctl32.lib
		rpcrt4.lib
		wsock32.lib
		winmm.lib
	)

	set_target_properties(wxfred2 PROPERTIES
		COMPILE_DEFINITIONS_DEBUG "PDB_DEBUGGING=1"
	)
endif(UNIX)

# Add install script to copy the executable to the FreeSpace path if possible and wanted
INSTALL(
	TARGETS wxfred2
	RUNTIME DESTINATION ${BINARY_DESTINATION}
	BUNDLE DESTINATION ${BINARY_DESTINATION}
)
if (FSO_INSTALL_DEBUG_FILES)
	if (MSVC)
		install(FILES "$<TARGET_PDB_FILE:wxfred2>"
				DESTINATION ${BINARY_DESTINATION}
				OPTIONAL)
	endif()
endif()

INCLUDE(util)
COPY_FILES_TO_TARGET(wxfred2)

include(CreateLaunchers)
create_target_launcher(wxfred2
	WORKING_DIRECTORY ${FSO_FREESPACE_PATH}
	ARGS ${FSO_RUN_ARGUMENTS})
