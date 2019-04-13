CMAKE_MINIMUM_REQUIRED(VERSION 3.2.0)

#TODO easy 20180324 set correct pixelink source dir after code refactor
SET(PIXELINK_SDK_DIR /opt/dm/pixelink)
MESSAGE(WARNING ${PIXELINK_SDK_DIR})

if(NOT DEFINED PIXELINK_SDK_DIR)
    MESSAGE(FATAL_ERROR
        "PIXELINK_SDK_DIR does not seem to be set in your cmake. This is needed to find Pixelink Desktop Video SDK components.")
endif(NOT DEFINED PIXELINK_SDK_DIR)

#TODO easy 20180324 move pixelink to docker /opt and update find_library
#FIND_LIBRARY(DeckLinkAPI_LIB
#    NAMES DeckLinkAPI
#    PATHS /usr/lib
#)
#if(NOT DeckLinkAPI_LIB)
#    MESSAGE(FATAL_ERROR
#        "Blackmagic DeckLink drivers do not seem to be installed on your system. They are needed for capturing video using a Blackmagic card.")
#endif(NOT DeckLinkAPI_LIB)

SET(PIXELINK_SDK_INCLUDE_DIRS
    ${PIXELINK_SDK_DIR}/include
    CACHE INTERNAL "Pixelink SDK include dirs"
)

SET(PIXELINK_SDK_LIBS
    "PxLApi"
#    "PxLApi -rdynamic"
    CACHE INTERNAL "Pixelink SDK libs to link against"
)

SET(PIXELINK_SDK_LIBRARY_DIRS
    ${PIXELINK_SDK_DIR}/lib
    CACHE INTERNAL "Pixelink SDK library directories to link against"
)

#set(LINKER_FLAGS "-rdynamic")
#set(LD_FLAGS ${LINKER_FLAGS})
#set(LDFLAGS ${LINKER_FLAGS})
#set(CMAKE_SHARED_LINKER_FLAGS ${LINKER_FLAGS})
