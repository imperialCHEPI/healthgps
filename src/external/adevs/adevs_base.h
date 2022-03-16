#ifndef _adevs_model_h_
#define _adevs_model_h_
#include <vector>
#include <exception>
#include <cassert>
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include "adevs_time.h"

/// Define this if you want runtime error checking on your use of the simulator.
#define ADEVS_ERROR_CHECK(cond,model,msg) \
	if (!(cond)) { \
		std::ostringstream buf; \
		buf << __FILE__ << ":" << __LINE__ << ":" << model->name() << ":" << msg; \
		throw std::logic_error(buf.str()); \
	}

namespace adevs
{

template <typename DataType, typename TimeType> class Model;
template <typename DataType, typename TimeType> class Simulator;

/**
 * \brief Interface to a simulation context for
 * sending messages, getting the time, and adding or removing
 * models.
 *
 * The simulation context passed to the Model update() and init()
 * method is the same context to which the Model was added. The
 * first template argument is the data type of a message and the
 * second is the type to use for time.
 */
template <typename DataType, typename TimeType=Time>
class SimEnv
{
	public:
		/**
		 * \brief Send a message to a model.
		 *
		 * Send a message that will be delivered via the destination's
		 * update method at now()+adevs_epsilon().
		 * @param src Who is sending the message?
		 * @param dst Who will receive the message?
		 * @param data The data to transmit */
		virtual void send(
				Model<DataType,TimeType>* src,
				Model<DataType,TimeType> *dst,
				DataType data) = 0;
		/** 
		 * \brief Add a model to the simulator.
		 *
		 * This method causes the new model to be
		 * assigned a state via Model::init(SimEnv*) at time now().
		 * The model being added cannot already be active in the simulation.
		 * @param model The model to add to the simulation */
		virtual void add(Model<DataType,TimeType> *model) = 0;
		/**
		 * \brief Remove a model from a running simulation.
		 *
		 * The model is removed at time now()+adevs_epsilon(). At that
		 * instant its Model::fini() method is called.
		 * @param model The model to remove */
		virtual void remove(Model<DataType,TimeType> *model) = 0;
		/// Get the current time
		virtual TimeType now() = 0;
		/// Destructor
		virtual ~SimEnv(){}
};

/**
 * \brief A model in the simulation.
 * 
 * The template argument is the data type to be communicated between models.
 */
template <typename DataType, typename TimeType=Time>
class Model
{
	public:
		Model():priority(adevs_inf<TimeType>()),heap_pos(0),
				remove(false),msgs(NULL),defer(NULL){}
		/**
		 * \brief Called when the model is added to the simulation.
		 *
		 * The model has been added to a simulation by calling the
		 * SimEnv::add(Model*) method. The effect of calling init()
		 * should be to assign an initial state at time now and return
		 * the time of its event.
		 * @param env The simulation context
		 * @return The time of the next event */
		virtual TimeType init(SimEnv<DataType,TimeType>* env) = 0;
		/** 
		 * \brief Called after the model has been removed from the simulation. 
		 *
		 * The model is being removed from a running simulation and
		 * has ceased to exist as an entity at time now().
		 * @param now The time at which the object no longer exists. */
		virtual void fini(TimeType now) = 0;
		/**
		 * \brief Called to assign a new state to the model at now() when
		 * input is present.
		 *
		 * Assign a state to the model at time now() and return the time of its
         * next new state. Note that the input was generated in the previous
         * simulation instant and so this calculates the new state using
         * state information from the previous instant.
         * @param env The simulation context.
         * @param x Messages sent to the model.
         * @return The time of the next call to update. */
		virtual TimeType update(
				SimEnv<DataType,TimeType> *env, std::vector<DataType> &x) = 0;
		/**
		 * \brief Called to assign a new state to the model at now().
		 *
		 * Assign a state to the model at time now() and return the time of its
         * next new state. An update() will occur at most once per instant 
		 * of simulation time.
         * @param env The simulation context. 
         * @return The time of the next call to update. */
		virtual TimeType update(SimEnv<DataType,TimeType> *env) = 0;
		/**
		 * \brief Relay a message to another model.
		 *
		 * When a message is sent to the model, its relay method is called
		 * immediately. If the method returns NULL then the message is
		 * discarded. If the method returns this then the message is delivered
		 * to this model. Otherwise, relay may return a model that will
		 * receive message as if send(this,relay(),msg) had been called at
		 * at the current time. The relay method will be called repeatedly
		 * with the same source and data until this or NULL is returned.
		 * the default behavior is to return this.
		 * @param src The model that sent this data.
		 * @param x The data.
		 * @return A model and data pair indicating the model to receive
		 * the data and the data it should receive.
		 */
		virtual std::pair<Model<DataType,TimeType>*,DataType>
			relay([[maybe_unused]] Model<DataType,TimeType> *src, DataType x) {
				return std::pair<Model<DataType,TimeType>*,DataType>(this,x);
		}
		/** 
		 * \brief Return a name for this model.
		 
		 * This is intended solely to facility debugging
		 * when ADEVS_ERROR_CHECK is enabled. The default
		 * implementation returns an empty string.
		 * @return A name for the model */
		virtual std::string name() { return ""; }
		/// Destructor 
		virtual ~Model()
		{
			if (msgs != NULL)
				delete msgs;
			if (defer != NULL)
				delete defer;
		}
		/**
		 * \brief Reset simulation support to reuse the model in a new
		 * simulation context.
		 *
		 * Resets the internally used simulation variables as though the
		 * constructor had been called. Do this only if you want to reuse
		 * the model in a second, new simulation. You will probably use
		 * this rarely, if ever.
		 */
		void reset()
		{
			if (msgs != NULL) delete msgs;
			if (defer != NULL ) delete defer;
			msgs = defer = NULL;
			remove = false;
			heap_pos = 0;
			priority = adevs_inf<TimeType>();
		}
	private:
		friend class Simulator<DataType,TimeType>;
		TimeType priority;
		size_t heap_pos;
		bool remove;
		std::vector<DataType> *msgs, *defer;
};


/**
 * \brief The simulator generates trajectories for its models.
 *
 * The template argument is the data type of a message.
 */
template <typename DataType, typename TimeType=Time>
class Simulator: public SimEnv<DataType,TimeType>
{
	public:
		/**
		 * \brief Constructor sets the simulation start time
		 * @param tStart The simulation start time. Default is zero.
		 */
		Simulator(TimeType tStart = adevs_zero<TimeType>());
		/// Destructor leaves all models intact.
		~Simulator();
		/**
		 * \brief Send a message to a model.
		 *
		 * This method is used by model's in the context to
		 * send to messages to each other. It can also
		 * be used to inject data into the simulation.
		 * This latter case can use src = NULL.
		 */
		void send(
				Model<DataType,TimeType> *src,
				Model<DataType,TimeType> *dst,
				DataType data);
		/**
		  * \brief Add a model to the simulation context. 
		  *
		  * This causes Model::init(SimEnv*) to
		  * be called at the current simulation time.
		  */
		void add(Model<DataType,TimeType> *model);
		/**
		  * \brief Remove a model from the simulation context. 
		  *
		  * This causes Model::fini() to
		  * be called at the next instant of simulation time.
		  */
		void remove(Model<DataType,TimeType> *model);
		/// Returns the current simulation time.
		TimeType now() { return m_now; }
		/// Returns the time of the next event.
		TimeType next_event_time() const;
		/// Execute update() methods at the next event time.
		void exec_next_event();
		/// Advance until the simulation time until it equals the supplied time.
		void exec_until(TimeType t);
	private:
		std::vector<std::vector<DataType>* > msg_lists;
		std::vector<Model<DataType,TimeType>* > heap;
		TimeType m_now;
		void percolate_down(Model<DataType,TimeType>* model);
		void percolate_up(Model<DataType,TimeType>* model);
		void percolate_away(Model<DataType,TimeType>* model);
		void percolate_in(Model<DataType,TimeType>* model);
		std::vector<DataType>* get_message_list();
		void replace_message_list(std::vector<DataType>* msgs);
};

template <typename DataType, typename TimeType>
Simulator<DataType,TimeType>::Simulator(TimeType tStart):
	m_now(tStart)
{
	// Put a sentinel at the top of the heap
	heap.push_back(NULL);
}

template <typename DataType, typename TimeType>
TimeType Simulator<DataType,TimeType>::next_event_time() const
{
	if (heap.size() == 1) return adevs_inf<TimeType>();
	return heap[1]->priority;
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::exec_until(TimeType t)
{
	while (next_event_time() <= t && next_event_time() < adevs_inf<TimeType>())
		exec_next_event();
	m_now = t;
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::exec_next_event()
{
	// If the heap is empty then there is nothing to do
	if (heap.size() == 1)
		return;
	// The current time will be the time of the next event
	m_now = heap[1]->priority;
	// Update everyone that is imminent
	while ((heap.size() > 1) && (heap[1]->priority == m_now))
	{
		// Get the model at the top
		Model<DataType,TimeType>* model = heap[1];
		// If this is a model to remove, then chuck it
		if (model->remove)
		{
			percolate_away(model);
			replace_message_list(model->msgs);
			replace_message_list(model->defer);
			model->msgs = model->defer = NULL;
			model->remove = false;
			// Do this last because the model might delete itself
			model->fini(m_now);
		}
		// Update its state and get the time advance
		else
		{
			if (model->msgs != NULL)
			{
				model->priority = model->update(this,*(model->msgs));
				// Time must advance!
				ADEVS_ERROR_CHECK(m_now < model->priority,model,"update(x) <= now()")
			}
			else
			{
				model->priority = model->update(this);
				// Time must advance!
				ADEVS_ERROR_CHECK(m_now < model->priority,model,"update() <= now()")
			}
			// Clear the message list
			replace_message_list(model->msgs);
			// If we have messages for now + adevs_epsilon() get those ready
			model->msgs = model->defer;
			// Now those messages are not defered
			model->defer = NULL;
			// Models to be removed or to receive input go at the next
			// logical instant of time
			if (model->msgs != NULL || model->remove)
				model->priority = m_now + adevs_epsilon<TimeType>();
			assert(m_now < model->priority);
			// Put it into the schedule
			if (model->priority < adevs_inf<TimeType>())
				percolate_down(model);
			else
				percolate_away(model);
		}
	}
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::add(Model<DataType,TimeType>* model)
{
	// Assign the model a state
	TimeType priority = model->init(this);
	ADEVS_ERROR_CHECK(m_now < priority,model,"init() <= now()")
	ADEVS_ERROR_CHECK(
		(model->msgs != NULL && model->priority == m_now+adevs_epsilon<TimeType>()) ||
		model->heap_pos == 0,model,"new model is already active")
	if (model->msgs == NULL)
	{
		model->priority = priority;
		if (model->priority < adevs_inf<TimeType>())
			percolate_in(model);
	}
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::remove(Model<DataType,TimeType>* model)
{
	model->remove = true;
	model->priority = m_now + adevs_epsilon<TimeType>();
	// Put it in the schedule
	if (model->heap_pos == 0)
		percolate_in(model);
	// Or move it up to the next time increment from now
	else
		percolate_up(model);
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::send(
		Model<DataType,TimeType>* src,
		Model<DataType,TimeType>* dst,
		DataType data)
{
	std::pair<Model<DataType,TimeType>*,DataType> r(dst->relay(src,data));
	/* If we are relaying this data to another model */
	while (r.first != dst && r.first != NULL)
	{
		/* Send it to the other model */
		send(dst,r.first,r.second);
		/* See who else we should send it to */
		r = dst->relay(src,data);
	}
	/* If we are done then return */
	if (r.first == NULL) return;
	assert(r.first == dst);
	/* Handle a model that is due to come up */
	if (dst->priority == m_now)
	{
		if (dst->defer == NULL)
			dst->defer = get_message_list();
		dst->defer->push_back(r.second);
	}
	/* Handle a model that won't act until the future */
	else
	{
		if (dst->msgs == NULL)
			dst->msgs = get_message_list();
		dst->msgs->push_back(r.second);
		dst->priority = m_now + adevs_epsilon<TimeType>();
		// Put it in the schedule
		assert(dst->priority < adevs_inf<TimeType>());
		if (dst->heap_pos == 0)
			percolate_in(dst);
		// Or move it up to the next time increment from now
		else
			percolate_up(dst);
	}
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::percolate_up(Model<DataType,TimeType>* model)
{
	assert(model->priority < adevs_inf<TimeType>());
	assert(heap[model->heap_pos] == model);
	size_t index = model->heap_pos;
	/* priority < heap_parent->priority stops percolating up
	 * as soon as possible. */
	while (heap[index/2] != NULL && model->priority < heap[index/2]->priority) 
	{
		heap[index] = heap[index/2];
		heap[index]->heap_pos = index;
		index /= 2;
	}
	heap[index] = model;
	heap[index]->heap_pos = index;
	assert(index != 0);
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::percolate_down(Model<DataType,TimeType>* model)
{
	assert(heap[model->heap_pos] == model);
	size_t child, index = model->heap_pos;
	for (; index*2 < heap.size(); index = child) 
	{
		child = index*2;
		if (child+1 < heap.size() && heap[child+1]->priority < heap[child]->priority)
			child++;
		if (heap[child]->priority < model->priority) 
		{
			heap[index] = heap[child];
			heap[index]->heap_pos = index;
		}
		else break;
	}
	heap[index] = model;
	heap[index]->heap_pos = index;
	assert(index != 0 && index < heap.size());
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::percolate_in(Model<DataType,TimeType>* model)
{
	assert(model->heap_pos == 0);
	model->heap_pos = heap.size();
	heap.push_back(model);
	percolate_up(model);
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::percolate_away(Model<DataType,TimeType>* model)
{
	// Remove the model by plugging the heap hole and then moving the plug
	Model<DataType,TimeType>* move = heap[model->heap_pos] = heap.back();
	move->heap_pos = model->heap_pos;
	heap.pop_back();
	if (heap.size() > 1)
	{
		const size_t parent = move->heap_pos/2;
		if (heap[parent] != NULL &&
				heap[move->heap_pos]->priority < heap[parent]->priority)
			percolate_up(move);
		else
			percolate_down(move);
	}
	model->priority = adevs_inf<TimeType>();
	model->heap_pos = 0;
}

template <typename DataType, typename TimeType>
std::vector<DataType>* Simulator<DataType,TimeType>::get_message_list()
{
	std::vector<DataType>* result;
	if (msg_lists.empty())
		return (result = new std::vector<DataType>());
	result = msg_lists.back();
	msg_lists.pop_back();
	return result;
}

template <typename DataType, typename TimeType>
void Simulator<DataType,TimeType>::replace_message_list(
		std::vector<DataType>* msgs)
{
	if (msgs == NULL) return;
	msgs->clear();
	msg_lists.push_back(msgs);
}

template <typename DataType, typename TimeType>
Simulator<DataType,TimeType>::~Simulator()
{
	for (unsigned i = 0; i < msg_lists.size(); i++)
		delete msg_lists[i];
}

}; // end of namespace

#endif
