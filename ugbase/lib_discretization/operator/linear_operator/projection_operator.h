/*
 * projection_operator.h
 *
 *  Created on: 04.12.2009
 *      Author: andreasvogel
 */

#ifndef __H__LIBDISCRETIZATION__OPERATOR__LINEAR_OPERATOR__PROJECTION_OPERATOR__
#define __H__LIBDISCRETIZATION__OPERATOR__LINEAR_OPERATOR__PROJECTION_OPERATOR__

// extern headers
#include <iostream>

// other ug4 modules
#include "common/common.h"
#include "lib_grid/lg_base.h"
#include "lib_algebra/lib_algebra.h"

namespace ug{

// TODO: This function should be put to an util file
/** AssembleVertexProjection
 *
 * This functions assembles the interpolation matrix between to
 * grid levels using only the Vertex degrees of freedom.
 *
 * \param[in] 	uCoarse			Grid function on coarse level
 * \param[in] 	uFine 			Grid function on fine level
 * \param[out]	mat 			Assembled interpolation matrix that interpolates u -> v
 *
 */
template <typename TFunction, typename TMatrix>
bool AssembleVertexProjection(TMatrix& mat, TFunction& uFine, TFunction& uCoarse)
{
	// check, that u and v are from same approximatio space
	if(&uFine.get_approximation_space() != &uCoarse.get_approximation_space())
		{UG_LOG("Interpolation only implemented for equal approximation space.\n"); return false;}

	// allow only lagrange P1 functions
	for(size_t fct = 0; fct < uFine.num_fct(); ++fct)
		if(uFine.local_shape_function_set_id(fct) != LSFS_LAGRANGEP1)
			{UG_LOG("Interpolation only implemented for Lagrange P1 functions.\n"); return false;}

	// get MultiGrid
	MultiGrid& grid = uFine.get_approximation_space().get_domain().get_grid();

	// get number of dofs on different levels
	const size_t numFineDoFs = uFine.num_dofs();
	const size_t numCoarseDoFs = uCoarse.num_dofs();

	// create matrix
	if(!mat.destroy())
		{UG_LOG("Cannot destroy Interpolation Matrix.\n"); return false;}
	if(!mat.create(numCoarseDoFs, numFineDoFs))
		{UG_LOG("Cannot create Interpolation Matrix.\n"); return false;}

	typename TFunction::algebra_index_vector_type fineInd;
	typename TFunction::algebra_index_vector_type coarseInd;

	// Vertex iterators
	geometry_traits<VertexBase>::iterator iter, iterBegin, iterEnd;

	// loop subsets of fine level
	for(int si = 0; si < uFine.num_subsets(); ++si)
	{
		iterBegin = uFine.template begin<Vertex>(si);
		iterEnd = uFine.template end<Vertex>(si);

		// loop nodes of fine subset
		for(iter = iterBegin; iter != iterEnd; ++iter)
		{
			// get father
			GeometricObject* geomObj = grid.get_parent(*iter);
			VertexBase* vert = dynamic_cast<VertexBase*>(geomObj);

			// Check if father is Vertex
			if(vert != NULL)
			{
				// get global indices
				uCoarse.get_inner_algebra_indices(vert, coarseInd);
			}
			else continue;

			// get global indices
			uFine.get_inner_algebra_indices(*iter, fineInd);

			for(size_t i = 0; i < coarseInd.size(); ++i)
				mat(coarseInd[i], fineInd[i]) = 1.0;
		}
	}
	return true;
}



template <typename TFunction>
class ProjectionOperator : public ILinearOperator<TFunction, TFunction> {
	public:
		// domain space
		typedef TFunction domain_function_type;

		// range space
		typedef TFunction  codomain_function_type;

	protected:
		typedef typename TFunction::algebra_type algebra_type;
		typedef typename algebra_type::matrix_type matrix_type;


	public:
		bool init(){return true;}

		// prepare Operator (u=coarse, v = fine)
		bool prepare(TFunction& uCoarseOut, TFunction& uFineIn)
		{
			if(!AssembleVertexProjection(m_matrix, uFineIn, uCoarseOut))
				{UG_LOG("ERROR in 'TransferOperator::prepare(u,v)': Cannot assemble interpolation matrix.\n"); return false;}
			return true;
		}

		// Project uFine to uCoarse; uCoarse = P(uFine);
		bool apply(TFunction& uCoarseOut, TFunction& uFineIn)
		{
			m_matrix.apply(uCoarseOut.get_vector(), uFineIn.get_vector());
#ifdef UG_PARALLEL
			uCoarseOut.copy_storage_type(uFineIn);
#endif
			return true;
		}

		/// Project uCoarse to uFine; uFine = P^{-1}(uCoarse);
		// ATTENTION: This will only affect fine nodes, that are also coarse node, thus
		//            this operation is not very senseful
		bool apply_transposed(TFunction& uCoarseOut, TFunction& uFineIn)
		{
			m_matrix.apply_transposed(uFineIn.get_vector(), uCoarseOut.get_vector());
#ifdef UG_PARALLEL
			uFineIn.copy_storage_type(uCoarseOut);
#endif
			return true;
		}

		// apply sub not implemented
		bool apply_sub(TFunction& u, TFunction& v)
		{
			UG_ASSERT(0, "Not Implemented.");
			return false;
		}

		~ProjectionOperator()
		{
			m_matrix.destroy();
		}

	protected:
		matrix_type m_matrix;
};


}

#endif /* __H__LIBDISCRETIZATION__OPERATOR__LINEAR_OPERATOR__PROJECTION_OPERATOR__ */
