#ifndef __CHANNELI_H__
#define __CHANNELI_H__

#include "ValueIteratorI.h"

BEGIN_NAMESPACE_CHANARCH

//////////////////////////////////////////////////////////////////////
//CLASS ChannelI
//
// The Channel interface gives access to the name and basic statistical
// information for an archived channel.
//
// The Channel does not know about the type of
// archived values since those may in fact
// change.
class ChannelI
{
public:
	virtual ~ChannelI ();

	//* Name of this channel
	virtual const char *getName () const = 0;

	//* Time when Channel was added to archive.
	virtual osiTime getCreateTime () const = 0;

	//* Time when first value was written.
	virtual osiTime getFirstTime ()  const = 0;

	//* Time when last value was written.
	virtual osiTime getLastTime ()   const = 0;

	//* Move CLASS ValueIterator for current Channel
	// to first, last, ... value
	bool getFirstValue (ValueIterator &values);
	bool getLastValue (ValueIterator &values);
	virtual bool getFirstValue (ValueIteratorI *values) = 0;
	virtual bool getLastValue (ValueIteratorI *values) = 0;

	//* Get value stamped >= time. time==0 results in call to getFirstValue
	virtual bool getValueAfterTime (const osiTime &time, ValueIteratorI *values) = 0;
	bool getValueAfterTime (const osiTime &time, ValueIterator &values);

	//* Get value stamped <= time
	virtual bool getValueBeforeTime (const osiTime &time, ValueIteratorI *values) = 0;
	virtual bool getValueNearTime (const osiTime &time, ValueIteratorI *values) = 0;
	bool getValueBeforeTime (const osiTime &time, ValueIterator &values);
	bool getValueNearTime (const osiTime &time, ValueIterator &values);

	virtual size_t lockBuffer (const ValueI &value, double period);

	virtual void addBuffer (const ValueI &value_arg, double period, size_t value_count);

	// Add value to last buffer.
	// returns false when buffer is full
	virtual bool addValue (const ValueI &value) = 0;

	// Call after adding all values to that buffer
	virtual void releaseBuffer ();
};

inline bool ChannelI::getFirstValue (ValueIterator &values)
{ return getFirstValue (values.getI()); }

inline bool ChannelI::getLastValue (ValueIterator &values)
{ return getLastValue (values.getI()); }
	
inline bool ChannelI::getValueAfterTime (const osiTime &time, ValueIterator &values)
{ return getValueAfterTime (time, values.getI()); }

inline bool ChannelI::getValueBeforeTime (const osiTime &time, ValueIterator &values)
{ return getValueBeforeTime (time, values.getI()); }

inline bool ChannelI::getValueNearTime (const osiTime &time, ValueIterator &values)
{ return getValueNearTime (time, values.getI()); }

END_NAMESPACE_CHANARCH

#endif //__CHANNELI_H__
