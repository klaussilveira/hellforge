include(GoogleTest)

add_executable(drtest
               Basic.cpp
               BuildingGenerator.cpp
               Brush.cpp
               Camera.cpp
               Clipboard.cpp
               CodeTokeniser.cpp
               ColourSchemes.cpp
               CommandSystem.cpp
               ContinuousBuffer.cpp
               CSG.cpp
               Curves.cpp
               DarkmodTxt.cpp
               ../plugins/dm.editing/DarkmodTxt.cpp
               ../plugins/dm.editing/MissionInfoTextFile.cpp
               DeclManager.cpp
               DefBlockSyntaxParser.cpp
               DefTokenisers.cpp
               Entity.cpp
               EntityClass.cpp
               EntityInspector.cpp
               Favourites.cpp
               FileTypes.cpp
               Filters.cpp
               Fx.cpp
               Game.cpp
               GeometryStore.cpp
               Grid.cpp
               HeadlessOpenGLContext.cpp
               ImageLoading.cpp
               KeyCode.cpp
               LayerManipulation.cpp
               MapExport.cpp
               MapMerging.cpp
               MapSavingLoading.cpp
               MaterialExport.cpp
               Materials.cpp
               math/Matrix3.cpp
               math/Matrix4.cpp
               math/Plane3.cpp
               math/Quaternion.cpp
               math/Vector.cpp
               MessageBus.cpp
               ModelExport.cpp
               Models.cpp
               Particles.cpp
               Patch.cpp
               PatchIterators.cpp
               PatchWelding.cpp
               PatchSewing.cpp
               PointTrace.cpp
               Prefabs.cpp
               Registry.cpp
               Renderer.cpp
               SceneNode.cpp
               SceneStatistics.cpp
               SelectionAlgorithm.cpp
               Selection.cpp
               SelectionGroup.cpp
               Settings.cpp
               Skin.cpp
               StairsGenerator.cpp
               SoundManager.cpp
               TerrainGenerator.cpp
               TerrainSculpt.cpp
               ${CMAKE_SOURCE_DIR}/radiant/selection/TerrainSculptAlgorithm.cpp
               VertexPaint.cpp
               ${CMAKE_SOURCE_DIR}/radiant/selection/VertexPaintAlgorithm.cpp
               VertexFragmentProgram.cpp
               ${CMAKE_SOURCE_DIR}/radiantcore/rendersystem/backend/glprogram/GenericVFPProgram.cpp
               TextureManipulation.cpp
               TestOrthoViewManager.cpp
               TextureTool.cpp
               Transformation.cpp
               TrimTool.cpp
               UndoRedo.cpp
               VFS.cpp
               WallGeometry.cpp
               ${CMAKE_SOURCE_DIR}/radiant/xyview/tools/WallGeometry.cpp
               WindingRendering.cpp
               WorldspawnColour.cpp
               XmlUtil.cpp)

find_package(Threads REQUIRED)

# Set up the paths such that the drtest executable can find the test resources
# and the core binary in the build workspace (in install/ and test/resources/)
get_filename_component(TEST_BASE_PATH "./" ABSOLUTE)
add_compile_definitions(TEST_BASE_PATH="${TEST_BASE_PATH}")

target_include_directories(drtest PRIVATE ../plugins/dm.editing)
target_include_directories(drtest PRIVATE ${CMAKE_SOURCE_DIR}/radiant)
target_include_directories(drtest PRIVATE ${CMAKE_SOURCE_DIR}/radiantcore)

target_link_libraries(drtest PUBLIC
                      math xmlutil scene module wxutil
                      ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}
                      ${SIGC_LIBRARIES} ${GLEW_LIBRARIES} ${X11_LIBRARIES}
                      PRIVATE Threads::Threads)
install(TARGETS drtest)

gtest_discover_tests(drtest)
