#ifndef __VALUEITERATORI_H__
#define __VALUEITERATORI_H__

#include "ValueI.h"

BEGIN_NAMESPACE_CHANARCH

//CLASS ValueIteratorI
// Value iterator iterface to be implemented.
// The preferred API is CLASS ValueIterator.
class ValueIteratorI
{
public:
	virtual ~ValueIteratorI ();

	//* Virtuals to be implemented by derived classes:
	virtual bool isValid () const = 0;
	virtual const ValueI * getValue () const = 0;
	virtual bool next () = 0;
	virtual bool prev () = 0;

	//* Number of "similar" values that can be read
	//  as one chunk in a block operation:
	virtual size_t determineChunk (const osiTime &until) = 0;

	//* Sampling period for current values in secs.
	virtual double getPeriod () const = 0;

	// Period could be considered property of Value like CtrlInfo,
	// but unlike CtrlInfo (for formatting), Value doesn't have to know
	// and in case of BinArchive the period information is in fact kept
	// in DataHeader, so BinValue has no easy access to it)

protected:
	ValueIteratorI () {} // cannot be user-created
};

//CLASS ValueIterator
//
// For easy loops a Value Iterator behaves like
// a CLASS ValueI-poiter:
//
//<OL>
//<LI> Bool-casts checks if there are values,
//<LI> Poiter-reference yields Value
//<LI> Increment moves to next Value
//</OL>
//<PRE>	
//	ValueIterator values(archive);
//	while (values)
//	{
//		cout << *values << endl;
//		sum += values->getDouble ();
//		++values;
//	}
//</PRE>
//
// (Implemented as "smart poiter" wrapper for CLASS ValueIteratorI iterface)
class ValueIterator
{
public:
	//* Obtain new, empty ValueIterator suitable for given Archive
	ValueIterator (const class Archive &archive);
	ValueIterator (ValueIteratorI *iter);
	ValueIterator ();

	~ValueIterator ();

	void attach (ValueIteratorI *iter);

	//* Check if iterator is at valid position
	operator bool () const;

	//* Retrieve current CLASS ValueI
	const ValueI * operator -> () const;
	const ValueI & operator * () const;

	//* Move to next/prev Value
	ValueIterator & operator ++ ();
	ValueIterator & operator -- ();

	//* Number of "similar" values that can be read
	// as one chunk in a block operation:
	size_t determineChunk (const osiTime &until);

	//* Sampling period for current values in secs.
	double getPeriod () const;

	// Direct access to iterface
	ValueIteratorI *getI ()				{	return _ptr; }
	const ValueIteratorI *getI () const	{	return _ptr; }

private:
	ValueIterator (const ValueIterator &rhs); // not impl.
	ValueIterator & operator = (const ValueIterator &rhs); // not impl.

	ValueIteratorI *_ptr;
};

inline ValueIterator::ValueIterator (ValueIteratorI *iter)
{	_ptr = iter; }

inline ValueIterator::ValueIterator ()
{	_ptr = 0; }

inline ValueIterator::~ValueIterator ()
{	delete _ptr; }

inline void ValueIterator::attach (ValueIteratorI *iter)
{
	delete _ptr;
	_ptr = iter;
}

inline ValueIterator::operator bool () const
{	return _ptr && _ptr->isValid (); }

inline const ValueI * ValueIterator::operator -> () const
{	return _ptr->getValue (); }

inline const ValueI & ValueIterator::operator * () const
{	return *_ptr->getValue (); }

inline ValueIterator& ValueIterator::operator ++ ()
{	_ptr->next (); return *this; }

inline ValueIterator & ValueIterator::operator -- ()
{	_ptr->prev (); return *this; }

inline size_t ValueIterator::determineChunk (const osiTime &until)
{	return _ptr->determineChunk (until); }

inline double ValueIterator::getPeriod () const
{	return _ptr->getPeriod (); }

END_NAMESPACE_CHANARCH

#endif //__VALUEITERATORI_H__
