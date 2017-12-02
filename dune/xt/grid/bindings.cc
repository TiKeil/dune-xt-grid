// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// Copyright 2009-2017 dune-xt-grid developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2016 - 2017)

#include "config.h"

#if HAVE_DUNE_PYBINDXI

#include <string>
#include <vector>

#include <dune/common/parallel/mpihelper.hh>

#if HAVE_DUNE_FEM
#include <dune/fem/misc/mpimanager.hh>
#endif

#include <dune/pybindxi/pybind11.h>
#include <dune/pybindxi/stl.h>

#include <dune/xt/common/bindings.hh>

#include "dd/subdomains/grid.hh"
#include "grids.hh"
#include "layers.hh"
#include "intersection.hh"
#include "type_traits.hh"

#include "boundaryinfo.bindings.hh"
#include "gridprovider.pbh"
#include "walker.bindings.hh"
#include "walker/apply-on.bindings.hh"


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

  bind_GridProvider<G>(m, grid_id);
  bind_make_cube_grid<G>(m, grid_id);

  bind_DdSubdomainsGridProvider<G>(m, grid_id);
  bind_make_cube_dd_subdomains_grid<G>(m, grid_id);

  bind_walker<G, Layers::adaptive_leaf, Backends::part>(m);
  bind_walker<G, Layers::dd_subdomain, Backends::part>(m);
  bind_walker<G, Layers::dd_subdomain_boundary, Backends::part>(m);
  bind_walker<G, Layers::dd_subdomain_coupling, Backends::part>(m);
  bind_walker<G, Layers::dd_subdomain_oversampled, Backends::part>(m);
  bind_walker<G, Layers::leaf, Backends::part>(m);
  bind_walker<G, Layers::level, Backends::part>(m);
  bind_walker<G, Layers::dd_subdomain, Backends::view>(m);
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

//  addbind_for_Grid<Dune::YaspGrid<2, Dune::EquidistantOffsetCoordinates<double, 2>>>(m);
//  addbind_for_Grid<Dune::YaspGrid<1, Dune::EquidistantOffsetCoordinates<double, 1>>>(m);
#if HAVE_DUNE_ALUGRID
  addbind_for_Grid<Dune::ALUGrid<2, 2, Dune::simplex, Dune::conforming>>(m);
#endif
  //#if HAVE_DUNE_UGGRID || HAVE_UG
  //  addbind_for_Grid<Dune::UGGrid<2>>(m);
  //#endif
  //#if HAVE_ALBERTA
  //  addbind_for_Grid<Dune::AlbertaGrid<2, 2>>(m);
  //#endif

  DUNE_XT_GRID_BOUNDARYINFO_BIND(m);
  DUNE_XT_GRID_WALKER_APPLYON_BIND(m);

  m.def("_init_mpi",
        [](const std::vector<std::string>& args) {
          int argc = Dune::XT::Common::numeric_cast<int>(args.size());
          char** argv = Dune::XT::Common::vector_to_main_args(args);
          Dune::MPIHelper::instance(argc, argv);
#if HAVE_DUNE_FEM
          Dune::Fem::MPIManager::initialize(argc, argv);
#endif
        },
        "args"_a = std::vector<std::string>());

  m.def("_init_logger",
        [](const ssize_t max_info_level,
           const ssize_t max_debug_level,
           const bool enable_warnings,
           const bool enable_colors,
           const std::string& info_color,
           const std::string& debug_color,
           const std::string& warning_color) {
          Dune::XT::Common::TimedLogger().create(
              max_info_level, max_debug_level, enable_warnings, enable_colors, info_color, debug_color, warning_color);
        },
        "max_info_level"_a = std::numeric_limits<ssize_t>::max(),
        "max_debug_level"_a = std::numeric_limits<ssize_t>::max(),
        "enable_warnings"_a = true,
        "enable_colors"_a = true,
        "info_color"_a = "blue",
        "debug_color"_a = "darkgray",
        "warning_color"_a = "red");

  m.def("_test_logger",
        [](const bool info, const bool debug, const bool warning) {
          auto logger = Dune::XT::Common::TimedLogger().get("dune.xt.grid");
          if (info)
            logger.info() << "info logging works!" << std::endl;
          if (debug)
            logger.debug() << "debug logging works!" << std::endl;
          if (warning)
            logger.warn() << "warning logging works!" << std::endl;
        },
        "info"_a = true,
        "debug"_a = true,
        "warning"_a = true);

  return m.ptr();
}

#endif // HAVE_DUNE_PYBINDXI
