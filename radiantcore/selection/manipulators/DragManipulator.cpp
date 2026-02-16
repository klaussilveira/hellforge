#include "DragManipulator.h"

#include "scene/Entity.h"
#include "selection/SelectionPool.h"
#include "selection/SelectionTestWalkers.h"
#include "selection/algorithm/Planes.h"

namespace selection
{

namespace
{
    // Predicate function used to pick selectable Sectables only when drag-manipulating
    bool filterSelectedItemsOnly(ISelectable* selectable)
    {
        return selectable->isSelected();
    }
}

DragManipulator::DragManipulator(ManipulationPivot& pivot, SelectionSystem& selectionSystem, ISceneSelectionTesterFactory& factory) :
    _pivot(pivot),
    _selectionSystem(selectionSystem),
    _testerFactory(factory),
    _freeResizeComponent(_resizeTranslatable),
    _resizeModeActive(false),
    _freeDragComponent(_dragTranslatable),
    _dragTranslatable(SelectionTranslator::TranslationCallback()),
    _entityScaleModeActive(false),
    _renderableEntityAABBs(_entityAABBs),
    _renderableEntityCornerPoints(_entityAABBs)
{}

DragManipulator::~DragManipulator()
{
    clearRenderables();
}

DragManipulator::Type DragManipulator::getType() const
{
	return Drag;
}

DragManipulator::Component* DragManipulator::getActiveComponent()
{
    if (_entityScaleModeActive)
    {
        return &_entityScaleComponent;
    }

    return _dragSelectable.isSelected() ? &_freeDragComponent : &_freeResizeComponent;
}

void DragManipulator::testSelect(SelectionTest& test, const Matrix4& pivot2world)
{
	_resizeModeActive = false;
	_entityScaleModeActive = false;
	_curScaleEntity.reset();

    // No drag manipulation in merge mode
    if (_selectionSystem.getSelectionMode() == SelectionMode::MergeAction) return;

    // First, check if a model entity corner point is hit (for scaling)
    if (testSelectEntityScale(test))
    {
        _entityScaleModeActive = true;
        return;
    }

    SelectionPool selector;

	switch (_selectionSystem.getSelectionMode())
	{
	case SelectionMode::Primitive:
		testSelectPrimitiveMode(test.getVolume(), test, selector);
		break;
	case SelectionMode::GroupPart:
		testSelectGroupPartMode(test.getVolume(), test, selector);
		break;
	case SelectionMode::Entity:
		testSelectEntityMode(test.getVolume(), test, selector);
		break;
	case SelectionMode::Component:
		testSelectComponentMode(test.getVolume(), test, selector);
		break;
	default:
        return;
	};

	for (auto& [_, selectable] : selector)
	{
		selectable->setSelected(true);
	}
}

bool DragManipulator::testSelectedItemsInScene(SelectionMode mode, const VolumeTest& view, SelectionTest& test)
{
    auto tester = _testerFactory.createSceneSelectionTester(mode);
    tester->testSelectSceneWithFilter(view, test, filterSelectedItemsOnly);

    return tester->hasSelectables();
}

void DragManipulator::testSelectPrimitiveMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
    // If testing for entities and worldspawn primitives fails check for group children too
    if (testSelectedItemsInScene(SelectionMode::Primitive, view, test) ||
        testSelectedItemsInScene(SelectionMode::GroupPart, view, test))
    {
        selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
        return;
    }

    // all direct hits failed, check for drag-selectable faces
    _resizeModeActive = algorithm::testSelectPlanes(selector, test);
}

void DragManipulator::testSelectGroupPartMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
	// Find all non-worldspawn child primitives that are selectable
	if (testSelectedItemsInScene(SelectionMode::GroupPart, view, test))
	{
		// Found a selectable primitive
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
		return;
	}

	// Check for selectable faces
	_resizeModeActive = algorithm::testSelectPlanes(selector, test);
}

void DragManipulator::testSelectEntityMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
	// Check, if an entity could be found
	if (testSelectedItemsInScene(SelectionMode::Entity, view, test))
	{
		selector.addSelectable(SelectionIntersection(0, 0), &_dragSelectable);
        return;
	}

    // Check for selectable faces
    _resizeModeActive = algorithm::testSelectPlanes(selector, test);
}

void DragManipulator::testSelectComponentMode(const VolumeTest& view, SelectionTest& test, SelectionPool& selector)
{
    auto tester = _testerFactory.createSceneSelectionTester(SelectionMode::Component);
    tester->testSelectScene(view, test); // don't restrict drag-selecting to selected components

    tester->foreachSelectable([&](auto selectable)
    {
        // greebo: Transient component selection: clicking an unselected
        // component will deselect all previously selected components beforehand
        if (!selectable->isSelected())
        {
            _selectionSystem.setSelectedAllComponents(false);
        }

        selector.addSelectable(SelectionIntersection(0, 0), selectable);
        _dragSelectable.setSelected(true);
    });
}

bool DragManipulator::testSelectEntityScale(SelectionTest& test)
{
	bool found = false;

	GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
	{
		if (found) return;

		Entity* entity = node->tryGetEntity();
		if (!entity || !entity->isModel()) return;

		const AABB& aabb = node->worldAABB();

		Vector3 points[8];
		aabb.getCorners(points);

		for (std::size_t i = 0; i < 8; ++i)
		{
			if (test.getVolume().TestPoint(points[i]))
			{
				_curScaleEntity = node;

				// Use the opposite corner as scale pivot
				Vector3 scalePivot = aabb.origin * 2 - points[i];

				_entityScaleComponent.setEntityNode(node);
				_entityScaleComponent.setScalePivot(scalePivot);

				found = true;
				break;
			}
		}
	});

	return found;
}

void DragManipulator::setSelected(bool select)
{
    _resizeModeActive = select;
    _entityScaleModeActive = select;
    _dragSelectable.setSelected(select);

    if (!select)
    {
        _curScaleEntity.reset();
    }
}

bool DragManipulator::isSelected() const
{
	return _entityScaleModeActive || _resizeModeActive || _dragSelectable.isSelected();
}

void DragManipulator::onPreRender(const RenderSystemPtr& renderSystem, const VolumeTest& volume)
{
    if (!renderSystem)
    {
        clearRenderables();
        _entityAABBs.clear();
        return;
    }

    if (!_lineShader)
    {
        _lineShader = renderSystem->capture(BuiltInShaderType::ManipulatorWireframe);
    }

    if (!_pointShader)
    {
        _pointShader = renderSystem->capture(BuiltInShaderType::BigPoint);
    }

    _entityAABBs.clear();

    // Collect AABBs from selected model entities
    GlobalSelectionSystem().foreachSelected([&](const scene::INodePtr& node)
    {
        Entity* entity = node->tryGetEntity();

        if (entity && entity->isModel())
        {
            _entityAABBs.push_back(node->worldAABB());
        }
    });

    if (!_entityAABBs.empty())
    {
        _renderableEntityCornerPoints.setColour(_curScaleEntity ? COLOUR_SELECTED() : COLOUR_SCREEN());
        _renderableEntityCornerPoints.queueUpdate();
        _renderableEntityAABBs.queueUpdate();

        _renderableEntityAABBs.update(_lineShader);
        _renderableEntityCornerPoints.update(_pointShader);
    }
    else
    {
        _renderableEntityCornerPoints.clear();
        _renderableEntityAABBs.clear();
    }
}

void DragManipulator::clearRenderables()
{
    _renderableEntityCornerPoints.clear();
    _renderableEntityAABBs.clear();
    _lineShader.reset();
    _pointShader.reset();
}

}
