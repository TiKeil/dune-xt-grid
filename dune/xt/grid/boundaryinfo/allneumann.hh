// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// The copyright lies with the authors of this file (see below).
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2016)
//   Rene Milk       (2016)

#ifndef DUNE_XT_GRID_BOUNDARYINFO_ALLNEUMANN_HH
#define DUNE_XT_GRID_BOUNDARYINFO_ALLNEUMANN_HH

#include <dune/xt/common/configuration.hh>
#include <dune/xt/common/memory.hh>

#include "interfaces.hh"
#include "types.hh"

namespace Dune {
namespace XT {
namespace Grid {


static inline Common::Configuration allneumann_boundaryinfo_default_config()
{
  return Common::Configuration("type", "xt.grid.boundaryinfo.allneumann");
}

#if (defined(BOOST_CLANG) && BOOST_CLANG) || (defined(BOOST_GCC) && BOOST_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
template <class IntersectionImp>
class AllNeumannBoundaryInfo : public BoundaryInfo<IntersectionImp>
{
  typedef BoundaryInfo<IntersectionImp> BaseType;

public:
  using typename BaseType::IntersectionType;

  static std::string static_id()
  {
    return allneumann_boundaryinfo_default_config().get<std::string>("type");
  }

  virtual const BoundaryType& type(const IntersectionType& intersection) const override final
  {
    if (intersection.boundary())
      return neumann_boundary_;
    return no_boundary_;
  }

protected:
  static constexpr NoBoundary no_boundary_{};
  static constexpr NeumannBoundary neumann_boundary_{};
}; // class AllNeumannBoundaryInfo
#if (defined(BOOST_CLANG) && BOOST_CLANG) || (defined(BOOST_GCC) && BOOST_GCC)
#pragma GCC diagnostic pop
#endif

template <class I>
constexpr NoBoundary AllNeumannBoundaryInfo<I>::no_boundary_;
template <class I>
constexpr NeumannBoundary AllNeumannBoundaryInfo<I>::neumann_boundary_;


template <class I>
std::unique_ptr<AllNeumannBoundaryInfo<I>>
make_allneumann_boundaryinfo(const Common::Configuration& /*cfg*/ = Common::Configuration())
{
  return XT::Common::make_unique<AllNeumannBoundaryInfo<I>>();
}


} // namespace Grid
} // namespace XT
} // namespace Dune

#endif // DUNE_XT_GRID_BOUNDARYINFO_ALLNEUMANN_HH
