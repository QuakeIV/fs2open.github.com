
INCLUDE(source_groups.cmake)

INCLUDE(voice_rec.cmake)

ADD_LIBRARY(code STATIC ${file_root} ${VOICE_REC_FILES})

# clear certain warnings
target_compile_options(code PRIVATE -Wno-parentheses) # was whining about unnecessary parenthesis, give me a break
target_compile_options(code PRIVATE -Wno-sizeof-pointer-memaccess) # was complaining about using source rather than destination size for strncpy, except destination size is not knowable so shut up
# TODO: following are temporary?
target_compile_options(code PRIVATE -Wno-stringop-truncation) # TODO: this at least likely needs to be resolved at some point (file name length limit related to some extent)
target_compile_options(code PRIVATE -Wno-stringop-overflow) # TODO: this at least likely needs to be resolved at some point (file name length limit related to some extent)
target_compile_options(code PRIVATE -Wno-format-overflow) # TODO: this at least likely needs to be resolved at some point (file name length limit related to some extent)
target_compile_options(code PRIVATE -Wno-format-truncation) # TODO: this at least likely needs to be resolved at some point (file name length limit related to some extent)
target_compile_options(code PRIVATE -Wno-implicit-fallthrough) # TODO: many cases, needs triage
target_compile_options(code PRIVATE -Wno-catch-value) # TODO: wtf? 'catching polymorphic type by value'
target_compile_options(code PRIVATE -Wno-memset-elt-size) # TODO: this is most likely legitimate "‘memset’ used with length equal to number of elements without multiplication by element size"
target_compile_options(code PRIVATE -Wno-deprecated-copy) # TODO: wtf? "implicitly-declared ‘luacpp::LuaValue& luacpp::LuaValue::operator=(const luacpp::LuaValue&)’ is deprecated", this may actually be the lua module itself complaining
target_compile_options(code PRIVATE -Wno-deprecated-declarations) # TODO: "FFmpeg.cpp:56:18: warning: ‘void av_register_all()’ is deprecated"
target_compile_options(code PRIVATE -Wno-shadow) # TODO: " warning: declaration of ‘Cutscenes’ shadows a global declaration", why are we doing so much weird C++ shit


target_compile_features(code PUBLIC ${REQUIRED_CXX_FEATURES})

INCLUDE(default_files.cmake)

if (MSVC)
	target_compile_definitions(code PUBLIC "PDB_DEBUGGING=1")
endif(MSVC)

TARGET_INCLUDE_DIRECTORIES(code PUBLIC ${CODE_HEADERS})
TARGET_INCLUDE_DIRECTORIES(code PUBLIC ${FREESPACE_HEADERS})

TARGET_INCLUDE_DIRECTORIES(code PUBLIC ${GENERATED_SOURCE_DIR})
TARGET_INCLUDE_DIRECTORIES(code PUBLIC ${GENERATED_SOURCE_DIR}/code)

target_link_libraries(code PUBLIC OpenGL)

TARGET_LINK_LIBRARIES(code PUBLIC ${OPENAL_LIBS})
TARGET_LINK_LIBRARIES(code PUBLIC ${LUA_LIBS})
TARGET_LINK_LIBRARIES(code PUBLIC ${PNG_LIBS})
TARGET_LINK_LIBRARIES(code PUBLIC ${JPEG_LIBS})

# SDL 2
find_package(SDL2 REQUIRED)
target_link_libraries(code PUBLIC SDL2::SDL2)

TARGET_LINK_LIBRARIES(code PUBLIC ffmpeg)

TARGET_LINK_LIBRARIES(code PUBLIC platform)
TARGET_LINK_LIBRARIES(code PUBLIC compiler)

target_link_libraries(code PUBLIC md5)

IF (FSO_USE_SPEECH)
	find_package(Speech REQUIRED)
	target_link_libraries(code PUBLIC speech)
ENDIF(FSO_USE_SPEECH)

TARGET_LINK_LIBRARIES(code PUBLIC jansson)

# Headers for standalone "gui" on UNIX
IF(UNIX)
	TARGET_LINK_LIBRARIES(code PUBLIC ${MONGOOSE_LIBS})
ENDIF(UNIX)

IF(FSO_BUILD_TOOLS)
	ADD_SUBDIRECTORY(cfilearchiver)

	ADD_SUBDIRECTORY(cfileextractor)
ENDIF(FSO_BUILD_TOOLS)

set_target_properties(code PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "prefix_header.h")

include(util)
configure_cotire(code)

# Ensure that Xcode generates debug symbols on macOS for Debug builds
set_target_properties(code PROPERTIES XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Debug] "YES")
set_target_properties(code PROPERTIES XCODE_ATTRIBUTE_COPY_PHASE_STRIP[variant=Debug] "NO")
set_target_properties(code PROPERTIES XCODE_ATTRIBUTE_STRIP_INSTALLED_PRODUCT[variant=Debug] "NO")
set_target_properties(code PROPERTIES XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN[variant=Debug] "NO")
