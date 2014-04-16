// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This package aids in the testing of code that uses channels.
package script

import (
	"fmt"
	"os"
	"rand"
	"reflect"
	"strings"
)

// An Event is an element in a partially ordered set that either sends a value
// to a channel or expects a value from a channel.
type Event struct {
	name         string
	occurred     bool
	predecessors []*Event
	action       action
}

type action interface {
	// getSend returns nil if the action is not a send action.
	getSend() sendAction
	// getRecv returns nil if the action is not a receive action.
	getRecv() recvAction
	// getChannel returns the channel that the action operates on.
	getChannel() interface{}
}

type recvAction interface {
	recvMatch(interface{}) bool
}

type sendAction interface {
	send()
}

// isReady returns true if all the predecessors of an Event have occurred.
func (e Event) isReady() bool {
	for _, predecessor := range e.predecessors {
		if !predecessor.occurred {
			return false
		}
	}

	return true
}

// A Recv action reads a value from a channel and uses reflect.DeepMatch to
// compare it with an expected value.
type Recv struct {
	Channel  interface{}
	Expected interface{}
}

func (r Recv) getRecv() recvAction { return r }

func (Recv) getSend() sendAction { return nil }

func (r Recv) getChannel() interface{} { return r.Channel }

func (r Recv) recvMatch(chanEvent interface{}) bool {
	c, ok := chanEvent.(channelRecv)
	if !ok || c.channel != r.Channel {
		return false
	}

	return reflect.DeepEqual(c.value, r.Expected)
}

// A RecvMatch action reads a value from a channel and calls a function to
// determine if the value matches.
type RecvMatch struct {
	Channel interface{}
	Match   func(interface{}) bool
}

func (r RecvMatch) getRecv() recvAction { return r }

func (RecvMatch) getSend() sendAction { return nil }

func (r RecvMatch) getChannel() interface{} { return r.Channel }

func (r RecvMatch) recvMatch(chanEvent interface{}) bool {
	c, ok := chanEvent.(channelRecv)
	if !ok || c.channel != r.Channel {
		return false
	}

	return r.Match(c.value)
}

// A Closed action matches if the given channel is closed. The closing is
// treated as an event, not a state, thus Closed will only match once for a
// given channel.
type Closed struct {
	Channel interface{}
}

func (r Closed) getRecv() recvAction { return r }

func (Closed) getSend() sendAction { return nil }

func (r Closed) getChannel() interface{} { return r.Channel }

func (r Closed) recvMatch(chanEvent interface{}) bool {
	c, ok := chanEvent.(channelClosed)
	if !ok || c.channel != r.Channel {
		return false
	}

	return true
}

// A Send action sends a value to a channel. The value must match the
// type of the channel exactly unless the channel if of type chan interface{}.
type Send struct {
	Channel interface{}
	Value   interface{}
}

func (Send) getRecv() recvAction { return nil }

func (s Send) getSend() sendAction { return s }

func (s Send) getChannel() interface{} { return s.Channel }

type empty struct {
	x interface{}
}

func newEmptyInterface(e empty) reflect.Value {
	return reflect.NewValue(e).(*reflect.StructValue).Field(0)
}

func (s Send) send() {
	// With reflect.ChanValue.Send, we must match the types exactly. So, if
	// s.Channel is a chan interface{} we convert s.Value to an interface{}
	// first.
	c := reflect.NewValue(s.Channel).(*reflect.ChanValue)
	var v reflect.Value
	if iface, ok := c.Type().(*reflect.ChanType).Elem().(*reflect.InterfaceType); ok && iface.NumMethod() == 0 {
		v = newEmptyInterface(empty{s.Value})
	} else {
		v = reflect.NewValue(s.Value)
	}
	c.Send(v)
}

// A Close action closes the given channel.
type Close struct {
	Channel interface{}
}

func (Close) getRecv() recvAction { return nil }

func (s Close) getSend() sendAction { return s }

func (s Close) getChannel() interface{} { return s.Channel }

func (s Close) send() { reflect.NewValue(s.Channel).(*reflect.ChanValue).Close() }

// A ReceivedUnexpected error results if no active Events match a value
// received from a channel.
type ReceivedUnexpected struct {
	Value interface{}
	ready []*Event
}

func (r ReceivedUnexpected) String() string {
	names := make([]string, len(r.ready))
	for i, v := range r.ready {
		names[i] = v.name
	}
	return fmt.Sprintf("received unexpected value on one of the channels: %#v. Runnable events: %s", r.Value, strings.Join(names, ", "))
}

// A SetupError results if there is a error with the configuration of a set of
// Events.
type SetupError string

func (s SetupError) String() string { return string(s) }

func NewEvent(name string, predecessors []*Event, action action) *Event {
	e := &Event{name, false, predecessors, action}
	return e
}

// Given a set of Events, Perform repeatedly iterates over the set and finds the
// subset of ready Events (that is, all of their predecessors have
// occurred). From that subset, it pseudo-randomly selects an Event to perform.
// If the Event is a send event, the send occurs and Perform recalculates the ready
// set. If the event is a receive event, Perform waits for a value from any of the
// channels that are contained in any of the events. That value is then matched
// against the ready events. The first event that matches is considered to
// have occurred and Perform recalculates the ready set.
//
// Perform continues this until all Events have occurred.
//
// Note that uncollected goroutines may still be reading from any of the
// channels read from after Perform returns.
//
// For example, consider the problem of testing a function that reads values on
// one channel and echos them to two output channels. To test this we would
// create three events: a send event and two receive events. Each of the
// receive events must list the send event as a predecessor but there is no
// ordering between the receive events.
//
//  send := NewEvent("send", nil, Send{c, 1})
//  recv1 := NewEvent("recv 1", []*Event{send}, Recv{c, 1})
//  recv2 := NewEvent("recv 2", []*Event{send}, Recv{c, 1})
//  Perform(0, []*Event{send, recv1, recv2})
//
// At first, only the send event would be in the ready set and thus Perform will
// send a value to the input channel. Now the two receive events are ready and
// Perform will match each of them against the values read from the output channels.
//
// It would be invalid to list one of the receive events as a predecessor of
// the other. At each receive step, all the receive channels are considered,
// thus Perform may see a value from a channel that is not in the current ready
// set and fail.
func Perform(seed int64, events []*Event) (err os.Error) {
	r := rand.New(rand.NewSource(seed))

	channels, err := getChannels(events)
	if err != nil {
		return
	}
	multiplex := make(chan interface{})
	for _, channel := range channels {
		go recvValues(multiplex, channel)
	}

Outer:
	for {
		ready, err := readyEvents(events)
		if err != nil {
			return err
		}

		if len(ready) == 0 {
			// All events occurred.
			break
		}

		event := ready[r.Intn(len(ready))]
		if send := event.action.getSend(); send != nil {
			send.send()
			event.occurred = true
			continue
		}

		v := <-multiplex
		for _, event := range ready {
			if recv := event.action.getRecv(); recv != nil && recv.recvMatch(v) {
				event.occurred = true
				continue Outer
			}
		}

		return ReceivedUnexpected{v, ready}
	}

	return nil
}

// getChannels returns all the channels listed in any receive events.
func getChannels(events []*Event) ([]interface{}, os.Error) {
	channels := make([]interface{}, len(events))

	j := 0
	for _, event := range events {
		if recv := event.action.getRecv(); recv == nil {
			continue
		}
		c := event.action.getChannel()
		if _, ok := reflect.NewValue(c).(*reflect.ChanValue); !ok {
			return nil, SetupError("one of the channel values is not a channel")
		}

		duplicate := false
		for _, other := range channels[0:j] {
			if c == other {
				duplicate = true
				break
			}
		}

		if !duplicate {
			channels[j] = c
			j++
		}
	}

	return channels[0:j], nil
}

// recvValues is a multiplexing helper function. It reads values from the given
// channel repeatedly, wrapping them up as either a channelRecv or
// channelClosed structure, and forwards them to the multiplex channel.
func recvValues(multiplex chan<- interface{}, channel interface{}) {
	c := reflect.NewValue(channel).(*reflect.ChanValue)

	for {
		v := c.Recv()
		if c.Closed() {
			multiplex <- channelClosed{channel}
			return
		}

		multiplex <- channelRecv{channel, v.Interface()}
	}
}

type channelClosed struct {
	channel interface{}
}

type channelRecv struct {
	channel interface{}
	value   interface{}
}

// readyEvents returns the subset of events that are ready.
func readyEvents(events []*Event) ([]*Event, os.Error) {
	ready := make([]*Event, len(events))

	j := 0
	eventsWaiting := false
	for _, event := range events {
		if event.occurred {
			continue
		}

		eventsWaiting = true
		if event.isReady() {
			ready[j] = event
			j++
		}
	}

	if j == 0 && eventsWaiting {
		names := make([]string, len(events))
		for _, event := range events {
			if event.occurred {
				continue
			}
			names[j] = event.name
		}

		return nil, SetupError("dependency cycle in events. These events are waiting to run but cannot: " + strings.Join(names, ", "))
	}

	return ready[0:j], nil
}
