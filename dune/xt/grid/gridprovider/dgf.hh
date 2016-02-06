// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// The copyright lies with the authors of this file (see below).
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Andreas Buhr    (2014)
//   Felix Schindler (2012 - 2016)
//   Kirsten Weber   (2012)
//   Rene Milk       (2012 - 2013, 2015)
//   Tobias Leibner  (2014)

#ifndef DUNE_XT_GRID_GRIDPROVIDER_DGF_HH
#define DUNE_XT_GRID_GRIDPROVIDER_DGF_HH

#include <memory>

#if HAVE_ALUGRID
#include <dune/grid/io/file/dgfparser/dgfalu.hh>
#endif
#include <dune/grid/io/file/dgfparser/dgfgridfactory.hh> // How convenient that GridPtr requires DGFGridFactory but ...
#include <dune/grid/io/file/dgfparser/dgfoned.hh>
#include <dune/grid/io/file/dgfparser/dgfs.hh>
#include <dune/grid/io/file/dgfparser/dgfyasp.hh>
#include <dune/grid/io/file/dgfparser/gridptr.hh> // ... does not include it!

#if HAVE_DUNE_ALUGRID
#include <dune/alugrid/dgf.hh>
#endif

#include <dune/xt/common/configuration.hh>
#include <dune/xt/common/string.hh>

#include "provider.hh"

namespace Dune {
namespace XT {
namespace Grid {


static inline std::string dgf_gridprovider_id()
{
  return "xt.grid.gridprovider.dgf";
}


static inline Common::Configuration dgf_gridprovider_default_config()
{
  Common::Configuration config;
  config["type"]     = dgf_gridprovider_id();
  config["filename"] = "dgf_1d_interval.dgf";
  return config;
}


template <class GridType>
class DgfGridProviderFactory
{
  static_assert(is_grid<GridType>::value, "");

public:
  static const bool available = true;

  static std::string static_id()
  {
    return dgf_gridprovider_id();
  }

  static Common::Configuration default_config()
  {
    auto cfg        = dgf_gridprovider_default_config();
    cfg["filename"] = std::string("dgf_") + Common::to_string(size_t(GridType::dimension)) + "d_interval.dgf";
    return cfg;
  }

  static GridProvider<GridType> create(const std::string& filename)
  {
    return GridProvider<GridType>(GridPtr<GridType>(filename).release());
  }

  static GridProvider<GridType> create(const Common::Configuration& cfg = default_config())
  {
    return create(cfg.get("filename", default_config().get<std::string>("filename")));
  }
}; // class DgfGridProviderFactory


template <class GridType>
typename std::enable_if<is_grid<GridType>::value, GridProvider<GridType>>::type
make_dgf_grid(const std::string& filename)
{
  return DgfGridProviderFactory<GridType>(filename);
}


template <class GridType>
typename std::enable_if<is_grid<GridType>::value, GridProvider<GridType>>::type
make_dgf_grid(const Common::Configuration& cfg = DgfGridProviderFactory<GridType>::default_config())
{
  return DgfGridProviderFactory<GridType>::create(cfg);
}


} // namespace Grid
} // namespace XT
} // namespace Dune

#endif // DUNE_XT_GRID_GRIDPROVIDER_DGF_HH
