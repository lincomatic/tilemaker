#include "osm_mem_tiles.h"
using namespace std;

OsmMemTiles::OsmMemTiles(unsigned int baseZoom)
	: TileDataSource(baseZoom) 
{ }

void OsmMemTiles::Clear() {
	tileIndex.clear();
}
