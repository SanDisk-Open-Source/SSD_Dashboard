// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package time

import (
	"syscall"
	"sync"
	"os"
)

// BUG(brainman): The Windows implementation assumes that
// this year's rules for daylight savings time apply to all previous
// and future years as well.

// TODO(brainman): use GetDynamicTimeZoneInformation, whenever posible (Vista and up),
// to improve on situation described in the bug above.

type zone struct {
	name                  string
	offset                int
	year                  int64
	month, day, dayofweek int
	hour, minute, second  int
	abssec                int64
	prev                  *zone
}

// Populate zone struct with Windows supplied information. Returns true, if data is valid.
func (z *zone) populate(bias, biasdelta int32, d *syscall.Systemtime, name []uint16) (dateisgood bool) {
	z.name = syscall.UTF16ToString(name)
	z.offset = int(bias)
	z.year = int64(d.Year)
	z.month = int(d.Month)
	z.day = int(d.Day)
	z.dayofweek = int(d.DayOfWeek)
	z.hour = int(d.Hour)
	z.minute = int(d.Minute)
	z.second = int(d.Second)
	dateisgood = d.Month != 0
	if dateisgood {
		z.offset += int(biasdelta)
	}
	z.offset = -z.offset * 60
	return
}

// Pre-calculte cutoff time in seconds since the Unix epoch, if data is supplied in "absolute" format.
func (z *zone) preCalculateAbsSec() {
	if z.year != 0 {
		z.abssec = (&Time{z.year, int(z.month), int(z.day), int(z.hour), int(z.minute), int(z.second), 0, 0, ""}).Seconds()
		// Time given is in "local" time. Adjust it for "utc".
		z.abssec -= int64(z.prev.offset)
	}
}

// Convert zone cutoff time to sec in number of seconds since the Unix epoch, given particualar year.
func (z *zone) cutoffSeconds(year int64) int64 {
	// Windows specifies daylight savings information in "day in month" format:
	// z.month is month number (1-12)
	// z.dayofweek is appropriate weekday (Sunday=0 to Saturday=6)
	// z.day is week within the month (1 to 5, where 5 is last week of the month)
	// z.hour, z.minute and z.second are absolute time
	t := &Time{year, int(z.month), 1, int(z.hour), int(z.minute), int(z.second), 0, 0, ""}
	t = SecondsToUTC(t.Seconds())
	i := int(z.dayofweek) - t.Weekday
	if i < 0 {
		i += 7
	}
	t.Day += i
	if week := int(z.day) - 1; week < 4 {
		t.Day += week * 7
	} else {
		// "Last" instance of the day.
		t.Day += 4 * 7
		if t.Day > months(year)[t.Month] {
			t.Day -= 7
		}
	}
	// Result is in "local" time. Adjust it for "utc".
	return t.Seconds() - int64(z.prev.offset)
}

// Is t before the cutoff for switching to z?
func (z *zone) isBeforeCutoff(t *Time) bool {
	var coff int64
	if z.year == 0 {
		// "day in month" format used
		coff = z.cutoffSeconds(t.Year)
	} else {
		// "absolute" format used
		coff = z.abssec
	}
	return t.Seconds() < coff
}

type zoneinfo struct {
	disabled         bool // daylight saving time is not used localy
	offsetIfDisabled int
	januaryIsStd     bool // is january 1 standard time?
	std, dst         zone
}

// Pick zone (std or dst) t time belongs to.
func (zi *zoneinfo) pickZone(t *Time) *zone {
	z := &zi.std
	if tz.januaryIsStd {
		if !zi.dst.isBeforeCutoff(t) && zi.std.isBeforeCutoff(t) {
			// after switch to daylight time and before the switch back to standard
			z = &zi.dst
		}
	} else {
		if zi.std.isBeforeCutoff(t) || !zi.dst.isBeforeCutoff(t) {
			// before switch to standard time or after the switch back to daylight
			z = &zi.dst
		}
	}
	return z
}

var tz zoneinfo
var initError os.Error
var onceSetupZone sync.Once

func setupZone() {
	var i syscall.Timezoneinformation
	if _, e := syscall.GetTimeZoneInformation(&i); e != 0 {
		initError = os.NewSyscallError("GetTimeZoneInformation", e)
		return
	}
	if !tz.std.populate(i.Bias, i.StandardBias, &i.StandardDate, i.StandardName[0:]) {
		tz.disabled = true
		tz.offsetIfDisabled = tz.std.offset
		return
	}
	tz.std.prev = &tz.dst
	tz.dst.populate(i.Bias, i.DaylightBias, &i.DaylightDate, i.DaylightName[0:])
	tz.dst.prev = &tz.std
	tz.std.preCalculateAbsSec()
	tz.dst.preCalculateAbsSec()
	// Is january 1 standard time this year?
	t := UTC()
	tz.januaryIsStd = tz.dst.cutoffSeconds(t.Year) < tz.std.cutoffSeconds(t.Year)
}

// Look up the correct time zone (daylight savings or not) for the given unix time, in the current location.
func lookupTimezone(sec int64) (zone string, offset int) {
	onceSetupZone.Do(setupZone)
	if initError != nil {
		return "", 0
	}
	if tz.disabled {
		return "", tz.offsetIfDisabled
	}
	t := SecondsToUTC(sec)
	z := &tz.std
	if tz.std.year == 0 {
		// "day in month" format used
		z = tz.pickZone(t)
	} else {
		// "absolute" format used
		if tz.std.year == t.Year {
			// we have rule for the year in question
			z = tz.pickZone(t)
		} else {
			// we do not have any information for that year,
			// will assume standard offset all year around
		}
	}
	return z.name, z.offset
}

// lookupByName returns the time offset for the
// time zone with the given abbreviation. It only considers
// time zones that apply to the current system.
func lookupByName(name string) (off int, found bool) {
	onceSetupZone.Do(setupZone)
	if initError != nil {
		return 0, false
	}
	if tz.disabled {
		return tz.offsetIfDisabled, false
	}
	switch name {
	case tz.std.name:
		return tz.std.offset, true
	case tz.dst.name:
		return tz.dst.offset, true
	}
	return 0, false
}
