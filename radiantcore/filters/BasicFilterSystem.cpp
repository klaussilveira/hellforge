#include "BasicFilterSystem.h"

#include <functional>

#include "icommandsystem.h"
#include "itextstream.h"
#include "iscenegraph.h"
#include "iregistry.h"
#include "igame.h"
#include "ishaders.h"

#include "module/StaticModule.h"
#include "InstanceUpdateWalker.h"
#include "SetObjectSelectionByFilterWalker.h"

namespace filters
{

namespace
{
    // Registry key for .game-defined filters
    const std::string RKEY_GAME_FILTERS = "/filtersystem//filter";

    const std::string RKEY_USER_FILTER_BASE = "user/ui/filtersystem";

    // Registry key for user-defined filters
    const std::string RKEY_USER_FILTERS = RKEY_USER_FILTER_BASE + "/filters//filter";

    // Registry key for persistent filter setting
    const std::string RKEY_USER_ACTIVE_FILTERS = RKEY_USER_FILTER_BASE + "//activeFilter";
}

void BasicFilterSystem::setAllFilterStates(bool state)
{
    if (state)
    {
        _activeFilters = _availableFilters;
    }
    else
    {
        _activeFilters.clear();
    }

    // Invalidate the visibility cache to force new values to be
    // loaded from the filters themselves
    _visibilityCache.clear();

    // Update the scenegraph instances
    update();

    _filterConfigChangedSignal.emit();

    // Trigger an immediate scene redraw
    GlobalSceneGraph().sceneChanged();
}

void BasicFilterSystem::selectObjectsByFilterCmd(const cmd::ArgumentList& args)
{
    if (args.size() != 1)
    {
        rMessage() << "Usage: SelectObjectsByFilter \"FilterName\"" << std::endl;
        return;
    }

    setObjectSelectionByFilter(args[0].getString(), true);
}

void BasicFilterSystem::deselectObjectsByFilterCmd(const cmd::ArgumentList& args)
{
    if (args.size() != 1)
    {
        rMessage() << "Usage: DeselectObjectsByFilter \"FilterName\"" << std::endl;
        return;
    }

    setObjectSelectionByFilter(args[0].getString(), false);
}

void BasicFilterSystem::setObjectSelectionByFilter(const std::string& filterName, bool select)
{
    if (!GlobalSceneGraph().root())
    {
        rError() << "No map loaded." << std::endl;
        return;
    }

    auto f = _availableFilters.find(filterName);

    if (f == _availableFilters.end())
    {
        rError() << "Cannot find the filter named " << filterName << std::endl;
        return;
    }

    SetObjectSelectionByFilterWalker walker(*f->second, select);
    GlobalSceneGraph().root()->traverse(walker);
}

void BasicFilterSystem::initialiseModule(const IApplicationContext& ctx)
{
    game::IGamePtr game = GlobalGameManager().currentGame();
    assert(game);

    // Ask the XML Registry for filter nodes (from .game file and from user's filters.xml)
    xml::NodeList filters = game->getLocalXPath(RKEY_GAME_FILTERS);
    xml::NodeList userFilters = GlobalRegistry().findXPath(RKEY_USER_FILTERS);

    rMessage() << "[filters] Loaded " << (filters.size() + userFilters.size())
              << " filters from registry." << std::endl;

    // Read-only filters
    addFiltersFromXML(filters, true);

    // user-defined filters
    addFiltersFromXML(userFilters, false);

    // Shortcuts to enable/disable all filters
    GlobalCommandSystem().addCommand(
        "ActivateAllFilters", [=](const cmd::ArgumentList&) { setAllFilterStates(true); }
    );
    GlobalCommandSystem().addCommand(
        "DeactivateAllFilters", [=](const cmd::ArgumentList&) { setAllFilterStates(false); }
    );

    GlobalCommandSystem().addCommand(SELECT_OBJECTS_BY_FILTER_CMD,
        std::bind(&BasicFilterSystem::selectObjectsByFilterCmd, this, std::placeholders::_1), { cmd::ARGTYPE_STRING });

    GlobalCommandSystem().addCommand(DESELECT_OBJECTS_BY_FILTER_CMD,
        std::bind(&BasicFilterSystem::deselectObjectsByFilterCmd, this, std::placeholders::_1), { cmd::ARGTYPE_STRING });
}

void BasicFilterSystem::addFiltersFromXML(const xml::NodeList& nodes, bool readOnly)
{
    // Load the list of active filter names from the user tree. There is no
    // guarantee that these are actually valid filters in the .game file
    std::set<std::string> activeFilterNames;
    xml::NodeList activeFilters = GlobalRegistry().findXPath(RKEY_USER_ACTIVE_FILTERS);

    for (const auto& node : activeFilters)
    {
        // Add the name of this filter to the set
        activeFilterNames.insert(node.getAttributeValue("name"));
    }

    // Iterate over the list of nodes, adding filter objects onto the list
    for (const auto& node : nodes)
    {
        // Add this SceneFilter to the list of available filters
        auto filter = std::make_shared<SceneFilter>(node, readOnly);
        SceneFilter::Ptr inserted = _availableFilters.emplace(filter->getName(), filter).first->second;

        // If this filter is in our active set, enable it
        bool filterShouldBeActive = activeFilterNames.find(filter->getName()) != activeFilterNames.end();
        ensureEventAdapter(*inserted);
        if (filterShouldBeActive)
        {
            _activeFilters.emplace(filter->getName(), inserted);
        }
    }
}

XmlFilterEventAdapter::Ptr BasicFilterSystem::ensureEventAdapter(SceneFilter& filter)
{
    auto existing = _eventAdapters.find(filter.getName());

    if (existing != _eventAdapters.end())
    {
        return existing->second;
    }

    auto result = _eventAdapters.emplace(filter.getName(),
        std::make_shared<XmlFilterEventAdapter>(filter));

    return result.first->second;
}

// Shut down the Filters module, saving active filters to registry
void BasicFilterSystem::shutdownModule()
{
    // Remove the existing set of active filter nodes
    GlobalRegistry().deleteXPath(RKEY_USER_ACTIVE_FILTERS);

    // Add a node for each active filter
    for (const auto& pair : _activeFilters)
    {
        GlobalRegistry().createKeyWithName(RKEY_USER_FILTER_BASE, "activeFilter", pair.first);
    }

    // Save user-defined filters too (delete all first)
    GlobalRegistry().deleteXPath(RKEY_USER_FILTER_BASE + "/filters");

    // Create the new top-level node to hold all filters
    auto filterParent = GlobalRegistry().createKey(RKEY_USER_FILTER_BASE + "/filters");

    for (const auto& [name, filter] : _availableFilters)
    {
        // Don't save stock filters
        if (!filter->isReadOnly()) {
            filter->saveToNode(filterParent);
        }
    }

    _visibilityCache.clear();
    _eventAdapters.clear();
    _activeFilters.clear();
    _availableFilters.clear();

    _filterCollectionChangedSignal.clear();
    _filterConfigChangedSignal.clear();
}

sigc::signal<void> BasicFilterSystem::filterConfigChangedSignal() const
{
    return _filterConfigChangedSignal;
}

sigc::signal<void> BasicFilterSystem::filterCollectionChangedSignal() const
{
    return _filterCollectionChangedSignal;
}

void BasicFilterSystem::update()
{
    // Update shaders first, so that nodes can judge whether they're hidden on basis of their texture
    updateShaders();

    // Now update the scene
    updateScene();
}

void BasicFilterSystem::forEachFilter(const std::function<void(const filters::SceneFilter&)>& func)
{
    // Visit each filter on the list, passing the name to the visitor
    for (const auto& pair : _availableFilters)
    {
        func(*pair.second);
    }
}

std::string BasicFilterSystem::getFilterEventName(const std::string& filter)
{
    auto f = _availableFilters.find(filter);

    return f != _availableFilters.end() ? f->second->getEventName() : std::string();
}

bool BasicFilterSystem::getFilterState(const std::string& filter) const
{
    return _activeFilters.find(filter) != _activeFilters.end();
}

// Change the state of a named filter
void BasicFilterSystem::setFilterState(const std::string& filter, bool state)
{
    assert(!_availableFilters.empty());

    if (state)
    {
        // Copy the filter to the active filters list
        _activeFilters.emplace(filter, _availableFilters.find(filter)->second);
    }
    else
    {
        // Remove filter from active filters list
        _activeFilters.erase(filter);
    }

    // Invalidate the visibility cache to force new values to be
    // loaded from the filters themselves
    _visibilityCache.clear();

    // Update the scenegraph instances
    update();

    _filterConfigChangedSignal.emit();

    // Trigger an immediate scene redraw
    GlobalSceneGraph().sceneChanged();
}

bool BasicFilterSystem::addFilter(const std::string& filterName, const FilterRules& ruleSet)
{
    auto f = _availableFilters.find(filterName);

    if (f != _availableFilters.end())
    {
        return false; // already exists
    }

    auto filter = std::make_shared<SceneFilter>(filterName, false);
    _availableFilters.emplace(filterName, filter);

    // Apply the ruleset
    filter->setRules(ruleSet);

    // Create the event adapter
    ensureEventAdapter(*filter);

    _filterCollectionChangedSignal.emit();

    return true;
}

bool BasicFilterSystem::removeFilter(const std::string& filter)
{
    auto f = _availableFilters.find(filter);

    // Refuse to delete if filter is not found or is read only
    if (f == _availableFilters.end() || f->second->isReadOnly())
    {
        return false;
    }

    _eventAdapters.erase(f->second->getName());

    // Check if the filter was active
    auto found = _activeFilters.find(f->first);
    bool wasActive = found != _activeFilters.end();

    if (wasActive)
    {
        _activeFilters.erase(found);
    }

    // Now remove the object from the available filters too
    _availableFilters.erase(f);

    _filterCollectionChangedSignal.emit();

    if (wasActive)
    {
        // Clear the cache, the rules have changed
        _visibilityCache.clear();

        _filterConfigChangedSignal.emit();

        update();
    }

    return true;
}

bool BasicFilterSystem::renameFilter(const std::string& oldFilterName, const std::string& newFilterName)
{
    // Check if the new name is already used
    auto c = _availableFilters.find(newFilterName);

    if (c != _availableFilters.end())
    {
        // Can't rename, name is already in use
        return false;
    }

    auto f = _availableFilters.find(oldFilterName);

    // Refuse to rename non-existent or read-only filters
    if (f == _availableFilters.end() || f->second->isReadOnly())
    {
        // Filter not found
        return false;
    }

    // Check if the filter was active
    auto found = _activeFilters.find(f->first);

    bool wasActive = found != _activeFilters.end();

    if (wasActive)
    {
        _activeFilters.erase(found);
    }

    // Perform the actual rename procedure
    f->second->setName(newFilterName);

    // Find the adapter to update the event bindings
    auto adapter = _eventAdapters.find(oldFilterName);

    if (adapter != _eventAdapters.end())
    {
        adapter->second->onEventNameChanged();

        // Re-insert the event adapter using a new key
        auto adapterPtr = adapter->second;
        _eventAdapters.erase(adapter);
        adapter = _eventAdapters.emplace(newFilterName, adapterPtr).first;
    }

    // Insert the new filter into the table
    _availableFilters.emplace(newFilterName, f->second);

    // If this filter is in our active set, enable it
    if (wasActive)
    {
        _activeFilters.emplace(newFilterName, f->second);
    }

    // Remove the old filter from the filtertable
    _availableFilters.erase(oldFilterName);

    _filterCollectionChangedSignal.emit();

    return true;
}

// Query whether an item is visible or filtered out
bool BasicFilterSystem::isVisible(const FilterType type, const std::string& name)
{
    // Check if this item is in the visibility cache, returning
    // its cached value if found
    auto cacheIter = _visibilityCache.find(name);

    if (cacheIter != _visibilityCache.end())
    {
        return cacheIter->second;
    }

    // Otherwise, walk the list of active filters to find a value for
    // this item.
    bool visFlag = true; // default if no filters modify it

    for (const auto& active : _activeFilters)
    {
        // Delegate the check to the filter object. If a filter returns
        // false for the visibility check, then the item is filtered
        // and we don't need any more checks.
        if (!active.second->isVisible(type, name))
        {
            visFlag = false;
            break;
        }
    }

    // Cache the result and return to caller
    _visibilityCache.emplace(name, visFlag);

    return visFlag;
}

bool BasicFilterSystem::isEntityVisible(const Entity& entity) const
{
    // Check each active filter in turn
    for (const auto& [name, filter] : _activeFilters)
    {
        // Check if the filter hides the entity class or any matching spawnargs
        if (!filter->isEntityVisible(entity))
        {
            return false;
        }
    }

    // No filters hid this entity, so it's still visible
    return true;
}

bool BasicFilterSystem::isFilteredTexture(const std::string& name) const
{
    // Check all available filters (not just active ones) to see if any
    // texture-type rule would hide this texture name
    for (const auto& [filterName, filter] : _availableFilters)
    {
        if (!filter->isVisible(FilterType::TEXTURE, name))
        {
            return true;
        }
    }

    return false;
}

FilterRules BasicFilterSystem::getRuleSet(const std::string& filter)
{
    auto f = _availableFilters.find(filter);

    return f != _availableFilters.end() ? f->second->getRuleSet() : FilterRules();
}

bool BasicFilterSystem::setFilterRules(const std::string& filter, const FilterRules& ruleSet)
{
    auto f = _availableFilters.find(filter);

    if (f != _availableFilters.end() && !f->second->isReadOnly())
    {
        // Apply the ruleset
        f->second->setRules(ruleSet);

        // Clear the cache, the ruleset has changed
        _visibilityCache.clear();

        _filterConfigChangedSignal.emit();

        update();

        return true;
    }

    return false; // not found or readonly
}

void BasicFilterSystem::pushState()
{
    // Store the current active filters state
    _stateStack.push_back(_activeFilters);
}

void BasicFilterSystem::popState()
{
    if (_stateStack.empty()) return;

    // Restore the previous state
    _activeFilters = _stateStack.back();
    _stateStack.pop_back();

    // Clear cache and update
    _visibilityCache.clear();
    update();
    _filterConfigChangedSignal.emit();
}

void BasicFilterSystem::updateSubgraph(const scene::INodePtr& root)
{
    // Construct an InstanceUpdateWalker and traverse the scenegraph to update
    // all instances
    InstanceUpdateWalker walker(*this);
    root->traverse(walker);
}

// Update scenegraph instances with filtered status
void BasicFilterSystem::updateScene()
{
    auto rootNode = GlobalSceneGraph().root();

    if (!rootNode) return;

    // pass scenegraph root to specialised routine
    updateSubgraph(rootNode);

    // Invoke onFiltersChanged on the root node
    rootNode->onFiltersChanged();
}

// Update scenegraph instances with filtered status
void BasicFilterSystem::updateShaders()
{
    // Construct a ShaderVisitor to traverse the shaders
    GlobalMaterialManager().foreachMaterial([this] (const MaterialPtr& material)
    {
        // Set the shader's visibility based on the current filter settings
        material->setVisible(
            isVisible(FilterType::TEXTURE, material->getName())
        );
    });
}

// RegisterableModule implementation
std::string BasicFilterSystem::getName() const
{
    static std::string _name(MODULE_FILTERSYSTEM);
    return _name;
}

StringSet BasicFilterSystem::getDependencies() const
{
    static StringSet _dependencies;

    if (_dependencies.empty())
    {
        _dependencies.insert(MODULE_XMLREGISTRY);
        _dependencies.insert(MODULE_GAMEMANAGER);
        _dependencies.insert(MODULE_COMMANDSYSTEM);
    }

    return _dependencies;
}

// Module instance
module::StaticModuleRegistration<BasicFilterSystem> filterSystemModule;

}
