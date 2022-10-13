/*! \file */ 
#ifndef _TILE_DATA_H
#define _TILE_DATA_H

#include <map>
#include <set>
#include <vector>
#include <memory>
#include "output_object.h"

typedef std::vector<OutputObjectRef>::const_iterator OutputObjectsConstIt;
typedef std::pair<OutputObjectsConstIt, OutputObjectsConstIt> OutputObjectsConstItPair;
typedef std::map<TileCoordinates, std::vector<OutputObjectRef>, TileCoordinatesCompare> TileIndex;
typedef std::set<TileCoordinates, TileCoordinatesCompare> TileCoordinatesSet;

class TileDataSource {

protected:	
	std::mutex mutex;
	TileIndex tileIndex;
	std::deque<OutputObject> objects;

	unsigned int baseZoom;

public:
	TileDataSource(unsigned int baseZoom) 
		: baseZoom(baseZoom) 
	{ }

	///This must be thread safe!
	void MergeTileCoordsAtZoom(unsigned int zoom, TileCoordinatesSet &dstCoords) {
		MergeTileCoordsAtZoom(zoom, baseZoom, tileIndex, dstCoords);
	}

	///This must be thread safe!
	void MergeSingleTileDataAtZoom(TileCoordinates dstIndex, unsigned int zoom, std::vector<OutputObjectRef> &dstTile) {
		MergeSingleTileDataAtZoom(dstIndex, zoom, baseZoom, tileIndex, dstTile);
	}

	OutputObjectRef CreateObject(OutputObject const &oo) {
		std::lock_guard<std::mutex> lock(mutex);
		objects.push_back(oo);
		return &objects.back();
	}

	void AddObject(TileCoordinates const &index, OutputObjectRef const &oo) {
		std::lock_guard<std::mutex> lock(mutex);
		tileIndex[index].push_back(oo);
	}

private:	
	static void MergeTileCoordsAtZoom(unsigned int zoom, unsigned int baseZoom, const TileIndex &srcTiles, TileCoordinatesSet &dstCoords);
	static void MergeSingleTileDataAtZoom(TileCoordinates dstIndex, unsigned int zoom, unsigned int baseZoom, const TileIndex &srcTiles, std::vector<OutputObjectRef> &dstTile);
};

TileCoordinatesSet GetTileCoordinates(std::vector<class TileDataSource *> const &sources, unsigned int zoom);

std::vector<OutputObjectRef> GetTileData(std::vector<class TileDataSource *> const &sources, TileCoordinates coordinates, unsigned int zoom);

OutputObjectsConstItPair GetObjectsAtSubLayer(std::vector<OutputObjectRef> const &data, uint_least8_t layerNum);

#endif //_TILE_DATA_H
