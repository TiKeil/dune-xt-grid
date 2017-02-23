// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// Copyright 2009-2017 dune-xt-grid developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2017)
//   Rene Milk       (2016)

#include <dune/xt/common/test/main.hxx>

#include <dune/xt/grid/gridprovider.hh>
#include <dune/xt/grid/grids.hh>
#include <dune/xt/grid/search.hh>
#include <dune/xt/grid/information.hh>
#include <dune/xt/common/logging.hh>

struct InLevelSearch : public testing::Test
{
  typedef TESTGRIDTYPE GridType;
  typedef Dune::XT::Grid::GridProvider<GridType> GridProviderType;

  InLevelSearch()
    : grid_provider_(Dune::XT::Grid::make_cube_grid<TESTGRIDTYPE>())
  {
  }

  void check()
  {
    const auto view = grid_provider_.leaf_view();
    const auto dimensions = Dune::XT::Grid::dimensions(grid_provider_.leaf_view());
    Dune::XT::Grid::EntityInlevelSearch<decltype(view)> search(view);
    const auto center = dimensions.view_center();
    const auto result = search(std::vector<std::remove_const<decltype(center)>::type>{center});
    EXPECT_GE(result.size(), 1);
  }

  GridProviderType grid_provider_;
}; // class ConstGridProviderBase


TEST_F(InLevelSearch, check)
{
  this->check();
}
