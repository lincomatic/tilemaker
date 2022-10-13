#include "shp_mem_tiles.h"
#include <iostream>
using namespace std;
namespace geom = boost::geometry;

ShpMemTiles::ShpMemTiles(OSMStore &osmStore, unsigned int baseZoom)
	: TileDataSource(baseZoom), osmStore(osmStore)
{ }

// Look for shapefile objects that fulfil a spatial query (e.g. intersects)
// Parameters:
// - shapefile layer name to search
// - bounding box to match against
// - indexQuery(rtree, results) lambda, implements: rtree.query(geom::index::covered_by(box), back_inserter(results))
// - checkQuery(osmstore, id) lambda, implements:   return geom::covered_by(osmStore.retrieve(id), geom)
vector<unsigned int> ShpMemTiles::QueryMatchingGeometries(const string &layerName, bool once, Box &box, 
	function<vector<IndexValue>(const RTree &rtree)> indexQuery, 
	function<bool(OutputObject const &oo)> checkQuery) const {
	
	// Find the layer
	auto f = indices.find(layerName); // f is an RTree
	if (f==indices.end()) {
		cerr << "Couldn't find indexed layer " << layerName << endl;
		return vector<unsigned int>();	// empty, relations not supported
	}
	
	// Run the index query
	vector<IndexValue> results = indexQuery(f->second);
	
	// Run the check query
	vector<unsigned int> ids;
	for (auto it: results) {
		unsigned int id = it.second;
		if (checkQuery(*cachedGeometries.at(id))) { ids.push_back(id); if (once) break; }
	}
	return ids;
}

vector<string> ShpMemTiles::namesOfGeometries(const vector<unsigned int> &ids) const {
	vector<string> names;
	for (unsigned int i=0; i<ids.size(); i++) {
		if (cachedGeometryNames.find(ids[i])!=cachedGeometryNames.end()) {
			names.push_back(cachedGeometryNames.at(ids[i]));
		}
	}
	return names;
}

void ShpMemTiles::CreateNamedLayerIndex(const std::string &layerName) {
	indices[layerName]=RTree();
}

OutputObjectRef ShpMemTiles::AddObject(uint_least8_t layerNum,
	const std::string &layerName, enum OutputGeometryType geomType,
	Geometry geometry, bool isIndexed, bool hasName, const std::string &name, AttributeStoreRef attributes, unsigned int minzoom) {

	geom::model::box<Point> box;
	geom::envelope(geometry, box);

	unsigned int id = cachedGeometries.size();
	if (isIndexed) {
		indices.at(layerName).insert(std::make_pair(box, id));
		if (hasName) cachedGeometryNames[id]=name;
	}

	OutputObjectRef oo;

	unsigned int tilex = 0, tiley = 0;
	switch(geomType) {
		case POINT_:
		{
			Point *p = boost::get<Point>(&geometry);
			if (p != nullptr) {
	
				Point sp(p->x()*10000000.0, p->y()*10000000.0);
				osmStore.store_point(osmStore.shp(), id, sp);
				oo = CreateObject(OutputObjectOsmStorePoint(
					geomType, layerNum, id, attributes, minzoom));
				cachedGeometries.push_back(oo);

				tilex =  lon2tilex(p->x(), baseZoom);
				tiley = latp2tiley(p->y(), baseZoom);
				AddObject(TileCoordinates(tilex, tiley), oo);
			}
		} break;

		case LINESTRING_:
		{
			osmStore.store_linestring(osmStore.shp(), id, boost::get<Linestring>(geometry));
			oo = CreateObject(OutputObjectOsmStoreLinestring(
						geomType, layerNum, id, attributes, minzoom));
			cachedGeometries.push_back(oo);

			addToTileIndexPolyline(oo, &geometry);
		} break;

		case POLYGON_:
		{
			osmStore.store_multi_polygon(osmStore.shp(), id, boost::get<MultiPolygon>(geometry));
			oo = CreateObject(OutputObjectOsmStoreMultiPolygon(
						geomType, layerNum, id, attributes, minzoom));
			cachedGeometries.push_back(oo);
			
			// add to tile index
			addToTileIndexByBbox(oo, 
				box.min_corner().get<0>(), box.min_corner().get<1>(), 
				box.max_corner().get<0>(), box.max_corner().get<1>());
		} break;

		default:
			break;
	}

	return oo;
}

// Add an OutputObject to all tiles between min/max lat/lon
void ShpMemTiles::addToTileIndexByBbox(OutputObjectRef &oo, double minLon, double minLatp, double maxLon, double maxLatp) {
	unsigned int minTileX =  lon2tilex(minLon, baseZoom);
	unsigned int maxTileX =  lon2tilex(maxLon, baseZoom);
	unsigned int minTileY = latp2tiley(minLatp, baseZoom);
	unsigned int maxTileY = latp2tiley(maxLatp, baseZoom);
	for (unsigned int x=min(minTileX,maxTileX); x<=max(minTileX,maxTileX); x++) {
		for (unsigned int y=min(minTileY,maxTileY); y<=max(minTileY,maxTileY); y++) {
			TileCoordinates index(x, y);
			AddObject(index, oo);
		}
	}
}

// Add an OutputObject to all tiles along a polyline
void ShpMemTiles::addToTileIndexPolyline(OutputObjectRef &oo, Geometry *geom) {

	const Linestring *ls = boost::get<Linestring>(geom);
	if(ls == nullptr) return;
	unsigned int lastx = UINT_MAX;
	unsigned int lasty;
	for (Linestring::const_iterator jt = ls->begin(); jt != ls->end(); ++jt) {
		unsigned int tilex =  lon2tilex(jt->get<0>(), baseZoom);
		unsigned int tiley = latp2tiley(jt->get<1>(), baseZoom);
		if (lastx==UINT_MAX) {
			AddObject(TileCoordinates(tilex, tiley), oo);
		} else if (lastx!=tilex || lasty!=tiley) {
			for (unsigned int x=min(tilex,lastx); x<=max(tilex,lastx); x++) {
				for (unsigned int y=min(tiley,lasty); y<=max(tiley,lasty); y++) {
					AddObject(TileCoordinates(x, y), oo);
				}
			}
		}
		lastx=tilex; lasty=tiley;
	}
}

