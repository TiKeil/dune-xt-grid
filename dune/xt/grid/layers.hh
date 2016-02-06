// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// The copyright lies with the authors of this file (see below).
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Andreas Buhr    (2014)
//   Felix Schindler (2014 - 2016)
//   Rene Milk       (2014 - 2016)

#ifndef DUNE_XT_GRID_LAYERS_HH
#define DUNE_XT_GRID_LAYERS_HH

#include <dune/common/typetraits.hh>

#if HAVE_DUNE_FEM
#include <dune/fem/gridpart/adaptiveleafgridpart.hh>
#include <dune/fem/gridpart/leafgridpart.hh>
#include <dune/fem/gridpart/levelgridpart.hh>
#endif

#include <dune/xt/grid/type_traits.hh>

namespace Dune {
namespace XT {
namespace Grid {

enum class Backends
{
  view,
  part
};


enum class Layers
{
  level,
  leaf,
  adaptive_leaf
};


/**
 * \brief Allows to statically create a leaf or level part or view (unspecialized variant).
 */
template <class GridType, Layers layer, Backends backend>
struct Layer
{
  static_assert(AlwaysFalse<GridType>::value, "No layer available for this combination!");

  typedef void type;

  static type create(const GridType& /*grid*/, const int /*level*/ = 0)
  {
    static_assert(AlwaysFalse<GridType>::value, "No layer available for this combination!");
  }

  static type create(GridType& /*grid*/, const int /*level*/ = 0)
  {
    static_assert(AlwaysFalse<GridType>::value, "No layer available for this combination!");
  }
}; // struct Layer


/**
 * \brief Allows to statically create a leaf or level part or view (leaf view variant).
 */
template <class GridType>
struct Layer<GridType, Layers::leaf, Backends::view>
{
  typedef typename GridType::LeafGridView type;

  static type create(const GridType& grid, const int /*level*/ = 0)
  {
    return grid.leafGridView();
  }
}; // struct Layer< ..., leaf, view >


/**
 * \brief Allows to statically create a leaf or level part or view (leaf view variant).
 */
template <class GridType>
struct Layer<GridType, Layers::level, Backends::view>
{
  typedef typename GridType::LevelGridView type;

  static type create(const GridType& grid, const int level)
  {
    assert(level >= 0);
    assert(level <= grid.maxLevel());
    return grid.levelGridView(level);
  }
}; // struct Layer< ..., level, view >


#if HAVE_DUNE_FEM


/**
 * \brief Allows to statically create a leaf or level part or view (leaf part variant, only from a non-const grid).
 */
template <class GridType>
struct Layer<GridType, Layers::leaf, Backends::part>
{
  typedef Fem::LeafGridPart<GridType> type;

  static type create(const GridType& /*grid*/, const int /*level*/ = 0)
  {
    static_assert(AlwaysFalse<GridType>::value,
                  "dune-fem does not allow the creation of a leaf grid part from a non-const grid!");
  }

  static type create(GridType& grid, const int /*level*/ = 0)
  {
    return type(grid);
  }
}; // struct Layer< ..., leaf, part >


/**
 * \brief Allows to statically create a leaf or level part or view (level part variant, only from a non-const grid).
 */
template <class GridType>
struct Layer<GridType, Layers::level, Backends::part>
{
  typedef Fem::LevelGridPart<GridType> type;

  static type create(const GridType& /*grid*/, const int /*level*/)
  {
    static_assert(AlwaysFalse<GridType>::value,
                  "dune-fem does not allow the creation of a level grid part from a non-const grid!");
  }

  static type create(GridType& grid, const int level)
  {
    assert(level >= 0);
    assert(level <= grid.maxLevel());
    return type(grid, level);
  }
}; // struct Layer< ..., level, part >


/**
 * \brief Allows to statically create a leaf or level part or view (leaf part variant, only from a non-const grid).
 */
template <class GridType>
struct Layer<GridType, Layers::adaptive_leaf, Backends::part>
{
  typedef Fem::AdaptiveLeafGridPart<GridType> type;

  static type create(const GridType& /*grid*/, const int /*level*/ = 0)
  {
    static_assert(AlwaysFalse<GridType>::value,
                  "dune-fem does not allow the creation of a leaf grid part from a non-const grid!");
  }

  static type create(GridType& grid, const int /*level*/ = 0)
  {
    return type(grid);
  }
}; // struct Layer< ..., leaf, part >


#else // HAVE_DUNE_FEM


template <class GridType>
struct Layer<GridType, Layers::leaf, Backends::part>
{
  static_assert(AlwaysFalse<GridType>::value, "You are missing dune-fem!");
};


template <class GridType>
struct Layer<GridType, Layers::level, Backends::part>
{
  static_assert(AlwaysFalse<GridType>::value, "You are missing dune-fem!");
};


template <class GridType>
struct Layer<GridType, Layers::adaptive_leaf, Backends::part>
{
  static_assert(AlwaysFalse<GridType>::value, "You are missing dune-fem!");
};


#endif // HAVE_DUNE_FEM

} // namespace Grid
} // namespace XT
} // namespace Dune

#endif // DUNE_XT_GRID_LAYERS_HH
