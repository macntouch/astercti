#ifndef _CHAN_EVENTS_H_
#define _CHAN_EVENTS_H_

#include "asterisk.h"
#include "observer.h"

/*class ChannelEvent
{
	enum ast_channel_state m_state;
	std::string m_ChannelID;
	std::string m_Channel;
	std::string m_CallerIDNum;
	std::string m_CallerIDName;
	std::string m_peerChannel;
};*/

class EventListener;

// Takes AmiMessage, calls corresponding method of listeners
class EventGenerator : public IObserver
{
	std::list<EventListener *> _listeners;
	void handleEvent(const AmiMessage &);
public:
	void add(EventListener &);
	void remove(EventListener &);
	void NotifyOnRing(const AmiMessage &);
	void NotifyOnOriginate(const AmiMessage &);
	void NotifyOnDial(const AmiMessage &);
	void NotifyOnUp(const AmiMessage &);
	void NotifyOnHangup(const AmiMessage &);
	void NotifyOnCdr(const AmiMessage &);
};

class EventListener
{
public:
	virtual void OnRing(const AmiMessage &);
	virtual void OnOriginate(const AmiMessage &);
	virtual void OnDial(const AmiMessage &);
	virtual void OnUp(const AmiMessage &);
	virtual void OnHangup(const AmiMessage &);
	virtual void OnCdr(const AmiMessage &);

};

#endif
