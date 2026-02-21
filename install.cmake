# Install main targets
install(TARGETS hellforge math xmlutil scene wxutil
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${PKGLIBDIR})
install(TARGETS radiantcore script sound
        LIBRARY DESTINATION ${PKGLIBDIR}/modules)

# Install Dark Mod plugins
if (ENABLE_DM_PLUGINS)
    install(TARGETS dm_stimresponse dm_objectives dm_difficulty dm_editing
                    dm_gui dm_gameconnection dm_conversation
            LIBRARY DESTINATION ${PKGLIBDIR}/plugins)
endif()

if (ENABLE_GIT_PLUGIN AND LIBGIT_FOUND)
    install(TARGETS vcs LIBRARY DESTINATION ${PKGLIBDIR}/plugins)
endif()

# Generate and install the .desktop file
configure_file(install/io.github.klaussilveira.hellforge.desktop.in
               install/io.github.klaussilveira.hellforge.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/install/io.github.klaussilveira.hellforge.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Install resources
file(GLOB XML_FILES "${PROJECT_SOURCE_DIR}/install/*.xml")
install(FILES ${XML_FILES} DESTINATION ${PKGDATADIR})

install(DIRECTORY install/games DESTINATION ${PKGDATADIR})
install(DIRECTORY install/bitmaps DESTINATION ${PKGDATADIR})
install(DIRECTORY install/gl DESTINATION ${PKGDATADIR})
install(DIRECTORY install/ui DESTINATION ${PKGDATADIR}
        FILES_MATCHING PATTERN "*.ttf" PATTERN "*.xrc")
install(DIRECTORY install/resources DESTINATION ${PKGDATADIR})

install(FILES ${PROJECT_SOURCE_DIR}/install/bitmaps/darkradiant_icon_64x64.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps
        RENAME io.github.klaussilveira.hellforge.png)
install(FILES ${PROJECT_SOURCE_DIR}/install/bitmaps/darkradiant_icon_128x128.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
        RENAME io.github.klaussilveira.hellforge.png)
install(FILES ${PROJECT_SOURCE_DIR}/install/io.github.klaussilveira.hellforge.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

# Install locale data
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.14")
    # CMake 3.14 and above support TYPE LOCALE, they deduct DESTINATION themselves
    install(DIRECTORY install/i18n/de TYPE LOCALE FILES_MATCHING PATTERN "*.mo")
else()
    install(DIRECTORY install/i18n/de DESTINATION ${CMAKE_INSTALL_LOCALEDIR}
            FILES_MATCHING PATTERN "*.mo")
endif()

# Install scripts
install(DIRECTORY install/scripts DESTINATION ${PKGDATADIR}
        FILES_MATCHING PATTERN "*.py")
