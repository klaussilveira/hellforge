#include "RadiantTest.h"

#include <algorithm>
#include <vector>
#include "ieclass.h"
#include "ilayer.h"
#include "iselection.h"
#include "iarray.h"
#include "scene/EntityNode.h"
#include "itransformable.h"
#include "icommandsystem.h"
#include "registry/registry.h"
#include "scenelib.h"
#include "selection/SingleItemSelector.h"
#include "selection/SelectedPlaneSet.h"
#include "render/View.h"
#include "algorithm/View.h"
#include "algorithm/Entity.h"
#include "algorithm/Primitives.h"
#include "algorithm/Scene.h"

namespace test
{

using TransformationTest = RadiantTest;

TEST_F(TransformationTest, MoveSelected)
{
    // Create an entity which has editor_mins/editor_maxs defined (GenericEntity)
    auto eclass = GlobalEntityClassManager().findClass("fixed_size_entity");
    auto entityNode = GlobalEntityModule().createEntity(eclass);

    GlobalMapModule().getRoot()->addChildNode(entityNode);

    Node_setSelected(entityNode, true);

    Vector3 originalPosition = entityNode->worldAABB().getOrigin();
    EXPECT_EQ(originalPosition, Vector3(0, 0, 0));

    Vector3 translation(10, 10, 10);
    GlobalCommandSystem().executeCommand("MoveSelection", cmd::Argument(translation));

    EXPECT_EQ(entityNode->worldAABB().getOrigin(), originalPosition + translation);
}

// #5608: Path entities rotate every time when dragged
TEST_F(TransformationTest, TranslationAfterRotatingGenericEntity)
{
    // Repro steps:
    // - New Map
    // - Create Player start somewhere in the ortho view
    // - Dragging the player start won't change the direction of the arrow
    // - Hit "Z-Axis Rotate" to rotate the player start 90 degrees
    // - Dragging the player start will now add another rotation by 90 degrees(every time)

    // Create an entity which has editor_mins/editor_maxs defined (GenericEntity)
    auto eclass = GlobalEntityClassManager().findClass("fixed_size_entity");
    auto entityNode = GlobalEntityModule().createEntity(eclass);

    GlobalMapModule().getRoot()->addChildNode(entityNode);

    Node_setSelected(entityNode, true);

    // Rotate about Z
    GlobalCommandSystem().executeCommand("RotateSelectionZ");

    auto initialAngle = string::convert<int>(entityNode->getEntity().getKeyValue("angle"));
    EXPECT_EQ(initialAngle, -90);

    // Translate as if the thing was dragged around
    GlobalCommandSystem().executeCommand("MoveSelection", cmd::Argument(Vector3(10, 10, 10)));

    auto angleAfterTransformation = string::convert<int>(entityNode->getEntity().getKeyValue("angle"));
    EXPECT_EQ(angleAfterTransformation, initialAngle);
}

// Use case from #5096: Rotating multiple objects made them drift from the center
TEST_F(TransformationTest, RotateSelectionBackAndForthKeepsPosition)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto brush1 = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(-64, 96, 0), Vector3(8, 8, 8)));
    auto brush2 = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(0, 0, 0), Vector3(8, 8, 8)));
    auto brush3 = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(96, 96, 0), Vector3(8, 8, 8)));

    Node_setSelected(brush1, true);
    Node_setSelected(brush2, true);
    Node_setSelected(brush3, true);

    AABB originalBounds;
    originalBounds.includeAABB(brush1->worldAABB());
    originalBounds.includeAABB(brush2->worldAABB());
    originalBounds.includeAABB(brush3->worldAABB());

    // Rotate left, then right by the same angle (b1k3rdude was testing this in the UI)
    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 15)));
    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, -15)));

    AABB finalBounds;
    finalBounds.includeAABB(brush1->worldAABB());
    finalBounds.includeAABB(brush2->worldAABB());
    finalBounds.includeAABB(brush3->worldAABB());

    EXPECT_TRUE(math::isNear(originalBounds.getOrigin(), finalBounds.getOrigin(), 0.01))
        << "Selection drifted to " << finalBounds.getOrigin() << " after rotating back and forth, "
        << "expected " << originalBounds.getOrigin();
}

// #6729: Cloned/copied object rotates around some arbitrary origin after being moved
TEST_F(TransformationTest, RotationPivotFollowsMovedSelection)
{
    registry::setValue("user/ui/offsetClonedObjects", 0);

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(64, 0, 0), Vector3(32, 8, 8)));
    Node_setSelected(brush, true);

    GlobalCommandSystem().executeCommand("CloneSelection");
    ASSERT_EQ(GlobalSelectionSystem().countSelected(), 1);

    auto clone = GlobalSelectionSystem().ultimateSelected();

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));
    GlobalCommandSystem().executeCommand("MoveSelection", cmd::Argument(Vector3(256, 128, 0)));

    Vector3 centerBeforeRotation = clone->worldAABB().getOrigin();
    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    EXPECT_TRUE(math::isNear(clone->worldAABB().getOrigin(), centerBeforeRotation, 0.01))
        << "Rotation used a stale pivot, the clone moved from " << centerBeforeRotation
        << " to " << clone->worldAABB().getOrigin();
}

// #6729: Object rotates around some arbitrary origin after switching off "rotate objects independently"
TEST_F(TransformationTest, RotationPivotIsUpdatedAfterFreeObjectRotation)
{
    auto entityNode = algorithm::createEntityByClassName("func_static");
    GlobalMapModule().getRoot()->addChildNode(entityNode);
    algorithm::createCuboidBrush(entityNode, AABB(Vector3(128, 0, 0), Vector3(8, 8, 8)));
    Node_setSelected(entityNode, true);

    registry::setValue("user/ui/rotateObjectsIndependently", true);
    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    registry::setValue("user/ui/rotateObjectsIndependently", false);

    Vector3 centerBeforeRotation = entityNode->worldAABB().getOrigin();
    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    EXPECT_TRUE(math::isNear(entityNode->worldAABB().getOrigin(), centerBeforeRotation, 0.01))
        << "Rotation used a stale pivot, the entity moved from " << centerBeforeRotation
        << " to " << entityNode->worldAABB().getOrigin();
}

namespace
{

constexpr double DevicePointOnManipulatorSphere = 0.1;

Vector3 getSelectionBoundsCenter()
{
    AABB bounds;

    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        bounds.includeAABB(node->worldAABB());
    });

    return bounds.getOrigin();
}

void selectThreeSpreadOutBrushes()
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    Node_setSelected(algorithm::createCuboidBrush(worldspawn, AABB(Vector3(-64, 96, 0), Vector3(8, 8, 8))), true);
    Node_setSelected(algorithm::createCuboidBrush(worldspawn, AABB(Vector3(0, 0, 0), Vector3(8, 8, 8))), true);
    Node_setSelected(algorithm::createCuboidBrush(worldspawn, AABB(Vector3(96, 96, 0), Vector3(8, 8, 8))), true);
}

// Rotates the current selection using the rotate manipulator, as if the user
// dragged the mouse from the pivot to the given device point
void rotateUsingManipulator(const Vector2& devicePoint)
{
    GlobalSelectionSystem().setActiveManipulator(selection::IManipulator::Rotate);

    auto manipulator = GlobalSelectionSystem().getActiveManipulator();
    auto pivot2World = GlobalSelectionSystem().getPivot2World();

    render::View view(false);
    algorithm::constructCenteredOrthoview(view, pivot2World.translation());

    manipulator->setSelected(false);

    GlobalSelectionSystem().onManipulationStart();
    manipulator->getActiveComponent()->beginTransformation(pivot2World, view, Vector2(0, 0));
    manipulator->getActiveComponent()->transform(pivot2World, view, devicePoint, 0);
    GlobalSelectionSystem().onManipulationChanged();
    GlobalSelectionSystem().onManipulationEnd();
}

// Grabs the pivot point of the rotate manipulator and drags it to the given device point
void dragPivotUsingManipulator(const Vector2& devicePoint, bool cancel)
{
    GlobalSelectionSystem().setActiveManipulator(selection::IManipulator::Rotate);

    auto manipulator = GlobalSelectionSystem().getActiveManipulator();
    auto pivot2World = GlobalSelectionSystem().getPivot2World();

    render::View view(false);
    algorithm::constructCenteredOrthoview(view, pivot2World.translation());
    auto test = algorithm::constructOrthoviewSelectionTest(view);

    manipulator->setSelected(false);
    manipulator->testSelect(test, pivot2World);
    ASSERT_TRUE(manipulator->isSelected()) << "Failed to grab the pivot point of the rotate manipulator";

    GlobalSelectionSystem().onManipulationStart();
    manipulator->getActiveComponent()->beginTransformation(pivot2World, view, Vector2(0, 0));
    manipulator->getActiveComponent()->transform(pivot2World, view, devicePoint, 0);
    GlobalSelectionSystem().onManipulationChanged();

    if (cancel)
    {
        GlobalSelectionSystem().onManipulationCancelled();
    }
    else
    {
        GlobalSelectionSystem().onManipulationEnd();
    }
}

}

// #5096: The anchored pivot must not be dropped by bounds changes elsewhere in the scene
TEST_F(TransformationTest, RotationPivotSurvivesUnrelatedSceneChanges)
{
    selectThreeSpreadOutBrushes();

    auto originalCenter = getSelectionBoundsCenter();

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 15)));

    // An unrelated brush appears in the scene, marking the pivot dirty
    algorithm::createCuboidBrush(GlobalMapModule().findOrInsertWorldspawn(),
        AABB(Vector3(512, 512, 0), Vector3(8, 8, 8)));

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, -15)));

    EXPECT_TRUE(math::isNear(getSelectionBoundsCenter(), originalCenter, 0.01))
        << "Selection drifted to " << getSelectionBoundsCenter()
        << " after an unrelated scene change discarded the rotation pivot, expected " << originalCenter;
}

// #5096: Rotating with the mouse must keep the pivot at the point we rotated about
TEST_F(TransformationTest, ManipulatorRotationKeepsPivotInPlace)
{
    selectThreeSpreadOutBrushes();

    GlobalSelectionSystem().setActiveManipulator(selection::IManipulator::Rotate);

    auto pivotBeforeRotation = GlobalSelectionSystem().getPivot2World().translation();
    ASSERT_TRUE(math::isNear(pivotBeforeRotation, getSelectionBoundsCenter(), 0.01));

    rotateUsingManipulator(Vector2(DevicePointOnManipulatorSphere, DevicePointOnManipulatorSphere));

    ASSERT_FALSE(math::isNear(getSelectionBoundsCenter(), pivotBeforeRotation, 0.01))
        << "Test setup problem: the bounds center is expected to shift when the selection is rotated";

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(), pivotBeforeRotation, 0.01))
        << "Pivot jumped from " << pivotBeforeRotation << " to the new bounds center "
        << GlobalSelectionSystem().getPivot2World().translation() << " after a mouse rotation";
}

// #6729: After a mouse rotation the pivot must still follow the selection when it is moved
TEST_F(TransformationTest, PivotFollowsSelectionMovedAfterManipulatorRotation)
{
    selectThreeSpreadOutBrushes();

    rotateUsingManipulator(Vector2(DevicePointOnManipulatorSphere, DevicePointOnManipulatorSphere));

    auto pivotAfterRotation = GlobalSelectionSystem().getPivot2World().translation();

    Vector3 translation(256, 128, 64);
    GlobalCommandSystem().executeCommand("MoveSelection", cmd::Argument(translation));

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(),
        pivotAfterRotation + translation, 0.01))
        << "Pivot is at " << GlobalSelectionSystem().getPivot2World().translation()
        << " after the selection moved, expected " << pivotAfterRotation + translation;
}

// #6729: A manually placed pivot must not stay behind when the selection is moved
TEST_F(TransformationTest, UserPlacedPivotFollowsMovedSelection)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(0, 0, 0), Vector3(64, 64, 64)));
    Node_setSelected(brush, true);

    dragPivotUsingManipulator(Vector2(0.25, 0.25), false);

    auto userPivot = GlobalSelectionSystem().getPivot2World().translation();
    ASSERT_FALSE(math::isNear(userPivot, brush->worldAABB().getOrigin(), 0.01))
        << "Test setup problem: the pivot was expected to be dragged away from the brush center";

    Vector3 translation(128, 256, 0);
    GlobalCommandSystem().executeCommand("MoveSelection", cmd::Argument(translation));

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(),
        userPivot + translation, 0.01))
        << "The manually placed pivot stayed at " << GlobalSelectionSystem().getPivot2World().translation()
        << " instead of following the selection to " << userPivot + translation;
}

TEST_F(TransformationTest, UserPlacedPivotIsKeptWhenRotating)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(0, 0, 0), Vector3(64, 64, 64)));
    Node_setSelected(brush, true);

    dragPivotUsingManipulator(Vector2(0.25, 0.25), false);

    auto userPivot = GlobalSelectionSystem().getPivot2World().translation();

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(), userPivot, 0.01))
        << "The manually placed pivot should not move when the selection is rotated about it";
}

TEST_F(TransformationTest, UserPlacedPivotIsResetOnSelectionChange)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(0, 0, 0), Vector3(64, 64, 64)));
    Node_setSelected(brush, true);

    dragPivotUsingManipulator(Vector2(0.25, 0.25), false);

    ASSERT_FALSE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(),
        brush->worldAABB().getOrigin(), 0.01));

    Node_setSelected(brush, false);
    Node_setSelected(brush, true);

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(),
        brush->worldAABB().getOrigin(), 0.01))
        << "Re-selecting the brush should have re-centered the pivot";
}

TEST_F(TransformationTest, CancellingPivotPlacementRestoresPivot)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCuboidBrush(worldspawn, AABB(Vector3(0, 0, 0), Vector3(64, 64, 64)));
    Node_setSelected(brush, true);

    dragPivotUsingManipulator(Vector2(0.25, 0.25), true);

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(),
        brush->worldAABB().getOrigin(), 0.01))
        << "Cancelling the pivot placement should have restored the pivot to the brush center";
}

TEST_F(TransformationTest, RotationPivotIsResetAfterUndo)
{
    selectThreeSpreadOutBrushes();

    auto originalCenter = getSelectionBoundsCenter();

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 45)));

    ASSERT_FALSE(math::isNear(getSelectionBoundsCenter(), originalCenter, 0.01))
        << "Test setup problem: the bounds center is expected to shift when the selection is rotated";

    GlobalCommandSystem().executeCommand("Undo");

    ASSERT_TRUE(math::isNear(getSelectionBoundsCenter(), originalCenter, 0.01))
        << "Undo did not restore the selection";

    EXPECT_TRUE(math::isNear(GlobalSelectionSystem().getPivot2World().translation(), originalCenter, 0.01))
        << "Pivot is at " << GlobalSelectionSystem().getPivot2World().translation()
        << " after undoing the rotation, expected " << originalCenter;
}

// #6729: The reported case, an entity whose bounding box is not centered on its origin
TEST_F(TransformationTest, RotatingMovedEntityKeepsItInPlace)
{
    auto entityNode = algorithm::createEntityByClassName("offset_box_entity");
    GlobalMapModule().getRoot()->addChildNode(entityNode);
    Node_setSelected(entityNode, true);

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));
    GlobalCommandSystem().executeCommand("MoveSelection", cmd::Argument(Vector3(512, -256, 0)));

    auto positionBeforeRotation = entityNode->worldAABB().getOrigin();
    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    EXPECT_TRUE(math::isNear(entityNode->worldAABB().getOrigin(), positionBeforeRotation, 0.01))
        << "The entity was expected to rotate on the spot, but it moved from "
        << positionBeforeRotation << " to " << entityNode->worldAABB().getOrigin();
}

// #6729: Same as above, but the entity is moved with the drag manipulator
TEST_F(TransformationTest, RotatingDragMovedEntityKeepsItInPlace)
{
    auto entityNode = algorithm::createEntityByClassName("offset_box_entity");
    GlobalMapModule().getRoot()->addChildNode(entityNode);
    Node_setSelected(entityNode, true);

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    GlobalSelectionSystem().setSelectionMode(selection::SelectionMode::Entity);
    GlobalSelectionSystem().setActiveManipulator(selection::IManipulator::Drag);

    render::View view(false);
    algorithm::constructCenteredOrthoview(view, entityNode->worldAABB().getOrigin());
    auto test = algorithm::constructOrthoviewSelectionTest(view);

    auto manipulator = GlobalSelectionSystem().getActiveManipulator();
    auto pivot2World = GlobalSelectionSystem().getPivot2World();

    manipulator->testSelect(test, pivot2World);
    ASSERT_TRUE(manipulator->isSelected());

    GlobalSelectionSystem().onManipulationStart();
    manipulator->getActiveComponent()->beginTransformation(pivot2World, view, Vector2(0, 0));
    manipulator->getActiveComponent()->transform(pivot2World, view, Vector2(0.5, 0.5), 0);
    GlobalSelectionSystem().onManipulationChanged();
    GlobalSelectionSystem().onManipulationEnd();

    auto positionBeforeRotation = entityNode->worldAABB().getOrigin();
    ASSERT_FALSE(math::isNear(positionBeforeRotation, Vector3(0, 0, 34), 0.01)) << "The entity should have been dragged";

    GlobalCommandSystem().executeCommand("RotateSelectedEulerXYZ", cmd::Argument(Vector3(0, 0, 90)));

    EXPECT_TRUE(math::isNear(entityNode->worldAABB().getOrigin(), positionBeforeRotation, 0.01))
        << "The entity was expected to rotate on the spot, but it moved from "
        << positionBeforeRotation << " to " << entityNode->worldAABB().getOrigin();
}

scene::INodePtr createAndSelectLight()
{
    // Create an entity which has editor_mins/editor_maxs defined (GenericEntity)
    auto eclass = GlobalEntityClassManager().findClass("light");
    auto entityNode = GlobalEntityModule().createEntity(eclass);

    GlobalMapModule().getRoot()->addChildNode(entityNode);

    Node_setSelected(entityNode, true);

    // Check the prerequisites
    EXPECT_EQ(entityNode->worldAABB(), AABB(Vector3(0, 0, 0), Vector3(320, 320, 320)));

    return entityNode;
}

void selectLightPlaneAt320(const scene::INodePtr& entityNode)
{
    // Select the plane on the "right"
    auto planeSelectable = Node_getPlaneSelectable(entityNode);
    EXPECT_TRUE(planeSelectable);

    // Construct an orthoview to test-select the light
    render::View view(false);
    algorithm::constructCenteredOrthoview(view, Vector3(400, 0, 0));

    SelectionVolume test = algorithm::constructOrthoviewSelectionTest(view);

    selection::SingleItemSelector selector;
    selection::SelectedPlaneSet selectedPlanes;
    planeSelectable->selectPlanes(selector, test, std::bind(&selection::SelectedPlaneSet::insert, &selectedPlanes, std::placeholders::_1));

    EXPECT_FALSE(selectedPlanes.empty()) << "Failed to select the light plane at X=+320";
    EXPECT_TRUE(selectedPlanes.contains(Plane3(1, 0, 0, 320))) << "Failed to select the light plane at X=+320";

    // Select that plane
    selector.getSelectable()->setSelected(true);
}

// #5644: Non uniform light volume scaling not working - the origin is translated ever further off the screen
TEST_F(TransformationTest, NonUniformLightDragResize)
{
    // Disable the symmetric drag-resize mode
    GlobalEntityModule().getSettings().setDragResizeEntitiesSymmetrically(false);

    auto entityNode = createAndSelectLight();
    selectLightPlaneAt320(entityNode);

    auto transformable = scene::node_cast<ITransformable>(entityNode);
    transformable->setType(TRANSFORM_COMPONENT); // we manipulate a component (a plane)
    transformable->setTranslation({ -64, 0, 0 });

    // The light origin should have moved one half of the translation to the left
    EXPECT_EQ(entityNode->worldAABB().getOrigin(), Vector3(-32, 0, 0)); // moved by 32 units
    EXPECT_EQ(entityNode->worldAABB().getExtents(), Vector3(288, 320, 320)); // reduced by 32 units

    // Now set the translation back to 0,0,0, the light should properly reset its transformation
    // to the original state and then apply a 0-length translation, i.e. do nothing
    transformable->setTranslation({ 0, 0, 0 });
    EXPECT_EQ(entityNode->worldAABB().getOrigin(), Vector3(0, 0, 0));
    EXPECT_EQ(entityNode->worldAABB().getExtents(), Vector3(320, 320, 320));
}

TEST_F(TransformationTest, UniformLightDragResize)
{
    // Disable the symmetric drag-resize mode
    GlobalEntityModule().getSettings().setDragResizeEntitiesSymmetrically(true);

    auto entityNode = createAndSelectLight();
    selectLightPlaneAt320(entityNode);

    auto transformable = scene::node_cast<ITransformable>(entityNode);
    transformable->setType(TRANSFORM_COMPONENT); // we manipulate a component (a plane)
    transformable->setTranslation({ -64, 0, 0 });

    // The light origin should not have moved, just the radius should be reduced by 64 on both sides
    EXPECT_EQ(entityNode->worldAABB().getOrigin(), Vector3(0, 0, 0));
    EXPECT_EQ(entityNode->worldAABB().getExtents(), Vector3(320-64, 320, 320)); // reduced by 64 units

    // Now set the translation back to 0,0,0, the light should properly reset its transformation
    // to the original state and then apply a 0-length translation, i.e. do nothing
    transformable->setTranslation({ 0, 0, 0 });
    EXPECT_EQ(entityNode->worldAABB().getOrigin(), Vector3(0, 0, 0));
    EXPECT_EQ(entityNode->worldAABB().getExtents(), Vector3(320, 320, 320));
}

TEST_F(TransformationTest, CloneSelectedPlacesNodeInActiveLayer)
{
    auto& layerManager = GlobalMapModule().getRoot()->getLayerManager();

    // Create a layer and make it active
    auto testLayerId = layerManager.createLayer("TestLayer");
    layerManager.setActiveLayer(testLayerId);

    // Create an entity on the default layer and select it
    auto entityNode = algorithm::createEntityByClassName("fixed_size_entity");
    GlobalMapModule().getRoot()->addChildNode(entityNode);
    entityNode->moveToLayer(0);
    Node_setSelected(entityNode, true);

    EXPECT_EQ(entityNode->getLayers(), scene::LayerList{ 0 });

    GlobalCommandSystem().executeCommand("CloneSelection");

    EXPECT_EQ(entityNode->getLayers(), scene::LayerList{ 0 });

    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1);
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        EXPECT_EQ(node->getLayers(), scene::LayerList{ testLayerId })
            << "Cloned node should be placed in the active layer";
    });
}

TEST_F(TransformationTest, CloneSelectedDefaultLayerStaysOnDefault)
{
    auto& layerManager = GlobalMapModule().getRoot()->getLayerManager();

    EXPECT_EQ(layerManager.getActiveLayer(), 0);

    auto entityNode = algorithm::createEntityByClassName("fixed_size_entity");
    GlobalMapModule().getRoot()->addChildNode(entityNode);
    Node_setSelected(entityNode, true);

    EXPECT_EQ(entityNode->getLayers(), scene::LayerList{ 0 });

    GlobalCommandSystem().executeCommand("CloneSelection");

    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1);
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        EXPECT_EQ(node->getLayers(), scene::LayerList{ 0 })
            << "Cloned node should be on the default layer when it is active";
    });
}

TEST_F(TransformationTest, CloneSelectedMovesChildrenToActiveLayer)
{
    auto& layerManager = GlobalMapModule().getRoot()->getLayerManager();

    auto testLayerId = layerManager.createLayer("TestLayer");
    layerManager.setActiveLayer(testLayerId);

    auto entityNode = algorithm::createEntityByClassName("func_static");
    GlobalMapModule().getRoot()->addChildNode(entityNode);
    auto brushNode = algorithm::createCubicBrush(entityNode);
    entityNode->moveToLayer(0);
    brushNode->moveToLayer(0);
    Node_setSelected(entityNode, true);

    EXPECT_EQ(entityNode->getLayers(), scene::LayerList{ 0 });
    EXPECT_EQ(brushNode->getLayers(), scene::LayerList{ 0 });

    GlobalCommandSystem().executeCommand("CloneSelection");

    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1);
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        EXPECT_EQ(node->getLayers(), scene::LayerList{ testLayerId })
            << "Cloned entity should be on the active layer";

        node->foreachNode([&](const scene::INodePtr& child)
        {
            EXPECT_EQ(child->getLayers(), scene::LayerList{ testLayerId })
                << "Cloned child brush should be on the active layer";
            return true;
        });
    });
}

TEST_F(TransformationTest, ArrayCloneLineFixedOffset)
{
    auto eclass = GlobalEntityClassManager().findClass("fixed_size_entity");
    auto entityNode = GlobalEntityModule().createEntity(eclass);

    GlobalMapModule().getRoot()->addChildNode(entityNode);
    Node_setSelected(entityNode, true);

    Vector3 originalPosition = entityNode->worldAABB().getOrigin();
    EXPECT_EQ(originalPosition, Vector3(0, 0, 0));

    // Create 3 copies with a fixed offset of (100, 0, 0), no rotation
    int count = 3;
    int offsetMethod = static_cast<int>(ui::ArrayOffsetMethod::Fixed);
    Vector3 offset(100, 0, 0);
    Vector3 rotation(0, 0, 0);

    GlobalCommandSystem().executeCommand("ArrayCloneSelectionLine",
        { cmd::Argument(count), cmd::Argument(offsetMethod),
          cmd::Argument(offset), cmd::Argument(rotation) });

    // We should have 4 selected items: original + 3 clones
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 4);

    // Collect positions of all selected nodes
    std::vector<Vector3> positions;
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        positions.push_back(node->worldAABB().getOrigin());
    });

    ASSERT_EQ(positions.size(), 4);

    // Sort by X to get deterministic order
    std::sort(positions.begin(), positions.end(),
        [](const Vector3& a, const Vector3& b) { return a.x() < b.x(); });

    EXPECT_TRUE(math::isNear(positions[0], Vector3(0, 0, 0), 0.1))
        << "Original should remain at origin";
    EXPECT_TRUE(math::isNear(positions[1], Vector3(100, 0, 0), 0.1))
        << "First clone should be at offset 1x";
    EXPECT_TRUE(math::isNear(positions[2], Vector3(200, 0, 0), 0.1))
        << "Second clone should be at offset 2x";
    EXPECT_TRUE(math::isNear(positions[3], Vector3(300, 0, 0), 0.1))
        << "Third clone should be at offset 3x";
}

TEST_F(TransformationTest, ArrayCloneLineEndpointOffset)
{
    auto eclass = GlobalEntityClassManager().findClass("fixed_size_entity");
    auto entityNode = GlobalEntityModule().createEntity(eclass);

    GlobalMapModule().getRoot()->addChildNode(entityNode);
    Node_setSelected(entityNode, true);

    // Endpoint mode: offset represents total distance, divided evenly among copies
    int count = 4;
    int offsetMethod = static_cast<int>(ui::ArrayOffsetMethod::Endpoint);
    Vector3 totalOffset(400, 0, 0);
    Vector3 rotation(0, 0, 0);

    GlobalCommandSystem().executeCommand("ArrayCloneSelectionLine",
        { cmd::Argument(count), cmd::Argument(offsetMethod),
          cmd::Argument(totalOffset), cmd::Argument(rotation) });

    // 5 selected: original + 4 clones
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 5);

    std::vector<Vector3> positions;
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        positions.push_back(node->worldAABB().getOrigin());
    });

    std::sort(positions.begin(), positions.end(),
        [](const Vector3& a, const Vector3& b) { return a.x() < b.x(); });

    ASSERT_EQ(positions.size(), 5);

    // Each clone should be offset by totalOffset/count = (100,0,0) * i
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_TRUE(math::isNear(positions[i], Vector3(100.0 * i, 0, 0), 0.1))
            << "Clone " << i << " position mismatch";
    }
}

TEST_F(TransformationTest, ArrayCloneCircle)
{
    auto eclass = GlobalEntityClassManager().findClass("fixed_size_entity");
    auto entityNode = GlobalEntityModule().createEntity(eclass);

    GlobalMapModule().getRoot()->addChildNode(entityNode);
    Node_setSelected(entityNode, true);

    int count = 4;
    double radius = 200.0;
    double startAngle = 0.0;
    double endAngle = 360.0;
    int rotateToCenter = 0;

    GlobalCommandSystem().executeCommand("ArrayCloneSelectionCircle",
        { cmd::Argument(count), cmd::Argument(radius),
          cmd::Argument(startAngle), cmd::Argument(endAngle),
          cmd::Argument(rotateToCenter) });

    // 5 selected: original + 4 clones
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 5);

    // Each clone should be a certain distance from origin
    int clonesAtExpectedRadius = 0;
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        auto pos = node->worldAABB().getOrigin();
        double dist = pos.getLength();

        // The original is at origin, clones within radius
        if (dist > 1.0)
        {
            EXPECT_NEAR(dist, radius, 1.0)
                << "Clone should be at the specified radius";
            ++clonesAtExpectedRadius;
        }
    });

    EXPECT_EQ(clonesAtExpectedRadius, 4) << "All 4 clones should be at the circle radius";
}

}
