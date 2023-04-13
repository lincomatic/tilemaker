#define BOOST_GEOMETRY_NO_ROBUSTNESS
#include "geom.h"

#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/index/rtree.hpp>

#include "geometry/correct.hpp"

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/irange.hpp>

typedef boost::geometry::model::segment<Point> simplify_segment;
typedef boost::geometry::index::rtree<simplify_segment, boost::geometry::index::quadratic<16>> simplify_rtree;

template<typename GeometryType>
static inline void simplify_ring(GeometryType const &input, GeometryType &output, double distance, simplify_rtree const &outer_rtree = simplify_rtree())
{
    std::deque<std::size_t> nodes(input.size());
    for(std::size_t i = 0; i < input.size(); ++i)
        nodes[i] = i;

	simplify_rtree rtree(
		boost::irange<std::size_t>(0, input.size() - 1)
		| boost::adaptors::transformed(std::function<simplify_segment(std::size_t)>([&input](std::size_t i) {
			return simplify_segment(input[i], input[i+1]);
		})));

    Box envelope; boost::geometry::envelope(input, envelope);

	for(std::size_t pq = input.size() - 2; pq--; ) {
        auto entry = pq;

        auto start = nodes[entry];
        auto middle = nodes[entry + 1];
        auto end = nodes[entry + 2];

        if (input[middle].x()==envelope.min_corner().x() ||
            input[middle].y()==envelope.min_corner().y() ||
            input[middle].x()==envelope.max_corner().x() ||
            input[middle].y()==envelope.max_corner().y()) continue;

        simplify_segment line(input[start], input[end]);

        double max_comp_distance = 0.0;
		std::size_t max_comp_i = start + 1;

        for(auto i = start + 1; i < end; ++i) {
			auto comp_distance = boost::geometry::comparable_distance(line, input[i]);
            if(comp_distance > max_comp_distance) {
				max_comp_distance = comp_distance;
				max_comp_i = i;
			}
		}

        if(boost::geometry::distance(line, input[max_comp_i]) < distance) {
			std::size_t query_count = 0;
            for(auto const &result: rtree | boost::geometry::index::adaptors::queried(boost::geometry::index::intersects(line)))
				++query_count;
            for(auto const &result: outer_rtree | boost::geometry::index::adaptors::queried(boost::geometry::index::intersects(line)))
				++query_count;

			std::size_t expected_count = std::min<std::size_t>(4, nodes.size() - 1);
            if(query_count == expected_count) {
                nodes.erase(nodes.begin() + entry + 1);
                rtree.remove(simplify_segment(input[start], input[middle]));
                rtree.remove(simplify_segment(input[middle], input[end]));
                rtree.insert(line);
            }
        }
    }

	output.resize(nodes.size());
	for(std::size_t i = 0; i < nodes.size(); ++i)
		output[i] = input[nodes[i]];
}

Polygon simplify(Polygon const &p, double max_distance) 
{
	Polygon result;

	simplify_rtree outer_rtree(
		boost::irange<std::size_t>(0, p.outer().size() - 1)
		| boost::adaptors::transformed(std::function<simplify_segment(std::size_t)>([&p](std::size_t i) {
			return simplify_segment(p.outer()[i], p.outer()[i+1]);
		})));

	for(auto const &inner: p.inners()) {
		Ring new_inner;
		simplify_ring(inner, new_inner, max_distance, outer_rtree);

		std::reverse(new_inner.begin(), new_inner.end());
		if(new_inner.size() > 3 && boost::geometry::perimeter(new_inner) > 3 * max_distance) {
			simplify_combine(result.inners(), std::move(new_inner));
		}
	}

	simplify_rtree inners_rtree;
	for(auto &inner: result.inners()) {
		std::reverse(inner.begin(), inner.end());

		inners_rtree.insert(
			boost::irange<std::size_t>(0, inner.size() - 1)
			| boost::adaptors::transformed(std::function<simplify_segment(std::size_t)>([&inner](std::size_t i) {
				return simplify_segment(inner[i], inner[i+1]);
			})));
	}

	simplify_ring(p.outer(), result.outer(), max_distance, inners_rtree);
	if(result.outer().size() > 3 && boost::geometry::perimeter(result.outer()) > 3 * max_distance) {
		return result;
	}

	result = Polygon();
	return result;
}

Linestring simplify(Linestring const &ls, double max_distance) 
{
	Linestring result;
	boost::geometry::simplify(ls, result, max_distance);
	return result;
}

MultiPolygon simplify(MultiPolygon const &mp, double max_distance) 
{
	MultiPolygon result_mp;
	for(auto const &p: mp) {
		Polygon new_p = simplify(p, max_distance);
    	if(!new_p.outer().empty()) {
			simplify_combine(result_mp, std::move(new_p));
		}
	}

	return result_mp;
}

void make_valid(MultiPolygon &mp)
{
	MultiPolygon result;
	for(auto const &p: mp) {
		geometry::correct(p, result, 1E-12);
	}
	mp = result;
}

// ---------------
// Sutherland-Hodgeman clipper
// ported from Volodymyr Agafonkin's https://github.com/mapbox/lineclip

// intersect a segment against one of the 4 lines that make up the bbox
Point intersect_edge(Point const &a, Point const &b, char edge, Box const &bbox) {
	if (edge & 8) return Point(a.x() + (b.x() - a.x()) * (bbox.max_corner().y() - a.y()) / (b.y() - a.y()), bbox.max_corner().y()); // top
	if (edge & 4) return Point(a.x() + (b.x() - a.x()) * (bbox.min_corner().y() - a.y()) / (b.y() - a.y()), bbox.min_corner().y()); // bottom
	if (edge & 2) return Point(bbox.max_corner().x(), a.y() + (b.y() - a.y()) * (bbox.max_corner().x() - a.x()) / (b.x() - a.x())); // right
	if (edge & 1) return Point(bbox.min_corner().x(), a.y() + (b.y() - a.y()) * (bbox.min_corner().x() - a.x()) / (b.x() - a.x())); // left
	throw std::runtime_error("intersect called on non-intersection");
}

// bit code reflects the point position relative to the bbox:
//         left  mid  right
//    top  1001  1000  1010
//    mid  0001  0000  0010
// bottom  0101  0100  0110

char bit_code(Point const &p, Box const &bbox) {
	char code = 0;
	if      (p.x() < bbox.min_corner().x()) code |= 1; // left
	else if (p.x() > bbox.max_corner().x()) code |= 2; // right
	if      (p.y() < bbox.min_corner().y()) code |= 4; // bottom
	else if (p.y() > bbox.max_corner().y()) code |= 8; // top
	return code;
}

// Sutherland-Hodgeman polygon clipping algorithm
void fast_clip(Ring &points, Box const &bbox) {
	// clip against each side of the clip rectangle
	for (char edge = 1; edge <= 8; edge *= 2) {
		Ring result;
		Point prev = points[points.size() - 1];
		bool prevInside = (bit_code(prev, bbox) & edge)==0;

		for (unsigned int i = 0; i<points.size(); i++) {
			Point p = points[i];
			bool inside = (bit_code(p, bbox) & edge)==0;

			// if segment goes through the clip window, add an intersection
			if (inside!=prevInside) result.emplace_back(intersect_edge(prev, p, edge, bbox));
			if (inside) result.emplace_back(p); // add a point if it's inside
			prev = p;
			prevInside = inside;
		}
		points = std::move(result);
		if (points.size()==0) break;
	}
}

// Wrappers for polygon/multipolygon
void fast_clip(Polygon &polygon, Box const &bbox) {
	fast_clip(polygon.outer(), bbox);
	if (polygon.outer().empty()) {
		polygon.inners().resize(0);
		return;
	}
	for (auto &inner: polygon.inners()) {
		fast_clip(inner, bbox);
	}
	polygon.inners().erase(std::remove_if(
		polygon.inners().begin(), polygon.inners().end(), 
		[](const Ring &inner) -> bool { return inner.empty(); }),
		polygon.inners().end());
}

void fast_clip(MultiPolygon &mp, Box const &bbox) {
	for (auto &polygon: mp) {
		fast_clip(polygon, bbox);
	}
	mp.erase(std::remove_if(
		mp.begin(), mp.end(), 
		[](const Polygon &poly) -> bool { return poly.outer().empty(); }),
		mp.end());
}
