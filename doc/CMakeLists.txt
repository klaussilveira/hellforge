set(DR_STAGING_DOCDIR "${DR_STAGING_ROOT}/share/doc/hellforge")
add_custom_command(
    OUTPUT manual.html
    COMMAND ${ASCIIDOCTOR}
    ARGS -a stylesheet=manual.css -D ${DR_STAGING_DOCDIR} ${CMAKE_CURRENT_SOURCE_DIR}/manual.adoc
    DEPENDS manual.adoc
)
add_custom_target(doc ALL DEPENDS manual.html)

# Copy the images into the staging directory for local preview
file(GLOB_RECURSE DOC_IMAGES img/*.png)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/img
        ${DR_STAGING_DOCDIR}/img
)

# Installation at package time
install(FILES ${DR_STAGING_DOCDIR}/manual.html DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY img DESTINATION ${CMAKE_INSTALL_DOCDIR} FILES_MATCHING PATTERN "*.png")
