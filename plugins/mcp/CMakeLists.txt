add_library(mcp MODULE McpPlugin.cpp)
target_compile_options(mcp PUBLIC ${SIGC_CFLAGS})
target_link_libraries(mcp PUBLIC wxutil)
set_target_properties(
    mcp PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${DR_STAGING_PLUGINSDIR}"
)
