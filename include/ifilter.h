#pragma once

#include "imodule.h"
#include "inode.h"
#include "scene/filters/SceneFilter.h"

#include <cassert>

#include <sigc++/signal.h>

const char* const MODULE_FILTERSYSTEM = "FilterSystem";

// Forward declaration
class Entity;

namespace filters
{

const char* const SELECT_OBJECTS_BY_FILTER_CMD = "SelectObjectsByFilter";
const char* const DESELECT_OBJECTS_BY_FILTER_CMD = "DeselectObjectsByFilter";

/**
 * @brief Interface for the FilterSystem
 *
 * The filter system provides a mechanism by which certain objects or materials
 * can be hidden from rendered views.
 *
 * The filter system operates an internal stack of system states, allowing the
 * current set of enabled filters to be saved and restored.
 */
class IFilterSystem: public RegisterableModule
{
public:

    /**
     * @brief Signal emitted when the state of filters has changed, filters have
     * been added or removed, or when rules have been altered.
     */
    virtual sigc::signal<void> filterConfigChangedSignal() const = 0;

    /**
     * @brief Signal emitted when filters are added, removed, or renamed.
     */
    virtual sigc::signal<void> filterCollectionChangedSignal() const = 0;

    /**
     * @brief Updates the "Filtered" status of all instances in the scenegraph
     * based on the current filter settings.
     */
    virtual void update() = 0;

    /**
     * @brief Updates the specified subgraph, including the given node and all
     * its children, based on the current filter settings.
     *
     * @param root The root node of the subgraph to update.
     */
    virtual void updateSubgraph(const scene::INodePtr& root) = 0;

    /**
     * @brief Visits all available filters and passes each filter's text name to
     * the provided visitor function.
     *
     * @param func A function object called with the filter name as an argument.
     */
    virtual void forEachFilter(const std::function<void(const SceneFilter&)>& func) = 0;

    /**
     * @brief Sets the state of the specified filter.
     *
     * @param filter The name of the filter to toggle.
     * @param state True to activate the filter, false to deactivate it.
     */
    virtual void setFilterState(const std::string& filter, bool state) = 0;

    /**
     * @brief Retrieves the state of the specified filter.
     *
     * @param filter The name of the filter to query.
     * @return True if the filter is active, false otherwise.
     */
    virtual bool getFilterState(const std::string& filter) const = 0;

    /**
     * @brief Duplicate the current filtersystem state and push it onto the
     * stack.
     *
     * The filtersystem state consists of the currently active filters.
     */
    virtual void pushState() = 0;

    /// Pops the last filtersystem state from the stack and restores it.
    virtual void popState() = 0;

    /**
     * @brief Retrieves the event name associated with the specified filter.
     *
     * @param filter The name of the filter.
     * @return The event name of the filter.
     */
    virtual std::string getFilterEventName(const std::string& filter) = 0;

    /**
     * @brief Tests whether a given item should be visible based on the
     * currently active filters.
     *
     * @param type The filter type to query.
     * @param name The string name of the item to query.
     * @return True if the item is visible, false otherwise.
     */
    virtual bool isVisible(const FilterType type, const std::string& name) = 0;

    /**
     * @brief Tests whether a given entity should be visible based on the
     * currently active filters.
     *
     * @param entity The entity to test.
     * @return True if the entity is visible, false otherwise.
     */
    virtual bool isEntityVisible(const Entity& entity) const = 0;

    /**
     * @brief Tests whether a given texture name would be hidden by any
     * available filter's texture rules, regardless of whether that filter
     * is currently active.
     *
     * This is used to identify "utility" textures (clip, caulk, trigger, etc.)
     * for purposes such as rendering them with reduced opacity.
     *
     * @param name The texture name to test.
     * @return True if any filter has a texture rule that would hide this name.
     */
    virtual bool isFilteredTexture(const std::string& name) const = 0;

    // ===== API for Filter Management and Editing =====

    /**
     * @brief Adds a new filter to the system with the specified ruleset.
     * The new filter is not set to read-only.
     *
     * @param filterName The name of the new filter.
     * @param ruleSet The ruleset to associate with the new filter.
     * @return True on success, false if the filter name already exists.
     */
    virtual bool addFilter(const std::string& filterName, const FilterRules& ruleSet) = 0;

    /**
     * @brief Removes the specified filter.
     *
     * @param filter The name of the filter to remove.
     * @return True on success, false otherwise.
     */
    virtual bool removeFilter(const std::string& filter) = 0;

    /**
     * @brief Renames the specified filter. This also updates the corresponding command
     * in the EventManager class.
     *
     * @param oldFilterName The current name of the filter.
     * @param newFilterName The new name for the filter.
     * @return True on success, false if the filter is not found or is read-only.
     */
    virtual bool renameFilter(const std::string& oldFilterName, const std::string& newFilterName) = 0;

    /**
     * @brief Retrieves the ruleset associated with the specified filter. The
     * order of rules is important.
     *
     * @param filter The name of the filter.
     * @return The ruleset of the filter.
     */
    virtual FilterRules getRuleSet(const std::string& filter) = 0;

    /**
     * @brief Replaces the existing ruleset of the specified filter with the
     * given criteria set. This applies only to non-read-only filters.
     *
     * @param filter The name of the filter.
     * @param ruleSet The new ruleset to apply.
     * @return True on success, false if the filter is not found or is read-only.
     */
    virtual bool setFilterRules(const std::string& filter, const FilterRules& ruleSet) = 0;
};

/// RAII class to push and pop filter state
class ScopedFilterState
{
    IFilterSystem& _filterSystem;

public:
    ScopedFilterState(IFilterSystem& filterSystem): _filterSystem(filterSystem)
    {
        _filterSystem.pushState();
    }

    ~ScopedFilterState()
    {
        _filterSystem.popState();
    }
};

}

inline filters::IFilterSystem& GlobalFilterSystem()
{
    static module::InstanceReference<filters::IFilterSystem> _reference(MODULE_FILTERSYSTEM);
    return _reference;
}
