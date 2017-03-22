// This file is part of the dune-xt-grid project:
//   https://github.com/dune-community/dune-xt-grid
// Copyright 2009-2017 dune-xt-grid developers and contributors. All rights reserved.
// License: Dual licensed as BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
//      or  GPL-2.0+ (http://opensource.org/licenses/gpl-license)
//          with "runtime exception" (http://www.dune-project.org/license.html)
// Authors:
//   Felix Schindler (2015 - 2017)
//   Rene Milk       (2015 - 2016)
//   Tobias Leibner  (2015 - 2017)

#ifndef DUNE_XT_GRID_VIEW_PERIODIC_HH
#define DUNE_XT_GRID_VIEW_PERIODIC_HH

#include <bitset>
#include <iterator>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <dune/geometry/typeindex.hh>
#include <dune/grid/common/gridview.hh>

#include <dune/xt/common/exceptions.hh>
#include <dune/xt/common/float_cmp.hh>
#include <dune/xt/common/memory.hh>

#include <dune/xt/grid/entity.hh>
#include <dune/xt/grid/rangegenerators.hh>
#include <dune/xt/grid/search.hh>
#include <dune/xt/grid/type_traits.hh>

namespace Dune {
namespace XT {
namespace Grid {

namespace internal {


template <bool codim_iters_provided,
          int codim,
          class DomainType,
          size_t dimDomain,
          class RealGridViewType,
          class IndexType,
          class Codim0EntityType>
struct IndexMapCreatorBase
{
  typedef std::vector<std::pair<bool, Codim0EntityType>> IntersectionMapType;
  static const size_t num_geometries = GlobalGeometryTypeIndex::size(dimDomain);

  IndexMapCreatorBase(
      const DomainType& lower_left,
      const DomainType& upper_right,
      const std::bitset<dimDomain>& periodic_directions,
      const RealGridViewType& real_grid_view,
      std::array<IndexType, dimDomain + 1>& entity_counts,
      std::array<IndexType, num_geometries>& type_counts,
      std::array<std::unordered_set<IndexType>, num_geometries>& entities_to_skip,
      std::array<std::vector<IndexType>, num_geometries>& new_indices,
      const std::pair<bool, Codim0EntityType>& nonperiodic_pair,
      std::array<std::unordered_map<IndexType, IntersectionMapType>, num_geometries>& entity_to_intersection_map_map)
    : lower_left_(lower_left)
    , upper_right_(upper_right)
    , periodic_directions_(periodic_directions)
    , real_grid_view_(real_grid_view)
    , real_index_set_(real_grid_view_.indexSet())
    , entity_counts_(entity_counts)
    , type_counts_(type_counts)
    , entities_to_skip_(entities_to_skip)
    , new_indices_(new_indices)
    , current_new_index_({})
    , nonperiodic_pair_(nonperiodic_pair)
    , entity_to_intersection_map_map_(entity_to_intersection_map_map)
  {
    for (const auto& geometry_type : real_index_set_.types(codim)) {
      const auto type_index = GlobalGeometryTypeIndex::index(geometry_type);
      const auto num_type_entities = real_index_set_.size(geometry_type);
      if (codim == 0)
        type_counts_[GlobalGeometryTypeIndex::index(geometry_type)] = num_type_entities;
      new_indices_[type_index].resize(num_type_entities);
    }
  }

  template <class EntityType, size_t cd = codim>
  typename std::enable_if<cd != 0, void>::type
  loop_body(const EntityType& entity, const std::size_t& type_index, const IndexType& old_index)
  {
    // check if entity is on a periodic boundary
    auto periodic_coords = entity.geometry().center();
    std::size_t num_upper_right_coords = 0;
    for (std::size_t ii = 0; ii < dimDomain; ++ii) {
      if (periodic_directions_[ii]) {
        if (XT::Common::FloatCmp::eq(periodic_coords[ii], upper_right_[ii])) {
          ++num_upper_right_coords;
          periodic_coords[ii] = lower_left_[ii];
        }
      }
    }

    if (num_upper_right_coords == 0) {
      // increase codim counter
      new_indices_[type_index][old_index] = current_new_index_[type_index];
      ++current_new_index_[type_index];
      // increase GeometryType counter
      ++type_counts_[type_index];
      ++entity_counts_[codim];
    } else {
      entities_to_skip_[type_index].insert(old_index);
      periodic_coords_.push_back(periodic_coords);
      periodic_coords_index_.push_back({type_index, old_index});
    }
  } // loop_body

  template <class EntityType, size_t cd = codim>
  typename std::enable_if<cd == 0, void>::type
  loop_body(const EntityType& entity, const std::size_t& type_index, const IndexType& entity_index)
  {
    if (entity.hasBoundaryIntersections()) {
      IntersectionMapType intersection_neighbor_map(entity.subEntities(1));
      const auto i_it_end = real_grid_view_.iend(entity);
      for (auto i_it = real_grid_view_.ibegin(entity); i_it != i_it_end; ++i_it) {
        const auto& intersection = *i_it;
        const int index_in_inside = intersection.indexInInside();
        if (intersection.boundary()) {
          bool is_periodic = false;
          auto periodic_neighbor_coords = intersection.geometry().center();
          size_t num_boundary_coords = 0;
          for (std::size_t ii = 0; ii < dimDomain; ++ii) {
            if (periodic_directions_[ii]) {
              if (XT::Common::FloatCmp::eq(periodic_neighbor_coords[ii], lower_left_[ii])) {
                is_periodic = true;
                periodic_neighbor_coords[ii] =
                    upper_right_[ii] - 1.0 / 100.0 * (entity.geometry().center()[ii] - lower_left_[ii]);
                ++num_boundary_coords;
              } else if (XT::Common::FloatCmp::eq(periodic_neighbor_coords[ii], upper_right_[ii])) {
                is_periodic = true;
                periodic_neighbor_coords[ii] =
                    lower_left_[ii] + 1.0 / 100.0 * (upper_right_[ii] - entity.geometry().center()[ii]);
                ++num_boundary_coords;
              }
            }
          }
          if (is_periodic) {
            assert(num_boundary_coords == 1);
            periodic_coords_.push_back(periodic_neighbor_coords);
            periodic_coords_index_.push_back(std::make_tuple(type_index, entity_index, index_in_inside));
          } else {
            intersection_neighbor_map[index_in_inside] = nonperiodic_pair_;
          }
        } else {
          intersection_neighbor_map[index_in_inside] = nonperiodic_pair_;
        }
      }
      entity_to_intersection_map_map_[type_index][entity_index] = intersection_neighbor_map;
    } // if (entity.hasBoundaryIntersections)
  } // loop body codim 0

  void after_loop()
  {
    if (codim == 0)
      entity_counts_[codim] = real_index_set_.size(0);

    // find periodic entities
    typename std::conditional<codim_iters_provided,
                              EntityInlevelSearch<RealGridViewType, codim>,
                              FallbackEntityInlevelSearch<RealGridViewType, codim>>::type
        entity_search_codim(real_grid_view_);
    const auto periodic_entity_ptrs = entity_search_codim(periodic_coords_);
    // assign index of periodic equivalent entity to entities that are replaced
    for (size_t vector_index = 0; vector_index < periodic_entity_ptrs.size(); ++vector_index) {
      const auto& index = periodic_coords_index_[vector_index];
      const auto& periodic_entity_ptr = periodic_entity_ptrs[vector_index];
      if (!periodic_entity_ptr)
        DUNE_THROW(Dune::InvalidStateException, "Could not find periodic neighbor entity");
      assign_after_search(index, *periodic_entity_ptr);
    }
  } // after_loop()

  template <class EntityType, size_t cd = codim>
  typename std::enable_if<cd != 0, void>::type assign_after_search(const std::pair<size_t, IndexType>& index,
                                                                   const EntityType& periodic_entity)
  {
    const auto& type_index = index.first;
    const auto& entity_index = index.second;
    const auto periodic_entity_index = real_index_set_.index(periodic_entity);
    const auto& periodic_entity_type_index = GlobalGeometryTypeIndex::index(periodic_entity.type());
    new_indices_[type_index][entity_index] = new_indices_[periodic_entity_type_index][periodic_entity_index];
  }

  template <class EntityType, size_t cd = codim>
  typename std::enable_if<cd == 0, void>::type assign_after_search(const std::tuple<size_t, IndexType, int>& index,
                                                                   const EntityType& periodic_entity)
  {
    const auto& type_index = std::get<0>(index);
    const auto& entity_index = std::get<1>(index);
    const auto& local_intersection_index = std::get<2>(index);
    entity_to_intersection_map_map_[type_index][entity_index][local_intersection_index] = {true, periodic_entity};
  }

  const DomainType& lower_left_;
  const DomainType& upper_right_;
  const std::bitset<dimDomain>& periodic_directions_;
  const RealGridViewType& real_grid_view_;
  const typename RealGridViewType::IndexSet& real_index_set_;
  std::array<IndexType, dimDomain + 1>& entity_counts_;
  std::array<IndexType, num_geometries>& type_counts_;
  std::array<std::unordered_set<IndexType>, num_geometries>& entities_to_skip_;
  std::array<std::vector<IndexType>, num_geometries>& new_indices_;
  std::vector<DomainType> periodic_coords_;
  typename std::conditional<codim == 0,
                            std::vector<std::tuple<size_t, IndexType, int>>,
                            std::vector<std::pair<size_t, IndexType>>>::type periodic_coords_index_;
  std::array<IndexType, num_geometries> current_new_index_;
  const std::pair<bool, Codim0EntityType>& nonperiodic_pair_;
  std::array<std::unordered_map<IndexType, IntersectionMapType>, num_geometries>& entity_to_intersection_map_map_;
};

template <bool codim_iters_provided,
          int codim,
          class DomainType,
          size_t dimDomain,
          class RealGridViewType,
          class IndexType,
          class EntityType>
struct IndexMapCreator
    : IndexMapCreatorBase<codim_iters_provided, codim, DomainType, dimDomain, RealGridViewType, IndexType, EntityType>
{
  typedef IndexMapCreatorBase<codim_iters_provided,
                              codim,
                              DomainType,
                              dimDomain,
                              RealGridViewType,
                              IndexType,
                              EntityType>
      BaseType;

  template <class... Args>
  IndexMapCreator(Args&&... args)
    : BaseType(std::forward<Args>(args)...)
  {
  }

  void create_index_map()
  {
    for (const auto& codim0_entity : elements(real_grid_view_)) {
      for (IndexType local_index = 0; local_index < codim0_entity.subEntities(codim); ++local_index) {
        const auto& entity = codim0_entity.template subEntity<codim>(local_index);
        const auto old_index = real_grid_view_.indexSet().index(entity);
        const auto type_index = GlobalGeometryTypeIndex::index(entity.type());
        if (!visited_entities_[type_index].count(old_index)) {
          this->loop_body(entity, type_index, old_index);
          visited_entities_[type_index].insert(old_index);
        } // if (entity has not been visited before)
      } // walk subentities in a given codimension
    } // walk codim0 entities
    this->after_loop();
  } // ... create_index_map(...)

  using BaseType::real_grid_view_;
  std::array<std::unordered_set<IndexType>, GlobalGeometryTypeIndex::size(dimDomain)> visited_entities_;
}; // struct IndexMapCreator< ... >

template <int codim, class DomainType, size_t dimDomain, class RealGridViewType, class IndexType, class EntityType>
struct IndexMapCreator<true, codim, DomainType, dimDomain, RealGridViewType, IndexType, EntityType>
    : IndexMapCreatorBase<true, codim, DomainType, dimDomain, RealGridViewType, IndexType, EntityType>
{
  typedef IndexMapCreatorBase<true, codim, DomainType, dimDomain, RealGridViewType, IndexType, EntityType> BaseType;

  template <class... Args>
  IndexMapCreator(Args&&... args)
    : BaseType(std::forward<Args>(args)...)
  {
  }

  void create_index_map()
  {
    for (const auto& entity : entities(real_grid_view_, Dune::Codim<codim>())) {
      const auto old_index = real_grid_view_.indexSet().index(entity);
      const auto type_index = GlobalGeometryTypeIndex::index(entity.type());
      this->loop_body(entity, type_index, old_index);
    }
    this->after_loop();
  } // ... create_index_map(...)

  using BaseType::real_grid_view_;
}; // struct IndexMapCreator< ... >

// forward
template <class RealGridViewImp, bool codim_iters_provided>
class PeriodicGridViewTraits;

/** \brief IndexSet for PeriodicGridView
 *
 * PeriodicIndexSet is derived from the IndexSet of the underlying GridView. Other than the IndexSet,
 * PeriodicIndexSet returns the same index for entities that are periodically equivalent, i.e. entities on periodic
 * boundaries that are regarded as the same entity in the periodic setting. Consequently, the PeriodicIndexSet is
 * usually smaller than the IndexSet and the size(...) methods return lower values than the corresponding methods of
 * the non-periodic IndexSet.
 *
 * \see PeriodicGridView
 */
template <class RealGridViewImp>
class PeriodicIndexSet : public Dune::IndexSet<typename RealGridViewImp::Grid,
                                               PeriodicIndexSet<RealGridViewImp>,
                                               typename RealGridViewImp::IndexSet::IndexType,
                                               typename RealGridViewImp::IndexSet::Types>
{
  typedef RealGridViewImp RealGridViewType;
  typedef PeriodicIndexSet<RealGridViewType> ThisType;
  typedef typename RealGridViewType::IndexSet RealIndexSetType;
  typedef typename Dune::IndexSet<typename RealGridViewType::Grid,
                                  ThisType,
                                  typename RealGridViewType::IndexSet::IndexType,
                                  typename RealGridViewType::IndexSet::Types>
      BaseType;

public:
  typedef typename RealIndexSetType::IndexType IndexType;
  typedef typename RealIndexSetType::Types Types;
  static const int dimDomain = RealGridViewType::dimension;
  static const size_t num_geometries = GlobalGeometryTypeIndex::size(dimDomain);

  PeriodicIndexSet(const RealIndexSetType& real_index_set,
                   const std::array<IndexType, dimDomain + 1>& entity_counts,
                   const std::array<IndexType, num_geometries>& type_counts,
                   const std::array<std::vector<IndexType>, num_geometries>& new_indices)
    : BaseType()
    , real_index_set_(real_index_set)
    , entity_counts_(entity_counts)
    , type_counts_(type_counts)
    , new_indices_(new_indices)
  {
    assert(entity_counts_.size() >= dimDomain + 1);
  }

  template <int cd>
  IndexType index(const typename RealGridViewType::Traits::template Codim<cd>::Entity& entity) const
  {
    IndexType real_entity_index = real_index_set_.template index<cd>(entity);
    if (cd == 0)
      return real_entity_index;
    else {
      const auto type_index = GlobalGeometryTypeIndex::index(entity.type());
      return new_indices_[type_index][real_entity_index];
    }
  }

  template <class EntityType>
  IndexType index(const EntityType& entity) const
  {
    return index<EntityType::codimension>(entity);
  }

  template <int cd>
  IndexType
  subIndex(const typename RealGridViewType::Traits::template Codim<cd>::Entity& entity, int i, unsigned int codim) const
  {
    IndexType real_sub_index = real_index_set_.template subIndex<cd>(entity, i, codim);
    if (codim == 0)
      return real_sub_index;
    else {
      const auto& ref_element = reference_element(entity);
      const auto type_index = GlobalGeometryTypeIndex::index(ref_element.type(i, codim));
      return new_indices_[type_index][real_sub_index];
    }
  }

  template <class EntityType>
  IndexType subIndex(const EntityType& entity, int i, unsigned int codim) const
  {
    return subIndex<EntityType::codimension>(entity, i, codim);
  }

  Types types(int codim) const
  {
    return real_index_set_.types(codim);
  }

  IndexType size(Dune::GeometryType type) const
  {
    const auto type_index = GlobalGeometryTypeIndex::index(type);
    return type_counts_[type_index];
  }

  IndexType size(int codim) const
  {
    assert(codim <= dimDomain);
    return entity_counts_[codim];
  }

  template <class EntityType>
  bool contains(const EntityType& entity) const
  {
    return real_index_set_.contains(entity);
  }

private:
  const RealIndexSetType& real_index_set_;
  const std::array<IndexType, dimDomain + 1>& entity_counts_;
  const std::array<IndexType, num_geometries>& type_counts_;
  const std::array<std::vector<IndexType>, num_geometries>& new_indices_;
}; // class PeriodicIndexSet<...>

/** \brief Intersection for PeriodicGridView
 *
 * PeriodicIntersection is derived from the Intersection of the underlying GridView. On the inside of the grid or if
 * periodic_ is false, the PeriodicIntersection will behave exactly like its BaseType. If periodic_ is true, the
 * PeriodicIntersection will return neighbor == true even if it actually is on the boundary. In this case, outside(),
 * geometryInOutside() and indexInOutside() are well-defined and give the information from the periodically adjacent
 * entity.
 *
 * \see PeriodicGridView
 */
template <class RealGridViewImp>
class PeriodicIntersection : public RealGridViewImp::Intersection
{
  typedef RealGridViewImp RealGridViewType;
  typedef PeriodicIntersection<RealGridViewType> ThisType;
  typedef typename RealGridViewType::Intersection BaseType;

public:
  using typename BaseType::LocalGeometry;
  typedef typename BaseType::Entity EntityType;
  typedef typename RealGridViewType::IntersectionIterator RealIntersectionIteratorType;
  static const size_t dimDomain = RealGridViewType::dimension;

  //! \brief Constructor from real intersection
  PeriodicIntersection(const BaseType& real_intersection,
                       const RealGridViewType& real_grid_view,
                       const std::pair<bool, EntityType>& periodic_pair)
    : BaseType(real_intersection)
    , periodic_(periodic_pair.first)
    , outside_(periodic_pair.second)
    , real_grid_view_(real_grid_view)
  {
  }

  // methods that differ from BaseType
  bool neighbor() const
  {
    if (periodic_)
      return true;
    else
      return BaseType::neighbor();
  } // bool neighbor() const

  EntityType outside() const
  {
    if (periodic_)
      return outside_;
    else
      return EntityType(BaseType::outside());
  } // ... outside() const

  LocalGeometry geometryInOutside() const
  {
    if (periodic_) {
      return find_intersection_in_outside().geometryInInside();
    } else {
      return BaseType::geometryInOutside();
    }
  } // ... geometryInOutside() const

  int indexInOutside() const
  {
    if (periodic_) {
      return find_intersection_in_outside().indexInInside();
    } else {
      return BaseType::indexInOutside();
    }
  } // int indexInOutside() const

private:
  // tries to find intersection in outside (works only if periodic_ == true)
  BaseType find_intersection_in_outside() const
  {
    const auto coords = this->geometry().center();
    RealIntersectionIteratorType outside_i_it = real_grid_view_.ibegin(outside_);
    const RealIntersectionIteratorType outside_i_it_end = real_grid_view_.iend(outside_);
    // walk over outside intersections and find an intersection on the boundary that differs only in one coordinate
    for (; outside_i_it != outside_i_it_end; ++outside_i_it) {
      const BaseType& curr_outside_intersection = *outside_i_it;
      if (curr_outside_intersection.boundary()) {
        const auto curr_outside_intersection_coords = curr_outside_intersection.geometry().center();
        size_t coord_difference_count = 0;
        for (size_t ii = 0; ii < dimDomain; ++ii) {
          if (Dune::XT::Common::FloatCmp::ne(curr_outside_intersection_coords[ii], coords[ii])) {
            ++coord_difference_count;
          }
        }
        if (coord_difference_count == size_t(1)) {
          return *outside_i_it;
        }
      }
    }
    DUNE_THROW(Dune::InvalidStateException, "Could not find outside intersection!");
    return *(real_grid_view_.ibegin(outside_));
  } // ... find_intersection_in_outside() const

protected:
  const bool periodic_;
  const EntityType outside_;
  const RealGridViewType& real_grid_view_;
}; // ... class PeriodicIntersection ...

/** \brief IntersectionIterator for PeriodicGridView
 *
 * PeriodicIntersectionIterator is derived from the IntersectionIterator of the underlying GridView and behaves exactly
 * like the underlying IntersectionIterator except that it returns a PeriodicIntersection in its operator* and
 * operator-> methods.
 *
 * \see PeriodicGridView
 */
template <class RealGridViewImp>
class PeriodicIntersectionIterator : public RealGridViewImp::IntersectionIterator
{
  typedef RealGridViewImp RealGridViewType;
  typedef typename RealGridViewType::IntersectionIterator BaseType;
  typedef PeriodicIntersectionIterator<RealGridViewImp> ThisType;

public:
  typedef typename BaseType::Intersection RealIntersectionType;
  typedef int IntersectionIndexType;
  typedef PeriodicIntersection<RealGridViewType> Intersection;
  typedef typename RealGridViewType::template Codim<0>::Entity EntityType;
  typedef std::pair<bool, EntityType> PeriodicPairType;
  static const size_t dimDomain = RealGridViewType::dimension;

  PeriodicIntersectionIterator(BaseType real_intersection_iterator,
                               const RealGridViewType& real_grid_view,
                               const EntityType& entity,
                               const std::vector<PeriodicPairType>& intersection_map,
                               const PeriodicPairType& nonperiodic_pair)
    : BaseType(real_intersection_iterator)
    , real_grid_view_(real_grid_view)
    , entity_(entity)
    , has_boundary_intersections_(entity_.hasBoundaryIntersections())
    , intersection_map_(intersection_map)
    , nonperiodic_pair_(nonperiodic_pair)
    , current_intersection_(create_current_intersection_safely())
  {
  }

  PeriodicIntersectionIterator(const ThisType& other)
    : BaseType(BaseType(other))
    , real_grid_view_(other.real_grid_view_)
    , entity_(other.entity_)
    , has_boundary_intersections_(other.has_boundary_intersections_)
    , intersection_map_(other.intersection_map_)
    , nonperiodic_pair_(other.nonperiodic_pair_)
    , current_intersection_(Dune::XT::Common::make_unique<Intersection>(*(other.current_intersection_)))
  {
  }

  // methods that differ from BaseType
  const Intersection operator*() const
  {
    current_intersection_ = create_current_intersection();
    return *current_intersection_;
  }

  const Intersection* operator->() const
  {
    current_intersection_ = create_current_intersection();
    return &(*current_intersection_);
  }

private:
  std::unique_ptr<Intersection> create_current_intersection() const
  {
    assert(!has_boundary_intersections_
           || intersection_map_.size() > static_cast<size_t>((BaseType::operator*()).indexInInside()));
    return Common::make_unique<Intersection>(BaseType::operator*(),
                                             real_grid_view_,
                                             has_boundary_intersections_
                                                 ? intersection_map_[(BaseType::operator*()).indexInInside()]
                                                 : (const PeriodicPairType&)nonperiodic_pair_);
  } // ... create_current_intersection() const

  std::unique_ptr<Intersection> create_current_intersection_safely() const
  {
    const bool is_iend = (*this == real_grid_view_.iend(entity_));
    const RealIntersectionType real_intersection = is_iend ? *real_grid_view_.ibegin(entity_) : BaseType::operator*();
    assert(is_iend || !has_boundary_intersections_
           || intersection_map_.size() > static_cast<size_t>(real_intersection.indexInInside()));
    return Common::make_unique<Intersection>(real_intersection,
                                             real_grid_view_,
                                             has_boundary_intersections_ && !is_iend
                                                 ? intersection_map_[real_intersection.indexInInside()]
                                                 : (const PeriodicPairType&)nonperiodic_pair_);
  } // ... create_current_intersection_safely() const

  const RealGridViewType& real_grid_view_;
  const EntityType& entity_;
  const bool has_boundary_intersections_;
  const std::vector<PeriodicPairType>& intersection_map_;
  const PeriodicPairType& nonperiodic_pair_;
  mutable std::unique_ptr<Intersection> current_intersection_;
}; // ... class PeriodicIntersectionIterator ...

// forward
template <class RealGridViewImp, bool codim_iters_provided>
class PeriodicGridViewImp;

//! Traits for PeriodicGridView
template <class RealGridViewImp, bool codim_iters_provided>
class PeriodicGridViewTraits : public RealGridViewImp::Traits
{
public:
  typedef RealGridViewImp RealGridViewType;
  // use types from RealGridViewType...
  typedef PeriodicGridViewImp<RealGridViewType, codim_iters_provided> GridViewImp;
  typedef typename RealGridViewType::Grid Grid;
  typedef PeriodicIndexSet<RealGridViewType> IndexSet;
  typedef typename RealGridViewType::CollectiveCommunication CollectiveCommunication;
  typedef typename RealGridViewType::Traits RealGridViewTraits;
  static const size_t num_geometries = GlobalGeometryTypeIndex::size(RealGridViewImp::dimension);

  template <int cd>
  struct Codim : public RealGridViewTraits::template Codim<cd>
  {
    /* PeriodicIterator is the same as the Iterator of the RealGridViewType, except that it visits only one entity of
     * several periodically equivalent entities */
    class PeriodicIterator : public RealGridViewTraits::template Codim<cd>::Iterator
    {
      typedef typename RealGridViewTraits::template Codim<cd>::Iterator BaseType;
      typedef PeriodicIterator ThisType;
      typedef typename RealGridViewType::IndexSet RealIndexSetType;

    public:
      typedef typename IndexSet::IndexType IndexType;
      typedef std::ptrdiff_t difference_type;
      typedef const typename RealGridViewImp::template Codim<0>::Entity value_type;
      typedef value_type* pointer;
      typedef value_type& reference;
      typedef std::forward_iterator_tag iterator_category;

      PeriodicIterator(BaseType real_iterator,
                       const std::array<std::unordered_set<IndexType>, num_geometries>* entities_to_skip,
                       const RealIndexSetType* real_index_set,
                       const BaseType& real_it_end)
        : BaseType(real_iterator)
        , entities_to_skip_(entities_to_skip)
        , real_index_set_(real_index_set)
        , real_it_end_(std::make_shared<const BaseType>(real_it_end))
      {
      }

      ThisType& operator++()
      {
        BaseType::operator++();
        while (cd > 0 && *this != *real_it_end_
               && (*entities_to_skip_)[GlobalGeometryTypeIndex::index((*this)->type())].count(
		      real_index_set_->index(this->operator*())))
          BaseType::operator++();
        return *this;
      }

      ThisType operator++(int)
      {
        return this->operator++();
      }

    private:
      const std::array<std::unordered_set<IndexType>, num_geometries>* entities_to_skip_;
      const RealIndexSetType* real_index_set_;
      std::shared_ptr<const BaseType> real_it_end_;
    };

    typedef PeriodicIterator Iterator;

    template <PartitionIteratorType pit>
    struct Partition : public RealGridViewTraits::template Codim<cd>::template Partition<pit>
    {
      /* PeriodicIterator is the same as the Iterator of the RealGridViewType, except that it visits only one entity of
       * several periodically equivalent entities */
      class PeriodicIterator : public RealGridViewTraits::template Codim<cd>::template Partition<pit>::Iterator
      {
        typedef typename RealGridViewTraits::template Codim<cd>::template Partition<pit>::Iterator BaseType;
        typedef PeriodicIterator ThisType;
        typedef typename RealGridViewType::IndexSet RealIndexSetType;

      public:
        typedef typename IndexSet::IndexType IndexType;
        typedef std::ptrdiff_t difference_type;
        typedef const typename RealGridViewImp::template Codim<0>::Entity value_type;
        typedef value_type* pointer;
        typedef value_type& reference;
        typedef std::forward_iterator_tag iterator_category;

        PeriodicIterator(BaseType real_iterator,
                         const std::array<std::unordered_set<IndexType>, num_geometries>* entities_to_skip,
                         const RealIndexSetType* real_index_set,
                         const BaseType& real_it_end)
          : BaseType(real_iterator)
          , entities_to_skip_(entities_to_skip)
          , real_index_set_(real_index_set)
          , real_it_end_(std::make_shared<const BaseType>(real_it_end))
        {
        }

        // methods that differ from BaseType
        ThisType& operator++()
        {
          BaseType::operator++();
          while (cd > 0 && *this != *real_it_end_
                 && (*entities_to_skip_)[GlobalGeometryTypeIndex::index((*this)->type())].count(
                        real_index_set_->index(this->operator*())))
            BaseType::operator++();
          return *this;
        }

        ThisType operator++(int)
        {
          return this->operator++();
        }

      private:
        const std::array<std::unordered_set<IndexType>, num_geometries>* entities_to_skip_;
        const RealIndexSetType* real_index_set_;
        std::shared_ptr<const BaseType> real_it_end_;
      };
      typedef PeriodicIterator Iterator;
    }; // struct Partition
  }; // ... struct Codim ...

  enum
  {
    conforming = RealGridViewTraits::conforming
  };
  enum
  {
    dimension = Grid::dimension
  };
  enum
  {
    dimensionworld = Grid::dimensionworld
  };

  typedef typename Grid::ctype ctype;

  // ...except for the Intersection and IntersectionIterator
  typedef PeriodicIntersection<RealGridViewType> Intersection;
  typedef PeriodicIntersectionIterator<RealGridViewType> IntersectionIterator;
}; // ... class PeriodicGridViewTraits ...

/** \brief Actual Implementation of PeriodicGridView
 *  \see PeriodicGridView
*/
template <class RealGridViewImp, bool codim_iters_provided>
class PeriodicGridViewImp : public RealGridViewImp
{
  typedef RealGridViewImp BaseType;
  typedef PeriodicGridViewImp<BaseType, codim_iters_provided> ThisType;
  typedef PeriodicGridViewTraits<BaseType, codim_iters_provided> Traits;

public:
  typedef typename BaseType::Grid Grid;
  typedef PeriodicIndexSet<BaseType> IndexSet;
  typedef typename BaseType::template Codim<0>::Entity EntityType;
  typedef PeriodicIntersectionIterator<BaseType> IntersectionIterator;
  typedef typename IntersectionIterator::RealIntersectionType RealIntersectionType;
  typedef typename Traits::IndexSet::IndexType IndexType;
  typedef int IntersectionIndexType;
  typedef typename RealIntersectionType::GlobalCoordinate DomainType;
  typedef PeriodicIntersection<BaseType> Intersection;
  typedef std::vector<std::pair<bool, EntityType>> IntersectionMapType;
  static const size_t dimDomain = BaseType::dimension;
  static const size_t num_geometries = GlobalGeometryTypeIndex::size(dimDomain);

  template <int cd>
  struct Codim : public Traits::template Codim<cd>
  {
  };

private:
  // compile time for loop to loop over the codimensions in constructor, see http://stackoverflow.com/a/11081785
  template <int codim, int to>
  struct static_for_loop_for_index_maps
  {
    template <class... Args>
    void operator()(Args&&... args)
    {
      IndexMapCreator<codim_iters_provided || codim == 0, codim, DomainType, dimDomain, BaseType, IndexType, EntityType>
          index_map_creator(std::forward<Args>(args)...);
      index_map_creator.create_index_map();
      static_for_loop_for_index_maps<codim + 1, to>()(std::forward<Args>(args)...);
    }
  };

  // specialization of static for loop to end the loop
  template <int to>
  struct static_for_loop_for_index_maps<to, to>
  {
    template <class... Args>
    void operator()(Args&&... /*args*/)
    {
    }
  };

public:
  PeriodicGridViewImp(const BaseType& real_grid_view, const std::bitset<dimDomain> periodic_directions)
    : BaseType(real_grid_view)
    , entity_to_intersection_map_map_(
          std::make_shared<std::array<std::unordered_map<IndexType, IntersectionMapType>, num_geometries>>())
    , periodic_directions_(periodic_directions)
    , entity_counts_(std::make_shared<std::array<IndexType, dimDomain + 1>>())
    , type_counts_(std::make_shared<std::array<IndexType, num_geometries>>())
    , entities_to_skip_(std::make_shared<std::array<std::unordered_set<IndexType>, num_geometries>>())
    , new_indices_(std::make_shared<std::array<std::vector<IndexType>, num_geometries>>())
    , real_index_set_(BaseType::indexSet())
  {
    this->update();
  } // constructor PeriodicGridViewImp(...)

  void update()
  {
    // find lower left and upper right corner of the grid
    auto entity_it = BaseType::template begin<0>();
    nonperiodic_pair_ = {false, EntityType(*entity_it)};
    DomainType lower_left = entity_it->geometry().center();
    DomainType upper_right = lower_left;
    for (const auto& entity : Dune::elements(*this)) {
      if (entity.hasBoundaryIntersections()) {
        const auto i_it_end = BaseType::iend(entity);
        for (auto i_it = BaseType::ibegin(entity); i_it != i_it_end; ++i_it) {
          const auto intersection_coords = i_it->geometry().center();
          for (std::size_t ii = 0; ii < dimDomain; ++ii) {
            upper_right[ii] = std::max(upper_right[ii], intersection_coords[ii]);
            lower_left[ii] = std::min(lower_left[ii], intersection_coords[ii]);
          }
        }
      }
    }


    // reset
    std::fill(entity_counts_->begin(), entity_counts_->end(), IndexType(0));
    std::fill(type_counts_->begin(), type_counts_->end(), IndexType(0));
    std::fill(entities_to_skip_->begin(), entities_to_skip_->end(), std::unordered_set<IndexType>());
    std::fill(new_indices_->begin(), new_indices_->end(), std::vector<IndexType>());
    std::fill(entity_to_intersection_map_map_->begin(),
              entity_to_intersection_map_map_->end(),
              std::unordered_map<IndexType, IntersectionMapType>());

    /* walk the grid for each codimension from 0 to dimDomain and create a map mapping indices from entitys of that
     * codimension on a periodic boundary to the index of the corresponding periodic equivalent entity that has the
     * most coordinates in common with the lower left corner of the grid */
    static_for_loop_for_index_maps<0, dimDomain + 1>()(lower_left,
                                                       upper_right,
                                                       periodic_directions_,
                                                       (const BaseType&)(*this),
                                                       *entity_counts_,
                                                       *type_counts_,
                                                       *entities_to_skip_,
                                                       *new_indices_,
                                                       nonperiodic_pair_,
                                                       *entity_to_intersection_map_map_);
    // create index_set
    index_set_ = std::make_shared<IndexSet>(real_index_set_, *entity_counts_, *type_counts_, *new_indices_);
  }

  int size(int codim) const
  {
    return index_set_->size(codim);
  }

  int size(const Dune::GeometryType& type) const
  {
    return index_set_->size(type);
  }

  template <int cd>
  typename Codim<cd>::Iterator begin() const
  {
    return typename Codim<cd>::Iterator(
        BaseType::template begin<cd>(), &(*entities_to_skip_), &real_index_set_, BaseType::template end<cd>());
  }

  template <int cd>
  typename Codim<cd>::Iterator end() const
  {
    return typename Codim<cd>::Iterator(
        BaseType::template end<cd>(), &(*entities_to_skip_), &real_index_set_, BaseType::template end<cd>());
  }

  template <int cd, PartitionIteratorType pitype>
  typename Codim<cd>::template Partition<pitype>::Iterator begin() const
  {
    return typename Codim<cd>::template Partition<pitype>::Iterator(BaseType::template begin<cd, pitype>(),
                                                                    &(*entities_to_skip_),
                                                                    &real_index_set_,
                                                                    BaseType::template end<cd, pitype>());
  }


  template <int cd, PartitionIteratorType pitype>
  typename Codim<cd>::template Partition<pitype>::Iterator end() const
  {
    return typename Codim<cd>::template Partition<pitype>::Iterator(BaseType::template end<cd, pitype>(),
                                                                    &(*entities_to_skip_),
                                                                    &real_index_set_,
                                                                    BaseType::template end<cd, pitype>());
  }

  const IndexSet& indexSet() const
  {
    return *index_set_;
  }

  IntersectionIterator ibegin(const typename Codim<0>::Entity& entity) const
  {
    const auto& type_index = GlobalGeometryTypeIndex::index(entity.type());
    assert(!entity.hasBoundaryIntersections()
           || (*entity_to_intersection_map_map_)[type_index].count(this->indexSet().index(entity)));
    return IntersectionIterator(BaseType::ibegin(entity),
                                *this,
                                entity,
                                entity.hasBoundaryIntersections()
                                    ? (*entity_to_intersection_map_map_)[type_index][this->indexSet().index(entity)]
                                    : (const IntersectionMapType&)empty_intersection_map_,
                                nonperiodic_pair_);

  } // ... ibegin(...)

  IntersectionIterator iend(const typename Codim<0>::Entity& entity) const
  {
    const auto& type_index = GlobalGeometryTypeIndex::index(entity.type());
    assert(!entity.hasBoundaryIntersections()
           || (*entity_to_intersection_map_map_)[type_index].count(this->indexSet().index(entity)));
    return IntersectionIterator(BaseType::iend(entity),
                                *this,
                                entity,
                                entity.hasBoundaryIntersections()
                                    ? (*entity_to_intersection_map_map_)[type_index][this->indexSet().index(entity)]
                                    : (const IntersectionMapType&)empty_intersection_map_,
                                nonperiodic_pair_);
  } // ... iend(...)

private:
  std::shared_ptr<std::array<std::unordered_map<IndexType, IntersectionMapType>, num_geometries>>
      entity_to_intersection_map_map_;
  static const IntersectionMapType empty_intersection_map_;
  const std::bitset<dimDomain> periodic_directions_;
  std::shared_ptr<IndexSet> index_set_;
  std::shared_ptr<std::array<IndexType, dimDomain + 1>> entity_counts_;
  std::shared_ptr<std::array<IndexType, num_geometries>> type_counts_;
  std::shared_ptr<std::array<std::unordered_set<IndexType>, num_geometries>> entities_to_skip_;
  std::shared_ptr<std::array<std::vector<IndexType>, num_geometries>> new_indices_;
  const typename BaseType::IndexSet& real_index_set_;
  static std::pair<bool, EntityType> nonperiodic_pair_;
}; // ... class PeriodicGridViewImp ...

template <class RealGridViewImp, bool codim_iters_provided>
const typename PeriodicGridViewImp<RealGridViewImp, codim_iters_provided>::IntersectionMapType
    PeriodicGridViewImp<RealGridViewImp, codim_iters_provided>::empty_intersection_map_;

template <class RealGridViewImp, bool codim_iters_provided>
std::pair<bool, typename PeriodicGridViewImp<RealGridViewImp, codim_iters_provided>::EntityType>
    PeriodicGridViewImp<RealGridViewImp, codim_iters_provided>::nonperiodic_pair_;

} // namespace internal

/** \brief GridView that takes an arbitrary Dune::GridView and adds periodic boundaries
 *
 * PeriodicGridView is templated by and derived from an arbitrary Dune::GridView. All methods are forwarded to the
 * underlying GridView except for begin, end, ibegin, iend, size and indexSet.
 * The ibegin and iend methods return a PeriodicIntersectionIterator which behaves like the underlying
 * IntersectionIterator except that it returns a PeriodicIntersection in its operator*. The PeriodicIntersection behaves
 * like an Intersection of the underlying GridView, but may return neighbor() == true and an outside() entity even if it
 * is on the boundary. The outside() entity is the entity adjacent to the periodically equivalent intersection, i.e. the
 * intersection at the same position on the opposite side of the grid.
 * The begin and end methods return a PeriodicIterator, which behaves exactly like the corresponding Iterator of the
 * underlying GridView except that visits only one entity of several periodically equivalent entities.
 * The indexSet() method returns a PeriodicIndexSet which returns the same index for entities that are periodically
 * equivalent. Consequently, the PeriodicIndexSet is usually smaller than the IndexSet. The size(...) methods return
 * the corresponding sizes of the PeriodicIndexSet
 * In the constructor, PeriodicGridViewImp will build a map mapping intersections on a periodic boundary to the
 * corresponding outside entity. Further, periodically equivalent entities will be identified and given the same index.
 * Thus, the construction may take quite some time as several grid walks have to be done.
 * By default, new indices will be assigned for all entities. This may take a lot of memory for fine grids. If
 * use_less_memory is set to true, as few entities as possible will get new indices, which saves memory but may
 * degrade performance.
 * By default, all coordinate directions will be made periodic. By supplying a std::bitset< dimension > you can decide
 * for each direction whether it should be periodic (1 means periodic, 0 means 'behave like underlying GridView in that
 * direction').

   \note
      -  PeriodicGridView will only work with GridViews on axis-parallel hyperrectangle grids
      -  Only cube and regular simplex grids have been tested so far. Other grids may not work properly. This is due to
      the heuristics for finding the periodic neighbor entity: Given an intersection on the boundary that shall be
      periodic, the coordinates intersection.geometry().center() are moved to the opposite side of the grid and then
      supplied to Dune::XT::Grid::EntityInLevelSearch. As the coordinates are on the boundary of the wanted entity,
      this search will fail for some grids. Thus, the coordinates are moved a little to the inside of the grid before
      searching for the entity. The moved coordinates will be inside the correct entity for cube and usual simplex grids
      but this is not guaranteed for arbitrary grids.
      - Currently the indices are zero-starting and consecutive per codimension. By the DUNE
      documentation, it should rather be zero-starting and consecutive per codimension AND GeometryType.
 */
template <class RealGridViewImp, bool codim_iters_provided = false>
class PeriodicGridView
    : XT::Common::ConstStorageProvider<internal::PeriodicGridViewImp<RealGridViewImp, codim_iters_provided>>,
      public Dune::GridView<internal::PeriodicGridViewTraits<RealGridViewImp, codim_iters_provided>>
{
  //  static_assert(!is_alugrid<typename RealGridViewImp::Grid>::value,
  //                "ALUGrid 1.52 and older cannot be used here due to missing entitity default ctor");
  typedef RealGridViewImp RealGridViewType;
  typedef typename Dune::GridView<internal::PeriodicGridViewTraits<RealGridViewType, codim_iters_provided>> BaseType;
  typedef
      typename XT::Common::ConstStorageProvider<internal::PeriodicGridViewImp<RealGridViewImp, codim_iters_provided>>
          ConstStorProv;
  typedef typename RealGridViewType::template Codim<0>::Geometry::GlobalCoordinate DomainType;

public:
  static const size_t dimension = RealGridViewType::dimension;

  PeriodicGridView(const RealGridViewType& real_grid_view,
                   const std::bitset<dimension> periodic_directions = std::bitset<dimension>().set())
    : ConstStorProv(new internal::PeriodicGridViewImp<RealGridViewType, codim_iters_provided>(real_grid_view,
                                                                                              periodic_directions))
    , BaseType(ConstStorProv::access())
  {
  }

  PeriodicGridView(const PeriodicGridView& other)
    : ConstStorProv(new internal::PeriodicGridViewImp<RealGridViewType, codim_iters_provided>(other.access()))
    , BaseType(ConstStorProv::access())
  {
  }

  void update()
  {
    BaseType::impl().update();
  }

}; // class PeriodicGridView


template <bool use_less_memory, class GV>
PeriodicGridView<GV, use_less_memory>
make_periodic_grid_view(const GV& real_grid_view,
                        const std::bitset<GV::dimension> periodic_directions = std::bitset<GV::dimension>().set())
{
  return PeriodicGridView<GV, use_less_memory>(real_grid_view, periodic_directions);
}

template <class GV>
PeriodicGridView<GV>
make_periodic_grid_view(const GV& real_grid_view,
                        const std::bitset<GV::dimension> periodic_directions = std::bitset<GV::dimension>().set())
{
  return PeriodicGridView<GV>(real_grid_view, periodic_directions);
}


template <class T, bool bb>
struct is_grid_view<PeriodicGridView<T, bb>> : public std::true_type
{
};


} // namespace Grid
} // namespace XT
} // namespace Dune

#endif // DUNE_XT_GRID_VIEW_PERIODIC_HH
