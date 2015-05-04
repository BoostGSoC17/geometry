// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2014-2015 Samuel Debionne, Grenoble, France.

// This file was modified by Oracle on 2015.
// Modifications copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_EXPAND_POINT_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_EXPAND_POINT_HPP

#include <cstddef>
#include <algorithm>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/coordinate_system.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/util/convert_on_spheroid.hpp>
#include <boost/geometry/util/math.hpp>
#include <boost/geometry/util/normalize_spheroidal_coordinates.hpp>
#include <boost/geometry/util/normalize_spheroidal_box_coordinates.hpp>
#include <boost/geometry/util/select_coordinate_type.hpp>

#include <boost/geometry/strategies/compare.hpp>
#include <boost/geometry/policies/compare.hpp>

#include <boost/geometry/algorithms/dispatch/expand.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace expand
{


struct point_on_spheroid
{
    template <typename Box, typename Point>
    static inline void apply(Box& box, Point const& point)
    {
        typedef typename fp_coordinate_type<Point>::type point_coordinate_type;

        typedef math::detail::constants_on_spheroid
            <
                point_coordinate_type,
                typename coordinate_system<Box>::type::units
            > constants;

        point_coordinate_type p_lon = geometry::get<0>(point);
        point_coordinate_type p_lat = geometry::get<1>(point);

        math::normalize_spheroidal_coordinates
            <
                typename coordinate_system<Point>::type::units,
                point_coordinate_type
            >(p_lon, p_lat);

        math::convert_coordinates
            <
                typename coordinate_system<Point>::type::units, // from
                typename coordinate_system<Box>::type::units // to
            >(p_lon, p_lat);

        typename coordinate_type<Box>::type
            b_lon_min = geometry::get<min_corner, 0>(box),
            b_lat_min = geometry::get<min_corner, 1>(box),
            b_lon_max = geometry::get<max_corner, 0>(box),
            b_lat_max = geometry::get<max_corner, 1>(box);

        math::normalize_spheroidal_box_coordinates
            <
                typename coordinate_system<Box>::type::units,
                typename coordinate_type<Box>::type
            >(b_lon_min, b_lat_min, b_lon_max, b_lat_max);

        if (math::equals(math::abs(p_lat), constants::max_latitude()))
        {
            // the point of expansion is the either the north or the
            // south pole; the only important coordinate here is the
            // pole's latitude, as the longitude can be anything;
            // we, thus, take into account the point's latitude only and return
            assign_values(box,
                          b_lon_min,
                          (std::min)(p_lat, b_lat_min),
                          b_lon_max,
                          (std::max)(p_lat, b_lat_max));
            return;
        }

        if (math::equals(b_lat_min, b_lat_max)
            && math::equals(math::abs(b_lat_min), constants::max_latitude()))
        {
            // the box degenerates to either the north or the south pole;
            // the only important coordinate here is the pole's latitude, 
            // as the longitude can be anything;
            // we thus take into account the box's latitude only and return
            assign_values(box,
                          p_lon,
                          (std::min)(p_lat, b_lat_min),
                          p_lon,
                          (std::max)(p_lat, b_lat_max));
            return;
        }

        // update latitudes
        b_lat_min = (std::min)(b_lat_min, p_lat);
        b_lat_max = (std::max)(b_lat_max, p_lat);

        // update longitudes
        if (math::smaller(p_lon, b_lon_min))
        {
            point_coordinate_type p_lon_shifted = p_lon + constants::period();
            if (math::larger(p_lon_shifted, b_lon_max))
            {
                if (math::smaller(b_lon_min - p_lon, p_lon_shifted - b_lon_max))
                {
                    b_lon_min = p_lon;
                }
                else
                {
                    b_lon_max = p_lon_shifted;
                }
            }
        }
        else if (math::larger(p_lon, b_lon_max))
        {
            // in this case, and since p_lon is normalized in the range
            // (-180, 180], we must have that b_lon_max <= 180
            if (b_lon_min < 0
                && math::larger(p_lon - b_lon_max,
                                constants::period() - p_lon + b_lon_min))
            {
                b_lon_min = p_lon;
                b_lon_max += constants::period();
            }
            else
            {
                b_lon_max = p_lon;
            }
        }

        assign_values(box, b_lon_min, b_lat_min, b_lon_max, b_lat_max);
    }
};


template
<
    typename StrategyLess, typename StrategyGreater,
    std::size_t Dimension, std::size_t DimensionCount
>
struct point_loop
{
    template <typename Box, typename Point>
    static inline void apply(Box& box, Point const& source)
    {
        typedef typename strategy::compare::detail::select_strategy
            <
                StrategyLess, 1, Point, Dimension
            >::type less_type;

        typedef typename strategy::compare::detail::select_strategy
            <
                StrategyGreater, -1, Point, Dimension
            >::type greater_type;

        typedef typename select_coordinate_type<Point, Box>::type coordinate_type;

        less_type less;
        greater_type greater;

        coordinate_type const coord = get<Dimension>(source);

        if (less(coord, get<min_corner, Dimension>(box)))
        {
            set<min_corner, Dimension>(box, coord);
        }

        if (greater(coord, get<max_corner, Dimension>(box)))
        {
            set<max_corner, Dimension>(box, coord);
        }

        point_loop
            <
                StrategyLess, StrategyGreater,
                Dimension + 1, DimensionCount
            >::apply(box, source);
    }
};


template
<
    typename StrategyLess, typename StrategyGreater, std::size_t DimensionCount
>
struct point_loop
    <
        StrategyLess, StrategyGreater, DimensionCount, DimensionCount
    >
{
    template <typename Box, typename Point>
    static inline void apply(Box&, Point const&) {}
};


}} // namespace detail::expand
#endif // DOXYGEN_NO_DETAIL

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

// Box + point -> new box containing also point
template
<
    typename BoxOut, typename Point,
    typename StrategyLess, typename StrategyGreater,
    typename CSTagOut, typename CSTag
>
struct expand
    <
        BoxOut, Point, StrategyLess, StrategyGreater,
        box_tag, point_tag, CSTagOut, CSTag
    > : detail::expand::point_loop
        <
            StrategyLess, StrategyGreater,
            0, dimension<Point>::type::value
        >
{};

template
<
    typename BoxOut, typename Point,
    typename StrategyLess, typename StrategyGreater
>
struct expand
    <
        BoxOut, Point, StrategyLess, StrategyGreater,
        box_tag, point_tag, spherical_equatorial_tag, spherical_equatorial_tag
    > : detail::expand::point_on_spheroid
{};

template
<
    typename BoxOut, typename Point,
    typename StrategyLess, typename StrategyGreater
>
struct expand
    <
        BoxOut, Point, StrategyLess, StrategyGreater,
        box_tag, point_tag, geographic_tag, geographic_tag
    > : detail::expand::point_on_spheroid
{};

} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_EXPAND_POINT_HPP
