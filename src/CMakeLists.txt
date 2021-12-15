target_sources(${TARGET_NAME_LIB} PRIVATE 
util.cxx
${CMAKE_CURRENT_LIST_DIR}/util.hxx
)

add_subdirectory(database)
add_subdirectory(game)
add_subdirectory(server)
add_subdirectory(logic)
add_subdirectory(pw_hash)