#pragma once

#include "imodule.h"
#include "ifilter.h"
#include "icommandsystem.h"

#include <map>
#include <string>
#include <vector>

#include "xmlutil/Node.h"
#include "scene/filters/SceneFilter.h"
#include "XmlFilterEventAdapter.h"

namespace filters
{

/// FilterSystem implementation class.
class BasicFilterSystem: public IFilterSystem
{
    // Hashtable of available filters, indexed by name
    typedef std::map<std::string, SceneFilter::Ptr> FilterTable;
    FilterTable _availableFilters;

    // Second table containing just the active filters
    FilterTable _activeFilters;

    // Cache of visibility flags for item names, to avoid having to
    // traverse the active filter list for each lookup
    typedef std::map<std::string, bool> StringFlagCache;
    StringFlagCache _visibilityCache;

    sigc::signal<void> _filterConfigChangedSignal;
    sigc::signal<void> _filterCollectionChangedSignal;

    typedef std::map<std::string, XmlFilterEventAdapter::Ptr> FilterAdapters;
    FilterAdapters _eventAdapters;

    // Stack for saved filter states
    std::vector<FilterTable> _stateStack;

private:

    // Perform a traversal of the scenegraph, setting or clearing the filtered
    // flag on Nodes depending on their entity class
    void updateScene();

    void updateShaders();
    void addFiltersFromXML(const xml::NodeList& nodes, bool readOnly);
    XmlFilterEventAdapter::Ptr ensureEventAdapter(SceneFilter& filter);
    void selectObjectsByFilterCmd(const cmd::ArgumentList& args);
    void deselectObjectsByFilterCmd(const cmd::ArgumentList& args);
    void setObjectSelectionByFilter(const std::string& filterName, bool select);

    // Activates or deactivates all known filters.
    void setAllFilterStates(bool state);

public:
    // FilterSystem implementation
    sigc::signal<void> filterConfigChangedSignal() const override;
    sigc::signal<void> filterCollectionChangedSignal() const override;
    void update() override;
    void updateSubgraph(const scene::INodePtr& root) override;
    void forEachFilter(const std::function<void(const SceneFilter&)>& func) override;
    std::string getFilterEventName(const std::string& filter) override;
    bool getFilterState(const std::string& filter) const override;
    void setFilterState(const std::string& filter, bool state) override;
    bool isVisible(const FilterType type, const std::string& name) override;
    bool isEntityVisible(const Entity& entity) const override;
    bool isFilteredTexture(const std::string& name) const override;
    bool addFilter(const std::string& filterName, const FilterRules& ruleSet) override;
    bool removeFilter(const std::string& filter) override;
    bool renameFilter(const std::string& oldFilterName, const std::string& newFilterName) override;
    FilterRules getRuleSet(const std::string& filter) override;
    bool setFilterRules(const std::string& filter, const FilterRules& ruleSet) override;
    void pushState() override;
    void popState() override;

    // RegisterableModule implementation
    std::string getName() const override;
    StringSet getDependencies() const override;
    void initialiseModule(const IApplicationContext& ctx) override;
    void shutdownModule() override;
};

} // namespace filters
