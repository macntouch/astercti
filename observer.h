#ifndef _OBSERVER_H_
#define _OBSERVER_H_

#include <map>
#include <list>
#include "amimessage.h"

class IObservable;
class IObserver
{
private:
    std::list<IObservable *> m_observables;
public:
    std::string observer_descr;
    IObserver();
    ~IObserver();
    void listens_to(IObservable &);
    virtual void handleEvent(const AmiMessage&) = 0;
};

class IObservable
{
	std::list<IObserver *> _observers;
public:
    std::string observable_descr;
	void add(IObserver &observer);
	void remove(IObserver &observer);
	void Notify(const AmiMessage &message);
};

class AmiMessageFilter : public IObserver, public IObservable
{
	virtual bool filter(const AmiMessage &message) = 0;
	void handleEvent(const AmiMessage& message);
};

class MyChanFilter : public AmiMessageFilter
{
	std::string m_channel_id;
public:
	MyChanFilter(std::string channel);
	bool filter(const AmiMessage &message);
	//void virtual OnCdr(const AmiMessage &message) = 0;
};

class InternalMessageFilter : public AmiMessageFilter
{
public:
	bool filter(const AmiMessage &message);
};

#endif
