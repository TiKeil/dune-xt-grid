// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// Copyright 2009-2018 dune-xt-grid developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2016 - 2017)
//   Rene Milk       (2018)

#include "config.h"

#include <string>
#include <vector>

#include <dune/common/parallel/mpihelper.hh>

#include <dune/pybindxi/pybind11.h>
#include <dune/pybindxi/stl.h>

#include <python/dune/xt/common/exceptions.bindings.hh>

#include <dune/xt/grid/dd/subdomains/grid.hh>
#include <dune/xt/grid/grids.hh>
#include <dune/xt/grid/layers.hh>
#include <dune/xt/grid/intersection.hh>
#include <dune/xt/grid/type_traits.hh>

#include <python/dune/xt/common/bindings.hh>
#include <python/dune/xt/grid/boundaryinfo.bindings.hh>
#include <python/dune/xt/grid/gridprovider.pbh>
#include <python/dune/xt/grid/walker.bindings.hh>
#include <python/dune/xt/grid/walker/apply-on.bindings.hh>


template <class G, Dune::XT::Grid::Layers layer, Dune::XT::Grid::Backends backend>
void bind_walker(pybind11::module& m)
{
  try {
    Dune::XT::Grid::bindings::Walker<G, layer, backend>::bind(m);
  } catch (std::runtime_error&) {
  }
} // ... bind_walker(...)


template <class G>
void addbind_for_Grid(pybind11::module& m)
{
  using namespace Dune::XT::Grid;

  const auto grid_id = Dune::XT::Grid::bindings::grid_name<G>::value();
  typedef typename Layer<G, Layers::dd_subdomain, Backends::view, DD::SubdomainGrid<G>>::type DdSubdomainPart;

  bind_GridProvider<G>(m, grid_id);
  bind_make_cube_grid<G>(m, grid_id);

  bind_DdSubdomainsGridProvider<G>(m, grid_id);
  bind_make_cube_dd_subdomains_grid<G>(m, grid_id);

  bind_walker<G, Layers::dd_subdomain, Backends::view>(m);
  bind_walker<G, Layers::dd_subdomain_boundary, Backends::view>(m);
  bind_walker<G, Layers::dd_subdomain_coupling, Backends::view>(m);
  bind_walker<G, Layers::dd_subdomain_oversampled, Backends::view>(m);
  bind_walker<G, Layers::leaf, Backends::view>(m);
  bind_walker<G, Layers::level, Backends::view>(m);
} // ... addbind_for_Grid(...)


PYBIND11_PLUGIN(_grid)
{
  namespace py = pybind11;
  using namespace pybind11::literals;

  py::module m("_grid", "dune-xt-grid");

  Dune::XT::Common::bindings::addbind_exceptions(m);

  py::module::import("dune.xt.common");

  addbind_for_Grid<Dune::YaspGrid<1, Dune::EquidistantOffsetCoordinates<double, 1>>>(m);
  addbind_for_Grid<Dune::YaspGrid<2, Dune::EquidistantOffsetCoordinates<double, 2>>>(m);
#if HAVE_DUNE_ALUGRID
  addbind_for_Grid<Dune::ALUGrid<2, 2, Dune::simplex, Dune::conforming>>(m);
#endif
#if HAVE_DUNE_UGGRID || HAVE_UG
  addbind_for_Grid<Dune::UGGrid<2>>(m);
#endif
#if HAVE_ALBERTA
  addbind_for_Grid<Dune::AlbertaGrid<2, 2>>(m);
#endif

  DUNE_XT_GRID_BOUNDARYINFO_BIND(m);
  DUNE_XT_GRID_WALKER_APPLYON_BIND(m);
  Dune::XT::Common::bindings::add_initialization(m, "dune.xt.grid");

  return m.ptr();
}
