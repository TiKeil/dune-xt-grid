// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// The copyright lies with the authors of this file (see below).
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2012 - 2016)
//   Rene Milk       (2012 - 2016)
//   Tobias Leibner  (2014 - 2016)

#ifndef DUNE_XT_TEST_GRID_PROVIDER_HH
#define DUNE_XT_TEST_GRID_PROVIDER_HH

#include <dune/xt/common/type_traits.hh>

#include <dune/xt/grid/boundaryinfo.hh>
#include <dune/xt/grid/gridprovider.hh>
#include <dune/xt/grid/grids.hh>

#include <dune/xt/common/test/gtest/gtest.h>


template <class GridType>
struct GridProviderBase : public testing::Test
{
  typedef Dune::XT::Grid::GridProvider<GridType> GridProviderType;

  static void check_layers(const GridProviderType& grid_provider)
  {
    using namespace Dune::XT::Grid;
    const std::shared_ptr<GridType>& DUNE_UNUSED(grid_ptr) = grid_provider.grid_ptr();
    auto DUNE_UNUSED(leaf_grid_view_1) = grid_provider.leaf_view();
    auto DUNE_UNUSED(leaf_grid_view_2) = grid_provider.template layer<Layers::leaf, Backends::view>();
#if HAVE_DUNE_FEM
    auto DUNE_UNUSED(leaf_grid_part_2) = grid_provider.template layer<Layers::leaf, Backends::part>();
#endif
    for (int level = 0; level <= grid_provider.max_level(); ++level) {
      auto DUNE_UNUSED(level_grid_view_1) = grid_provider.level_view(level);
      auto DUNE_UNUSED(level_grid_view_2) = grid_provider.template layer<Layers::leaf, Backends::view>(level);
#if HAVE_DUNE_FEM
      auto DUNE_UNUSED(level_grid_part_2) = grid_provider.template layer<Layers::leaf, Backends::part>(level);
#endif
    }
  } // ... check_layers()

  template <class G, bool enabled = true>
  struct Helper
  {
    static void check_visualize(const G& grid_provider)
    {
      auto type_str = Dune::XT::Common::Typename<G>::value();
      grid_provider.visualize();
      grid_provider.visualize(type_str + "_a");
      grid_provider.visualize(Dune::XT::Grid::alldirichlet_boundaryinfo_default_config());
      grid_provider.visualize(Dune::XT::Grid::alldirichlet_boundaryinfo_default_config(), type_str + "_b");
    }
  };

  template <class G>
  struct Helper<G, false>
  {
    static void check_visualize(const G& /*grid_provider*/)
    {
    }
  };

  static void check_visualize(const GridProviderType& grid_provider)
  {
    Helper < GridProviderType, GridProviderType::dimDomain<4>::check_visualize(grid_provider);
  }
}; // class ConstGridProviderBase


#endif // DUNE_XT_TEST_GRID_PROVIDER_HH
