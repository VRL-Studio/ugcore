/*
 * surface_dof_distribution.cpp
 *
 *  Created on: 24.01.2012
 *      Author: andreasvogel
 */

#include "surface_dof_distribution.h"
#include "mg_dof_distribution_impl.h"
#include "lib_grid/file_io/file_io.h"
#include "lib_grid/algorithms/debug_util.h"

using namespace std;

namespace ug{

////////////////////////////////////////////////////////////////////////////////
// SurfaceDoFDistribution
////////////////////////////////////////////////////////////////////////////////

///	constructor
SurfaceDoFDistribution::
SurfaceDoFDistribution(SmartPtr<MultiGrid> spMG,
                       SmartPtr<MGSubsetHandler> spMGSH,
                       ConstSmartPtr<DoFDistributionInfo> spDDInfo,
                       SmartPtr<SurfaceView> spSurfView,
                       int level, bool bGrouped)
		:	DoFDistributionInfoProvider(spDDInfo),
		 	MGDoFDistribution(spMG, spMGSH, spDDInfo, bGrouped),
		 	DoFDistribution(*this, spSurfView, GridLevel(level, GridLevel::SURFACE, false)),
		 	m_spSurfView(spSurfView),
		 	m_level(level),
		 	m_bRedistribute(false)
{
	init();
}


template <typename TBaseElem>
void SurfaceDoFDistribution::init()
{
	typedef typename traits<TBaseElem>::iterator iterator;
	static const int dim = TBaseElem::dim;

	for(int si = 0; si < num_subsets(); ++si)
	{
	// 	skip if no dofs to be distributed
		if(max_dofs(dim, si) == 0) continue;

	//	get iterators of elems
		iterator iter = begin<TBaseElem>(si);
		iterator iterEnd = end<TBaseElem>(si);

	// 	loop elems
		for(; iter != iterEnd; ++iter)
		{
		// 	get vertex
			TBaseElem* elem = *iter;

		//	a) add new indices

		//	add element
			const ReferenceObjectID roid = elem->reference_object_id();
			add(elem, roid, si, m_levInfo);

		//	b) if copy exists, copy also to parent and grand-parents and ... etc.
			//\todo: save this execution in non-adaptive case
			TBaseElem* parent = parent_if_copy(elem);
			while(parent){
				copy(parent, elem);
				elem = parent;
				parent = parent_if_copy(elem);
			}
		}
	} // end subset
}

void SurfaceDoFDistribution::init()
{
	m_levInfo.vNumIndexOnSubset.resize(num_subsets());

	if(max_dofs(VERTEX) > 0) init<VertexBase>();
	if(max_dofs(EDGE) > 0)   init<EdgeBase>();
	if(max_dofs(FACE) > 0)   init<Face>();
	if(max_dofs(VOLUME) > 0) init<Volume>();

#ifdef UG_PARALLEL
	m_pDistGridMgr = m_spMG->distributed_grid_manager();
	create_layouts_and_communicator();
#endif
}

template <typename TBaseElem>
void SurfaceDoFDistribution::reinit(std::vector<std::pair<size_t,size_t> >& vReplaced)
{
	typedef typename traits<TBaseElem>::iterator iterator;
	static const int dim = TBaseElem::dim;

	for(int si = 0; si < num_subsets(); ++si)
	{
	// 	skip if no dofs to be distributed
		if(max_dofs(dim, si) == 0) continue;

	//	get iterators of elems
		iterator iter = begin<TBaseElem>(si);
		iterator iterEnd = end<TBaseElem>(si);

	// 	loop elems
		for(; iter != iterEnd; ++iter)
		{
		// 	get vertex
			TBaseElem* elem = *iter;

		//	a) add new indices

		//	add element
			const ReferenceObjectID roid = elem->reference_object_id();
			add(elem, roid, si, m_levInfo,vReplaced);

		//	b) if copy exists, copy also to parent and grand-parents and ... etc.
			//\todo: save this execution in non-adaptive case
			TBaseElem* parent = parent_if_copy(elem);
			while(parent){
				copy(parent, elem);
				elem = parent;
				parent = parent_if_copy(elem);
			}
		}
	} // end subset
}

void SurfaceDoFDistribution::reinit()
{
	m_levInfo.clear_all();

	std::vector<std::pair<size_t,size_t> > vReplaced;

	if(max_dofs(VERTEX) > 0) reinit<VertexBase>(vReplaced);
	if(max_dofs(EDGE) > 0)   reinit<EdgeBase>(vReplaced);
	if(max_dofs(FACE) > 0)   reinit<Face>(vReplaced);
	if(max_dofs(VOLUME) > 0) reinit<Volume>(vReplaced);

	//	adapt managed vectors
	if(!vReplaced.empty())
		copy_values(vReplaced, false);

#ifdef UG_PARALLEL
	m_pDistGridMgr = m_spMG->distributed_grid_manager();
	create_layouts_and_communicator();
#endif

}

void SurfaceDoFDistribution::redistribute_dofs(bool bReinit)
{
	m_levInfo.clear_all();
	m_sFreeIndex.clear();

	if (bReinit==false)
		init();
	else
		reinit();

	resize_values(num_indices());
}

template <typename TBaseElem>
void SurfaceDoFDistribution::
add_unassigned_elements()
{
	typedef typename traits<TBaseElem>::iterator iterator;
	static const int dim = TBaseElem::dim;

	for(int si = 0; si < num_subsets(); ++si){
	// 	skip if no dofs to be distributed
		if(max_dofs(dim, si) == 0) continue;

		for(iterator iter = begin<TBaseElem>(si);
			iter != end<TBaseElem>(si); ++iter)
		{
			TBaseElem* elem = *iter;
			if(obj_index(elem) != NOT_YET_ASSIGNED)
				continue;

			const ReferenceObjectID roid = elem->reference_object_id();
			add_from_free(elem, roid, si, m_levInfo);

			TBaseElem* parent = parent_if_copy(elem);
			while(parent){
				copy(parent, elem);
				elem = parent;
				parent = parent_if_copy(elem);
			}
		}
	} // end subset
}

template <typename TBaseElem>
void SurfaceDoFDistribution::
remove_ghost_entries()
{
	#ifdef UG_PARALLEL
		UG_ASSERT(m_pMG->distributed_grid_manager(),
				  "A distributed grid manager has to be present in a parallel environment.");
		DistributedGridManager& dgm = *m_pMG->distributed_grid_manager();
		GridLayoutMap& glm = dgm.grid_layout_map();
		typedef typename GridLayoutMap::Types<TBaseElem>::Layout	TLayout;
		typedef typename TLayout::Interface							TInterface;

		if(glm.has_layout<TBaseElem>(INT_V_MASTER)){
			TLayout& masterLayout = glm.get_layout<TBaseElem>(INT_V_MASTER);
			for(size_t lvl = 0; lvl < masterLayout.num_levels(); ++lvl){
				for(typename TLayout::iterator iter = masterLayout.begin(lvl);
					iter != masterLayout.end(lvl); ++iter)
				{
					TInterface& intfc = masterLayout.interface(iter);
					for(typename TInterface::iterator eiter = intfc.begin();
						eiter != intfc.end(); ++eiter)
					{
						TBaseElem* e = intfc.get_element(eiter);
						size_t objInd = obj_index(e);
						if(dgm.is_ghost(e)
						   && (objInd != NOT_YET_ASSIGNED))
						{
						//	we have to erase the object from the distribution
							erase(e, e->reference_object_id(),
								  m_spMGSH->get_subset_index(e),
								  m_levInfo);

						//	set the index of the object and of all its parent
						//	copies to NOT_YET_ASSIGNED
							TBaseElem* telem = e;
							while(telem){
								obj_index(telem) = NOT_YET_ASSIGNED;
								telem = parent_if_copy(telem);
							}
						}
					}
				}
			}
		}
	#endif
}

void SurfaceDoFDistribution::
parallel_redistribution_ended()
{
	m_levInfo.vNumIndexOnSubset.resize(num_subsets(), 0);

	if(max_dofs(VERTEX) > 0) remove_ghost_entries<VertexBase>();
	if(max_dofs(EDGE) > 0)   remove_ghost_entries<EdgeBase>();
	if(max_dofs(FACE) > 0)   remove_ghost_entries<Face>();
	if(max_dofs(VOLUME) > 0) remove_ghost_entries<Volume>();

	if(max_dofs(VERTEX) > 0) add_unassigned_elements<VertexBase>();
	if(max_dofs(EDGE) > 0)   add_unassigned_elements<EdgeBase>();
	if(max_dofs(FACE) > 0)   add_unassigned_elements<Face>();
	if(max_dofs(VOLUME) > 0) add_unassigned_elements<Volume>();

	resize_values(lev_info().sizeIndexSet);

// DEBUG CHECKS... REMOVE THOSE AS SOON AS EVERYTHING WORKS STABLE
////	iterate over all vertices in the surface view and check whether all are assigned
//	for(typename traits<VertexBase>::iterator iter = begin<VertexBase>();
//		iter != end<VertexBase>(); ++iter)
//	{
//		if(m_spSurfView->is_ghost(*iter)) continue;
//		if(obj_index(*iter) == NOT_YET_ASSIGNED){
//			UG_LOG("ERROR: UNASSIGNED SURFACE VERTEX FOUND AT: "
//					<< GetGeometricObjectCenter(*m_pMG, *iter) << endl);
//		}
//	}
//
////	iterate over all edges in the surface view and check whether all vertices are assigned
//	for(typename traits<EdgeBase>::iterator iter = begin<EdgeBase>();
//		iter != end<EdgeBase>(); ++iter)
//	{
//		if(m_spSurfView->is_ghost(*iter)) continue;
//		EdgeBase* e = *iter;
//		for(size_t i = 0; i < e->num_vertices(); ++i){
//			if(obj_index(e->vertex(i)) == NOT_YET_ASSIGNED){
//				UG_LOG("ERROR: UNASSIGNED SURFACE-EDGE-VERTEX FOUND AT: "
//						<< GetGeometricObjectCenter(*m_pMG, e->vertex(i)) << endl);
//			}
//		}
//	}
//
////	iterate over all faces in the surface view and check whether all vertices are assigned
//	for(typename traits<Face>::iterator iter = begin<Face>();
//		iter != end<Face>(); ++iter)
//	{
//		if(m_spSurfView->is_ghost(*iter)) continue;
//		Face* e = *iter;
//		for(size_t i = 0; i < e->num_vertices(); ++i){
//			if(obj_index(e->vertex(i)) == NOT_YET_ASSIGNED){
//				UG_LOG("ERROR: UNASSIGNED SURFACE-FACE-VERTEX FOUND AT: "
//						<< GetGeometricObjectCenter(*m_pMG, e->vertex(i)) << endl);
//			}
//		}
//	}

//	DEFRAGMENT HAS TO BE CALLED EXTERNALLY! OR EXECUTE THE FOLLOWING CODE
//	#ifdef UG_PARALLEL
//		create_layouts_and_communicator();
//	#endif
//
//	resize_values(lev_info().sizeIndexSet);
}


#ifdef UG_PARALLEL
void SurfaceDoFDistribution::create_layouts_and_communicator()
{
	pcl::ProcessCommunicator commWorld;

//  -----------------------------------
//	CREATE PROCESS COMMUNICATOR
//  -----------------------------------
//	The idea  of local processes is to exclude processes from
//	e.g. norm computation that does not have a grid on a given
//	level. If no DoFs exist on the level, that level is excluded from
//	norm computations. In those cases the process votes false for
//	the subcommunicator.

// 	choose if this process participates
	bool participate = !commWorld.empty() && (num_indices() > 0);

//	create process communicator for interprocess layouts
	lev_info().layouts()->proc_comm() = commWorld.create_sub_communicator(participate);

//  -----------------------------------
//	CREATE INDEX LAYOUTS ON LEVEL
//  -----------------------------------

	create_index_layout(lev_info().layouts()->master(), INT_H_MASTER);
	create_index_layout(lev_info().layouts()->slave(), INT_H_SLAVE);

//	no vertical layouts in surface dof distribution
	lev_info().layouts()->vertical_master().clear();
	lev_info().layouts()->vertical_slave().clear();
}

void SurfaceDoFDistribution::create_index_layout(IndexLayout& layout,
                                                 int keyType)
{
//	clear layout
	layout.clear();

//	add the index from grid layouts
	if(max_dofs(VERTEX)) add_indices_from_layouts<VertexBase>(layout, keyType);
	if(max_dofs(EDGE))   add_indices_from_layouts<EdgeBase>(layout, keyType);
	if(max_dofs(FACE))   add_indices_from_layouts<Face>(layout, keyType);
	if(max_dofs(VOLUME)) add_indices_from_layouts<Volume>(layout, keyType);

//	touching an interface means creation. Thus we remove the empty interfaces
//	to avoid storage, communication (should not happen any longer) etc...
	pcl::RemoveEmptyInterfaces(layout);
}

template <typename TBaseElem>
void SurfaceDoFDistribution::add_indices_from_layouts(IndexLayout& indexLayout,
                                                      int keyType)
{
//	get the grid layout map
	GridLayoutMap& layoutMap = m_pDistGridMgr->grid_layout_map();

//	check if layout present
	if(!layoutMap.has_layout<TBaseElem>(keyType)) return;

//	choose level, that must be looped
	int fromLev = 0, toLev = layoutMap.get_layout<TBaseElem>(keyType).num_levels() - 1;
	if(m_level == GridLevel::TOPLEVEL){
		if(toLev < 0) toLev = 0;
	}
	else toLev = m_level;

//	loop all level
	for(int level = fromLev; level <= toLev; ++level)
	{
	//	get element layout
		typedef typename GridLayoutMap::Types<TBaseElem>::Layout::LevelLayout TLayout;
		TLayout& elemLayout = layoutMap.get_layout<TBaseElem>(keyType).layout_on_level(level);

	//	iterator for grid element interfaces
		typedef typename TLayout::iterator InterfaceIterator;

	//	type of grid element interfaces
		typedef typename TLayout::Interface ElemInterface;

	//	iterator for grid elements
		typedef typename ElemInterface::iterator ElemIterator;

	//	type of index interfaces
		typedef IndexLayout::Interface IndexInterface;

	//	vector for algebra indices
		std::vector<size_t> vIndex;

	//	iterate over all grid element interfaces
		for(InterfaceIterator iIter = elemLayout.begin();
			iIter != elemLayout.end(); ++iIter)
		{
		//	get a grid element interface
			ElemInterface& elemInterface = elemLayout.interface(iIter);

		//	get a corresponding index interface
			IndexInterface& indexInterface = indexLayout.interface(
												elemLayout.proc_id(iIter));

		//	iterate over entries in the grid element interface
			for(ElemIterator eIter = elemInterface.begin();
				eIter != elemInterface.end(); ++eIter)
			{
			//	get the grid element
				typename ElemInterface::Element elem = elemInterface.get_element(eIter);

			//	check if element is on surface (i.e. has no children). Shadows are
			//	not taken into account here, since their indices are already added
			//	to the interface by the shadowing objects
				//if(multi_grid()->has_children(elem)) {continue;}

			//	check if element is a ghost element, i.e. it is a surface element
			//	but only due to a hierarchical cut of the grid in order to
			//	refine it further on another process. These cuts lead to so called
			//	vertical interfaces.
				//if(m_spSurfView->is_ghost(elem)) {continue;}
				if(!m_spSurfView->is_surface_element(elem)) {continue;}

			//	get the algebraic indices on the grid element
				MGDoFDistribution::inner_algebra_indices(elem, vIndex);

			//	add the indices to the interface
				for(size_t i = 0; i < vIndex.size(); ++i)
					indexInterface.push_back(vIndex[i]);
			}
		}
	}
}
#endif

template <typename TBaseElem>
bool SurfaceDoFDistribution::defragment(std::vector<std::pair<size_t,size_t> >& vReplaced)
{
	typedef typename traits<TBaseElem>::iterator iterator;
	static const int dim = TBaseElem::dim;

//	if nothing to do, return
	if(!lev_info().free_index_available()) return true;

	int numElem = 0;
//	loop subsets
	for(int si = 0; si < num_subsets(); ++si)
	{
	// 	skip if no DoFs to be distributed
		if(max_dofs(dim, si) == 0) continue;

		iterator iter = begin<TBaseElem>(si);
		iterator iterEnd = end<TBaseElem>(si);

	// 	loop elems
		for(; iter != iterEnd; ++iter)
		{
		// 	get element
			TBaseElem* elem = *iter;

			numElem++;

		//	get roid
			const ReferenceObjectID roid = elem->reference_object_id();

		//	only for elements with dofs
			if(num_dofs(roid, si) == 0) continue;

		//	check correct index and replace if needed
			if (MGDoFDistribution::defragment(elem, roid, si, lev_info(), vReplaced)==false) return false;

		//	if copy exists, copy also to parent and grand-parents and ... etc.
			//\todo: save this execution in non-adaptive case
			TBaseElem* parent = parent_if_copy(elem);
			while(parent){
				copy(parent, elem);
				elem = parent;
				parent = parent_if_copy(elem);
			}
		}
	}
	return true;
}

void SurfaceDoFDistribution::defragment()
{
//	defragment
	std::vector<std::pair<size_t,size_t> > vReplaced;
	bool valid = true;

	if (m_bRedistribute==false){
		if(max_dofs(VERTEX) > 0) valid=defragment<VertexBase>(vReplaced);
		if((max_dofs(EDGE) > 0)&&(valid==true)) valid=defragment<EdgeBase>(vReplaced);
		if((max_dofs(FACE) > 0)&&(valid==true)) valid=defragment<Face>(vReplaced);
		if((max_dofs(VOLUME) > 0)&&(valid==true)) valid=defragment<Volume>(vReplaced);
	} else valid=false;

	// if no valid index set redistribute
	if (valid==false){
		redistribute_dofs(true);
	} else {
		//	check that only invalid indices left
		for(size_t m=1;m<lev_info().max_multiplicity();m++)
			for(LevInfo<std::set<size_t> >::iterator it = lev_info().begin(m); it != lev_info().end(m); ++it)
				UG_ASSERT(*it >= lev_info().numIndex, "After defragment still index in "
				          	  	  	  	  "valid range present in free index container.");

		//	clear container
		for(size_t m=1;m<lev_info().max_multiplicity();m++){
			lev_info().sizeIndexSet -= lev_info().num_free_index(m);
			lev_info().clear(m);
		}

		for(size_t m=1;m<lev_info().max_multiplicity();m++)
			if(lev_info().free_index_available(m))
				UG_THROW("Internal error: Still free indices available after "
								"defragment: " <<  lev_info().num_free_index(m));

		if(lev_info().numIndex != lev_info().sizeIndexSet)
			UG_THROW("Internal error: numIndex and sizeIndexSet must be "
								"equal after defragment, since the index set does not "
								"contain holes anymore. But numIndex = "<<lev_info().numIndex
								<<", sizeIndexSet = "<<lev_info().sizeIndexSet);

		//	adapt managed vectors
		if(!vReplaced.empty())
			copy_values(vReplaced, true);

		//	num indices may have changed
		resize_values(num_indices());

		#ifdef UG_PARALLEL
			create_layouts_and_communicator();
		#endif
	}
}

template <typename TBaseElem>
inline void SurfaceDoFDistribution::obj_created(TBaseElem* obj, GeometricObject* pParent,
                        bool replacesParent)
{
	const static int gbo = geometry_traits<TBaseElem>::BASE_OBJECT_ID;

	if(max_dofs(gbo) == 0)
		return;

//	case 1: if replacesParent == true, only an obj (e.g. HangingVertex)
//			is replaced by a similar obj (e.g. Vertex). This case is
//			handled in obj_to_be_erased(), that will be called in addition
//			to this callback with replacedBy != NULL.
	if(replacesParent) return;

//	case 2: A real insertion in the multigrid: add indices
	if(parallel_redistribution_mode()){
		if(TBaseElem* parent = parent_if_copy(obj))
			copy(obj, parent);
		else
			obj_index(obj) = NOT_YET_ASSIGNED;
	}
	else{
		if(add(obj,
				obj->reference_object_id(),
				m_spMGSH->get_subset_index(obj),
				m_levInfo))
		{
			if(!parallel_redistribution_mode()){
			//	the insertion changed the size of the index range. Thus, we have to
			//	resize the managed vectors. We do this now, since a transfer callback
			//	may be listen to the object creation and will interpolate the values. Thus,
			//	the vector entries must already be valid.
				resize_values(lev_info().sizeIndexSet);

			//	the parent, that will be covered after the creation, will no longer be part
			//	of the surface. But we still need the values for the transfer callbacks.
			//	Therefore, we leave the "old" indices attached and valid for the moment.
			//	All prolongation callbacks are invoked now
				if(pParent){
					for(size_t i = 0; i < m_vProlongation[gbo].size(); ++i)
						m_vProlongation[gbo][i]->prolongate_values(obj, pParent, *this);
				}
			}
		}
	}
	
	if (m_bRedistribute==true) return;

//	Now we remember that the indices on the parent object must be removed when
//	defragmentation is called. We do this only, if the parent is of same
//	base element type. This is possible, since for each refined element, there
//	is at least one "finer" element, that will be inserted, of same base type.
	TBaseElem* parent = parent_if_same_type(obj);
	if(parent)
	{
		if(obj_index(parent) != NOT_YET_ASSIGNED){
			erase(parent,
				  parent->reference_object_id(),
				  m_spMGSH->get_subset_index(parent),
				  m_levInfo);
		}

	//	copy the index from the newly created object to its parent and grand parents.
	//	this is important to correctly erase ghost copies when redistribution is done
		TBaseElem* telem = parent;
		while(telem){
			copy(telem, obj);
			telem = parent_if_copy(telem);
		}
	}

}

template <typename TElem>
void SurfaceDoFDistribution::
add_unassigned_sides(TElem* e)
{
	if(!TElem::HAS_SIDES)
		return;

	typedef typename TElem::side TSide;
	typename Grid::traits<TSide>::secure_container	sides;
	m_spMG->associated_elements(sides, e);
	for(size_t i = 0; i < sides.size(); ++i){
		TSide * s = sides[i];
		if(max_dofs(TSide::BASE_OBJECT_ID) > 0){
			if(obj_index(s) == NOT_YET_ASSIGNED){
				add_from_free(s, s->reference_object_id(),
							  m_spMGSH->get_subset_index(s), m_levInfo);
				TSide* ts = s;
				while(ts){
					copy(ts, s);
					ts = parent_if_copy(ts);
				}
			}
		}
		add_unassigned_sides(s);
	}
}

template <typename TBaseElem>
inline void SurfaceDoFDistribution::
obj_to_be_erased(TBaseElem* obj, TBaseElem* replacedBy)
{
	const static int gbo = geometry_traits<TBaseElem>::BASE_OBJECT_ID;

	if((!parallel_redistribution_mode()) && replacedBy && replacedBy->is_constrained())
	{
		size_t oldSize = lev_info().sizeIndexSet;
		if(max_dofs(TBaseElem::BASE_OBJECT_ID) > 0){
			if((obj_index(obj) == NOT_YET_ASSIGNED))
			{
			//	the element has to receive a valid index and it's parents too,
			//	since those are shadowed elements now
				add_from_free(replacedBy, replacedBy->reference_object_id(),
							  m_spMGSH->get_subset_index(replacedBy), m_levInfo);
				TBaseElem* telem = replacedBy;
				while(telem){
					copy(telem, replacedBy);
					telem = parent_if_copy(telem);
				}
				add_unassigned_sides(replacedBy);
			}
			else
				copy(replacedBy, obj);
		}
		else
			add_unassigned_sides(replacedBy);

		if(lev_info().sizeIndexSet > oldSize)
			resize_values(lev_info().sizeIndexSet);

		return;
	}

	if(max_dofs(TBaseElem::BASE_OBJECT_ID) == 0)
		return;

//	case 1: Only replacement. Just copy indices from one obj to the other
	if(replacedBy){
		copy(replacedBy, obj);
		return;
	}

//	different behavior is required for element erasure during adaption and
//	during redistribution.
//	When an element is removed during redistribution, it will be inserted on
//	another process later on. This means, that parents of the removed element
//	on the local process won't be contained in the surface grid after
//	redistribution is done...
	if(!parallel_redistribution_mode()){
		GeometricObject* parent = get_parent(obj);
	//	case 2: Element disappears, but parent is identical. Thus, the parent
	//			has the same indices attached. All indices remain valid on
	//			every surface level. No resizement in the index set must be
	//			performed.
		if(parent_if_copy(obj)) {
			size_t objIndex = obj_index(obj);
			if(!m_spSurfView->is_surface_element(obj)){
			//	obj is not a member of the surface view and hasn't got a valid index.
			//	It's parent, however, becomes a member of the surface view when obj
			//	is erased and therefore has to be added to the surface view.
			//todo: PERFORM SOME KIND OF RESTRICTION!!!
				size_t oldSize = lev_info().sizeIndexSet;
				add_from_free(parent, parent->reference_object_id(),
							  m_spMGSH->get_subset_index(parent), m_levInfo);
				TBaseElem* telem = static_cast<TBaseElem*>(parent);
				while(telem){
					copy(telem, obj);
					telem = parent_if_copy(telem);
				}
				if(lev_info().sizeIndexSet > oldSize)
					resize_values(lev_info().sizeIndexSet);
				return;
			}
			else if(objIndex == obj_index(parent)){
				return;
			}
		}

	//	case 3: The object that will be erased has no identical parent on the
	//			coarser grid. In this case we have to remove the index from
	//			the index set.

	//	All prolongation callbacks are invoked now
		if(parent){
			for(size_t i = 0; i < m_vRestriction[gbo].size(); ++i){
				m_vRestriction[gbo][i]->restrict_values(obj, parent, *this);
			}
		}
	}

//	remember that index from object is now no longer in index set
	if(m_spSurfView->is_surface_element(obj)
	   &&(obj_index(obj) != NOT_YET_ASSIGNED))
	{
		erase(obj,
			  obj->reference_object_id(),
			  m_spMGSH->get_subset_index(obj),
			  m_levInfo);

		if(parallel_redistribution_mode()){
		//	set the index of the object and of all its parent copies to NOT_YET_ASSIGNED
		//	this is important to correctly erase ghost copies when the redistribution is done
			TBaseElem* telem = obj;
			while(telem){
				obj_index(telem) = NOT_YET_ASSIGNED;
				telem = parent_if_copy(telem);
			}
		}
	}
}


void SurfaceDoFDistribution::vertex_created(Grid* grid, VertexBase* vrt, GeometricObject* pParent, bool replacesParent) {obj_created<VertexBase>(vrt, pParent, replacesParent);}
void SurfaceDoFDistribution::edge_created(Grid* grid, EdgeBase* e, GeometricObject* pParent, bool replacesParent) {obj_created<EdgeBase>(e, pParent, replacesParent);}
void SurfaceDoFDistribution::face_created(Grid* grid, Face* f, GeometricObject* pParent, bool replacesParent) {obj_created<Face>(f, pParent, replacesParent);}
void SurfaceDoFDistribution::volume_created(Grid* grid, Volume* vol, GeometricObject* pParent, bool replacesParent) {obj_created<Volume>(vol, pParent, replacesParent);}

void SurfaceDoFDistribution::vertex_to_be_erased(Grid* grid, VertexBase* vrt, VertexBase* replacedBy) {obj_to_be_erased<VertexBase>(vrt, replacedBy);}
void SurfaceDoFDistribution::edge_to_be_erased(Grid* grid, EdgeBase* e, EdgeBase* replacedBy) {obj_to_be_erased<EdgeBase>(e, replacedBy);}
void SurfaceDoFDistribution::face_to_be_erased(Grid* grid, Face* f, Face* replacedBy) {obj_to_be_erased<Face>(f, replacedBy);}
void SurfaceDoFDistribution::volume_to_be_erased(Grid* grid, Volume* vol, Volume* replacedBy) {obj_to_be_erased<Volume>(vol, replacedBy);}


template <typename TBaseElem>
void SurfaceDoFDistribution::
get_connections(std::vector<std::vector<size_t> >& vvConnection) const
{
//	dimension of Base Elem
	static const int dim = TBaseElem::dim;

//	Adjacent geometric objects
	std::vector<VertexBase*> vVrts;
	std::vector<EdgeBase*> vEdges;
	std::vector<Face*> vFaces;
	std::vector<Volume*> vVols;

// 	Iterators
	typedef typename traits<TBaseElem>::const_iterator const_iterator;
	const_iterator iterEnd = end<TBaseElem>();

//	loop elem
	for(const_iterator iter = begin<TBaseElem>(); iter != iterEnd; ++iter)
	{
	// 	Get elem
		TBaseElem* elem = *iter;

	//	vector of indices
		std::vector<size_t> vIndex;

	//	Get connected elements
		if(dim >= VERTEX && max_dofs(VERTEX) > 0) {
			m_spSurfView->collect_associated(vVrts, elem);
			changable_indices<VertexBase>(vIndex, vVrts);
		}
		if(dim >= EDGE   && max_dofs(EDGE) > 0)	{
			m_spSurfView->collect_associated(vEdges, elem);
			changable_indices<EdgeBase>(vIndex, vEdges);
		}
		if(dim >= FACE   && max_dofs(FACE) > 0)	{
			m_spSurfView->collect_associated(vFaces, elem);
			changable_indices<Face>(vIndex, vFaces);
		}
		if(dim >= VOLUME && max_dofs(VOLUME) > 0) {
			m_spSurfView->collect_associated(vVols, elem);
			changable_indices<Volume>(vIndex, vVols);
		}

	//	remove doubles
		std::sort(vIndex.begin(), vIndex.end());
		vIndex.erase(std::unique(vIndex.begin(), vIndex.end()), vIndex.end());

	//	add coupling to adjacency graph
		std::vector<size_t>::iterator it;

		for(size_t i = 0; i < vIndex.size(); ++i)
			for(size_t j = i; j < vIndex.size(); ++j)
			{
				const size_t iIndex = vIndex[i];
				const size_t jIndex = vIndex[j];

			//	add connection (iIndex->jIndex)
				it = find(vvConnection[iIndex].begin(), vvConnection[iIndex].end(), jIndex);
				if(it == vvConnection[iIndex].end()) vvConnection[iIndex].push_back(jIndex);


			//	add the opposite direction (adjInd -> index)
				it = find(vvConnection[jIndex].begin(), vvConnection[jIndex].end(), iIndex);
				if(it == vvConnection[jIndex].end()) vvConnection[jIndex].push_back(iIndex);
			}
	}
}

bool SurfaceDoFDistribution::
get_connections(std::vector<std::vector<size_t> >& vvConnection) const
{
//	if no subset given, we're done
	if(num_subsets() == 0) return true;

//	check that in all subsets same number of functions and at least one
//	if this is not the case for non-grouped DoFs, we cannot allow reordering
	if(!MGDoFDistribution::grouped())
	{
		size_t numDoFs = 0;
		for(int si = 0; si < num_subsets(); ++si)
			for(int i = 0; i < NUM_REFERENCE_OBJECTS; ++i)
			{
				const ReferenceObjectID roid = (ReferenceObjectID) i;
				if(num_dofs(roid,si) == 0) continue;
				if(numDoFs == 0) {numDoFs = num_dofs(roid,si); continue;}
				if(num_dofs(roid,si) != numDoFs)
					UG_THROW("SurfaceDoFDistribution::get_connections: "
							"Currently only implemented iff same number of DoFs on"
							" all geometric objects in all subsets: \n"
							"num_dofs("<<roid<<","<<si<<")="<<num_dofs(roid,si)<<
							", but previously found "<<numDoFs);
			}
	}

//	clear neighbors
	vvConnection.clear(); vvConnection.resize(num_indices());

//	get_connections<VertexBase>(vvConnection);
	get_connections<EdgeBase>(vvConnection);
	get_connections<Face>(vvConnection);
	get_connections<Volume>(vvConnection);

	return true;
}

template <typename TBaseElem>
void SurfaceDoFDistribution::permute_indices(const std::vector<size_t>& vNewInd)
{
// 	loop Vertices
	typedef typename traits<TBaseElem>::const_iterator const_iterator;

	const_iterator iterEnd = end<TBaseElem>();
	for(const_iterator iter = begin<TBaseElem>(); iter != iterEnd; ++iter)
	{
	// 	get vertex
		TBaseElem* elem = *iter;

	// 	get current (old) index
		const size_t oldIndex = obj_index(elem);

	//	replace old index by new one
		obj_index(elem) = vNewInd[oldIndex];

	//	if copy exists, copy also to parent and grand-parents and ... etc.
		//\todo: save this execution in non-adaptive case
		TBaseElem* parent = parent_if_copy(elem);
		while(parent){
			copy(parent, elem);
			elem = parent;
			parent = parent_if_copy(elem);
		}
	}
}

void SurfaceDoFDistribution::permute_indices(const std::vector<size_t>& vNewInd)
{
	if(max_dofs(VERTEX)) permute_indices<VertexBase>(vNewInd);
	if(max_dofs(EDGE))   permute_indices<EdgeBase>(vNewInd);
	if(max_dofs(FACE))   permute_indices<Face>(vNewInd);
	if(max_dofs(VOLUME)) permute_indices<Volume>(vNewInd);

#ifdef UG_PARALLEL
	create_layouts_and_communicator();
#endif

//	permute indices in associated vectors
	permute_values(vNewInd);
}


} // end namespace ug
