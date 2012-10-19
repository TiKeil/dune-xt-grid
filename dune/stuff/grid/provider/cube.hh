#ifndef DUNE_STUFF_GRID_PROVIDER_CUBE_HH
#define DUNE_STUFF_GRID_PROVIDER_CUBE_HH

#ifdef HAVE_CMAKE_CONFIG
#include "cmake_config.h"
#elif defined(HAVE_CONFIG_H)
#include <config.h>
#endif // ifdef HAVE_CMAKE_CONFIG

#ifdef HAVE_DUNE_GRID

// system
#include <sstream>
#include <type_traits>
#include <boost/assign/list_of.hpp>

// dune-common
#include <dune/common/parametertree.hh>
#include <dune/common/shared_ptr.hh>
#include <dune/common/exceptions.hh>
#include <dune/common/fvector.hh>
#include <dune/common/static_assert.hh>

// dune-grid
#include <dune/grid/utility/structuredgridfactory.hh>
#include <dune/grid/yaspgrid.hh>
#ifdef HAVE_ALUGRID
#include <dune/grid/alugrid.hh>
#endif
#include <dune/grid/sgrid.hh>

// local
#include "interface.hh"

namespace Dune {

namespace Stuff {

namespace Grid {

namespace Provider {

/**
 *  \brief  Creates a grid of a cube in various dimensions.
 *
 *          Default implementation using the Dune::StructuredGridFactory to create a grid of a cube in 1, 2 or 3
 *          dimensions. Tested with
 *          <ul><li> \c YASPGRID, \c variant 1, dim = 1, 2, 3,
 *          <li> \c SGRID, \c variant 1, dim = 1, 2, 3,
 *          <li> \c ALUGRID_SIMPLEX, \c variant 2, dim = 2, 3,
 *          <li> \c ALUGRID_CONFORM, \c variant 2, dim = 2, 2 and
 *          <li> \c ALUGRID_CUBE, \c variant 1, dim = 2, 3.</ul>
 *  \tparam GridImp
 *          Type of the underlying grid.
 *  \tparam variant
 *          Type of the codim 0 elements:
 *          <ul><li>\c 1: cubes
 *          <li>2: simplices</ul>
 **/
template <typename GridImp, int variant>
class GenericCube : public Interface<GridImp>
{
public:
  //! Type of the provided grid.
  typedef GridImp GridType;

private:
  typedef typename GridType::LeafGridView GridViewType;

public:
  //! Dimension of the provided grid.
  static const unsigned int dim = GridType::dimension;

  //! Type of the grids coordinates.
  typedef Dune::FieldVector<typename GridType::ctype, dim> CoordinateType;

  //! Unique identifier: \c stuff.grid.provider.cube
  static const std::string static_id;

  /**
   *  \brief      Creates a cube.
   *  \param[in]  paramTree
   *              A Dune::ParameterTree containing
   *              <ul><li> the following keys directly or
   *              <li> a subtree named Cube::id, containing the following keys. If a subtree is present, it is always
   *selected. Also it is solely selceted, so that all keys in the supertree are ignored.</ul>
   *              The actual keys are:
   *              <ul><li> \c lowerLeft: \a double that is used as a lower left corner in each dimension.
   *              <li> \c upperRight: \a double that is used as an upper right corner in each dimension.
   *              <li> \c numElements.D \a int to denote the number of elements in direction of dimension D (has to be
   *given for each dimension seperately).
   *              <li> \c level: \a int level of refinement. If given, overrides numElements and creates \f$ 2^level \f$
   *elements per dimension.
   *              </ul>
   **/
  GenericCube(const Dune::ParameterTree paramTree)
    : lowerLeft_(0.0)
    , upperRight_(1.0)
  {
    // select subtree (if necessary)
    Dune::ParameterTree paramTree_ = paramTree;
    if (paramTree.hasSub(static_id))
      paramTree_ = paramTree.sub(static_id);
    // get outer bounds
    const double ll = paramTree_.get("lowerLeft", 0.0);
    const double rr = paramTree_.get("upperRight", 1.0);
    assert(ll < rr);
    lowerLeft_  = ll;
    upperRight_ = rr;
    // get number of elements per dim
    if (paramTree.hasKey("level"))
      std::fill(numElements_.begin(), numElements_.end(), std::pow(2, paramTree.get("level", 1)));
    else {
      for (unsigned int d = 0; d < dim; ++d) {
        std::stringstream s;
        s << "numElements." << d;
        numElements_[d] = paramTree.get(s.str(), 1);
      }
    }
    buildGrid();
  } // Cube(const Dune::ParameterTree& paramTree)

  /**
   *  \brief      Creates a cube.
   *  \param[in]  lowerLeft
   *              A vector that is used as a lower left corner.
   *  \param[in]  upperRight
   *              A vector that is used as a upper right corner.
   *  \param[in]  level (optional)
   *              Level of refinement (see constructor for details).
   **/
  GenericCube(const CoordinateType& _lowerLeft, const CoordinateType& _upperRight, const int level = 1)
    : lowerLeft_(_lowerLeft)
    , upperRight_(_upperRight)
  {
    std::fill(numElements_.begin(), numElements_.end(), std::pow(2, level));
    buildGrid();
  }

  /**
   *  \brief      Creates a cube.
   *  \param[in]  lowerLeft
   *              A double that is used as a lower left corner in each dimension.
   *  \param[in]  upperRight
   *              A double that is used as a upper right corner in each dimension.
   *  \param[in]  level (optional)
   *              Level of refinement (see constructor for details).
   **/
  GenericCube(const double _lowerLeft, const double _upperRight, const int level = 1)
    : lowerLeft_(_lowerLeft)
    , upperRight_(_upperRight)
  {
    std::fill(numElements_.begin(), numElements_.end(), std::pow(2, level));
    buildGrid();
  }

  /**
    \brief      Creates a cube. This signature allows to prescribe anisotopic refinement
    \param[in]  lowerLeft
                A double that is used as a lower left corner in each dimension.
    \param[in]  upperRight
                A double that is used as a upper right corner in each dimension.
    \param[in]  elements_per_dim number of elements in each dimension.
                can contain 0 to dim elements (missing dimension are initialized to 1)
    \tparam Coord anything that CoordinateType allows to copy construct from
    \tparam ContainerType some sequence type that functions with std::begin/end
    \tparam T an unsigned integral Type
    **/
  template <class Coord, class ContainerType>
  GenericCube(const Coord _lowerLeft, const Coord _upperRight,
              const ContainerType elements_per_dim = boost::assign::list_of<typename ContainerType::value_type>()
                                                         .repeat(GridType::dimensionworld,
                                                                 typename ContainerType::value_type(1)))
    : lowerLeft_(_lowerLeft)
    , upperRight_(_upperRight)
  {
    static_assert(std::is_unsigned<typename ContainerType::value_type>::value
                      && std::is_integral<typename ContainerType::value_type>::value,
                  "only unsigned integral number of elements per dimension allowed");
    // base init in case input is shorter
    std::fill(numElements_.begin(), numElements_.end(), 1);
    std::copy(elements_per_dim.begin(), elements_per_dim.end(), numElements_.begin());
    buildGrid();
  }

  virtual std::string id() const
  {
    return static_id;
  }

  /**
    \brief  Provides access to the created grid.
    \return Reference to the grid.
    **/
  virtual GridType& grid()
  {
    return *grid_;
  }

  /**
   *  \brief  Provides const access to the created grid.
   *  \return Reference to the grid.
   **/
  virtual const GridType& grid() const
  {
    return *grid_;
  }

  //! access to shared ptr
  virtual Dune::shared_ptr<GridType> gridPtr()
  {
    return grid_;
  }

  virtual const Dune::shared_ptr<const GridType> gridPtr() const
  {
    return grid_;
  }

  const CoordinateType& lowerLeft() const
  {
    return lowerLeft_;
  }

  const CoordinateType& upperRight() const
  {
    return upperRight_;
  }

  /**
   *  \brief      Visualizes the grid using Dune::VTKWriter.
   *  \param[in]  filename
   **/

private:
  void buildGrid()
  {
    dune_static_assert(variant >= 1 && variant <= 2, "only variant 1 and 2 are valid");
    switch (variant) {
      case 1:
        grid_ = Dune::StructuredGridFactory<GridType>::createCubeGrid(lowerLeft_, upperRight_, numElements_);
        break;
      case 2:
      default:
        grid_ = Dune::StructuredGridFactory<GridType>::createSimplexGrid(lowerLeft_, upperRight_, numElements_);
        break;
    }
  } // void buildGrid(const CoordinateType& lowerLeft, const CoordinateType& upperRight)


  CoordinateType lowerLeft_;
  CoordinateType upperRight_;
  Dune::array<unsigned int, dim> numElements_;
  Dune::shared_ptr<GridType> grid_;
}; // class GenericCube

template <typename GridImp, int variant>
const std::string GenericCube<GridImp, variant>::static_id = "stuff.grid.provider.cube";

template <typename GridType>
struct ElementVariant
{
  static const int id = 2;
};

template <int dim>
struct ElementVariant<Dune::YaspGrid<dim>>
{
  static const int id = 1;
};

template <int dim>
struct ElementVariant<Dune::SGrid<dim, dim>>
{
  static const int id = 1;
};

#if HAVE_ALUGRID
template <int dim>
struct ElementVariant<Dune::ALUCubeGrid<dim, dim>>
{
  static const int id = 1;
};
#endif

// default implementation of a cube for any grid
// tested for
// dim = 2
//  ALUGRID_SIMPLEX, variant 2
//  ALUGRID_CONFORM, variant 2
// dim = 3
//  ALUGRID_SIMPLEX, variant 2
template <class GridType>
class Cube : public GenericCube<GridType, ElementVariant<GridType>::id>
{
private:
  typedef GenericCube<GridType, ElementVariant<GridType>::id> BaseType;

public:
  typedef typename BaseType::CoordinateType CoordinateType;

  Cube(const Dune::ParameterTree& paramTree)
    : BaseType(paramTree)
  {
  }

  Cube(const CoordinateType& lowerLeft, const CoordinateType& upperRight, const int level = 1)
    : BaseType(lowerLeft, upperRight, level)
  {
  }

  Cube(const double lowerLeft, const double upperRight, const int level = 1)
    : BaseType(lowerLeft, upperRight, level)
  {
  }

  template <class Coord, class ContainerType>
  Cube(const Coord lowerLeft, const Coord upperRight,
       const ContainerType elements_per_dim = boost::assign::list_of<typename ContainerType::value_type>().repeat(
           GridType::dimensionworld, typename ContainerType::value_type(1)))
    : BaseType(lowerLeft, upperRight, elements_per_dim)
  {
  }
}; // class Cube

template <class GridType>
class UnitCube : public Cube<GridType>
{
private:
  typedef Cube<GridType> BaseType;

public:
  UnitCube(const Dune::ParameterTree& paramTree)
    : BaseType(0.0, 1.0, paramTree.get("level", 1))
  {
  }

  UnitCube(const int level = 1)
    : BaseType(0.0, 1.0, level)
  {
  }
}; // class UnitCube

} // namespace Provider

} // namespace Grid

} // namespace Stuff

} // namespace Dune

#endif // HAVE_DUNE_GRID

#endif // DUNE_STUFF_GRID_PROVIDER_CUBE_HH
