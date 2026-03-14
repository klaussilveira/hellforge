#pragma once

#include "iclipper.h"
#include "icommandsystem.h"
#include "math/Plane3.h"

// Contains the routines for brush subtract, merge and hollow

class BrushNode;
typedef std::shared_ptr<BrushNode> BrushNodePtr;

class Plane3;
class Brush;

namespace brush {
namespace algorithm {

/**
 * greebo: Hollows the given brush. Note that this selects the
 *         resulting brushes and removes the source brush from the scene.
 *
 * @makeRoom: Set this to TRUE to move the wall brushes outside a bit
 *            (see makeRoomForSelectedBrushes() routine).
 */
void hollowBrush(const BrushNodePtr& sourceBrush, bool makeRoom);

/**
 * greebo: Hollows all currently selected brushes.
 */
void hollowSelectedBrushes(const cmd::ArgumentList& args);

/** greebo: This tries to move the newly created brushes towards the outside
 * 			so that the corners don't overlap (works only for rectangular prisms).
 */
void makeRoomForSelectedBrushes(const cmd::ArgumentList& args);

/**
 * greebo: Subtracts the brushes from all surrounding unselected brushes.
 */
void subtractBrushesFromUnselected(const cmd::ArgumentList& args);

/**
 * greebo: Attempts to merge the selected brushes.
 */
void mergeSelectedBrushes(const cmd::ArgumentList& args);

/**
 * Intersects selected brushes, keeping only the overlapping volume.
 */
void intersectSelectedBrushes(const cmd::ArgumentList& args);

/**
 * Creates a sealed room (6 wall brushes) around the current selection.
 * Computes the bounding box of all selected nodes, expands it by one
 * grid unit, creates a cuboid brush, and applies the Make Room algorithm.
 */
void sealSelectedEntities(const cmd::ArgumentList& args);

/**
 * Creates a new brush connecting two selected faces from different brushes.
 * Both faces must have the same number of vertices.
 */
void bridgeSelectedFaces(const cmd::ArgumentList& args);

/**
 * Connect the various events to the functions in this namespace
 */
void registerCommands();

} // namespace algorihtm
} // namespace brush

