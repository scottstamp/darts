cmake_minimum_required(VERSION 3.16)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(DartsScoreboard)
