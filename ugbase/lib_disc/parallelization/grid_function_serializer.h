// created by Sebastian Reiter
// s.b.reiter@gmail.com
// Feb 27, 2013

#ifndef __H__UG__grid_function_serializer__
#define __H__UG__grid_function_serializer__

#include "lib_grid/algorithms/serialization.h"
#include "lib_grid/algorithms/attachment_util.h"

namespace ug{

template <class TGridFct>
class GridFunctionSerializer : public GridDataSerializer
{
	public:
		static SPGridDataSerializer create(Grid* g, SmartPtr<TGridFct> fct)
		{return SPGridDataSerializer(new GridFunctionSerializer(g, fct));}

		GridFunctionSerializer() : m_grid(NULL)							{}

		GridFunctionSerializer(Grid* g, SmartPtr<TGridFct> fct) :
			m_grid(g), m_fct(fct)	{}

		virtual ~GridFunctionSerializer()				{detach_entries();}

		void set_function(Grid* g, SmartPtr<TGridFct> fct)
		{
			m_grid = g;
			m_fct = fct;
		}

		//virtual void write_info(BinaryBuffer& out) const;

		//virtual void read_info(BinaryBuffer& in);

		virtual void write_data(BinaryBuffer& out, VertexBase* o) const	{write(out, o);}
		virtual void write_data(BinaryBuffer& out, EdgeBase* o) const	{write(out, o);}
		virtual void write_data(BinaryBuffer& out, Face* o) const		{write(out, o);}
		virtual void write_data(BinaryBuffer& out, Volume* o) const		{write(out, o);}

		virtual void read_data(BinaryBuffer& in, VertexBase* o)			{read(in, o);}
		virtual void read_data(BinaryBuffer& in, EdgeBase* o)			{read(in, o);}
		virtual void read_data(BinaryBuffer& in, Face* o)				{read(in, o);}
		virtual void read_data(BinaryBuffer& in, Volume* o)				{read(in, o);}


	///	prepares the attachment into which temporary data will be written
	/**	during calls to read_data, data will be stored in intermediate arrays, since
	 * the size of the new grid (e.g. after distribution) can't be determined until the whole
	 * grid has been deserialized (sometimes multiple grid parts have to be deserialized).*/
		virtual void deserialization_starts()
		{
			attach_entries();
		}

	///	copy values into the actual grid function.
	/**	during calls to read_data, data was stored in intermediate arrays, since
	 * the size of the new grid (e.g. after distribution) can't be determined until the whole
	 * grid has been deserialized (sometimes multiple grid parts have to be deserialized).*/
		virtual void deserialization_done()
		{
			copy_values_to_grid_function<VertexBase>();
			copy_values_to_grid_function<EdgeBase>();
			copy_values_to_grid_function<Face>();
			copy_values_to_grid_function<Volume>();
			detach_entries();
		}

	private:
		typedef typename TGridFct::value_type	value_type;

		struct Entry{
			std::vector<std::pair<bool, value_type> >	values;
		};

		typedef Attachment<Entry>	AEntry;

		void attach_entries()
		{
			UG_ASSERT(m_grid, "Make sure to set a grid before starting deserialization!");
			UG_ASSERT(!m_grid->has_vertex_attachment(m_aEntry), "deserialization_done has to be called"
					" before deserialization is restarted!");

			m_grid->attach_to_all(m_aEntry);
			m_aaEntry.access(*m_grid, m_aEntry);
		}

		void detach_entries()
		{
			if(m_grid && m_grid->has_vertex_attachment(m_aEntry)){
				m_grid->detach_from_all(m_aEntry);
			}
		}

		template <class TElem>
		void copy_values_to_grid_function()
		{
			TGridFct& fct = *m_fct;
			std::vector<size_t>	indices;

			for(typename TGridFct::template traits<TElem>::const_iterator iter = fct.template begin<TElem>();
				iter != fct.template end<TElem>(); ++iter)
			{
				TElem* e = *iter;
				Entry& entry = m_aaEntry[e];

				if(!entry.values.empty()){
					indices.clear();
					fct.inner_algebra_indices(e, indices);

					UG_ASSERT(entry.values.size() == indices.size(), "Wrong number of values given");

					if(entry.values.size() == indices.size()){
						for(size_t i = 0; i < indices.size(); ++i){
							if(entry.values[i].first && (indices[i] < fct.size()))
								fct[indices[i]] = entry.values[i].second;
						}
					}
				}
			}
		}

		template <class TElem>
		void write(BinaryBuffer& out, TElem* e) const
		{
			const char valid = 1;
			const char invalid = 0;

			const TGridFct& fct = *m_fct;
			std::vector<size_t>	indices;
			fct.inner_algebra_indices(e, indices);

			Serialize(out, int(indices.size()));
			for(size_t i = 0; i < indices.size(); ++i){
				if(indices[i] < fct.size()){
					Serialize(out, valid);
					Serialize(out, fct[indices[i]]);
				}
				else
					Serialize(out, invalid);
			}
		}

		template <class TElem>
		void read(BinaryBuffer& in, TElem* e)
		{
			int numVals;
			Deserialize(in, numVals);

			Entry& entry = m_aaEntry[e];
			entry.values.resize(numVals, std::make_pair<bool, value_type>(false, 0));

			for(int i = 0; i < numVals; ++i){
				char isValid;
				Deserialize(in, isValid);
				if(isValid){
					entry.values[i].first = true;
					Deserialize(in, entry.values[i].second);
				}
			}
		}

	private:
		Grid*									m_grid;
		SmartPtr<TGridFct>						m_fct;
		AEntry									m_aEntry;
		MultiElementAttachmentAccessor<AEntry>	m_aaEntry;
};

}// end of namespace

#endif
