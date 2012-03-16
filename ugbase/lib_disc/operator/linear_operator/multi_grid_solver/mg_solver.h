/*
 * mg_solver.h
 *
 *  Created on: 07.12.2009
 *      Author: andreasvogel
 */

#ifndef __H__UG__LIB_DISC__MULTI_GRID_SOLVER__MG_SOLVER__
#define __H__UG__LIB_DISC__MULTI_GRID_SOLVER__MG_SOLVER__

// extern includes
#include <vector>
#include <iostream>

// other ug4 modules
#include "common/common.h"
#include "lib_grid/lg_base.h"
#ifdef UG_PARALLEL
	#include "lib_grid/parallelization/distributed_grid.h"
#endif
#include "lib_algebra/operator/operator_iterator_interface.h"
#include "lib_algebra/operator/operator_inverse_interface.h"
#include "lib_algebra/operator/operator_interface.h"
#include "lib_algebra/operator/preconditioner/jacobi.h"

// library intern headers
#include "lib_disc/function_spaces/grid_function_util.h"
#include "lib_disc/operator/linear_operator/assembled_linear_operator.h"

namespace ug{

/// geometric multi grid preconditioner
/**
 * This class implements one step of the geometric multi grid as a
 * preconditioner for linear iteration schemes such as linear iteration, cg
 * or bicgstab.
 *
 * The coarse grid spaces are build up according to the Approximation Space
 * that is set from outside. In addition an Assembling routine must be
 * specified that is used to assemble the coarse grid matrices.
 *
 * \tparam		TApproximationSpace		Type of Approximation Space
 * \tparam		TAlgebra				Type of Algebra
 */
template <typename TDomain, typename TAlgebra>
class AssembledMultiGridCycle :
	virtual public ILinearIterator<	typename TAlgebra::vector_type,
									typename TAlgebra::vector_type>
{
	public:
	///	Domain
		typedef TDomain domain_type;

	///	Algebra type
		typedef TAlgebra algebra_type;

	///	type of assembling
		typedef IAssemble<TAlgebra> assemble_type;

	///	Vector type
		typedef typename algebra_type::vector_type vector_type;

	///	Matrix type
		typedef typename algebra_type::matrix_type matrix_type;

	///	Prolongation Operator
		typedef IProlongationOperator<TAlgebra> prolongation_operator_type;

	///	Projection Operator
		typedef IProjectionOperator<vector_type, vector_type> projection_operator_type;

	///	Base Solver type
		typedef ILinearOperatorInverse<vector_type, vector_type> base_solver_type;

	///	Smoother type
		typedef ILinearIterator<vector_type, vector_type> smoother_type;

	///	Own type
		typedef ILinearIterator<vector_type, vector_type> base_type;

	public:
	/// constructor setting approximation space
		AssembledMultiGridCycle(SmartPtr<ApproximationSpace<TDomain> > approxSpace) :
			m_pAss(NULL), m_spApproxSpace(approxSpace),
			m_topLev(0), m_baseLev(0), m_bBaseParallel(true), m_cycleType(1),
			m_numPreSmooth(2), m_numPostSmooth(2),
			m_bAdaptive(true),
			m_spSmootherPrototype(new Jacobi<TAlgebra>()),
			m_spProjectionPrototype(new P1Projection<TDomain,TAlgebra>(m_spApproxSpace)),
			m_spProlongationPrototype(new P1Prolongation<TDomain,TAlgebra>(m_spApproxSpace)),
			m_NonGhostMarker(*m_spApproxSpace->domain()->grid()),
			m_pBaseSolver(NULL),
			m_spDebugWriter(NULL), m_dbgIterCnt(0)
		{};

	///////////////////////////////////////////////////////////////////////////
	//	Setup
	///////////////////////////////////////////////////////////////////////////

	/// sets the assembling procedure that is used to compute coarse grid matrices
		void set_discretization(assemble_type& ass)
			{m_pAss = &ass;}

	///	sets the level where exact solving is performed in the mg cycle
		void set_base_level(int baseLevel) {m_baseLev = baseLevel;}

	///	sets the base solver that is used
		void set_base_solver(base_solver_type& baseSolver)
			{m_pBaseSolver = &baseSolver;}

	///	sets if the base solver is applied in parallel
		void set_parallel_base_solver(bool bParallel) {m_bBaseParallel = bParallel;}

	///	sets the cycle type (1 = V-cycle, 2 = W-cycle, ...)
		void set_cycle_type(int type) {m_cycleType = type;}

	///	sets the number of pre-smoothing steps to be performed
		void set_num_presmooth(int num) {m_numPreSmooth = num;}

	///	sets the number of post-smoothing steps to be performed
		void set_num_postsmooth(int num) {m_numPostSmooth = num;}

	///	sets the smoother that is used
		void set_smoother(SmartPtr<smoother_type> smoother)
			{m_spSmootherPrototype = smoother;}

	///	sets the prolongation operator
		void set_prolongation_operator(SmartPtr<prolongation_operator_type> P)
			{m_spProlongationPrototype = P;}

	///	sets the projection operator
		void set_projection_operator(SmartPtr<IProjectionOperator<vector_type, vector_type> > P)
			{m_spProjectionPrototype = P;}

	///////////////////////////////////////////////////////////////////////////
	//	Linear Solver interface methods
	///////////////////////////////////////////////////////////////////////////

	///	name
		virtual const char* name() const {return "Geometric MultiGrid";}

	/// Prepare for Operator J(u) and linearization point u (current solution)
		virtual bool init(ILinearOperator<vector_type, vector_type>& J, const vector_type& u);

	///	Prepare for Linear Operartor L
		virtual bool init(ILinearOperator<vector_type, vector_type>& L);

	///	Compute new correction c = B*d
		virtual bool apply(vector_type& c, const vector_type& d);

	///	Compute new correction c = B*d and return new defect d := d - A*c
		virtual bool apply_update_defect(vector_type& c, vector_type& d);

	///	Clone
		base_type* clone();

	///	Destructor
		~AssembledMultiGridCycle() {top_level_required(0);};

 	protected:
 	/// compute correction on level and update defect
		bool lmgc(size_t lev);

	////////////////////////////////////////////////////////////////
	//	The methods in this section rely on each other and should be called in sequence
	///	performs presmoothing on the given level
		bool presmooth(size_t lev);

	///	performs restriction on to the level below
	/**	Pass a pointer to a bool variable to restrictionPerformedOut, to find out,
	 * whether the restriction was performed. The restriction
	 * won't be performed, if no DoFs are in the level below.*/
		bool restriction(size_t lev, bool* restrictionPerformedOut);

	///	performs prolongation to the level above
	/**	We have to let prolongaton know whether the previous restriction
	 * was performed.*/
		bool prolongation(size_t lev, bool resumingFromBelow);

	///	performs postsmoothin
		bool postsmooth(size_t lev);
	//	end of section
	////////////////////////////////////////////////////////////////

	///	compute base solver
		bool base_solve(size_t lev);

	/// performs smoothing on level l, nu times
		bool smooth(vector_type& c, vector_type& d, vector_type& t,
		            MatrixOperator<vector_type, vector_type, matrix_type>& A,
		            smoother_type& S, size_t lev, int nu);

	///	returns the number of allocated levels
		size_t num_levels() const {return m_vLevData.size();}

	///	allocates the memory
		bool top_level_required(size_t topLevel);

	///	initializes common part
		bool init_common(bool nonlinear);

	///	initializes the smoother and base solver
		bool init_smoother();

	///	initializes the coarse grid matrices for non-linear case
		bool init_non_linear_level_operator();

	///	initializes the coarse grid matrices for linear case
		bool init_linear_level_operator();

	///	initializes the smoother and base solver
		bool init_base_solver();

	///	initializes the prolongation
		bool init_prolongation();

	///	initializes the prolongation
		bool init_projection();

	///	projects a grid function from the surface to the levels
		bool project_surface_to_level(std::vector<vector_type*> vLevelFunc,
		                              const vector_type& surfFunc);

	///	projects a grid function from the levels to the surface
		bool project_level_to_surface(vector_type& surfFunc,
		                              std::vector<const vector_type*> vLevelFunc);

	///	assembles the missing matrix part on the coarse level, that must be
	///	added if the correction has been computed to ensure a correctly updated
	///	defect. (i.e. assembles A^c, with d^f -= A^c * c^c)
		bool init_missing_coarse_grid_coupling(const vector_type* u);

	///	checks if all necessary pointers have been set
		bool check_setting() const;

	protected:
	/// operator to invert (surface grid)
		matrix_type* m_pSurfaceMat;

	///	assembling routine for coarse grid matrices
		assemble_type* m_pAss;

	///	approximation space for level and surface grid
		SmartPtr<ApproximationSpace<TDomain> > m_spApproxSpace;

	///	top level (i.e. highest level in hierarchy. This is the surface level
	///	in case of non-adaptive refinement)
		size_t m_topLev;

	///	base level (where exact inverse is computed)
		size_t m_baseLev;

	///	flag, if to solve base problem in parallel
		bool m_bBaseParallel;

	///	cylce type (1 = V-cycle, 2 = W-cylcle, ...)
		int m_cycleType;

	///	number of Presmooth steps
		int m_numPreSmooth;

	///	number of Postsmooth steps
		int m_numPostSmooth;

	///	flag indicating if grid is full refined
		bool m_bAdaptive;

	///	mapping from surface to top level (only valid in case of full refinement)
		std::vector<size_t> m_vSurfToTopMap;

	///	prototype for smoother
		SmartPtr<smoother_type> m_spSmootherPrototype;

	///	prototype for projection operator
		SmartPtr<projection_operator_type> m_spProjectionPrototype;

	///	prototype for prolongation operator
		SmartPtr<prolongation_operator_type> m_spProlongationPrototype;

		////////////////////////////////////
		// Storage for each grid level
		////////////////////////////////////

		struct LevData
		{
		//	constructor
			LevData() : spLevDD(0), Smoother(0), Projection(0), Prolongation(0){};

		//	prepares the grid level data for appication
			void update(size_t lev,
			            SmartPtr<LevelDoFDistribution> levelDD,
			            SmartPtr<ApproximationSpace<TDomain> > approxSpace,
			            assemble_type& ass,
			            smoother_type& smoother,
			            projection_operator_type& projection,
			            prolongation_operator_type& prolongation,
			            BoolMarker& nonGhostMarker);

		//	returns if ghosts are present on the level
			bool has_ghosts() const {return num_smooth_indices() != num_indices();}

		//	number of smoothing indices
			size_t num_smooth_indices() const {return m_numSmoothIndices;}

		//	number of indices on whole level
			size_t num_indices() const
				{UG_ASSERT(spLevDD.valid(), "Missing LevDD"); return spLevDD->num_indices();}

		//	returns the smoothing matrix (depends if smooth patch needed or not)
			MatrixOperator<vector_type, vector_type, matrix_type>&
			get_smooth_mat()
			{
				if(has_ghosts()) return SmoothMat;
				else return LevMat;
			}

		//	returns the smoother
			smoother_type& get_smoother() {return *Smoother;}

		//	returns the vectors used for smoothing (patch only vectors)
			vector_type& get_smooth_solution() {if(has_ghosts()) return su; else return u;}
			vector_type& get_smooth_defect() {if(has_ghosts()) return sd; else return d;}
			vector_type& get_smooth_correction(){if(has_ghosts()) return sc; else return c;}
			vector_type& get_smooth_tmp(){if(has_ghosts()) return st; else return t;}

		//	copies values of defect to smoothing patch
			void copy_defect_to_smooth_patch()
			{
				#ifdef UG_PARALLEL
				if(has_ghosts()) {
					for(size_t i = 0; i < vMap.size(); ++i) sd[i] = d[ vMap[i] ];
					sd.copy_storage_type(d);
				}
				#endif
			}

		//	copies values of tmp to smoothing patch
			void copy_tmp_to_smooth_patch()
			{
				#ifdef UG_PARALLEL
				if(has_ghosts()) {
					for(size_t i = 0; i < vMap.size(); ++i) st[i] = t[ vMap[i] ];
					st.copy_storage_type(t);
				}
				#endif
			}

		//	copies values of defect from smoothing patch
			void copy_defect_from_smooth_patch(bool clearGhosts = false)
			{
				#ifdef UG_PARALLEL
				if(has_ghosts()) {
					if(clearGhosts) d.set(0.0);
					for(size_t i = 0; i < vMap.size(); ++i) d[ vMap[i] ] = sd[i];
					d.copy_storage_type(sd);
				}
				#endif
			}

		//	copies values of defect to smoothing patch
			void copy_correction_from_smooth_patch(bool clearGhosts = false)
			{
				#ifdef UG_PARALLEL
				if(has_ghosts()) {
					if(clearGhosts) c.set(0.0);
					for(size_t i = 0; i < vMap.size(); ++i) c[ vMap[i] ] = sc[i];
					c.copy_storage_type(sc);
				}
				#endif
			}

		//	destructor
			~LevData();

			public:
		//	level DoF Distribution
			SmartPtr<LevelDoFDistribution> spLevDD;

		//	Approximation Space
			SmartPtr<ApproximationSpace<TDomain> > m_spApproxSpace;

		//	matrix operator for whole grid level
			MatrixOperator<vector_type, vector_type, matrix_type> LevMat;

		//	matrix for smoothing on smoothing patch of grid level
			MatrixOperator<vector_type, vector_type, matrix_type> SmoothMat;

		//	smoother
			smoother_type* Smoother;

		//	projection operator
			projection_operator_type* Projection;

		//	prolongation operator
			prolongation_operator_type* Prolongation;

		//	vectors needed
			vector_type u, c, d, t;

		//	vectors needed for smoothing
			vector_type su, sc, sd, st;

		//	missing coarse grid correction
			matrix_type CoarseGridContribution;

		//	map for smoothing
			std::vector<size_t> vMap;
			std::vector<int> vMapFlag;

		//	number of smooth indices
			size_t m_numSmoothIndices;

#ifdef UG_PARALLEL
		//	interfaces needed for smoothing
			IndexLayout SmoothMasterLayout, SmoothSlaveLayout;
#endif
		};

	///	storage for all level
		std::vector<LevData*> m_vLevData;

	///	bool marker of non-ghosts
		BoolMarker m_NonGhostMarker;

	///	base solver for the coarse problem
		base_solver_type* m_pBaseSolver;

		std::vector<vector_type*> level_defects()
		{
			std::vector<vector_type*> vVec;
			for(size_t i = 0; i < m_vLevData.size(); ++i)
			{
				if(m_vLevData[i]->num_smooth_indices() > 0)
					vVec.push_back(&m_vLevData[i]->d);
				else vVec.push_back(NULL);
			}
			return vVec;
		}

		std::vector<const vector_type*> const_level_defects() const
		{
			std::vector<const vector_type*> vVec;
			for(size_t i = 0; i < m_vLevData.size(); ++i)
			{
				if(m_vLevData[i]->num_smooth_indices() > 0)
					vVec.push_back(&m_vLevData[i]->d);
				else vVec.push_back(NULL);
			}
			return vVec;
		}

		std::vector<vector_type*> level_corrections()
		{
			std::vector<vector_type*> vVec;
			for(size_t i = 0; i < m_vLevData.size(); ++i)
			{
				if(m_vLevData[i]->num_smooth_indices() > 0)
					vVec.push_back(&m_vLevData[i]->c);
				else vVec.push_back(NULL);
			}
			return vVec;
		}

		std::vector<const vector_type*> const_level_corrections() const
		{
			std::vector<const vector_type*> vVec;
			for(size_t i = 0; i < m_vLevData.size(); ++i)
			{
				if(m_vLevData[i]->num_smooth_indices() > 0)
					vVec.push_back(&m_vLevData[i]->c);
				else vVec.push_back(NULL);
			}
			return vVec;
		}

		std::vector<vector_type*> level_solutions()
		{
			std::vector<vector_type*> vVec;
			for(size_t i = 0; i < m_vLevData.size(); ++i)
			{
				if(m_vLevData[i]->num_smooth_indices() > 0)
					vVec.push_back(&m_vLevData[i]->u);
				else vVec.push_back(NULL);
			}
			return vVec;
		}

#ifdef UG_PARALLEL
	/**
	 *	gathers the vector using vertical interfaces, returns if this proc
	 *  will still have dofs on the next level (iff has no vertical slaves)
	 */
		bool gather_vertical(vector_type& d);

	/**
	 *	broadcasts the vector using vertical interfaces.
	 */
		void broadcast_vertical(vector_type& t);

	/// communicator
		pcl::InterfaceCommunicator<IndexLayout> m_Com;
#endif

	public:
	///	set debug output
	/**
	 * If a DebugWriter is passed by this method, the multi grid cycle writes
	 * the level/surface vectors and matrices for debug purposes.
	 *
	 * \param[in]	debugWriter		Debug Writer to use
	 */
		void set_debug(SmartPtr<IDebugWriter<algebra_type> > spDebugWriter)
		{
			m_spDebugWriter = spDebugWriter;
		}

	protected:
	///	writes debug output for a level vector
	/**
	 * This method writes the level vector to a debug file, if a debug writer
	 * has been set.
	 *
	 * \param[in]		vec			Level Vector to write for debug purpose
	 * \param[in]		filename	Filename
	 * \param[in]		level		grid level corresponding to the vector
	 */
		void write_level_debug(const vector_type& vec, const char* filename, size_t lev);

	///	writes debug output for a level matrix
	/**
	 * This method writes the level matrix to a debug file, if a debug writer
	 * has been set.
	 *
	 * \param[in]		mat			Level Matrix to write for debug purpose
	 * \param[in]		filename	Filename
	 * \param[in]		level		grid level corresponding to the matrix
	 */
		void write_level_debug(const matrix_type& mat, const char* filename, size_t lev);

	///	writes debug output for a surface vector
	/**
	 * This method writes the surface vector to a debug file, if a debug writer
	 * has been set.
	 *
	 * \param[in]		vec			Level Vector to write for debug purpose
	 * \param[in]		filename	Filename
	 */
		void write_surface_debug(const vector_type& vec, const char* filename);

	///	writes debug output for a surface matrix
	/**
	 * This method writes the surface matrix to a debug file, if a debug writer
	 * has been set.
	 *
	 * \param[in]		mat			Level Matrix to write for debug purpose
	 * \param[in]		filename	Filename
	 */
		void write_surface_debug(const matrix_type& mat, const char* filename);

	///	logs a level-data-struct to the terminal
		void log_level_data(size_t lvl);

	///	Debug Writer
		SmartPtr<IDebugWriter<algebra_type> > m_spDebugWriter;

	///	counter for debug, to distinguish the iterations
		int m_dbgIterCnt;
};

} // end namespace ug

// include implementation
#include "mg_solver_impl.hpp"

#endif /* __H__UG__LIB_DISC__MULTI_GRID_SOLVER__MG_SOLVER__ */
