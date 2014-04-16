// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// A package of simple functions to manipulate strings.
package strings

import (
	"unicode"
	"utf8"
)

// explode splits s into an array of UTF-8 sequences, one per Unicode character (still strings) up to a maximum of n (n < 0 means no limit).
// Invalid UTF-8 sequences become correct encodings of U+FFF8.
func explode(s string, n int) []string {
	if n == 0 {
		return nil
	}
	l := utf8.RuneCountInString(s)
	if n <= 0 || n > l {
		n = l
	}
	a := make([]string, n)
	var size, rune int
	i, cur := 0, 0
	for ; i+1 < n; i++ {
		rune, size = utf8.DecodeRuneInString(s[cur:])
		a[i] = string(rune)
		cur += size
	}
	// add the rest, if there is any
	if cur < len(s) {
		a[i] = s[cur:]
	}
	return a
}

// Count counts the number of non-overlapping instances of sep in s.
func Count(s, sep string) int {
	if sep == "" {
		return utf8.RuneCountInString(s) + 1
	}
	c := sep[0]
	l := len(sep)
	n := 0
	if l == 1 {
		// special case worth making fast
		for i := 0; i < len(s); i++ {
			if s[i] == c {
				n++
			}
		}
		return n
	}
	for i := 0; i+l <= len(s); i++ {
		if s[i] == c && s[i:i+l] == sep {
			n++
			i += l - 1
		}
	}
	return n
}

// Contains returns true if substr is within s.
func Contains(s, substr string) bool {
	return Index(s, substr) != -1
}

// Index returns the index of the first instance of sep in s, or -1 if sep is not present in s.
func Index(s, sep string) int {
	n := len(sep)
	if n == 0 {
		return 0
	}
	c := sep[0]
	if n == 1 {
		// special case worth making fast
		for i := 0; i < len(s); i++ {
			if s[i] == c {
				return i
			}
		}
		return -1
	}
	// n > 1
	for i := 0; i+n <= len(s); i++ {
		if s[i] == c && s[i:i+n] == sep {
			return i
		}
	}
	return -1
}

// LastIndex returns the index of the last instance of sep in s, or -1 if sep is not present in s.
func LastIndex(s, sep string) int {
	n := len(sep)
	if n == 0 {
		return len(s)
	}
	c := sep[0]
	if n == 1 {
		// special case worth making fast
		for i := len(s) - 1; i >= 0; i-- {
			if s[i] == c {
				return i
			}
		}
		return -1
	}
	// n > 1
	for i := len(s) - n; i >= 0; i-- {
		if s[i] == c && s[i:i+n] == sep {
			return i
		}
	}
	return -1
}

// IndexRune returns the index of the first instance of the Unicode code point
// rune, or -1 if rune is not present in s.
func IndexRune(s string, rune int) int {
	for i, c := range s {
		if c == rune {
			return i
		}
	}
	return -1
}

// IndexAny returns the index of the first instance of any Unicode code point
// from chars in s, or -1 if no Unicode code point from chars is present in s.
func IndexAny(s, chars string) int {
	if len(chars) > 0 {
		for i, c := range s {
			for _, m := range chars {
				if c == m {
					return i
				}
			}
		}
	}
	return -1
}

// LastIndexAny returns the index of the last instance of any Unicode code
// point from chars in s, or -1 if no Unicode code point from chars is
// present in s.
func LastIndexAny(s, chars string) int {
	if len(chars) > 0 {
		for i := len(s); i > 0; {
			rune, size := utf8.DecodeLastRuneInString(s[0:i])
			i -= size
			for _, m := range chars {
				if rune == m {
					return i
				}
			}
		}
	}
	return -1
}

// Generic split: splits after each instance of sep,
// including sepSave bytes of sep in the subarrays.
func genSplit(s, sep string, sepSave, n int) []string {
	if n == 0 {
		return nil
	}
	if sep == "" {
		return explode(s, n)
	}
	if n < 0 {
		n = Count(s, sep) + 1
	}
	c := sep[0]
	start := 0
	a := make([]string, n)
	na := 0
	for i := 0; i+len(sep) <= len(s) && na+1 < n; i++ {
		if s[i] == c && (len(sep) == 1 || s[i:i+len(sep)] == sep) {
			a[na] = s[start : i+sepSave]
			na++
			start = i + len(sep)
			i += len(sep) - 1
		}
	}
	a[na] = s[start:]
	return a[0 : na+1]
}

// Split slices s into substrings separated by sep and returns a slice of
// the substrings between those separators.
// If sep is empty, Split splits after each UTF-8 sequence.
// The count determines the number of substrings to return:
//   n > 0: at most n substrings; the last substring will be the unsplit remainder.
//   n == 0: the result is nil (zero substrings)
//   n < 0: all substrings
func Split(s, sep string, n int) []string { return genSplit(s, sep, 0, n) }

// SplitAfter slices s into substrings after each instance of sep and
// returns a slice of those substrings.
// If sep is empty, Split splits after each UTF-8 sequence.
// The count determines the number of substrings to return:
//   n > 0: at most n substrings; the last substring will be the unsplit remainder.
//   n == 0: the result is nil (zero substrings)
//   n < 0: all substrings
func SplitAfter(s, sep string, n int) []string {
	return genSplit(s, sep, len(sep), n)
}

// Fields splits the string s around each instance of one or more consecutive white space
// characters, returning an array of substrings of s or an empty list if s contains only white space.
func Fields(s string) []string {
	return FieldsFunc(s, unicode.IsSpace)
}

// FieldsFunc splits the string s at each run of Unicode code points c satisfying f(c)
// and returns an array of slices of s. If all code points in s satisfy f(c) or the
// string is empty, an empty slice is returned.
func FieldsFunc(s string, f func(int) bool) []string {
	// First count the fields.
	n := 0
	inField := false
	for _, rune := range s {
		wasInField := inField
		inField = !f(rune)
		if inField && !wasInField {
			n++
		}
	}

	// Now create them.
	a := make([]string, n)
	na := 0
	fieldStart := -1 // Set to -1 when looking for start of field.
	for i, rune := range s {
		if f(rune) {
			if fieldStart >= 0 {
				a[na] = s[fieldStart:i]
				na++
				fieldStart = -1
			}
		} else if fieldStart == -1 {
			fieldStart = i
		}
	}
	if fieldStart != -1 { // Last field might end at EOF.
		a[na] = s[fieldStart:]
	}
	return a
}

// Join concatenates the elements of a to create a single string.   The separator string
// sep is placed between elements in the resulting string.
func Join(a []string, sep string) string {
	if len(a) == 0 {
		return ""
	}
	if len(a) == 1 {
		return a[0]
	}
	n := len(sep) * (len(a) - 1)
	for i := 0; i < len(a); i++ {
		n += len(a[i])
	}

	b := make([]byte, n)
	bp := 0
	for i := 0; i < len(a); i++ {
		s := a[i]
		for j := 0; j < len(s); j++ {
			b[bp] = s[j]
			bp++
		}
		if i+1 < len(a) {
			s = sep
			for j := 0; j < len(s); j++ {
				b[bp] = s[j]
				bp++
			}
		}
	}
	return string(b)
}

// HasPrefix tests whether the string s begins with prefix.
func HasPrefix(s, prefix string) bool {
	return len(s) >= len(prefix) && s[0:len(prefix)] == prefix
}

// HasSuffix tests whether the string s ends with suffix.
func HasSuffix(s, suffix string) bool {
	return len(s) >= len(suffix) && s[len(s)-len(suffix):] == suffix
}

// Map returns a copy of the string s with all its characters modified
// according to the mapping function. If mapping returns a negative value, the character is
// dropped from the string with no replacement.
func Map(mapping func(rune int) int, s string) string {
	// In the worst case, the string can grow when mapped, making
	// things unpleasant.  But it's so rare we barge in assuming it's
	// fine.  It could also shrink but that falls out naturally.
	maxbytes := len(s) // length of b
	nbytes := 0        // number of bytes encoded in b
	b := make([]byte, maxbytes)
	for _, c := range s {
		rune := mapping(c)
		if rune >= 0 {
			wid := 1
			if rune >= utf8.RuneSelf {
				wid = utf8.RuneLen(rune)
			}
			if nbytes+wid > maxbytes {
				// Grow the buffer.
				maxbytes = maxbytes*2 + utf8.UTFMax
				nb := make([]byte, maxbytes)
				copy(nb, b[0:nbytes])
				b = nb
			}
			nbytes += utf8.EncodeRune(b[nbytes:maxbytes], rune)
		}
	}
	return string(b[0:nbytes])
}

// Repeat returns a new string consisting of count copies of the string s.
func Repeat(s string, count int) string {
	b := make([]byte, len(s)*count)
	bp := 0
	for i := 0; i < count; i++ {
		for j := 0; j < len(s); j++ {
			b[bp] = s[j]
			bp++
		}
	}
	return string(b)
}


// ToUpper returns a copy of the string s with all Unicode letters mapped to their upper case.
func ToUpper(s string) string { return Map(unicode.ToUpper, s) }

// ToLower returns a copy of the string s with all Unicode letters mapped to their lower case.
func ToLower(s string) string { return Map(unicode.ToLower, s) }

// ToTitle returns a copy of the string s with all Unicode letters mapped to their title case.
func ToTitle(s string) string { return Map(unicode.ToTitle, s) }

// ToUpperSpecial returns a copy of the string s with all Unicode letters mapped to their
// upper case, giving priority to the special casing rules.
func ToUpperSpecial(_case unicode.SpecialCase, s string) string {
	return Map(func(r int) int { return _case.ToUpper(r) }, s)
}

// ToLowerSpecial returns a copy of the string s with all Unicode letters mapped to their
// lower case, giving priority to the special casing rules.
func ToLowerSpecial(_case unicode.SpecialCase, s string) string {
	return Map(func(r int) int { return _case.ToLower(r) }, s)
}

// ToTitleSpecial returns a copy of the string s with all Unicode letters mapped to their
// title case, giving priority to the special casing rules.
func ToTitleSpecial(_case unicode.SpecialCase, s string) string {
	return Map(func(r int) int { return _case.ToTitle(r) }, s)
}

// isSeparator reports whether the rune could mark a word boundary.
// TODO: update when package unicode captures more of the properties.
func isSeparator(rune int) bool {
	// ASCII alphanumerics and underscore are not separators
	if rune <= 0x7F {
		switch {
		case '0' <= rune && rune <= '9':
			return false
		case 'a' <= rune && rune <= 'z':
			return false
		case 'A' <= rune && rune <= 'Z':
			return false
		case rune == '_':
			return false
		}
		return true
	}
	// Letters and digits are not separators
	if unicode.IsLetter(rune) || unicode.IsDigit(rune) {
		return false
	}
	// Otherwise, all we can do for now is treat spaces as separators.
	return unicode.IsSpace(rune)
}

// BUG(r): The rule Title uses for word boundaries does not handle Unicode punctuation properly.

// Title returns a copy of the string s with all Unicode letters that begin words
// mapped to their title case.
func Title(s string) string {
	// Use a closure here to remember state.
	// Hackish but effective. Depends on Map scanning in order and calling
	// the closure once per rune.
	prev := ' '
	return Map(
		func(r int) int {
			if isSeparator(prev) {
				prev = r
				return unicode.ToTitle(r)
			}
			prev = r
			return r
		},
		s)
}

// TrimLeftFunc returns a slice of the string s with all leading
// Unicode code points c satisfying f(c) removed.
func TrimLeftFunc(s string, f func(r int) bool) string {
	i := indexFunc(s, f, false)
	if i == -1 {
		return ""
	}
	return s[i:]
}

// TrimRightFunc returns a slice of the string s with all trailing
// Unicode code points c satisfying f(c) removed.
func TrimRightFunc(s string, f func(r int) bool) string {
	i := lastIndexFunc(s, f, false)
	if i >= 0 && s[i] >= utf8.RuneSelf {
		_, wid := utf8.DecodeRuneInString(s[i:])
		i += wid
	} else {
		i++
	}
	return s[0:i]
}

// TrimFunc returns a slice of the string s with all leading
// and trailing Unicode code points c satisfying f(c) removed.
func TrimFunc(s string, f func(r int) bool) string {
	return TrimRightFunc(TrimLeftFunc(s, f), f)
}

// IndexFunc returns the index into s of the first Unicode
// code point satisfying f(c), or -1 if none do.
func IndexFunc(s string, f func(r int) bool) int {
	return indexFunc(s, f, true)
}

// LastIndexFunc returns the index into s of the last
// Unicode code point satisfying f(c), or -1 if none do.
func LastIndexFunc(s string, f func(r int) bool) int {
	return lastIndexFunc(s, f, true)
}

// indexFunc is the same as IndexFunc except that if
// truth==false, the sense of the predicate function is
// inverted.
func indexFunc(s string, f func(r int) bool, truth bool) int {
	start := 0
	for start < len(s) {
		wid := 1
		rune := int(s[start])
		if rune >= utf8.RuneSelf {
			rune, wid = utf8.DecodeRuneInString(s[start:])
		}
		if f(rune) == truth {
			return start
		}
		start += wid
	}
	return -1
}

// lastIndexFunc is the same as LastIndexFunc except that if
// truth==false, the sense of the predicate function is
// inverted.
func lastIndexFunc(s string, f func(r int) bool, truth bool) int {
	for i := len(s); i > 0; {
		rune, size := utf8.DecodeLastRuneInString(s[0:i])
		i -= size
		if f(rune) == truth {
			return i
		}
	}
	return -1
}

func makeCutsetFunc(cutset string) func(rune int) bool {
	return func(rune int) bool { return IndexRune(cutset, rune) != -1 }
}

// Trim returns a slice of the string s with all leading and
// trailing Unicode code points contained in cutset removed.
func Trim(s string, cutset string) string {
	if s == "" || cutset == "" {
		return s
	}
	return TrimFunc(s, makeCutsetFunc(cutset))
}

// TrimLeft returns a slice of the string s with all leading
// Unicode code points contained in cutset removed.
func TrimLeft(s string, cutset string) string {
	if s == "" || cutset == "" {
		return s
	}
	return TrimLeftFunc(s, makeCutsetFunc(cutset))
}

// TrimRight returns a slice of the string s, with all trailing
// Unicode code points contained in cutset removed.
func TrimRight(s string, cutset string) string {
	if s == "" || cutset == "" {
		return s
	}
	return TrimRightFunc(s, makeCutsetFunc(cutset))
}

// TrimSpace returns a slice of the string s, with all leading
// and trailing white space removed, as defined by Unicode.
func TrimSpace(s string) string {
	return TrimFunc(s, unicode.IsSpace)
}

// Replace returns a copy of the string s with the first n
// non-overlapping instances of old replaced by new.
// If n < 0, there is no limit on the number of replacements.
func Replace(s, old, new string, n int) string {
	if old == new || n == 0 {
		return s // avoid allocation
	}

	// Compute number of replacements.
	if m := Count(s, old); m == 0 {
		return s // avoid allocation
	} else if n < 0 || m < n {
		n = m
	}

	// Apply replacements to buffer.
	t := make([]byte, len(s)+n*(len(new)-len(old)))
	w := 0
	start := 0
	for i := 0; i < n; i++ {
		j := start
		if len(old) == 0 {
			if i > 0 {
				_, wid := utf8.DecodeRuneInString(s[start:])
				j += wid
			}
		} else {
			j += Index(s[start:], old)
		}
		w += copy(t[w:], s[start:j])
		w += copy(t[w:], new)
		start = j + len(old)
	}
	w += copy(t[w:], s[start:])
	return string(t[0:w])
}
