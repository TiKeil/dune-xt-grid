// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// Copyright 2009-2018 dune-xt-grid developers and contributors. All rights
// reserved.
// License: Dual licensed as BSD 2-Clause License
// (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2016 - 2017)
//   Rene Milk       (2016, 2018)

/**
  * This file is intended as a starting point for quick testing.
  */

#include <dune/xt/common/test/main.hxx>
#include <dune/xt/grid/dd/glued.hh>

#include <map>
#include <set>
#include <sstream>
#include <vector>

#include <dune/grid/common/rangegenerators.hh>

#include <dune/xt/common/memory.hh>
#include <dune/xt/common/test/gtest/gtest.h>
#include <dune/xt/grid/gridprovider.hh>
#include <dune/xt/grid/grids.hh>

#include <dune/xt/grid/layers.hh>

#include <dune/grid/common/gridfactory.hh>

using namespace Dune;
using namespace Dune::XT::Grid;

struct GluedDdGridTest : public ::testing::Test
{
  using MacroGridType = YaspGrid<2, EquidistantOffsetCoordinates<double, 2>>;
  using LocalGridType = MacroGridType; // UGGrid<2>;

  template <class G, bool anything = true>
  struct get_local_layer
  {
    static const constexpr Layers type = Layers::level;
  };

  static const constexpr Layers local_layer = get_local_layer<LocalGridType>::type;

  void setup()
  {
    if (!macro_grid_)
      macro_grid_ = std::make_unique<GridProvider<MacroGridType>>(make_cube_grid<MacroGridType>(0., 1., 4, 0));
    ASSERT_NE(macro_grid_, nullptr) << "This should not happen!";
    if (!dd_grid_)
      dd_grid_ = std::make_unique<DD::Glued<MacroGridType, LocalGridType, local_layer>>(
          *macro_grid_,
          2,
          /*prepare_glues=*/false,
          /*allow_for_broken_orientation_of_coupling_intersections=*/true);
    ASSERT_NE(dd_grid_, nullptr) << "This should not happen!";
    for (auto&& macro_entity : Dune::elements(dd_grid_->macro_grid_view())) {
      EXPECT_EQ(dd_grid_->max_local_level(macro_entity), (local_layer == Layers::level) ? 2 : -1);
    }
  } // ... setup()

  std::unique_ptr<GridProvider<MacroGridType>> macro_grid_;
  std::unique_ptr<DD::Glued<MacroGridType, LocalGridType, local_layer>> dd_grid_;
};

TEST_F(GluedDdGridTest, setup_works)
{
  this->setup();
  dd_grid_->visualize("testgrid");
}

TEST_F(GluedDdGridTest, micro_view)
{
  this->setup();
  auto macro_leaf_view = dd_grid_->macro_grid_view();
  for (auto&& macro_entity : elements(macro_leaf_view)) {
    // get local grid
    auto local_grid = dd_grid_->local_grid(macro_entity);
    const auto local_leaf_view = local_grid.leaf_view();
    const auto& local_index_set = local_leaf_view.indexSet();
    for (auto&& micro_entity : elements(local_leaf_view)) {
      const size_t micro_index = local_index_set.index(micro_entity);
      // std::cout << micro_index << std::endl;
    } // walk the micro grid
  } // walk the macro grid
}

TEST_F(GluedDdGridTest, Oversampling)
{
  this->setup();
  dd_grid_->setup_oversampling_grids(0, 3);
  //  dd_grid_->setup_oversampling_grid(4,0,3);
  auto LocalGrid = dd_grid_->local_oversampling_grid(15);
  LocalGrid.visualize("LocalGrid");
}
