// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// The copyright lies with the authors of this file (see below).
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2016)
//   Rene Milk       (2016)

#ifndef DUNE_XT_GRID_BOUNDARYINFO_INTERFACE_HH
#define DUNE_XT_GRID_BOUNDARYINFO_INTERFACE_HH

#include <string>

#include <dune/xt/common/fvector.hh>

#include <dune/xt/grid/type_traits.hh>

namespace Dune {
namespace XT {
namespace Grid {

#if (defined(BOOST_CLANG) && BOOST_CLANG) || (defined(BOOST_GCC) && BOOST_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
class BoundaryType
{
protected:
  virtual std::string id() const = 0;

public:
  virtual bool operator==(const BoundaryType& other) const
  {
    return id() == other.id();
  }

  virtual bool operator!=(const BoundaryType& other) const
  {
    return !operator==(other);
  }

private:
  friend std::ostream& operator<<(std::ostream&, const BoundaryType&);
}; // class BoundaryType


std::ostream& operator<<(std::ostream& out, const BoundaryType& type);


template <class IntersectionImp>
class BoundaryInfo
{
  static_assert(is_intersection<IntersectionImp>::value, "");

public:
  typedef IntersectionImp IntersectionType;
  typedef typename IntersectionType::ctype DomainFieldType;
  static const size_t dimDomain = IntersectionType::dimension;
  static const size_t dimWorld = IntersectionType::dimensionworld;
  typedef Common::FieldVector<DomainFieldType, dimDomain> DomainType;
  typedef Common::FieldVector<DomainFieldType, dimWorld> WorldType;

  virtual const BoundaryType& type(const IntersectionType& intersection) const = 0;

  static std::string static_id()
  {
    return "xt.grid.boundaryinfo";
  }
}; // class BoundaryInfo
#if (defined(BOOST_CLANG) && BOOST_CLANG) || (defined(BOOST_GCC) && BOOST_GCC)
#pragma GCC diagnostic pop
#endif


} // namespace Grid
} // namespace XT
} // namespace Dune

#endif // DUNE_XT_GRID_BOUNDARYINFO_INTERFACE_HH
