/*
 * lib_algebra.h
 *
 *  Created on: 02.07.2009
 *      Author: andreasvogel
 */

#ifndef __H__LIB_ALGEBRA__LIB_ALGEBRA__
#define __H__LIB_ALGEBRA__LIB_ALGEBRA__

#include <iomanip>
// other ug4 modules
#include "common/common.h"

#include "local_matrix_vector/flex_local_matrix_vector.h"

// library intern includes
#include "lib_algebra/multi_index/multi_indices.h"

// parallel support
#ifdef UG_PARALLEL
	#include "lib_algebra/parallelization/parallelization.h"
#endif



/////////////////////////////////////////////
/////////////////////////////////////////////
//   ublas algebra
/////////////////////////////////////////////
/////////////////////////////////////////////

#include "ublas_algebra/ublas_matrix.h"
#include "ublas_algebra/ublas_vector.h"
#include "ublas_algebra/ublas_linearsolver.h"

namespace ug {

/** Define different algebra types.
 *  An Algebra should export the following typedef:
 *  - matrix_type
 *  - vector_type
 *  - index_type
 */
class UblasAlgebra{
	public:
		// matrix type
		typedef UblasMatrix matrix_type;

		// vector type
		typedef UblasVector vector_type;

		// index_type
		typedef MultiIndex<1> index_type;
};


} // namespace ug

/////////////////////////////////////////////
/////////////////////////////////////////////
//   Hypre Algebra
/////////////////////////////////////////////
/////////////////////////////////////////////


#if 0

#include "hypre_algebra/hyprematrix.h"
#include "hypre_algebra/hyprevector.h"
#include "hypre_algebra/hyprelinearsolver.h"

namespace ug{
class HypreAlgebra{
	public:
		// matrix type
		typedef HypreMatrix matrix_type;

		// vector type
		typedef HypreVector vector_type;

		// index_type
		typedef MultiIndex<1> index_type;

		typedef HYPREboomerAMG linear_solver_type;

};
}

#endif

/////////////////////////////////////////////
/////////////////////////////////////////////
//   Martin Algebra
/////////////////////////////////////////////
/////////////////////////////////////////////
//#define USE_MARTIN_ALGEBRA
#ifdef USE_MARTIN_ALGEBRA
#include "martin_algebra/vector.h"

namespace ug
{
class MartinAlgebra
	{
	public:
		// matrix type
		typedef SparseMatrix<double> matrix_type;

		// vector type
		typedef Vector<double> vector_type;

		// index_type
		typedef MultiIndex<1> index_type;

		//	typedef HYPREboomerAMG linear_solver_type;
	};

  // this will soon be moved
bool diag_step(const SparseMatrix<number>& A, Vector<number>& x, Vector<number>& b, number damp)
{
	//exit(3);
	UG_ASSERT(x.size() == b.size() && x.size() == A.getCols(), x << ", " << b << " and " << A << " need to have same size.");

	for(size_t j=0; j < x.size(); j++)
		x[j] = b[j] / A.getDiag(j);

	return true;
}
}

#endif

#endif /* __H__LIB_ALGEBRA__LIB_ALGEBRA__ */
