/*! \file */ 
#ifndef _COORDINATES_H
#define _COORDINATES_H

#include <iostream>
#include "geom.h"
#include <utility>
#include <unordered_set>

#ifdef FAT_TILE_INDEX
typedef uint32_t TileCoordinate;
#else
typedef uint16_t TileCoordinate;
#endif
class TileCoordinates_ {

public:
	TileCoordinate x, y;

	TileCoordinates_();
	TileCoordinates_(TileCoordinate x, TileCoordinate y);

	bool operator ==(const TileCoordinates_ & obj) const
	{
		if (x != obj.x)
			return false;
		return y == obj.y;
	}
};
struct TileCoordinatesCompare {
    bool operator()(const class TileCoordinates_& a, const class TileCoordinates_& b) const {
		if(a.x > b.x)
			return false;
		if(a.x < b.x)
			return true;
        return a.y < b.y;
    }
};
typedef class TileCoordinates_ TileCoordinates;
namespace std {
	template<> struct hash<TileCoordinates> {
		size_t operator()(const TileCoordinates & obj) const {
			return hash<TileCoordinate>()(obj.x) ^ hash<TileCoordinate>()(obj.y);
		}
	};
}

struct LatpLon {
	int32_t latp;
	int32_t lon;
};
inline bool operator==(const LatpLon &a, const LatpLon &b) { return a.latp==b.latp && a.lon==b.lon; }
namespace std {
	/// Hashing function so we can use an unordered_set
	template<>
	struct hash<LatpLon> {
		size_t operator()(const LatpLon &ll) const {
			return std::hash<int32_t>()(ll.latp) ^ std::hash<int32_t>()(ll.lon);
		}
	};
}

typedef std::vector<LatpLon> LatpLonVec;
typedef std::deque<LatpLon> LatpLonDeque;

double deg2rad(double deg);
double rad2deg(double rad);

// max/min latitudes
constexpr double MaxLat = 85.0511;
constexpr double MinLat = -MaxLat;

// Project latitude (spherical Mercator)
// (if calling with raw coords, remember to divide/multiply by 10000000.0)
double lat2latp(double lat);
double latp2lat(double latp);

// Tile conversions
double lon2tilexf(double lon, unsigned int z);
double latp2tileyf(double latp, unsigned int z);
double lat2tileyf(double lat, unsigned int z);
unsigned int lon2tilex(double lon, unsigned int z);
unsigned int latp2tiley(double latp, unsigned int z);
unsigned int lat2tiley(double lat, unsigned int z);
double tilex2lon(unsigned int x, unsigned int z);
double tiley2latp(unsigned int y, unsigned int z);
double tiley2lat(unsigned int y, unsigned int z);

// Get a tile index
TileCoordinates latpLon2index(LatpLon ll, unsigned int baseZoom);

// Earth's (mean) radius
// http://nssdc.gsfc.nasa.gov/planetary/factsheet/earthfact.html
// http://mathworks.com/help/map/ref/earthradius.html
constexpr double RadiusMeter = 6371000;

// Convert to actual length
double degp2meter(double degp, double latp);

double meter2degp(double meter, double latp);

void insertIntermediateTiles(Linestring const &points, unsigned int baseZoom, std::unordered_set<TileCoordinates> &tileSet);
void insertIntermediateTiles(Ring const &points, unsigned int baseZoom, std::unordered_set<TileCoordinates> &tileSet);

// the range between smallest y and largest y is filled, for each x
void fillCoveredTiles(std::unordered_set<TileCoordinates> &tileSet);

// ------------------------------------------------------
// Helper class for dealing with spherical Mercator tiles

class TileBbox { 

public:
	double minLon, maxLon, minLat, maxLat, minLatp, maxLatp;
	double xmargin, ymargin, xscale, yscale;
	TileCoordinates index;
	unsigned int zoom;
	bool hires;
	Box clippingBox;

	TileBbox(TileCoordinates i, unsigned int z, bool h);

	std::pair<int,int> scaleLatpLon(double latp, double lon) const;
	std::pair<double, double> floorLatpLon(double latp, double lon) const;

	Box getTileBox() const;
	Box getExtendBox() const;
};

// Round coordinates to integer coordinates of bbox
// TODO: This should be self-intersection aware!!
MultiPolygon round_coordinates(TileBbox const &bbox, MultiPolygon const &mp);

#endif //_COORDINATES_H

