// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package os_test

import (
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	. "os"
	"strings"
	"syscall"
	"testing"
)

var dot = []string{
	"env_unix.go",
	"error.go",
	"file.go",
	"os_test.go",
	"time.go",
	"types.go",
}

type sysDir struct {
	name  string
	files []string
}

var sysdir = func() (sd *sysDir) {
	switch syscall.OS {
	case "windows":
		sd = &sysDir{
			Getenv("SystemRoot") + "\\system32\\drivers\\etc",
			[]string{
				"hosts",
				"networks",
				"protocol",
				"services",
			},
		}
	default:
		sd = &sysDir{
			"/etc",
			[]string{
				"group",
				"hosts",
				"passwd",
			},
		}
	}
	return
}()

func size(name string, t *testing.T) int64 {
	file, err := Open(name, O_RDONLY, 0)
	defer file.Close()
	if err != nil {
		t.Fatal("open failed:", err)
	}
	var buf [100]byte
	len := 0
	for {
		n, e := file.Read(buf[0:])
		len += n
		if e == EOF {
			break
		}
		if e != nil {
			t.Fatal("read failed:", err)
		}
	}
	return int64(len)
}

func equal(name1, name2 string) (r bool) {
	switch syscall.OS {
	case "windows":
		r = strings.ToLower(name1) == strings.ToLower(name2)
	default:
		r = name1 == name2
	}
	return
}

func newFile(testName string, t *testing.T) (f *File) {
	// Use a local file system, not NFS.
	// On Unix, override $TMPDIR in case the user
	// has it set to an NFS-mounted directory.
	dir := ""
	if syscall.OS != "windows" {
		dir = "/tmp"
	}
	f, err := ioutil.TempFile(dir, "_Go_"+testName)
	if err != nil {
		t.Fatalf("open %s: %s", testName, err)
	}
	return
}

var sfdir = sysdir.name
var sfname = sysdir.files[0]

func TestStat(t *testing.T) {
	path := sfdir + "/" + sfname
	dir, err := Stat(path)
	if err != nil {
		t.Fatal("stat failed:", err)
	}
	if !equal(sfname, dir.Name) {
		t.Error("name should be ", sfname, "; is", dir.Name)
	}
	filesize := size(path, t)
	if dir.Size != filesize {
		t.Error("size should be", filesize, "; is", dir.Size)
	}
}

func TestFstat(t *testing.T) {
	path := sfdir + "/" + sfname
	file, err1 := Open(path, O_RDONLY, 0)
	defer file.Close()
	if err1 != nil {
		t.Fatal("open failed:", err1)
	}
	dir, err2 := file.Stat()
	if err2 != nil {
		t.Fatal("fstat failed:", err2)
	}
	if !equal(sfname, dir.Name) {
		t.Error("name should be ", sfname, "; is", dir.Name)
	}
	filesize := size(path, t)
	if dir.Size != filesize {
		t.Error("size should be", filesize, "; is", dir.Size)
	}
}

func TestLstat(t *testing.T) {
	path := sfdir + "/" + sfname
	dir, err := Lstat(path)
	if err != nil {
		t.Fatal("lstat failed:", err)
	}
	if !equal(sfname, dir.Name) {
		t.Error("name should be ", sfname, "; is", dir.Name)
	}
	filesize := size(path, t)
	if dir.Size != filesize {
		t.Error("size should be", filesize, "; is", dir.Size)
	}
}

func testReaddirnames(dir string, contents []string, t *testing.T) {
	file, err := Open(dir, O_RDONLY, 0)
	defer file.Close()
	if err != nil {
		t.Fatalf("open %q failed: %v", dir, err)
	}
	s, err2 := file.Readdirnames(-1)
	if err2 != nil {
		t.Fatalf("readdirnames %q failed: %v", dir, err2)
	}
	for _, m := range contents {
		found := false
		for _, n := range s {
			if n == "." || n == ".." {
				t.Errorf("got %s in directory", n)
			}
			if equal(m, n) {
				if found {
					t.Error("present twice:", m)
				}
				found = true
			}
		}
		if !found {
			t.Error("could not find", m)
		}
	}
}

func testReaddir(dir string, contents []string, t *testing.T) {
	file, err := Open(dir, O_RDONLY, 0)
	defer file.Close()
	if err != nil {
		t.Fatalf("open %q failed: %v", dir, err)
	}
	s, err2 := file.Readdir(-1)
	if err2 != nil {
		t.Fatalf("readdir %q failed: %v", dir, err2)
	}
	for _, m := range contents {
		found := false
		for _, n := range s {
			if equal(m, n.Name) {
				if found {
					t.Error("present twice:", m)
				}
				found = true
			}
		}
		if !found {
			t.Error("could not find", m)
		}
	}
}

func TestReaddirnames(t *testing.T) {
	testReaddirnames(".", dot, t)
	testReaddirnames(sysdir.name, sysdir.files, t)
}

func TestReaddir(t *testing.T) {
	testReaddir(".", dot, t)
	testReaddir(sysdir.name, sysdir.files, t)
}

// Read the directory one entry at a time.
func smallReaddirnames(file *File, length int, t *testing.T) []string {
	names := make([]string, length)
	count := 0
	for {
		d, err := file.Readdirnames(1)
		if err != nil {
			t.Fatalf("readdir %q failed: %v", file.Name(), err)
		}
		if len(d) == 0 {
			break
		}
		names[count] = d[0]
		count++
	}
	return names[0:count]
}

// Check that reading a directory one entry at a time gives the same result
// as reading it all at once.
func TestReaddirnamesOneAtATime(t *testing.T) {
	// big directory that doesn't change often.
	dir := "/usr/bin"
	if syscall.OS == "windows" {
		dir = Getenv("SystemRoot") + "\\system32"
	}
	file, err := Open(dir, O_RDONLY, 0)
	defer file.Close()
	if err != nil {
		t.Fatalf("open %q failed: %v", dir, err)
	}
	all, err1 := file.Readdirnames(-1)
	if err1 != nil {
		t.Fatalf("readdirnames %q failed: %v", dir, err1)
	}
	file1, err2 := Open(dir, O_RDONLY, 0)
	if err2 != nil {
		t.Fatalf("open %q failed: %v", dir, err2)
	}
	small := smallReaddirnames(file1, len(all)+100, t) // +100 in case we screw up
	for i, n := range all {
		if small[i] != n {
			t.Errorf("small read %q mismatch: %v", small[i], n)
		}
	}
}

func TestHardLink(t *testing.T) {
	// Hardlinks are not supported under windows.
	if syscall.OS == "windows" {
		return
	}
	from, to := "hardlinktestfrom", "hardlinktestto"
	Remove(from) // Just in case.
	file, err := Open(to, O_CREAT|O_WRONLY, 0666)
	if err != nil {
		t.Fatalf("open %q failed: %v", to, err)
	}
	defer Remove(to)
	if err = file.Close(); err != nil {
		t.Errorf("close %q failed: %v", to, err)
	}
	err = Link(to, from)
	if err != nil {
		t.Fatalf("link %q, %q failed: %v", to, from, err)
	}
	defer Remove(from)
	tostat, err := Stat(to)
	if err != nil {
		t.Fatalf("stat %q failed: %v", to, err)
	}
	fromstat, err := Stat(from)
	if err != nil {
		t.Fatalf("stat %q failed: %v", from, err)
	}
	if tostat.Dev != fromstat.Dev || tostat.Ino != fromstat.Ino {
		t.Errorf("link %q, %q did not create hard link", to, from)
	}
}

func TestSymLink(t *testing.T) {
	// Symlinks are not supported under windows.
	if syscall.OS == "windows" {
		return
	}
	from, to := "symlinktestfrom", "symlinktestto"
	Remove(from) // Just in case.
	file, err := Open(to, O_CREAT|O_WRONLY, 0666)
	if err != nil {
		t.Fatalf("open %q failed: %v", to, err)
	}
	defer Remove(to)
	if err = file.Close(); err != nil {
		t.Errorf("close %q failed: %v", to, err)
	}
	err = Symlink(to, from)
	if err != nil {
		t.Fatalf("symlink %q, %q failed: %v", to, from, err)
	}
	defer Remove(from)
	tostat, err := Stat(to)
	if err != nil {
		t.Fatalf("stat %q failed: %v", to, err)
	}
	if tostat.FollowedSymlink {
		t.Fatalf("stat %q claims to have followed a symlink", to)
	}
	fromstat, err := Stat(from)
	if err != nil {
		t.Fatalf("stat %q failed: %v", from, err)
	}
	if tostat.Dev != fromstat.Dev || tostat.Ino != fromstat.Ino {
		t.Errorf("symlink %q, %q did not create symlink", to, from)
	}
	fromstat, err = Lstat(from)
	if err != nil {
		t.Fatalf("lstat %q failed: %v", from, err)
	}
	if !fromstat.IsSymlink() {
		t.Fatalf("symlink %q, %q did not create symlink", to, from)
	}
	fromstat, err = Stat(from)
	if err != nil {
		t.Fatalf("stat %q failed: %v", from, err)
	}
	if !fromstat.FollowedSymlink {
		t.Fatalf("stat %q did not follow symlink", from)
	}
	s, err := Readlink(from)
	if err != nil {
		t.Fatalf("readlink %q failed: %v", from, err)
	}
	if s != to {
		t.Fatalf("after symlink %q != %q", s, to)
	}
	file, err = Open(from, O_RDONLY, 0)
	if err != nil {
		t.Fatalf("open %q failed: %v", from, err)
	}
	file.Close()
}

func TestLongSymlink(t *testing.T) {
	// Symlinks are not supported under windows.
	if syscall.OS == "windows" {
		return
	}
	s := "0123456789abcdef"
	// Long, but not too long: a common limit is 255.
	s = s + s + s + s + s + s + s + s + s + s + s + s + s + s + s
	from := "longsymlinktestfrom"
	Remove(from) // Just in case.
	err := Symlink(s, from)
	if err != nil {
		t.Fatalf("symlink %q, %q failed: %v", s, from, err)
	}
	defer Remove(from)
	r, err := Readlink(from)
	if err != nil {
		t.Fatalf("readlink %q failed: %v", from, err)
	}
	if r != s {
		t.Fatalf("after symlink %q != %q", r, s)
	}
}

func TestRename(t *testing.T) {
	from, to := "renamefrom", "renameto"
	Remove(to) // Just in case.
	file, err := Open(from, O_CREAT|O_WRONLY, 0666)
	if err != nil {
		t.Fatalf("open %q failed: %v", to, err)
	}
	if err = file.Close(); err != nil {
		t.Errorf("close %q failed: %v", to, err)
	}
	err = Rename(from, to)
	if err != nil {
		t.Fatalf("rename %q, %q failed: %v", to, from, err)
	}
	defer Remove(to)
	_, err = Stat(to)
	if err != nil {
		t.Errorf("stat %q failed: %v", to, err)
	}
}

func TestForkExec(t *testing.T) {
	var cmd, adir, expect string
	var args []string
	r, w, err := Pipe()
	if err != nil {
		t.Fatalf("Pipe: %v", err)
	}
	if syscall.OS == "windows" {
		cmd = Getenv("COMSPEC")
		args = []string{Getenv("COMSPEC"), "/c cd"}
		adir = Getenv("SystemRoot")
		expect = Getenv("SystemRoot") + "\r\n"
	} else {
		cmd = "/bin/pwd"
		args = []string{"pwd"}
		adir = "/"
		expect = "/\n"
	}
	pid, err := ForkExec(cmd, args, nil, adir, []*File{nil, w, Stderr})
	if err != nil {
		t.Fatalf("ForkExec: %v", err)
	}
	w.Close()

	var b bytes.Buffer
	io.Copy(&b, r)
	output := b.String()
	if output != expect {
		args[0] = cmd
		t.Errorf("exec %q returned %q wanted %q", strings.Join(args, " "), output, expect)
	}
	Wait(pid, 0)
}

func checkMode(t *testing.T, path string, mode uint32) {
	dir, err := Stat(path)
	if err != nil {
		t.Fatalf("Stat %q (looking for mode %#o): %s", path, mode, err)
	}
	if dir.Mode&0777 != mode {
		t.Errorf("Stat %q: mode %#o want %#o", path, dir.Mode, mode)
	}
}

func TestChmod(t *testing.T) {
	// Chmod is not supported under windows.
	if syscall.OS == "windows" {
		return
	}
	f := newFile("TestChmod", t)
	defer Remove(f.Name())
	defer f.Close()

	if err := Chmod(f.Name(), 0456); err != nil {
		t.Fatalf("chmod %s 0456: %s", f.Name(), err)
	}
	checkMode(t, f.Name(), 0456)

	if err := f.Chmod(0123); err != nil {
		t.Fatalf("chmod %s 0123: %s", f.Name(), err)
	}
	checkMode(t, f.Name(), 0123)
}

func checkUidGid(t *testing.T, path string, uid, gid int) {
	dir, err := Stat(path)
	if err != nil {
		t.Fatalf("Stat %q (looking for uid/gid %d/%d): %s", path, uid, gid, err)
	}
	if dir.Uid != uid {
		t.Errorf("Stat %q: uid %d want %d", path, dir.Uid, uid)
	}
	if dir.Gid != gid {
		t.Errorf("Stat %q: gid %d want %d", path, dir.Gid, gid)
	}
}

func TestChown(t *testing.T) {
	// Chown is not supported under windows.
	if syscall.OS == "windows" {
		return
	}
	// Use TempDir() to make sure we're on a local file system,
	// so that the group ids returned by Getgroups will be allowed
	// on the file.  On NFS, the Getgroups groups are
	// basically useless.
	f := newFile("TestChown", t)
	defer Remove(f.Name())
	defer f.Close()
	dir, err := f.Stat()
	if err != nil {
		t.Fatalf("stat %s: %s", f.Name(), err)
	}

	// Can't change uid unless root, but can try
	// changing the group id.  First try our current group.
	gid := Getgid()
	t.Log("gid:", gid)
	if err = Chown(f.Name(), -1, gid); err != nil {
		t.Fatalf("chown %s -1 %d: %s", f.Name(), gid, err)
	}
	checkUidGid(t, f.Name(), dir.Uid, gid)

	// Then try all the auxiliary groups.
	groups, err := Getgroups()
	if err != nil {
		t.Fatalf("getgroups: %s", err)
	}
	t.Log("groups: ", groups)
	for _, g := range groups {
		if err = Chown(f.Name(), -1, g); err != nil {
			t.Fatalf("chown %s -1 %d: %s", f.Name(), g, err)
		}
		checkUidGid(t, f.Name(), dir.Uid, g)

		// change back to gid to test fd.Chown
		if err = f.Chown(-1, gid); err != nil {
			t.Fatalf("fchown %s -1 %d: %s", f.Name(), gid, err)
		}
		checkUidGid(t, f.Name(), dir.Uid, gid)
	}
}

func checkSize(t *testing.T, f *File, size int64) {
	dir, err := f.Stat()
	if err != nil {
		t.Fatalf("Stat %q (looking for size %d): %s", f.Name(), size, err)
	}
	if dir.Size != size {
		t.Errorf("Stat %q: size %d want %d", f.Name(), dir.Size, size)
	}
}

func TestTruncate(t *testing.T) {
	f := newFile("TestTruncate", t)
	defer Remove(f.Name())
	defer f.Close()

	checkSize(t, f, 0)
	f.Write([]byte("hello, world\n"))
	checkSize(t, f, 13)
	f.Truncate(10)
	checkSize(t, f, 10)
	f.Truncate(1024)
	checkSize(t, f, 1024)
	f.Truncate(0)
	checkSize(t, f, 0)
	f.Write([]byte("surprise!"))
	checkSize(t, f, 13+9) // wrote at offset past where hello, world was.
}

// Use TempDir() to make sure we're on a local file system,
// so that timings are not distorted by latency and caching.
// On NFS, timings can be off due to caching of meta-data on
// NFS servers (Issue 848).
func TestChtimes(t *testing.T) {
	f := newFile("TestChtimes", t)
	defer Remove(f.Name())
	defer f.Close()

	f.Write([]byte("hello, world\n"))
	f.Close()

	preStat, err := Stat(f.Name())
	if err != nil {
		t.Fatalf("Stat %s: %s", f.Name(), err)
	}

	// Move access and modification time back a second
	const OneSecond = 1e9 // in nanoseconds
	err = Chtimes(f.Name(), preStat.Atime_ns-OneSecond, preStat.Mtime_ns-OneSecond)
	if err != nil {
		t.Fatalf("Chtimes %s: %s", f.Name(), err)
	}

	postStat, err := Stat(f.Name())
	if err != nil {
		t.Fatalf("second Stat %s: %s", f.Name(), err)
	}

	if postStat.Atime_ns >= preStat.Atime_ns {
		t.Errorf("Atime_ns didn't go backwards; was=%d, after=%d",
			preStat.Atime_ns,
			postStat.Atime_ns)
	}

	if postStat.Mtime_ns >= preStat.Mtime_ns {
		t.Errorf("Mtime_ns didn't go backwards; was=%d, after=%d",
			preStat.Mtime_ns,
			postStat.Mtime_ns)
	}
}

func TestChdirAndGetwd(t *testing.T) {
	// TODO(brainman): file.Chdir() is not implemented on windows.
	if syscall.OS == "windows" {
		return
	}
	fd, err := Open(".", O_RDONLY, 0)
	if err != nil {
		t.Fatalf("Open .: %s", err)
	}
	// These are chosen carefully not to be symlinks on a Mac
	// (unlike, say, /var, /etc, and /tmp).
	dirs := []string{"/", "/usr/bin"}
	for mode := 0; mode < 2; mode++ {
		for _, d := range dirs {
			if mode == 0 {
				err = Chdir(d)
			} else {
				fd1, err := Open(d, O_RDONLY, 0)
				if err != nil {
					t.Errorf("Open %s: %s", d, err)
					continue
				}
				err = fd1.Chdir()
				fd1.Close()
			}
			pwd, err1 := Getwd()
			err2 := fd.Chdir()
			if err2 != nil {
				// We changed the current directory and cannot go back.
				// Don't let the tests continue; they'll scribble
				// all over some other directory.
				fmt.Fprintf(Stderr, "fchdir back to dot failed: %s\n", err2)
				Exit(1)
			}
			if err != nil {
				fd.Close()
				t.Fatalf("Chdir %s: %s", d, err)
			}
			if err1 != nil {
				fd.Close()
				t.Fatalf("Getwd in %s: %s", d, err1)
			}
			if pwd != d {
				fd.Close()
				t.Fatalf("Getwd returned %q want %q", pwd, d)
			}
		}
	}
	fd.Close()
}

func TestTime(t *testing.T) {
	// Just want to check that Time() is getting something.
	// A common failure mode on Darwin is to get 0, 0,
	// because it returns the time in registers instead of
	// filling in the structure passed to the system call.
	// Too bad the compiler doesn't know that
	// 365.24*86400 is an integer.
	sec, nsec, err := Time()
	if sec < (2009-1970)*36524*864 {
		t.Errorf("Time() = %d, %d, %s; not plausible", sec, nsec, err)
	}
}

func TestSeek(t *testing.T) {
	f := newFile("TestSeek", t)
	defer Remove(f.Name())
	defer f.Close()

	const data = "hello, world\n"
	io.WriteString(f, data)

	type test struct {
		in     int64
		whence int
		out    int64
	}
	var tests = []test{
		{0, 1, int64(len(data))},
		{0, 0, 0},
		{5, 0, 5},
		{0, 2, int64(len(data))},
		{0, 0, 0},
		{-1, 2, int64(len(data)) - 1},
		{1 << 33, 0, 1 << 33},
		{1 << 33, 2, 1<<33 + int64(len(data))},
	}
	for i, tt := range tests {
		off, err := f.Seek(tt.in, tt.whence)
		if off != tt.out || err != nil {
			if e, ok := err.(*PathError); ok && e.Error == EINVAL && tt.out > 1<<32 {
				// Reiserfs rejects the big seeks.
				// http://code.google.com/p/go/issues/detail?id=91
				break
			}
			t.Errorf("#%d: Seek(%v, %v) = %v, %v want %v, nil", i, tt.in, tt.whence, off, err, tt.out)
		}
	}
}

type openErrorTest struct {
	path  string
	mode  int
	error Error
}

var openErrorTests = []openErrorTest{
	{
		sfdir + "/no-such-file",
		O_RDONLY,
		ENOENT,
	},
	{
		sfdir,
		O_WRONLY,
		EISDIR,
	},
	{
		sfdir + "/" + sfname + "/no-such-file",
		O_WRONLY,
		ENOTDIR,
	},
}

func TestOpenError(t *testing.T) {
	for _, tt := range openErrorTests {
		f, err := Open(tt.path, tt.mode, 0)
		if err == nil {
			t.Errorf("Open(%q, %d) succeeded", tt.path, tt.mode)
			f.Close()
			continue
		}
		perr, ok := err.(*PathError)
		if !ok {
			t.Errorf("Open(%q, %d) returns error of %T type; want *os.PathError", tt.path, tt.mode, err)
		}
		if perr.Error != tt.error {
			t.Errorf("Open(%q, %d) = _, %q; want %q", tt.path, tt.mode, perr.Error.String(), tt.error.String())
		}
	}
}

func run(t *testing.T, cmd []string) string {
	// Run /bin/hostname and collect output.
	r, w, err := Pipe()
	if err != nil {
		t.Fatal(err)
	}
	pid, err := ForkExec("/bin/hostname", []string{"hostname"}, nil, "/", []*File{nil, w, Stderr})
	if err != nil {
		t.Fatal(err)
	}
	w.Close()

	var b bytes.Buffer
	io.Copy(&b, r)
	Wait(pid, 0)
	output := b.String()
	if n := len(output); n > 0 && output[n-1] == '\n' {
		output = output[0 : n-1]
	}
	if output == "" {
		t.Fatalf("%v produced no output", cmd)
	}

	return output
}


func TestHostname(t *testing.T) {
	// There is no other way to fetch hostname on windows, but via winapi.
	if syscall.OS == "windows" {
		return
	}
	// Check internal Hostname() against the output of /bin/hostname.
	// Allow that the internal Hostname returns a Fully Qualified Domain Name
	// and the /bin/hostname only returns the first component
	hostname, err := Hostname()
	if err != nil {
		t.Fatalf("%v", err)
	}
	want := run(t, []string{"/bin/hostname"})
	if hostname != want {
		i := strings.Index(hostname, ".")
		if i < 0 || hostname[0:i] != want {
			t.Errorf("Hostname() = %q, want %q", hostname, want)
		}
	}
}

func TestReadAt(t *testing.T) {
	f := newFile("TestReadAt", t)
	defer Remove(f.Name())
	defer f.Close()

	const data = "hello, world\n"
	io.WriteString(f, data)

	b := make([]byte, 5)
	n, err := f.ReadAt(b, 7)
	if err != nil || n != len(b) {
		t.Fatalf("ReadAt 7: %d, %r", n, err)
	}
	if string(b) != "world" {
		t.Fatalf("ReadAt 7: have %q want %q", string(b), "world")
	}
}

func TestWriteAt(t *testing.T) {
	f := newFile("TestWriteAt", t)
	defer Remove(f.Name())
	defer f.Close()

	const data = "hello, world\n"
	io.WriteString(f, data)

	n, err := f.WriteAt([]byte("WORLD"), 7)
	if err != nil || n != 5 {
		t.Fatalf("WriteAt 7: %d, %v", n, err)
	}

	b, err := ioutil.ReadFile(f.Name())
	if err != nil {
		t.Fatalf("ReadFile %s: %v", f.Name(), err)
	}
	if string(b) != "hello, WORLD\n" {
		t.Fatalf("after write: have %q want %q", string(b), "hello, WORLD\n")
	}
}

func writeFile(t *testing.T, fname string, flag int, text string) string {
	f, err := Open(fname, flag, 0666)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	n, err := io.WriteString(f, text)
	if err != nil {
		t.Fatalf("WriteString: %d, %v", n, err)
	}
	f.Close()
	data, err := ioutil.ReadFile(fname)
	if err != nil {
		t.Fatalf("ReadFile: %v", err)
	}
	return string(data)
}

func TestAppend(t *testing.T) {
	const f = "append.txt"
	defer Remove(f)
	s := writeFile(t, f, O_CREAT|O_TRUNC|O_RDWR, "new")
	if s != "new" {
		t.Fatalf("writeFile: have %q want %q", s, "new")
	}
	s = writeFile(t, f, O_APPEND|O_RDWR, "|append")
	if s != "new|append" {
		t.Fatalf("writeFile: have %q want %q", s, "new|append")
	}
}

func TestStatDirWithTrailingSlash(t *testing.T) {
	// Create new dir, in _test so it will get
	// cleaned up by make if not by us.
	path := "_test/_TestStatDirWithSlash_"
	err := MkdirAll(path, 0777)
	if err != nil {
		t.Fatalf("MkdirAll %q: %s", path, err)
	}
	defer RemoveAll(path)

	// Stat of path should succeed.
	_, err = Stat(path)
	if err != nil {
		t.Fatal("stat failed:", err)
	}

	// Stat of path+"/" should succeed too.
	_, err = Stat(path + "/")
	if err != nil {
		t.Fatal("stat failed:", err)
	}
}
