/*
 * nl_gauss_seidel.h
 *
 *  Created on: 07.01.2013
 *  (main parts are based on the structure of
 *  	newton.h by Andreas Vogel)
 *
 *      Author: raphaelprohl
 */

#ifndef NL_GAUSS_SEIDEL_H_
#define NL_GAUSS_SEIDEL_H_


#include "lib_algebra/operator/interface/operator_inverse.h"

// modul intern headers
#include "lib_disc/assemble_interface.h"
#include "lib_disc/operator/non_linear_operator/assembled_non_linear_operator.h"

namespace ug {

template <typename TDomain, typename TAlgebra>
class NLGaussSeidelSolver
	: public IOperatorInverse<typename TAlgebra::vector_type>,
	  public DebugWritingObject<TAlgebra>
{
	public:
	///	Algebra type
		typedef TAlgebra algebra_type;

	///	Vector type
		typedef typename TAlgebra::vector_type vector_type;

	///	Matrix type
		typedef typename TAlgebra::matrix_type matrix_type;

	///	Domain type
		typedef TDomain domain_type;

	///	GridFunction type
		typedef GridFunction<domain_type, SurfaceDoFDistribution, algebra_type> gridfunc_type;

	protected:
		typedef DebugWritingObject<TAlgebra> base_writer_type;
		using base_writer_type::write_debug;

	public:
	///	constructor setting operator
		NLGaussSeidelSolver(SmartPtr<IOperator<vector_type> > N);

	///	constructor using assembling
		NLGaussSeidelSolver(IAssemble<algebra_type>* pAss);

	///	default constructor
		NLGaussSeidelSolver();

	///	constructor
		NLGaussSeidelSolver(SmartPtr<IConvergenceCheck<vector_type> > spConvCheck);

		void set_convergence_check(SmartPtr<IConvergenceCheck<vector_type> > spConvCheck);
		void set_damp(number damp) {m_damp = damp;}

		/// This operator inverts the Operator N: Y -> X
		virtual bool init(SmartPtr<IOperator<vector_type> > N);

		/// prepare Operator
		virtual bool prepare(vector_type& u);

		/// preprocess
		bool preprocess(gridfunc_type& u);

		/// apply Operator, i.e. N^{-1}(0) = u
		virtual bool apply(vector_type& u);

		/// solve
		bool solve(gridfunc_type& u);

	private:
	///	help functions for debug output
	///	\{
		void write_debug(const vector_type& vec, const char* filename);
		void write_debug(const matrix_type& mat, const char* filename);
	/// \}

	private:
		SmartPtr<IConvergenceCheck<vector_type> > m_spConvCheck;
		number m_damp;

		vector_type m_d;
		vector_type m_c;

		SmartPtr<AssembledOperator<algebra_type> > m_N;
		SmartPtr<AssembledLinearOperator<algebra_type> > m_J;
		IAssemble<algebra_type>* m_pAss;

		///	call counter
		int m_dgbCall;

		///	bool marker of diag-DoFs contributions
		BoolMarker m_vDiagMarker;
};

}

#include "nl_gauss_seidel_impl.h"

#endif /* NL_GAUSS_SEIDEL_H_ */
