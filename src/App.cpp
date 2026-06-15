#include "App.hpp"
#include "DebugManager.hpp"
#include "MapManager.hpp"

// Shared map state used by gameplay systems that need collision queries.
std::unique_ptr<MapManager> g_MapManager;
