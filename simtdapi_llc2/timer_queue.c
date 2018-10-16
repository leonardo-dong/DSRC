/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University & Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Erik Nordstr�m, <erik.nordstrom@it.uu.se>
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include "timer_queue.h"
#include "list.h"

static LIST(TQ);
static void timer_add(struct timer *t);

int timer_init(struct timer *t, timeout_func_t f, void *data)
{
    if (!t)
	return -1;

    INIT_LIST_ELM(&t->l);
    t->handler = f;
    t->data = data;
    t->timeout.tv_sec = 0;
    t->timeout.tv_usec = 0;
    t->used = 0;

    return 0;
}

/* Called when a timer should timeout */
void timer_timeout(struct timeval *now)
{
    LIST(expTQ);
    list_t *pos, *tmp;

    /* Remove expired timers from TQ and add them to expTQ */
    list_foreach_safe(pos, tmp, &TQ) {
	struct timer *t = (struct timer *) pos;

	if (timeval_diff(&t->timeout, now) > 0)
	    break;

	list_detach(&t->l);
	list_add_tail(&expTQ, &t->l);
    }

    /* Execute expired timers in expTQ safely by removing them at the head */
    while (!list_empty(&expTQ)) {
	struct timer *t = (struct timer *) list_first(&expTQ);
	list_detach(&t->l);
	t->used = 0;
	
	/* Execute handler function for expired timer... */
	if (t->handler) {
	    t->handler(t->data);
	}
    }
}

static void timer_add(struct timer *t)
{
    list_t *pos;
    /* Sanity checks: */

    if (!t) {
		perror("NULL timer!!!\n");
		exit(-1);
    }
    if (!t->handler) {
		perror("NULL handler!!!\n");
		exit(-1);
    }

    /* Make sure we remove unexpired timers before adding a new timeout... */
    if (t->used)
		timer_remove(t);

    t->used = 1;

    /* Base case when queue is empty: */
    if (list_empty(&TQ)) {
		list_add(&TQ, &t->l);
    } else {

	list_foreach(pos, &TQ) {
	    struct timer *curr = (struct timer *) pos;
	    if (timeval_diff(&t->timeout, &curr->timeout) < 0) {
		break;
	    }
	}
		list_add(pos->prev, &t->l);
    }

    return;
}

int timer_remove(struct timer *t)
{
    int res = 1;

    if (!t)
	return -1;


    if (list_unattached(&t->l))
	res = 0;
    else
	list_detach(&t->l);

    t->used = 0;

    return res;
}


int timer_timeout_now(struct timer *t)
{
    if (timer_remove(t)) {

		t->handler(t->data);
		return 1;
    }
    return -1;
}


void timer_set_timeout(struct timer *t, long msec)
{
    if (t->used) {
	timer_remove(t);
    }

    gettimeofday(&t->timeout, NULL);

    t->timeout.tv_usec += msec * 1000;
    t->timeout.tv_sec += t->timeout.tv_usec / 1000000;
    t->timeout.tv_usec = t->timeout.tv_usec % 1000000;

    timer_add(t);
}

long timer_left(struct timer *t)
{
    struct timeval now;

    if (!t)
	return -1;

    gettimeofday(&now, NULL);

    return timeval_diff(&now, &t->timeout);
}

struct timeval * timer_age_queue()
{
    struct timeval now;
    struct timer *t;
    static struct timeval remaining;

    gettimeofday(&now, NULL);

    fflush(stdout);

    if (list_empty(&TQ))
	return NULL;

    timer_timeout(&now);

    /* Check emptyness again since the list might have been updated by a
     * timeout */
    if (list_empty(&TQ))
	return NULL;

    t = (struct timer *) TQ.next;

    remaining.tv_usec = (t->timeout.tv_usec - now.tv_usec);
    remaining.tv_sec = (t->timeout.tv_sec - now.tv_sec);

    if (remaining.tv_usec < 0) {
	remaining.tv_usec += 1000000;
	remaining.tv_sec -= 1;
    }
    return (&remaining);
}

