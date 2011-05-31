//  This file is parsed from UG 3.9.
//  It provides the Gauss Quadratures for a reference pyramid.


#include "../quadrature.h"

namespace ug{

template <>
class GaussQuadrature<ReferencePyramid, 2>
{
	public:
	/// Dimension of integration domain
		static const size_t dim = 3;

	/// Order of quadrature rule
		static const size_t p = 2;

	/// Number of integration points
		static const size_t nip = 8;

	/// Constructor
		GaussQuadrature();

	/// number of integration points
		inline size_t size() const {return nip;}

	/// returns i'th integration point
		inline const MathVector<dim>& point(size_t i) const
			{UG_ASSERT(i < size(), "Wrong index"); return m_vPoint[i];}

	/// returns all positions in an array of size()
		inline const MathVector<dim>* points() const {return m_vPoint;}

	/// return the i'th weight
		inline number weight(size_t i) const
			{UG_ASSERT(i < size(), "Wrong index"); return m_vWeight[i];}

	/// returns all weights in an array of size()
		inline const number* weights() const {return m_vWeight;}

	/// returns the order
		inline size_t order() const {return p;}

	protected:
	/// integration points
		MathVector<dim> m_vPoint[nip];

	/// weights
		number m_vWeight[nip];
};

}; // namespace ug

