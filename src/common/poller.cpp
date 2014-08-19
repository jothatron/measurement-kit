/*-
 * This file is part of Libight <https://libight.github.io/>.
 *
 * Libight is free software. See AUTHORS and LICENSE for more
 * information on the copying conditions.
 */

#include <string.h>
#include <stdexcept>
#include <stdlib.h>

#ifndef WIN32
# include <signal.h>
#endif

#include <event2/event.h>
#include <event2/dns.h>

#include "net/ll2sock.h"

#include "common/poller.h"
#include "common/utils.h"
#include "common/log.h"

/*
 * IghtDelayedCall implementation
 */

IghtDelayedCall::IghtDelayedCall(double t, std::function<void(void)> &&f)
{
	timeval timeo;

	this->func = new std::function<void(void)>();

	if ((this->evp = event_new(ight_get_global_event_base(),
	    IGHT_SOCKET_INVALID, EV_TIMEOUT, this->dispatch,
	    this->func)) == NULL) {
		delete (this->func);
		throw std::bad_alloc();
	}

	if (event_add(this->evp, ight_timeval_init(&timeo, t)) != 0) {
		delete (this->func);
		event_free(this->evp);
		throw std::runtime_error("cannot register new event");
	}

	std::swap(*this->func, f);
}

void
IghtDelayedCall::dispatch(evutil_socket_t socket, short event, void *opaque)
{
	auto funcptr = static_cast<std::function<void(void)> *>(opaque);
	if (*funcptr)
		(*funcptr)();

	// Avoid compiler warning
	(void) socket;
	(void) event;
}

IghtDelayedCall::~IghtDelayedCall(void)
{
	delete (this->func);  /* delete handles NULL */
	if (this->evp)
		event_free(this->evp);
}

/*
 * IghtPoller implementation
 */

#ifndef WIN32
static void
IghtPoller_sigint(int signo, short event, void *opaque)
{
	(void) signo;
	(void) event;

	auto self = (IghtPoller *) opaque;
	self->break_loop();
}
#endif

IghtPoller::IghtPoller(void)
{
	if ((this->base = event_base_new()) == NULL) {
		throw std::bad_alloc();
	}

	if ((this->dnsbase = evdns_base_new(this->base, 1)) == NULL) {
		event_base_free(this->base);
		throw std::bad_alloc();
	}

#ifndef WIN32
	/*
	 * Note: The move semantic is incompatible with this object
	 * because we pass `this` to `event_new()`.
	 */
	if ((this->evsignal = event_new(this->base, SIGINT, EV_SIGNAL,
	    IghtPoller_sigint, this)) == NULL) {
		evdns_base_free(this->dnsbase, 1);
		event_base_free(this->base);
		throw std::bad_alloc();
	}
#endif
}

IghtPoller::~IghtPoller(void)
{
	event_free(this->evsignal);
	evdns_base_free(this->dnsbase, 1);
	event_base_free(this->base);
}

void
IghtPoller::break_loop_on_sigint_(int enable)
{
#ifndef WIN32
	/*
	 * XXX There is a comment in libevent/signal.c saying that, for
	 * historical reasons, "only one event base can be set up to use
	 * [signals] at a time", unless kqueue is used as backend.
	 *
	 * We don't plan to use multiple pollers and this function is not
	 * meant to be used by the real Android application; yet, it is
	 * good to remember that this limitation exists.
	 */
	if (enable) {
		if (event_add(this->evsignal, NULL) != 0)
			throw std::runtime_error("cannot add SIGINT event");
	} else {
		if (event_del(this->evsignal) != 0)
			throw std::runtime_error("cannot del SIGINT event");
	}
#endif
}

void
IghtPoller::loop(void)
{
	auto result = event_base_dispatch(this->base);
	if (result < 0)
		throw std::runtime_error("event_base_dispatch() failed");
	if (result == 1)
		ight_warn("loop: no pending and/or active events");
}

void
IghtPoller::break_loop(void)
{
	if (event_base_loopbreak(this->base) != 0)
		throw std::runtime_error("event_base_loopbreak() failed");
}

/*
 * API to access/use global objects.
 */

IghtPoller GLOBAL_POLLER;

IghtPoller *
ight_get_global_poller(void)
{
	return (&GLOBAL_POLLER);
}

event_base *
ight_get_global_event_base(void)
{
	return (GLOBAL_POLLER.get_event_base());
}

evdns_base *
ight_get_global_evdns_base(void)
{
	return (GLOBAL_POLLER.get_evdns_base());
}

void
ight_loop(void)
{
	GLOBAL_POLLER.loop();
}

void
ight_break_loop(void)
{
	GLOBAL_POLLER.break_loop();
}
