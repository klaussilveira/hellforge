#pragma once

/* greebo: The interface of the grid system
 *
 * Use these methods to set/get the grid size of the xyviews
 */
#include <stdexcept>
#include "imodule.h"
#include <sigc++/signal.h>

enum GridSize
{
	GRID_0125 = -3,
	GRID_025 = -2,
	GRID_05 = -1,
	GRID_1 = 0,
	GRID_2 = 1,
	GRID_4 = 2,
	GRID_8 = 3,
	GRID_16 = 4,
	GRID_32 = 5,
	GRID_64 = 6,
	GRID_128 = 7,
	GRID_256 = 8,
	GRID_M_0125 = 9,
	GRID_M_025 = 10,
	GRID_M_05 = 11,
	GRID_M_1 = 12,
	GRID_M_2 = 13,
	GRID_M_4 = 14,
	GRID_M_8 = 15,
};

namespace grid
{

constexpr float UNITS_PER_METER = 40.0f;

constexpr const char* RKEY_COORD_LABEL_MODE = "user/ui/grid/coordLabelMode";

enum class CoordLabelMode
{
	Units = 0,
	Metric = 1,
	Both = 2,
};

inline const char* getStringForSize(GridSize size)
{
	switch (size)
	{
	case GRID_0125: return "0.125";
	case GRID_025:  return "0.25";
	case GRID_05:  return "0.5";
	case GRID_1:  return "1";
	case GRID_2:  return "2";
	case GRID_4:  return "4";
	case GRID_8:  return "8";
	case GRID_16:  return "16";
	case GRID_32:  return "32";
	case GRID_64:  return "64";
	case GRID_128:  return "128";
	case GRID_256:  return "256";
	case GRID_M_0125: return "0.125m";
	case GRID_M_025:  return "0.25m";
	case GRID_M_05:   return "0.5m";
	case GRID_M_1:    return "1m";
	case GRID_M_2:    return "2m";
	case GRID_M_4:    return "4m";
	case GRID_M_8:    return "8m";
	default:
		throw new std::logic_error("Grid size not handled in switch!");
	};
}

inline float getStepForSize(GridSize size)
{
	switch (size)
	{
	case GRID_0125: return 0.125f;
	case GRID_025:  return 0.25f;
	case GRID_05:   return 0.5f;
	case GRID_1:    return 1.0f;
	case GRID_2:    return 2.0f;
	case GRID_4:    return 4.0f;
	case GRID_8:    return 8.0f;
	case GRID_16:   return 16.0f;
	case GRID_32:   return 32.0f;
	case GRID_64:   return 64.0f;
	case GRID_128:  return 128.0f;
	case GRID_256:  return 256.0f;
	case GRID_M_0125: return 5.0f;
	case GRID_M_025:  return 10.0f;
	case GRID_M_05:   return 20.0f;
	case GRID_M_1:    return 40.0f;
	case GRID_M_2:    return 80.0f;
	case GRID_M_4:    return 160.0f;
	case GRID_M_8:    return 320.0f;
	default:
		throw new std::logic_error("Grid size not handled in switch!");
	};
}

inline bool isMetric(GridSize size)
{
	return size >= GRID_M_0125;
}

// The space the grid is dividing. Regular map editing is using the
// World grid, while the Texture Tool is working in UV space.
enum class Space
{
    World,
    Texture,
};

}

// grid renderings
enum GridLook
{
	GRIDLOOK_LINES,
	GRIDLOOK_DOTLINES,
	GRIDLOOK_MOREDOTLINES,
	GRIDLOOK_CROSSES,
	GRIDLOOK_DOTS,
	GRIDLOOK_BIGDOTS,
	GRIDLOOK_SQUARES,
};

constexpr const char* const MODULE_GRID("Grid");

class IGridManager :
	public RegisterableModule
{
public:
	virtual ~IGridManager() {}

	virtual void setGridSize(GridSize gridSize) = 0;

    // Returns the currently active grid size enum value
    virtual GridSize getActiveGridSize() const = 0;

    // Returns the grid spacing in units of the given space
    virtual float getGridSize(grid::Space = grid::Space::World) const = 0;

    // Returns the grid power of the currently active grid size
	virtual int getGridPower(grid::Space = grid::Space::World) const = 0;

    // Returns the base number the exponent is applied to (e.g. 2)
	virtual int getGridBase(grid::Space = grid::Space::World) const = 0;

	virtual void gridDown() = 0;
	virtual void gridUp() = 0;

	virtual GridLook getMajorLook() const = 0;
	virtual GridLook getMinorLook() const = 0;

    /// Signal emitted when the grid is changed
	virtual sigc::signal<void> signal_gridChanged() const = 0;
};

// This is the accessor for the grid module
inline IGridManager& GlobalGrid()
{
	static module::InstanceReference<IGridManager> _reference(MODULE_GRID);
	return _reference;
}
