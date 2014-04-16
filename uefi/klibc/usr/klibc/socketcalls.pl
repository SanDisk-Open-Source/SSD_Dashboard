#!/usr/bin/perl

$v = $ENV{'KBUILD_VERBOSE'};
$quiet = defined($v) ? !$v : 0;

@args = ();
for $arg ( @ARGV ) {
    if ( $arg =~ /^-/ ) {
	if ( $arg eq '-q' ) {
	    $quiet = 1;
	} else {
	    die "$0: Unknown option: $arg\n";
	}
    } else {
	push(@args, $arg);
    }
}
($file, $arch, $outputdir) = @args;

if (!open(FILE, "< $file")) {
    die "$file: $!\n";
}

print "socketcall-objs := ";
while ( defined($line = <FILE>) ) {
    chomp $line;
    $line =~ s/\s*(|\#.*|\/\/.*)$//;	# Strip comments and trailing blanks
    next unless $line;

    if ( $line =~ /^\s*\<\?\>\s*(.*)\s+([_a-zA-Z][_a-zA-Z0-9]+)\s*\((.*)\)\s*\;$/ ) {
	$type = $1;
	$name = $2;
	$argv = $3;

	@args = split(/\s*\,\s*/, $argv);
	@cargs = ();

	$i = 0;
	for $arg ( @args ) {
	    push(@cargs, "$arg a".$i++);
	}
	$nargs = $i;
	print " \\\n\t${name}.o";

	if ( $arch eq 'i386' ) {
	    open(OUT, "> ${outputdir}/${name}.S")
		or die "$0: Cannot open ${outputdir}/${name}.S\n";

	    print OUT "#include <sys/socketcalls.h>\n";
	    print OUT "\n";
	    print OUT "\t.text\n";
	    print OUT "\t.align	4\n";
	    print OUT "\t.globl	${name}\n";
	    print OUT "\t.type	${name},\@function\n";
	    print OUT "${name}:\n";
	    print OUT "\tpushl	\$SYS_\U${name}\n";
	    print OUT "\tjmp	__socketcall_common\n";
	    print OUT "\t.size ${name},.-${name}\n";
	    close(OUT);
	} else {
	    open(OUT, "> ${outputdir}/${name}.c")
		or die "$0: Cannot open ${outputdir}/${name}.c\n";

	    print OUT "#include \"socketcommon.h\"\n";
	    print OUT "\n";
	    print OUT "#if _KLIBC_SYS_SOCKETCALL || !defined(__NR_${name})\n\n";

	    print OUT "extern long __socketcall(int, const unsigned long *);\n\n";

	    print OUT "$type $name (", join(', ', @cargs), ")\n";
	    print OUT "{\n";
	    print OUT "    unsigned long args[$nargs];\n";
	    for ( $i = 0 ; $i < $nargs ; $i++ ) {
		print OUT "    args[$i] = (unsigned long)a$i;\n";
	    }
	    print OUT "    return ($type) __socketcall(SYS_\U${name}\E, args);\n";
	    print OUT "}\n\n";

	    print OUT "#endif\n";

	    close(OUT);
	}
    } else {
	die "$file:$.: Could not parse input\n";
    }
}

print "\n";
