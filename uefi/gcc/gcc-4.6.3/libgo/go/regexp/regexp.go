// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Package regexp implements a simple regular expression library.
//
// The syntax of the regular expressions accepted is:
//
//	regexp:
//		concatenation { '|' concatenation }
//	concatenation:
//		{ closure }
//	closure:
//		term [ '*' | '+' | '?' ]
//	term:
//		'^'
//		'$'
//		'.'
//		character
//		'[' [ '^' ] { character-range } ']'
//		'(' regexp ')'
//	character-range:
//		character [ '-' character ]
//
// All characters are UTF-8-encoded code points.  Backslashes escape special
// characters, including inside character classes.  The standard Go character
// escapes are also recognized: \a \b \f \n \r \t \v.
//
// There are 16 methods of Regexp that match a regular expression and identify
// the matched text.  Their names are matched by this regular expression:
//
//	Find(All)?(String)?(Submatch)?(Index)?
//
// If 'All' is present, the routine matches successive non-overlapping
// matches of the entire expression.  Empty matches abutting a preceding
// match are ignored.  The return value is a slice containing the successive
// return values of the corresponding non-'All' routine.  These routines take
// an extra integer argument, n; if n >= 0, the function returns at most n
// matches/submatches.
//
// If 'String' is present, the argument is a string; otherwise it is a slice
// of bytes; return values are adjusted as appropriate.
//
// If 'Submatch' is present, the return value is a slice identifying the
// successive submatches of the expression.  Submatches are matches of
// parenthesized subexpressions within the regular expression, numbered from
// left to right in order of opening parenthesis.  Submatch 0 is the match of
// the entire expression, submatch 1 the match of the first parenthesized
// subexpression, and so on.
//
// If 'Index' is present, matches and submatches are identified by byte index
// pairs within the input string: result[2*n:2*n+1] identifies the indexes of
// the nth submatch.  The pair for n==0 identifies the match of the entire
// expression.  If 'Index' is not present, the match is identified by the
// text of the match/submatch.  If an index is negative, it means that
// subexpression did not match any string in the input.
//
// (There are a few other methods that do not match this pattern.)
//
package regexp

import (
	"bytes"
	"io"
	"os"
	"strings"
	"utf8"
)

var debug = false

// Error is the local type for a parsing error.
type Error string

func (e Error) String() string {
	return string(e)
}

// Error codes returned by failures to parse an expression.
var (
	ErrInternal            = Error("internal error")
	ErrUnmatchedLpar       = Error("unmatched '('")
	ErrUnmatchedRpar       = Error("unmatched ')'")
	ErrUnmatchedLbkt       = Error("unmatched '['")
	ErrUnmatchedRbkt       = Error("unmatched ']'")
	ErrBadRange            = Error("bad range in character class")
	ErrExtraneousBackslash = Error("extraneous backslash")
	ErrBadClosure          = Error("repeated closure (**, ++, etc.)")
	ErrBareClosure         = Error("closure applies to nothing")
	ErrBadBackslash        = Error("illegal backslash escape")
)

const (
	iStart     = iota // beginning of program
	iEnd              // end of program: success
	iBOT              // '^' beginning of text
	iEOT              // '$' end of text
	iChar             // 'a' regular character
	iCharClass        // [a-z] character class
	iAny              // '.' any character including newline
	iNotNL            // [^\n] special case: any character but newline
	iBra              // '(' parenthesized expression: 2*braNum for left, 2*braNum+1 for right
	iAlt              // '|' alternation
	iNop              // do nothing; makes it easy to link without patching
)

// An instruction executed by the NFA
type instr struct {
	kind  int    // the type of this instruction: iChar, iAny, etc.
	index int    // used only in debugging; could be eliminated
	next  *instr // the instruction to execute after this one
	// Special fields valid only for some items.
	char   int        // iChar
	braNum int        // iBra, iEbra
	cclass *charClass // iCharClass
	left   *instr     // iAlt, other branch
}

func (i *instr) print() {
	switch i.kind {
	case iStart:
		print("start")
	case iEnd:
		print("end")
	case iBOT:
		print("bot")
	case iEOT:
		print("eot")
	case iChar:
		print("char ", string(i.char))
	case iCharClass:
		i.cclass.print()
	case iAny:
		print("any")
	case iNotNL:
		print("notnl")
	case iBra:
		if i.braNum&1 == 0 {
			print("bra", i.braNum/2)
		} else {
			print("ebra", i.braNum/2)
		}
	case iAlt:
		print("alt(", i.left.index, ")")
	case iNop:
		print("nop")
	}
}

// Regexp is the representation of a compiled regular expression.
// The public interface is entirely through methods.
type Regexp struct {
	expr        string // the original expression
	prefix      string // initial plain text string
	prefixBytes []byte // initial plain text bytes
	inst        []*instr
	start       *instr // first instruction of machine
	prefixStart *instr // where to start if there is a prefix
	nbra        int    // number of brackets in expression, for subexpressions
}

type charClass struct {
	negate bool // is character class negated? ([^a-z])
	// slice of int, stored pairwise: [a-z] is (a,z); x is (x,x):
	ranges     []int
	cmin, cmax int
}

func (cclass *charClass) print() {
	print("charclass")
	if cclass.negate {
		print(" (negated)")
	}
	for i := 0; i < len(cclass.ranges); i += 2 {
		l := cclass.ranges[i]
		r := cclass.ranges[i+1]
		if l == r {
			print(" [", string(l), "]")
		} else {
			print(" [", string(l), "-", string(r), "]")
		}
	}
}

func (cclass *charClass) addRange(a, b int) {
	// range is a through b inclusive
	cclass.ranges = append(cclass.ranges, a, b)
	if a < cclass.cmin {
		cclass.cmin = a
	}
	if b > cclass.cmax {
		cclass.cmax = b
	}
}

func (cclass *charClass) matches(c int) bool {
	if c < cclass.cmin || c > cclass.cmax {
		return cclass.negate
	}
	ranges := cclass.ranges
	for i := 0; i < len(ranges); i = i + 2 {
		if ranges[i] <= c && c <= ranges[i+1] {
			return !cclass.negate
		}
	}
	return cclass.negate
}

func newCharClass() *instr {
	i := &instr{kind: iCharClass}
	i.cclass = new(charClass)
	i.cclass.ranges = make([]int, 0, 4)
	i.cclass.cmin = 0x10FFFF + 1 // MaxRune + 1
	i.cclass.cmax = -1
	return i
}

func (re *Regexp) add(i *instr) *instr {
	i.index = len(re.inst)
	re.inst = append(re.inst, i)
	return i
}

type parser struct {
	re    *Regexp
	nlpar int // number of unclosed lpars
	pos   int
	ch    int
}

func (p *parser) error(err Error) {
	panic(err)
}

const endOfFile = -1

func (p *parser) c() int { return p.ch }

func (p *parser) nextc() int {
	if p.pos >= len(p.re.expr) {
		p.ch = endOfFile
	} else {
		c, w := utf8.DecodeRuneInString(p.re.expr[p.pos:])
		p.ch = c
		p.pos += w
	}
	return p.ch
}

func newParser(re *Regexp) *parser {
	p := new(parser)
	p.re = re
	p.nextc() // load p.ch
	return p
}

func special(c int) bool {
	for _, r := range `\.+*?()|[]^$` {
		if c == r {
			return true
		}
	}
	return false
}

func ispunct(c int) bool {
	for _, r := range "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" {
		if c == r {
			return true
		}
	}
	return false
}

var escapes = []byte("abfnrtv")
var escaped = []byte("\a\b\f\n\r\t\v")

func escape(c int) int {
	for i, b := range escapes {
		if int(b) == c {
			return i
		}
	}
	return -1
}

func (p *parser) checkBackslash() int {
	c := p.c()
	if c == '\\' {
		c = p.nextc()
		switch {
		case c == endOfFile:
			p.error(ErrExtraneousBackslash)
		case ispunct(c):
			// c is as delivered
		case escape(c) >= 0:
			c = int(escaped[escape(c)])
		default:
			p.error(ErrBadBackslash)
		}
	}
	return c
}

func (p *parser) charClass() *instr {
	i := newCharClass()
	cc := i.cclass
	if p.c() == '^' {
		cc.negate = true
		p.nextc()
	}
	left := -1
	for {
		switch c := p.c(); c {
		case ']', endOfFile:
			if left >= 0 {
				p.error(ErrBadRange)
			}
			// Is it [^\n]?
			if cc.negate && len(cc.ranges) == 2 &&
				cc.ranges[0] == '\n' && cc.ranges[1] == '\n' {
				nl := &instr{kind: iNotNL}
				p.re.add(nl)
				return nl
			}
			// Special common case: "[a]" -> "a"
			if !cc.negate && len(cc.ranges) == 2 && cc.ranges[0] == cc.ranges[1] {
				c := &instr{kind: iChar, char: cc.ranges[0]}
				p.re.add(c)
				return c
			}
			p.re.add(i)
			return i
		case '-': // do this before backslash processing
			p.error(ErrBadRange)
		default:
			c = p.checkBackslash()
			p.nextc()
			switch {
			case left < 0: // first of pair
				if p.c() == '-' { // range
					p.nextc()
					left = c
				} else { // single char
					cc.addRange(c, c)
				}
			case left <= c: // second of pair
				cc.addRange(left, c)
				left = -1
			default:
				p.error(ErrBadRange)
			}
		}
	}
	panic("unreachable")
}

func (p *parser) term() (start, end *instr) {
	switch c := p.c(); c {
	case '|', endOfFile:
		return nil, nil
	case '*', '+', '?':
		p.error(ErrBareClosure)
	case ')':
		if p.nlpar == 0 {
			p.error(ErrUnmatchedRpar)
		}
		return nil, nil
	case ']':
		p.error(ErrUnmatchedRbkt)
	case '^':
		p.nextc()
		start = p.re.add(&instr{kind: iBOT})
		return start, start
	case '$':
		p.nextc()
		start = p.re.add(&instr{kind: iEOT})
		return start, start
	case '.':
		p.nextc()
		start = p.re.add(&instr{kind: iAny})
		return start, start
	case '[':
		p.nextc()
		start = p.charClass()
		if p.c() != ']' {
			p.error(ErrUnmatchedLbkt)
		}
		p.nextc()
		return start, start
	case '(':
		p.nextc()
		p.nlpar++
		p.re.nbra++ // increment first so first subexpr is \1
		nbra := p.re.nbra
		start, end = p.regexp()
		if p.c() != ')' {
			p.error(ErrUnmatchedLpar)
		}
		p.nlpar--
		p.nextc()
		bra := &instr{kind: iBra, braNum: 2 * nbra}
		p.re.add(bra)
		ebra := &instr{kind: iBra, braNum: 2*nbra + 1}
		p.re.add(ebra)
		if start == nil {
			if end == nil {
				p.error(ErrInternal)
				return
			}
			start = ebra
		} else {
			end.next = ebra
		}
		bra.next = start
		return bra, ebra
	default:
		c = p.checkBackslash()
		p.nextc()
		start = &instr{kind: iChar, char: c}
		p.re.add(start)
		return start, start
	}
	panic("unreachable")
}

func (p *parser) closure() (start, end *instr) {
	start, end = p.term()
	if start == nil {
		return
	}
	switch p.c() {
	case '*':
		// (start,end)*:
		alt := &instr{kind: iAlt}
		p.re.add(alt)
		end.next = alt   // after end, do alt
		alt.left = start // alternate brach: return to start
		start = alt      // alt becomes new (start, end)
		end = alt
	case '+':
		// (start,end)+:
		alt := &instr{kind: iAlt}
		p.re.add(alt)
		end.next = alt   // after end, do alt
		alt.left = start // alternate brach: return to start
		end = alt        // start is unchanged; end is alt
	case '?':
		// (start,end)?:
		alt := &instr{kind: iAlt}
		p.re.add(alt)
		nop := &instr{kind: iNop}
		p.re.add(nop)
		alt.left = start // alternate branch is start
		alt.next = nop   // follow on to nop
		end.next = nop   // after end, go to nop
		start = alt      // start is now alt
		end = nop        // end is nop pointed to by both branches
	default:
		return
	}
	switch p.nextc() {
	case '*', '+', '?':
		p.error(ErrBadClosure)
	}
	return
}

func (p *parser) concatenation() (start, end *instr) {
	for {
		nstart, nend := p.closure()
		switch {
		case nstart == nil: // end of this concatenation
			if start == nil { // this is the empty string
				nop := p.re.add(&instr{kind: iNop})
				return nop, nop
			}
			return
		case start == nil: // this is first element of concatenation
			start, end = nstart, nend
		default:
			end.next = nstart
			end = nend
		}
	}
	panic("unreachable")
}

func (p *parser) regexp() (start, end *instr) {
	start, end = p.concatenation()
	for {
		switch p.c() {
		default:
			return
		case '|':
			p.nextc()
			nstart, nend := p.concatenation()
			alt := &instr{kind: iAlt}
			p.re.add(alt)
			alt.left = start
			alt.next = nstart
			nop := &instr{kind: iNop}
			p.re.add(nop)
			end.next = nop
			nend.next = nop
			start, end = alt, nop
		}
	}
	panic("unreachable")
}

func unNop(i *instr) *instr {
	for i.kind == iNop {
		i = i.next
	}
	return i
}

func (re *Regexp) eliminateNops() {
	for _, inst := range re.inst {
		if inst.kind == iEnd {
			continue
		}
		inst.next = unNop(inst.next)
		if inst.kind == iAlt {
			inst.left = unNop(inst.left)
		}
	}
}

func (re *Regexp) dump() {
	print("prefix <", re.prefix, ">\n")
	for _, inst := range re.inst {
		print(inst.index, ": ")
		inst.print()
		if inst.kind != iEnd {
			print(" -> ", inst.next.index)
		}
		print("\n")
	}
}

func (re *Regexp) doParse() {
	p := newParser(re)
	start := &instr{kind: iStart}
	re.add(start)
	s, e := p.regexp()
	start.next = s
	re.start = start
	e.next = re.add(&instr{kind: iEnd})

	if debug {
		re.dump()
		println()
	}

	re.eliminateNops()
	if debug {
		re.dump()
		println()
	}
	re.setPrefix()
	if debug {
		re.dump()
		println()
	}
}

// Extract regular text from the beginning of the pattern,
// possibly after a leading iBOT.
// That text can be used by doExecute to speed up matching.
func (re *Regexp) setPrefix() {
	var b []byte
	var utf = make([]byte, utf8.UTFMax)
	var inst *instr
	// First instruction is start; skip that.  Also skip any initial iBOT.
	inst = re.inst[0].next
	for inst.kind == iBOT {
		inst = inst.next
	}
Loop:
	for ; inst.kind != iEnd; inst = inst.next {
		// stop if this is not a char
		if inst.kind != iChar {
			break
		}
		// stop if this char can be followed by a match for an empty string,
		// which includes closures, ^, and $.
		switch inst.next.kind {
		case iBOT, iEOT, iAlt:
			break Loop
		}
		n := utf8.EncodeRune(utf, inst.char)
		b = append(b, utf[0:n]...)
	}
	// point prefixStart instruction to first non-CHAR after prefix
	re.prefixStart = inst
	re.prefixBytes = b
	re.prefix = string(b)
}

// String returns the source text used to compile the regular expression.
func (re *Regexp) String() string {
	return re.expr
}

// Compile parses a regular expression and returns, if successful, a Regexp
// object that can be used to match against text.
func Compile(str string) (regexp *Regexp, error os.Error) {
	regexp = new(Regexp)
	// doParse will panic if there is a parse error.
	defer func() {
		if e := recover(); e != nil {
			regexp = nil
			error = e.(Error) // Will re-panic if error was not an Error, e.g. nil-pointer exception
		}
	}()
	regexp.expr = str
	regexp.inst = make([]*instr, 0, 10)
	regexp.doParse()
	return
}

// MustCompile is like Compile but panics if the expression cannot be parsed.
// It simplifies safe initialization of global variables holding compiled regular
// expressions.
func MustCompile(str string) *Regexp {
	regexp, error := Compile(str)
	if error != nil {
		panic(`regexp: compiling "` + str + `": ` + error.String())
	}
	return regexp
}

// NumSubexp returns the number of parenthesized subexpressions in this Regexp.
func (re *Regexp) NumSubexp() int { return re.nbra }

// The match arena allows us to reduce the garbage generated by tossing
// match vectors away as we execute.  Matches are ref counted and returned
// to a free list when no longer active.  Increases a simple benchmark by 22X.
type matchArena struct {
	head *matchVec
	len  int // length of match vector
}

type matchVec struct {
	m    []int // pairs of bracketing submatches. 0th is start,end
	ref  int
	next *matchVec
}

func (a *matchArena) new() *matchVec {
	if a.head == nil {
		const N = 10
		block := make([]matchVec, N)
		for i := 0; i < N; i++ {
			b := &block[i]
			b.next = a.head
			a.head = b
		}
	}
	m := a.head
	a.head = m.next
	m.ref = 0
	if m.m == nil {
		m.m = make([]int, a.len)
	}
	return m
}

func (a *matchArena) free(m *matchVec) {
	m.ref--
	if m.ref == 0 {
		m.next = a.head
		a.head = m
	}
}

func (a *matchArena) copy(m *matchVec) *matchVec {
	m1 := a.new()
	copy(m1.m, m.m)
	return m1
}

func (a *matchArena) noMatch() *matchVec {
	m := a.new()
	for i := range m.m {
		m.m[i] = -1 // no match seen; catches cases like "a(b)?c" on "ac"
	}
	m.ref = 1
	return m
}

type state struct {
	inst     *instr // next instruction to execute
	prefixed bool   // this match began with a fixed prefix
	match    *matchVec
}

// Append new state to to-do list.  Leftmost-longest wins so avoid
// adding a state that's already active.  The matchVec will be inc-ref'ed
// if it is assigned to a state.
func (a *matchArena) addState(s []state, inst *instr, prefixed bool, match *matchVec, pos, end int) []state {
	switch inst.kind {
	case iBOT:
		if pos == 0 {
			s = a.addState(s, inst.next, prefixed, match, pos, end)
		}
		return s
	case iEOT:
		if pos == end {
			s = a.addState(s, inst.next, prefixed, match, pos, end)
		}
		return s
	case iBra:
		match.m[inst.braNum] = pos
		s = a.addState(s, inst.next, prefixed, match, pos, end)
		return s
	}
	l := len(s)
	// States are inserted in order so it's sufficient to see if we have the same
	// instruction; no need to see if existing match is earlier (it is).
	for i := 0; i < l; i++ {
		if s[i].inst == inst {
			return s
		}
	}
	s = append(s, state{inst, prefixed, match})
	match.ref++
	if inst.kind == iAlt {
		s = a.addState(s, inst.left, prefixed, a.copy(match), pos, end)
		// give other branch a copy of this match vector
		s = a.addState(s, inst.next, prefixed, a.copy(match), pos, end)
	}
	return s
}

// Accepts either string or bytes - the logic is identical either way.
// If bytes == nil, scan str.
func (re *Regexp) doExecute(str string, bytestr []byte, pos int) []int {
	var s [2][]state
	s[0] = make([]state, 0, 10)
	s[1] = make([]state, 0, 10)
	in, out := 0, 1
	var final state
	found := false
	end := len(str)
	if bytestr != nil {
		end = len(bytestr)
	}
	anchored := re.inst[0].next.kind == iBOT
	if anchored && pos > 0 {
		return nil
	}
	// fast check for initial plain substring
	if re.prefix != "" {
		advance := 0
		if anchored {
			if bytestr == nil {
				if !strings.HasPrefix(str, re.prefix) {
					return nil
				}
			} else {
				if !bytes.HasPrefix(bytestr, re.prefixBytes) {
					return nil
				}
			}
		} else {
			if bytestr == nil {
				advance = strings.Index(str[pos:], re.prefix)
			} else {
				advance = bytes.Index(bytestr[pos:], re.prefixBytes)
			}
		}
		if advance == -1 {
			return nil
		}
		pos += advance
	}
	arena := &matchArena{nil, 2 * (re.nbra + 1)}
	for startPos := pos; pos <= end; {
		if !found && (pos == startPos || !anchored) {
			// prime the pump if we haven't seen a match yet
			match := arena.noMatch()
			match.m[0] = pos
			s[out] = arena.addState(s[out], re.start.next, false, match, pos, end)
			arena.free(match) // if addState saved it, ref was incremented
		} else if len(s[out]) == 0 {
			// machine has completed
			break
		}
		in, out = out, in // old out state is new in state
		// clear out old state
		old := s[out]
		for _, state := range old {
			arena.free(state.match)
		}
		s[out] = old[0:0] // truncate state vector
		charwidth := 1
		c := endOfFile
		if pos < end {
			if bytestr == nil {
				c, charwidth = utf8.DecodeRuneInString(str[pos:end])
			} else {
				c, charwidth = utf8.DecodeRune(bytestr[pos:end])
			}
		}
		pos += charwidth
		for _, st := range s[in] {
			switch st.inst.kind {
			case iBOT:
			case iEOT:
			case iChar:
				if c == st.inst.char {
					s[out] = arena.addState(s[out], st.inst.next, st.prefixed, st.match, pos, end)
				}
			case iCharClass:
				if st.inst.cclass.matches(c) {
					s[out] = arena.addState(s[out], st.inst.next, st.prefixed, st.match, pos, end)
				}
			case iAny:
				if c != endOfFile {
					s[out] = arena.addState(s[out], st.inst.next, st.prefixed, st.match, pos, end)
				}
			case iNotNL:
				if c != endOfFile && c != '\n' {
					s[out] = arena.addState(s[out], st.inst.next, st.prefixed, st.match, pos, end)
				}
			case iBra:
			case iAlt:
			case iEnd:
				// choose leftmost longest
				if !found || // first
					st.match.m[0] < final.match.m[0] || // leftmost
					(st.match.m[0] == final.match.m[0] && pos-charwidth > final.match.m[1]) { // longest
					if final.match != nil {
						arena.free(final.match)
					}
					final = st
					final.match.ref++
					final.match.m[1] = pos - charwidth
				}
				found = true
			default:
				st.inst.print()
				panic("unknown instruction in execute")
			}
		}
	}
	if final.match == nil {
		return nil
	}
	// if match found, back up start of match by width of prefix.
	if final.prefixed && len(final.match.m) > 0 {
		final.match.m[0] -= len(re.prefix)
	}
	return final.match.m
}

// LiteralPrefix returns a literal string that must begin any match
// of the regular expression re.  It returns the boolean true if the
// literal string comprises the entire regular expression.
func (re *Regexp) LiteralPrefix() (prefix string, complete bool) {
	c := make([]int, len(re.inst)-2) // minus start and end.
	// First instruction is start; skip that.
	i := 0
	for inst := re.inst[0].next; inst.kind != iEnd; inst = inst.next {
		// stop if this is not a char
		if inst.kind != iChar {
			return string(c[:i]), false
		}
		c[i] = inst.char
		i++
	}
	return string(c[:i]), true
}

// MatchString returns whether the Regexp matches the string s.
// The return value is a boolean: true for match, false for no match.
func (re *Regexp) MatchString(s string) bool { return len(re.doExecute(s, nil, 0)) > 0 }

// Match returns whether the Regexp matches the byte slice b.
// The return value is a boolean: true for match, false for no match.
func (re *Regexp) Match(b []byte) bool { return len(re.doExecute("", b, 0)) > 0 }


// MatchString checks whether a textual regular expression
// matches a string.  More complicated queries need
// to use Compile and the full Regexp interface.
func MatchString(pattern string, s string) (matched bool, error os.Error) {
	re, err := Compile(pattern)
	if err != nil {
		return false, err
	}
	return re.MatchString(s), nil
}

// Match checks whether a textual regular expression
// matches a byte slice.  More complicated queries need
// to use Compile and the full Regexp interface.
func Match(pattern string, b []byte) (matched bool, error os.Error) {
	re, err := Compile(pattern)
	if err != nil {
		return false, err
	}
	return re.Match(b), nil
}

// ReplaceAllString returns a copy of src in which all matches for the Regexp
// have been replaced by repl.  No support is provided for expressions
// (e.g. \1 or $1) in the replacement string.
func (re *Regexp) ReplaceAllString(src, repl string) string {
	return re.ReplaceAllStringFunc(src, func(string) string { return repl })
}

// ReplaceAllStringFunc returns a copy of src in which all matches for the
// Regexp have been replaced by the return value of of function repl (whose
// first argument is the matched string).  No support is provided for
// expressions (e.g. \1 or $1) in the replacement string.
func (re *Regexp) ReplaceAllStringFunc(src string, repl func(string) string) string {
	lastMatchEnd := 0 // end position of the most recent match
	searchPos := 0    // position where we next look for a match
	buf := new(bytes.Buffer)
	for searchPos <= len(src) {
		a := re.doExecute(src, nil, searchPos)
		if len(a) == 0 {
			break // no more matches
		}

		// Copy the unmatched characters before this match.
		io.WriteString(buf, src[lastMatchEnd:a[0]])

		// Now insert a copy of the replacement string, but not for a
		// match of the empty string immediately after another match.
		// (Otherwise, we get double replacement for patterns that
		// match both empty and nonempty strings.)
		if a[1] > lastMatchEnd || a[0] == 0 {
			io.WriteString(buf, repl(src[a[0]:a[1]]))
		}
		lastMatchEnd = a[1]

		// Advance past this match; always advance at least one character.
		_, width := utf8.DecodeRuneInString(src[searchPos:])
		if searchPos+width > a[1] {
			searchPos += width
		} else if searchPos+1 > a[1] {
			// This clause is only needed at the end of the input
			// string.  In that case, DecodeRuneInString returns width=0.
			searchPos++
		} else {
			searchPos = a[1]
		}
	}

	// Copy the unmatched characters after the last match.
	io.WriteString(buf, src[lastMatchEnd:])

	return buf.String()
}

// ReplaceAll returns a copy of src in which all matches for the Regexp
// have been replaced by repl.  No support is provided for expressions
// (e.g. \1 or $1) in the replacement text.
func (re *Regexp) ReplaceAll(src, repl []byte) []byte {
	return re.ReplaceAllFunc(src, func([]byte) []byte { return repl })
}

// ReplaceAllFunc returns a copy of src in which all matches for the
// Regexp have been replaced by the return value of of function repl (whose
// first argument is the matched []byte).  No support is provided for
// expressions (e.g. \1 or $1) in the replacement string.
func (re *Regexp) ReplaceAllFunc(src []byte, repl func([]byte) []byte) []byte {
	lastMatchEnd := 0 // end position of the most recent match
	searchPos := 0    // position where we next look for a match
	buf := new(bytes.Buffer)
	for searchPos <= len(src) {
		a := re.doExecute("", src, searchPos)
		if len(a) == 0 {
			break // no more matches
		}

		// Copy the unmatched characters before this match.
		buf.Write(src[lastMatchEnd:a[0]])

		// Now insert a copy of the replacement string, but not for a
		// match of the empty string immediately after another match.
		// (Otherwise, we get double replacement for patterns that
		// match both empty and nonempty strings.)
		if a[1] > lastMatchEnd || a[0] == 0 {
			buf.Write(repl(src[a[0]:a[1]]))
		}
		lastMatchEnd = a[1]

		// Advance past this match; always advance at least one character.
		_, width := utf8.DecodeRune(src[searchPos:])
		if searchPos+width > a[1] {
			searchPos += width
		} else if searchPos+1 > a[1] {
			// This clause is only needed at the end of the input
			// string.  In that case, DecodeRuneInString returns width=0.
			searchPos++
		} else {
			searchPos = a[1]
		}
	}

	// Copy the unmatched characters after the last match.
	buf.Write(src[lastMatchEnd:])

	return buf.Bytes()
}

// QuoteMeta returns a string that quotes all regular expression metacharacters
// inside the argument text; the returned string is a regular expression matching
// the literal text.  For example, QuoteMeta(`[foo]`) returns `\[foo\]`.
func QuoteMeta(s string) string {
	b := make([]byte, 2*len(s))

	// A byte loop is correct because all metacharacters are ASCII.
	j := 0
	for i := 0; i < len(s); i++ {
		if special(int(s[i])) {
			b[j] = '\\'
			j++
		}
		b[j] = s[i]
		j++
	}
	return string(b[0:j])
}

// Find matches in slice b if b is non-nil, otherwise find matches in string s.
func (re *Regexp) allMatches(s string, b []byte, n int, deliver func([]int)) {
	var end int
	if b == nil {
		end = len(s)
	} else {
		end = len(b)
	}

	for pos, i, prevMatchEnd := 0, 0, -1; i < n && pos <= end; {
		matches := re.doExecute(s, b, pos)
		if len(matches) == 0 {
			break
		}

		accept := true
		if matches[1] == pos {
			// We've found an empty match.
			if matches[0] == prevMatchEnd {
				// We don't allow an empty match right
				// after a previous match, so ignore it.
				accept = false
			}
			var width int
			if b == nil {
				_, width = utf8.DecodeRuneInString(s[pos:end])
			} else {
				_, width = utf8.DecodeRune(b[pos:end])
			}
			if width > 0 {
				pos += width
			} else {
				pos = end + 1
			}
		} else {
			pos = matches[1]
		}
		prevMatchEnd = matches[1]

		if accept {
			deliver(matches)
			i++
		}
	}
}

// Find returns a slice holding the text of the leftmost match in b of the regular expression.
// A return value of nil indicates no match.
func (re *Regexp) Find(b []byte) []byte {
	a := re.doExecute("", b, 0)
	if a == nil {
		return nil
	}
	return b[a[0]:a[1]]
}

// FindIndex returns a two-element slice of integers defining the location of
// the leftmost match in b of the regular expression.  The match itself is at
// b[loc[0]:loc[1]].
// A return value of nil indicates no match.
func (re *Regexp) FindIndex(b []byte) (loc []int) {
	a := re.doExecute("", b, 0)
	if a == nil {
		return nil
	}
	return a[0:2]
}

// FindString returns a string holding the text of the leftmost match in s of the regular
// expression.  If there is no match, the return value is an empty string,
// but it will also be empty if the regular expression successfully matches
// an empty string.  Use FindStringIndex or FindStringSubmatch if it is
// necessary to distinguish these cases.
func (re *Regexp) FindString(s string) string {
	a := re.doExecute(s, nil, 0)
	if a == nil {
		return ""
	}
	return s[a[0]:a[1]]
}

// FindStringIndex returns a two-element slice of integers defining the
// location of the leftmost match in s of the regular expression.  The match
// itself is at s[loc[0]:loc[1]].
// A return value of nil indicates no match.
func (re *Regexp) FindStringIndex(s string) []int {
	a := re.doExecute(s, nil, 0)
	if a == nil {
		return nil
	}
	return a[0:2]
}

// FindSubmatch returns a slice of slices holding the text of the leftmost
// match of the regular expression in b and the matches, if any, of its
// subexpressions, as defined by the 'Submatch' descriptions in the package
// comment.
// A return value of nil indicates no match.
func (re *Regexp) FindSubmatch(b []byte) [][]byte {
	a := re.doExecute("", b, 0)
	if a == nil {
		return nil
	}
	ret := make([][]byte, len(a)/2)
	for i := range ret {
		if a[2*i] >= 0 {
			ret[i] = b[a[2*i]:a[2*i+1]]
		}
	}
	return ret
}

// FindSubmatchIndex returns a slice holding the index pairs identifying the
// leftmost match of the regular expression in b and the matches, if any, of
// its subexpressions, as defined by the 'Submatch' and 'Index' descriptions
// in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindSubmatchIndex(b []byte) []int {
	return re.doExecute("", b, 0)
}

// FindStringSubmatch returns a slice of strings holding the text of the
// leftmost match of the regular expression in s and the matches, if any, of
// its subexpressions, as defined by the 'Submatch' description in the
// package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindStringSubmatch(s string) []string {
	a := re.doExecute(s, nil, 0)
	if a == nil {
		return nil
	}
	ret := make([]string, len(a)/2)
	for i := range ret {
		if a[2*i] >= 0 {
			ret[i] = s[a[2*i]:a[2*i+1]]
		}
	}
	return ret
}

// FindStringSubmatchIndex returns a slice holding the index pairs
// identifying the leftmost match of the regular expression in s and the
// matches, if any, of its subexpressions, as defined by the 'Submatch' and
// 'Index' descriptions in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindStringSubmatchIndex(s string) []int {
	return re.doExecute(s, nil, 0)
}

const startSize = 10 // The size at which to start a slice in the 'All' routines.

// FindAll is the 'All' version of Find; it returns a slice of all successive
// matches of the expression, as defined by the 'All' description in the
// package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAll(b []byte, n int) [][]byte {
	if n < 0 {
		n = len(b) + 1
	}
	result := make([][]byte, 0, startSize)
	re.allMatches("", b, n, func(match []int) {
		result = append(result, b[match[0]:match[1]])
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllIndex is the 'All' version of FindIndex; it returns a slice of all
// successive matches of the expression, as defined by the 'All' description
// in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllIndex(b []byte, n int) [][]int {
	if n < 0 {
		n = len(b) + 1
	}
	result := make([][]int, 0, startSize)
	re.allMatches("", b, n, func(match []int) {
		result = append(result, match[0:2])
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllString is the 'All' version of FindString; it returns a slice of all
// successive matches of the expression, as defined by the 'All' description
// in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllString(s string, n int) []string {
	if n < 0 {
		n = len(s) + 1
	}
	result := make([]string, 0, startSize)
	re.allMatches(s, nil, n, func(match []int) {
		result = append(result, s[match[0]:match[1]])
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllStringIndex is the 'All' version of FindStringIndex; it returns a
// slice of all successive matches of the expression, as defined by the 'All'
// description in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllStringIndex(s string, n int) [][]int {
	if n < 0 {
		n = len(s) + 1
	}
	result := make([][]int, 0, startSize)
	re.allMatches(s, nil, n, func(match []int) {
		result = append(result, match[0:2])
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllSubmatch is the 'All' version of FindSubmatch; it returns a slice
// of all successive matches of the expression, as defined by the 'All'
// description in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllSubmatch(b []byte, n int) [][][]byte {
	if n < 0 {
		n = len(b) + 1
	}
	result := make([][][]byte, 0, startSize)
	re.allMatches("", b, n, func(match []int) {
		slice := make([][]byte, len(match)/2)
		for j := range slice {
			if match[2*j] >= 0 {
				slice[j] = b[match[2*j]:match[2*j+1]]
			}
		}
		result = append(result, slice)
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllSubmatchIndex is the 'All' version of FindSubmatchIndex; it returns
// a slice of all successive matches of the expression, as defined by the
// 'All' description in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllSubmatchIndex(b []byte, n int) [][]int {
	if n < 0 {
		n = len(b) + 1
	}
	result := make([][]int, 0, startSize)
	re.allMatches("", b, n, func(match []int) {
		result = append(result, match)
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllStringSubmatch is the 'All' version of FindStringSubmatch; it
// returns a slice of all successive matches of the expression, as defined by
// the 'All' description in the package comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllStringSubmatch(s string, n int) [][]string {
	if n < 0 {
		n = len(s) + 1
	}
	result := make([][]string, 0, startSize)
	re.allMatches(s, nil, n, func(match []int) {
		slice := make([]string, len(match)/2)
		for j := range slice {
			if match[2*j] >= 0 {
				slice[j] = s[match[2*j]:match[2*j+1]]
			}
		}
		result = append(result, slice)
	})
	if len(result) == 0 {
		return nil
	}
	return result
}

// FindAllStringSubmatchIndex is the 'All' version of
// FindStringSubmatchIndex; it returns a slice of all successive matches of
// the expression, as defined by the 'All' description in the package
// comment.
// A return value of nil indicates no match.
func (re *Regexp) FindAllStringSubmatchIndex(s string, n int) [][]int {
	if n < 0 {
		n = len(s) + 1
	}
	result := make([][]int, 0, startSize)
	re.allMatches(s, nil, n, func(match []int) {
		result = append(result, match)
	})
	if len(result) == 0 {
		return nil
	}
	return result
}
