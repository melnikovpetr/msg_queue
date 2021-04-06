cmake_minimum_required(VERSION 3.5)

project(msg_queue LANGUAGES C)

execute_process(COMMAND uname -r OUTPUT_VARIABLE LINUX_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "LINUX_VER:/usr/src/linux-headers-${LINUX_VER}/include")
include_directories(${PROJECT_NAME} PUBLIC "/usr/src/linux-headers-${LINUX_VER}/include/")

add_custom_target(msg_queue_lkm
	COMMAND "make"
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	SOURCES
		"Makefile"
		"msg_queue.h"
		"msg_queue_lkm.c"
		"msg_queue_app.c"
		"msg_queue_dmn.c"
		"msg_queue_lkm_fops.c"
		"msg_queue_lkm_qops.c"
)
add_executable(msg_queue_app "msg_queue_app.c")
add_executable(msg_queue_dmn "msg_queue_dmn.c")
