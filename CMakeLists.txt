cmake_minimum_required(VERSION 3.5)
project("secamiz0r")

add_library(secamiz0r MODULE frei0r.h secamiz0r.c)

if(MSVC)
	target_sources(secamiz0r PRIVATE frei0r_1_0.def)
endif()

set_target_properties(secamiz0r PROPERTIES PREFIX "")