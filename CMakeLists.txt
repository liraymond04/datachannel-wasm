cmake_minimum_required(VERSION 3.7)
project(datachannel-wasm
	DESCRIPTION "C++ WebRTC Data Channels for WebAssembly in browsers"
	VERSION 0.3.0
	LANGUAGES CXX)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	message(FATAL_ERROR "datachannel-wasm must be compiled with Emscripten.")
endif()

set(WASM_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wasm/src)
set(DATACHANNELS_SRC
	${WASM_SRC_DIR}/candidate.cpp
	${WASM_SRC_DIR}/capi.cpp
	${WASM_SRC_DIR}/channel.cpp
	${WASM_SRC_DIR}/configuration.cpp
	${WASM_SRC_DIR}/description.cpp
	${WASM_SRC_DIR}/datachannel.cpp
	${WASM_SRC_DIR}/peerconnection.cpp
	${WASM_SRC_DIR}/websocket.cpp)

add_library(datachannel-wasm STATIC ${DATACHANNELS_SRC})
set_target_properties(datachannel-wasm PROPERTIES
	VERSION ${PROJECT_VERSION}
	CXX_STANDARD 17)

target_include_directories(datachannel-wasm PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/wasm/include)
target_include_directories(datachannel-wasm PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/wasm/include/rtc)

target_link_options(datachannel-wasm PUBLIC
	"SHELL:-s ASYNCIFY"
	"SHELL:--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/wasm/js/webrtc.js\"")

target_link_libraries(datachannel-wasm websocket.js)

execute_process(COMMAND ${CMAKE_CXX_COMPILER} --cflags
    OUTPUT_VARIABLE EM_CFLAGS
    COMMAND_ERROR_IS_FATAL ANY
)
string(STRIP "${EM_CFLAGS}" EM_CFLAGS)
find_package(Python3 REQUIRED)
message("Python: ${Python3_EXECUTABLE}")
set(ENV{EXTRA_FLAGS} "${EM_CFLAGS}")
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/fix_compile_commands.py
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
