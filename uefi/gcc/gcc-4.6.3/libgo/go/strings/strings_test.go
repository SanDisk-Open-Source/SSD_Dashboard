// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package strings_test

import (
	"os"
	. "strings"
	"testing"
	"unicode"
	"utf8"
)

func eq(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := 0; i < len(a); i++ {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

var abcd = "abcd"
var faces = "☺☻☹"
var commas = "1,2,3,4"
var dots = "1....2....3....4"

type IndexTest struct {
	s   string
	sep string
	out int
}

var indexTests = []IndexTest{
	{"", "", 0},
	{"", "a", -1},
	{"", "foo", -1},
	{"fo", "foo", -1},
	{"foo", "foo", 0},
	{"oofofoofooo", "f", 2},
	{"oofofoofooo", "foo", 4},
	{"barfoobarfoo", "foo", 3},
	{"foo", "", 0},
	{"foo", "o", 1},
	{"abcABCabc", "A", 3},
	// cases with one byte strings - test special case in Index()
	{"", "a", -1},
	{"x", "a", -1},
	{"x", "x", 0},
	{"abc", "a", 0},
	{"abc", "b", 1},
	{"abc", "c", 2},
	{"abc", "x", -1},
}

var lastIndexTests = []IndexTest{
	{"", "", 0},
	{"", "a", -1},
	{"", "foo", -1},
	{"fo", "foo", -1},
	{"foo", "foo", 0},
	{"foo", "f", 0},
	{"oofofoofooo", "f", 7},
	{"oofofoofooo", "foo", 7},
	{"barfoobarfoo", "foo", 9},
	{"foo", "", 3},
	{"foo", "o", 2},
	{"abcABCabc", "A", 3},
	{"abcABCabc", "a", 6},
}

var indexAnyTests = []IndexTest{
	{"", "", -1},
	{"", "a", -1},
	{"", "abc", -1},
	{"a", "", -1},
	{"a", "a", 0},
	{"aaa", "a", 0},
	{"abc", "xyz", -1},
	{"abc", "xcz", 2},
	{"a☺b☻c☹d", "uvw☻xyz", 2 + len("☺")},
	{"aRegExp*", ".(|)*+?^$[]", 7},
	{dots + dots + dots, " ", -1},
}
var lastIndexAnyTests = []IndexTest{
	{"", "", -1},
	{"", "a", -1},
	{"", "abc", -1},
	{"a", "", -1},
	{"a", "a", 0},
	{"aaa", "a", 2},
	{"abc", "xyz", -1},
	{"abc", "ab", 1},
	{"a☺b☻c☹d", "uvw☻xyz", 2 + len("☺")},
	{"a.RegExp*", ".(|)*+?^$[]", 8},
	{dots + dots + dots, " ", -1},
}

// Execute f on each test case.  funcName should be the name of f; it's used
// in failure reports.
func runIndexTests(t *testing.T, f func(s, sep string) int, funcName string, testCases []IndexTest) {
	for _, test := range testCases {
		actual := f(test.s, test.sep)
		if actual != test.out {
			t.Errorf("%s(%q,%q) = %v; want %v", funcName, test.s, test.sep, actual, test.out)
		}
	}
}

func TestIndex(t *testing.T)        { runIndexTests(t, Index, "Index", indexTests) }
func TestLastIndex(t *testing.T)    { runIndexTests(t, LastIndex, "LastIndex", lastIndexTests) }
func TestIndexAny(t *testing.T)     { runIndexTests(t, IndexAny, "IndexAny", indexAnyTests) }
func TestLastIndexAny(t *testing.T) { runIndexTests(t, LastIndexAny, "LastIndexAny", lastIndexAnyTests) }

type ExplodeTest struct {
	s string
	n int
	a []string
}

var explodetests = []ExplodeTest{
	{"", -1, []string{}},
	{abcd, 4, []string{"a", "b", "c", "d"}},
	{faces, 3, []string{"☺", "☻", "☹"}},
	{abcd, 2, []string{"a", "bcd"}},
}

func TestExplode(t *testing.T) {
	for _, tt := range explodetests {
		a := Split(tt.s, "", tt.n)
		if !eq(a, tt.a) {
			t.Errorf("explode(%q, %d) = %v; want %v", tt.s, tt.n, a, tt.a)
			continue
		}
		s := Join(a, "")
		if s != tt.s {
			t.Errorf(`Join(explode(%q, %d), "") = %q`, tt.s, tt.n, s)
		}
	}
}

type SplitTest struct {
	s   string
	sep string
	n   int
	a   []string
}

var splittests = []SplitTest{
	{abcd, "a", 0, nil},
	{abcd, "a", -1, []string{"", "bcd"}},
	{abcd, "z", -1, []string{"abcd"}},
	{abcd, "", -1, []string{"a", "b", "c", "d"}},
	{commas, ",", -1, []string{"1", "2", "3", "4"}},
	{dots, "...", -1, []string{"1", ".2", ".3", ".4"}},
	{faces, "☹", -1, []string{"☺☻", ""}},
	{faces, "~", -1, []string{faces}},
	{faces, "", -1, []string{"☺", "☻", "☹"}},
	{"1 2 3 4", " ", 3, []string{"1", "2", "3 4"}},
	{"1 2", " ", 3, []string{"1", "2"}},
	{"123", "", 2, []string{"1", "23"}},
	{"123", "", 17, []string{"1", "2", "3"}},
}

func TestSplit(t *testing.T) {
	for _, tt := range splittests {
		a := Split(tt.s, tt.sep, tt.n)
		if !eq(a, tt.a) {
			t.Errorf("Split(%q, %q, %d) = %v; want %v", tt.s, tt.sep, tt.n, a, tt.a)
			continue
		}
		if tt.n == 0 {
			continue
		}
		s := Join(a, tt.sep)
		if s != tt.s {
			t.Errorf("Join(Split(%q, %q, %d), %q) = %q", tt.s, tt.sep, tt.n, tt.sep, s)
		}
	}
}

var splitaftertests = []SplitTest{
	{abcd, "a", -1, []string{"a", "bcd"}},
	{abcd, "z", -1, []string{"abcd"}},
	{abcd, "", -1, []string{"a", "b", "c", "d"}},
	{commas, ",", -1, []string{"1,", "2,", "3,", "4"}},
	{dots, "...", -1, []string{"1...", ".2...", ".3...", ".4"}},
	{faces, "☹", -1, []string{"☺☻☹", ""}},
	{faces, "~", -1, []string{faces}},
	{faces, "", -1, []string{"☺", "☻", "☹"}},
	{"1 2 3 4", " ", 3, []string{"1 ", "2 ", "3 4"}},
	{"1 2 3", " ", 3, []string{"1 ", "2 ", "3"}},
	{"1 2", " ", 3, []string{"1 ", "2"}},
	{"123", "", 2, []string{"1", "23"}},
	{"123", "", 17, []string{"1", "2", "3"}},
}

func TestSplitAfter(t *testing.T) {
	for _, tt := range splitaftertests {
		a := SplitAfter(tt.s, tt.sep, tt.n)
		if !eq(a, tt.a) {
			t.Errorf(`Split(%q, %q, %d) = %v; want %v`, tt.s, tt.sep, tt.n, a, tt.a)
			continue
		}
		s := Join(a, "")
		if s != tt.s {
			t.Errorf(`Join(Split(%q, %q, %d), %q) = %q`, tt.s, tt.sep, tt.n, tt.sep, s)
		}
	}
}

type FieldsTest struct {
	s string
	a []string
}

var fieldstests = []FieldsTest{
	{"", []string{}},
	{" ", []string{}},
	{" \t ", []string{}},
	{"  abc  ", []string{"abc"}},
	{"1 2 3 4", []string{"1", "2", "3", "4"}},
	{"1  2  3  4", []string{"1", "2", "3", "4"}},
	{"1\t\t2\t\t3\t4", []string{"1", "2", "3", "4"}},
	{"1\u20002\u20013\u20024", []string{"1", "2", "3", "4"}},
	{"\u2000\u2001\u2002", []string{}},
	{"\n™\t™\n", []string{"™", "™"}},
	{faces, []string{faces}},
}

func TestFields(t *testing.T) {
	for _, tt := range fieldstests {
		a := Fields(tt.s)
		if !eq(a, tt.a) {
			t.Errorf("Fields(%q) = %v; want %v", tt.s, a, tt.a)
			continue
		}
	}
}

func TestFieldsFunc(t *testing.T) {
	pred := func(c int) bool { return c == 'X' }
	var fieldsFuncTests = []FieldsTest{
		{"", []string{}},
		{"XX", []string{}},
		{"XXhiXXX", []string{"hi"}},
		{"aXXbXXXcX", []string{"a", "b", "c"}},
	}
	for _, tt := range fieldsFuncTests {
		a := FieldsFunc(tt.s, pred)
		if !eq(a, tt.a) {
			t.Errorf("FieldsFunc(%q) = %v, want %v", tt.s, a, tt.a)
		}
	}
}


// Test case for any function which accepts and returns a single string.
type StringTest struct {
	in, out string
}

// Execute f on each test case.  funcName should be the name of f; it's used
// in failure reports.
func runStringTests(t *testing.T, f func(string) string, funcName string, testCases []StringTest) {
	for _, tc := range testCases {
		actual := f(tc.in)
		if actual != tc.out {
			t.Errorf("%s(%q) = %q; want %q", funcName, tc.in, actual, tc.out)
		}
	}
}

var upperTests = []StringTest{
	{"", ""},
	{"abc", "ABC"},
	{"AbC123", "ABC123"},
	{"azAZ09_", "AZAZ09_"},
	{"\u0250\u0250\u0250\u0250\u0250", "\u2C6F\u2C6F\u2C6F\u2C6F\u2C6F"}, // grows one byte per char
}

var lowerTests = []StringTest{
	{"", ""},
	{"abc", "abc"},
	{"AbC123", "abc123"},
	{"azAZ09_", "azaz09_"},
	{"\u2C6D\u2C6D\u2C6D\u2C6D\u2C6D", "\u0251\u0251\u0251\u0251\u0251"}, // shrinks one byte per char
}

const space = "\t\v\r\f\n\u0085\u00a0\u2000\u3000"

var trimSpaceTests = []StringTest{
	{"", ""},
	{"abc", "abc"},
	{space + "abc" + space, "abc"},
	{" ", ""},
	{" \t\r\n \t\t\r\r\n\n ", ""},
	{" \t\r\n x\t\t\r\r\n\n ", "x"},
	{" \u2000\t\r\n x\t\t\r\r\ny\n \u3000", "x\t\t\r\r\ny"},
	{"1 \t\r\n2", "1 \t\r\n2"},
	{" x\x80", "x\x80"},
	{" x\xc0", "x\xc0"},
	{"x \xc0\xc0 ", "x \xc0\xc0"},
	{"x \xc0", "x \xc0"},
	{"x \xc0 ", "x \xc0"},
	{"x \xc0\xc0 ", "x \xc0\xc0"},
	{"x ☺\xc0\xc0 ", "x ☺\xc0\xc0"},
	{"x ☺ ", "x ☺"},
}

func tenRunes(rune int) string {
	r := make([]int, 10)
	for i := range r {
		r[i] = rune
	}
	return string(r)
}

// User-defined self-inverse mapping function
func rot13(rune int) int {
	step := 13
	if rune >= 'a' && rune <= 'z' {
		return ((rune - 'a' + step) % 26) + 'a'
	}
	if rune >= 'A' && rune <= 'Z' {
		return ((rune - 'A' + step) % 26) + 'A'
	}
	return rune
}

func TestMap(t *testing.T) {
	// Run a couple of awful growth/shrinkage tests
	a := tenRunes('a')
	// 1.  Grow.  This triggers two reallocations in Map.
	maxRune := func(rune int) int { return unicode.MaxRune }
	m := Map(maxRune, a)
	expect := tenRunes(unicode.MaxRune)
	if m != expect {
		t.Errorf("growing: expected %q got %q", expect, m)
	}

	// 2. Shrink
	minRune := func(rune int) int { return 'a' }
	m = Map(minRune, tenRunes(unicode.MaxRune))
	expect = a
	if m != expect {
		t.Errorf("shrinking: expected %q got %q", expect, m)
	}

	// 3. Rot13
	m = Map(rot13, "a to zed")
	expect = "n gb mrq"
	if m != expect {
		t.Errorf("rot13: expected %q got %q", expect, m)
	}

	// 4. Rot13^2
	m = Map(rot13, Map(rot13, "a to zed"))
	expect = "a to zed"
	if m != expect {
		t.Errorf("rot13: expected %q got %q", expect, m)
	}

	// 5. Drop
	dropNotLatin := func(rune int) int {
		if unicode.Is(unicode.Latin, rune) {
			return rune
		}
		return -1
	}
	m = Map(dropNotLatin, "Hello, 세계")
	expect = "Hello"
	if m != expect {
		t.Errorf("drop: expected %q got %q", expect, m)
	}
}

func TestToUpper(t *testing.T) { runStringTests(t, ToUpper, "ToUpper", upperTests) }

func TestToLower(t *testing.T) { runStringTests(t, ToLower, "ToLower", lowerTests) }

func TestSpecialCase(t *testing.T) {
	lower := "abcçdefgğhıijklmnoöprsştuüvyz"
	upper := "ABCÇDEFGĞHIİJKLMNOÖPRSŞTUÜVYZ"
	u := ToUpperSpecial(unicode.TurkishCase, upper)
	if u != upper {
		t.Errorf("Upper(upper) is %s not %s", u, upper)
	}
	u = ToUpperSpecial(unicode.TurkishCase, lower)
	if u != upper {
		t.Errorf("Upper(lower) is %s not %s", u, upper)
	}
	l := ToLowerSpecial(unicode.TurkishCase, lower)
	if l != lower {
		t.Errorf("Lower(lower) is %s not %s", l, lower)
	}
	l = ToLowerSpecial(unicode.TurkishCase, upper)
	if l != lower {
		t.Errorf("Lower(upper) is %s not %s", l, lower)
	}
}

func TestTrimSpace(t *testing.T) { runStringTests(t, TrimSpace, "TrimSpace", trimSpaceTests) }

type TrimTest struct {
	f               func(string, string) string
	in, cutset, out string
}

var trimTests = []TrimTest{
	{Trim, "abba", "a", "bb"},
	{Trim, "abba", "ab", ""},
	{TrimLeft, "abba", "ab", ""},
	{TrimRight, "abba", "ab", ""},
	{TrimLeft, "abba", "a", "bba"},
	{TrimRight, "abba", "a", "abb"},
	{Trim, "<tag>", "<>", "tag"},
	{Trim, "* listitem", " *", "listitem"},
	{Trim, `"quote"`, `"`, "quote"},
	{Trim, "\u2C6F\u2C6F\u0250\u0250\u2C6F\u2C6F", "\u2C6F", "\u0250\u0250"},
	//empty string tests
	{Trim, "abba", "", "abba"},
	{Trim, "", "123", ""},
	{Trim, "", "", ""},
	{TrimLeft, "abba", "", "abba"},
	{TrimLeft, "", "123", ""},
	{TrimLeft, "", "", ""},
	{TrimRight, "abba", "", "abba"},
	{TrimRight, "", "123", ""},
	{TrimRight, "", "", ""},
	{TrimRight, "☺\xc0", "☺", "☺\xc0"},
}

func TestTrim(t *testing.T) {
	for _, tc := range trimTests {
		actual := tc.f(tc.in, tc.cutset)
		var name string
		switch tc.f {
		case Trim:
			name = "Trim"
		case TrimLeft:
			name = "TrimLeft"
		case TrimRight:
			name = "TrimRight"
		default:
			t.Error("Undefined trim function")
		}
		if actual != tc.out {
			t.Errorf("%s(%q, %q) = %q; want %q", name, tc.in, tc.cutset, actual, tc.out)
		}
	}
}

type predicate struct {
	f    func(r int) bool
	name string
}

var isSpace = predicate{unicode.IsSpace, "IsSpace"}
var isDigit = predicate{unicode.IsDigit, "IsDigit"}
var isUpper = predicate{unicode.IsUpper, "IsUpper"}
var isValidRune = predicate{
	func(r int) bool {
		return r != utf8.RuneError
	},
	"IsValidRune",
}

type TrimFuncTest struct {
	f       predicate
	in, out string
}

func not(p predicate) predicate {
	return predicate{
		func(r int) bool {
			return !p.f(r)
		},
		"not " + p.name,
	}
}

var trimFuncTests = []TrimFuncTest{
	{isSpace, space + " hello " + space, "hello"},
	{isDigit, "\u0e50\u0e5212hello34\u0e50\u0e51", "hello"},
	{isUpper, "\u2C6F\u2C6F\u2C6F\u2C6FABCDhelloEF\u2C6F\u2C6FGH\u2C6F\u2C6F", "hello"},
	{not(isSpace), "hello" + space + "hello", space},
	{not(isDigit), "hello\u0e50\u0e521234\u0e50\u0e51helo", "\u0e50\u0e521234\u0e50\u0e51"},
	{isValidRune, "ab\xc0a\xc0cd", "\xc0a\xc0"},
	{not(isValidRune), "\xc0a\xc0", "a"},
}

func TestTrimFunc(t *testing.T) {
	for _, tc := range trimFuncTests {
		actual := TrimFunc(tc.in, tc.f.f)
		if actual != tc.out {
			t.Errorf("TrimFunc(%q, %q) = %q; want %q", tc.in, tc.f.name, actual, tc.out)
		}
	}
}

type IndexFuncTest struct {
	in          string
	f           predicate
	first, last int
}

var indexFuncTests = []IndexFuncTest{
	{"", isValidRune, -1, -1},
	{"abc", isDigit, -1, -1},
	{"0123", isDigit, 0, 3},
	{"a1b", isDigit, 1, 1},
	{space, isSpace, 0, len(space) - 3}, // last rune in space is 3 bytes
	{"\u0e50\u0e5212hello34\u0e50\u0e51", isDigit, 0, 18},
	{"\u2C6F\u2C6F\u2C6F\u2C6FABCDhelloEF\u2C6F\u2C6FGH\u2C6F\u2C6F", isUpper, 0, 34},
	{"12\u0e50\u0e52hello34\u0e50\u0e51", not(isDigit), 8, 12},

	// tests of invalid UTF-8
	{"\x801", isDigit, 1, 1},
	{"\x80abc", isDigit, -1, -1},
	{"\xc0a\xc0", isValidRune, 1, 1},
	{"\xc0a\xc0", not(isValidRune), 0, 2},
	{"\xc0☺\xc0", not(isValidRune), 0, 4},
	{"\xc0☺\xc0\xc0", not(isValidRune), 0, 5},
	{"ab\xc0a\xc0cd", not(isValidRune), 2, 4},
	{"a\xe0\x80cd", not(isValidRune), 1, 2},
	{"\x80\x80\x80\x80", not(isValidRune), 0, 3},
}

func TestIndexFunc(t *testing.T) {
	for _, tc := range indexFuncTests {
		first := IndexFunc(tc.in, tc.f.f)
		if first != tc.first {
			t.Errorf("IndexFunc(%q, %s) = %d; want %d", tc.in, tc.f.name, first, tc.first)
		}
		last := LastIndexFunc(tc.in, tc.f.f)
		if last != tc.last {
			t.Errorf("LastIndexFunc(%q, %s) = %d; want %d", tc.in, tc.f.name, last, tc.last)
		}
	}
}

func equal(m string, s1, s2 string, t *testing.T) bool {
	if s1 == s2 {
		return true
	}
	e1 := Split(s1, "", -1)
	e2 := Split(s2, "", -1)
	for i, c1 := range e1 {
		if i > len(e2) {
			break
		}
		r1, _ := utf8.DecodeRuneInString(c1)
		r2, _ := utf8.DecodeRuneInString(e2[i])
		if r1 != r2 {
			t.Errorf("%s diff at %d: U+%04X U+%04X", m, i, r1, r2)
		}
	}
	return false
}

func TestCaseConsistency(t *testing.T) {
	// Make a string of all the runes.
	a := make([]int, unicode.MaxRune+1)
	for i := range a {
		a[i] = i
	}
	s := string(a)
	// convert the cases.
	upper := ToUpper(s)
	lower := ToLower(s)

	// Consistency checks
	if n := utf8.RuneCountInString(upper); n != unicode.MaxRune+1 {
		t.Error("rune count wrong in upper:", n)
	}
	if n := utf8.RuneCountInString(lower); n != unicode.MaxRune+1 {
		t.Error("rune count wrong in lower:", n)
	}
	if !equal("ToUpper(upper)", ToUpper(upper), upper, t) {
		t.Error("ToUpper(upper) consistency fail")
	}
	if !equal("ToLower(lower)", ToLower(lower), lower, t) {
		t.Error("ToLower(lower) consistency fail")
	}
	/*
		  These fail because of non-one-to-oneness of the data, such as multiple
		  upper case 'I' mapping to 'i'.  We comment them out but keep them for
		  interest.
		  For instance: CAPITAL LETTER I WITH DOT ABOVE:
			unicode.ToUpper(unicode.ToLower('\u0130')) != '\u0130'

		if !equal("ToUpper(lower)", ToUpper(lower), upper, t) {
			t.Error("ToUpper(lower) consistency fail");
		}
		if !equal("ToLower(upper)", ToLower(upper), lower, t) {
			t.Error("ToLower(upper) consistency fail");
		}
	*/
}

type RepeatTest struct {
	in, out string
	count   int
}

var RepeatTests = []RepeatTest{
	{"", "", 0},
	{"", "", 1},
	{"", "", 2},
	{"-", "", 0},
	{"-", "-", 1},
	{"-", "----------", 10},
	{"abc ", "abc abc abc ", 3},
}

func TestRepeat(t *testing.T) {
	for _, tt := range RepeatTests {
		a := Repeat(tt.in, tt.count)
		if !equal("Repeat(s)", a, tt.out, t) {
			t.Errorf("Repeat(%v, %d) = %v; want %v", tt.in, tt.count, a, tt.out)
			continue
		}
	}
}

func runesEqual(a, b []int) bool {
	if len(a) != len(b) {
		return false
	}
	for i, r := range a {
		if r != b[i] {
			return false
		}
	}
	return true
}

type RunesTest struct {
	in    string
	out   []int
	lossy bool
}

var RunesTests = []RunesTest{
	{"", []int{}, false},
	{" ", []int{32}, false},
	{"ABC", []int{65, 66, 67}, false},
	{"abc", []int{97, 98, 99}, false},
	{"\u65e5\u672c\u8a9e", []int{26085, 26412, 35486}, false},
	{"ab\x80c", []int{97, 98, 0xFFFD, 99}, true},
	{"ab\xc0c", []int{97, 98, 0xFFFD, 99}, true},
}

func TestRunes(t *testing.T) {
	for _, tt := range RunesTests {
		a := []int(tt.in)
		if !runesEqual(a, tt.out) {
			t.Errorf("[]int(%q) = %v; want %v", tt.in, a, tt.out)
			continue
		}
		if !tt.lossy {
			// can only test reassembly if we didn't lose information
			s := string(a)
			if s != tt.in {
				t.Errorf("string([]int(%q)) = %x; want %x", tt.in, s, tt.in)
			}
		}
	}
}

func TestReadRune(t *testing.T) {
	testStrings := []string{"", abcd, faces, commas}
	for _, s := range testStrings {
		reader := NewReader(s)
		res := ""
		for {
			r, _, e := reader.ReadRune()
			if e == os.EOF {
				break
			}
			if e != nil {
				t.Errorf("Reading %q: %s", s, e)
				break
			}
			res += string(r)
		}
		if res != s {
			t.Errorf("Reader(%q).ReadRune() produced %q", s, res)
		}
	}
}

type ReplaceTest struct {
	in       string
	old, new string
	n        int
	out      string
}

var ReplaceTests = []ReplaceTest{
	{"hello", "l", "L", 0, "hello"},
	{"hello", "l", "L", -1, "heLLo"},
	{"hello", "x", "X", -1, "hello"},
	{"", "x", "X", -1, ""},
	{"radar", "r", "<r>", -1, "<r>ada<r>"},
	{"", "", "<>", -1, "<>"},
	{"banana", "a", "<>", -1, "b<>n<>n<>"},
	{"banana", "a", "<>", 1, "b<>nana"},
	{"banana", "a", "<>", 1000, "b<>n<>n<>"},
	{"banana", "an", "<>", -1, "b<><>a"},
	{"banana", "ana", "<>", -1, "b<>na"},
	{"banana", "", "<>", -1, "<>b<>a<>n<>a<>n<>a<>"},
	{"banana", "", "<>", 10, "<>b<>a<>n<>a<>n<>a<>"},
	{"banana", "", "<>", 6, "<>b<>a<>n<>a<>n<>a"},
	{"banana", "", "<>", 5, "<>b<>a<>n<>a<>na"},
	{"banana", "", "<>", 1, "<>banana"},
	{"banana", "a", "a", -1, "banana"},
	{"banana", "a", "a", 1, "banana"},
	{"☺☻☹", "", "<>", -1, "<>☺<>☻<>☹<>"},
}

func TestReplace(t *testing.T) {
	for _, tt := range ReplaceTests {
		if s := Replace(tt.in, tt.old, tt.new, tt.n); s != tt.out {
			t.Errorf("Replace(%q, %q, %q, %d) = %q, want %q", tt.in, tt.old, tt.new, tt.n, s, tt.out)
		}
	}
}

type TitleTest struct {
	in, out string
}

var TitleTests = []TitleTest{
	{"", ""},
	{"a", "A"},
	{" aaa aaa aaa ", " Aaa Aaa Aaa "},
	{" Aaa Aaa Aaa ", " Aaa Aaa Aaa "},
	{"123a456", "123a456"},
	{"double-blind", "Double-Blind"},
	{"ÿøû", "Ÿøû"},
}

func TestTitle(t *testing.T) {
	for _, tt := range TitleTests {
		if s := Title(tt.in); s != tt.out {
			t.Errorf("Title(%q) = %q, want %q", tt.in, s, tt.out)
		}
	}
}

type ContainsTest struct {
	str, substr string
	expected    bool
}

var ContainsTests = []ContainsTest{
	{"abc", "bc", true},
	{"abc", "bcd", false},
	{"abc", "", true},
	{"", "a", false},
}

func TestContains(t *testing.T) {
	for _, ct := range ContainsTests {
		if Contains(ct.str, ct.substr) != ct.expected {
			t.Errorf("Contains(%s, %s) = %v, want %v",
				ct.str, ct.substr, !ct.expected, ct.expected)
		}
	}
}
