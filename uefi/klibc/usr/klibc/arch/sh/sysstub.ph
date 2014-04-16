# -*- perl -*-
#
# arch/sh/sysstub.ph
#
# Script to generate system call stubs
#

sub make_sysstub($$$$$@) {
    my($outputdir, $fname, $type, $sname, $stype, @args) = @_;

    open(OUT, '>', "${outputdir}/${fname}.S");
    print OUT "#include <asm/unistd.h>\n";
    print OUT "\n";
    print OUT "\t.section\t\".text.syscall\",\"ax\"\n";
    print OUT "\t.type\t${fname},\#function\n";
    print OUT "\t.globl\t${fname}\n";
    print OUT "\t.align\t2\n";
    print OUT "\t.import __syscall_common\n";
    print OUT "${fname}:\n";
    print OUT "\t  mov.l\t1f, r3\n";
    print OUT "\t  jmp\t\@r3\n";
    print OUT "#if __NR_${sname} >= 128\n";
    print OUT "\t  mov.l\t2f, r3\n";
    print OUT "#else\n";
    print OUT "\t  mov\t# __NR_${sname}, r3\n";
    print OUT "#endif\n";
    print OUT "\t.size ${fname},.-${fname}\n";
    print OUT "\n";
    print OUT "\t.align\t2\n";
    print OUT "\t.import\t__syscall_common\n";
    print OUT "1:\t.long\t__syscall_common\n";
    print OUT "#if __NR_${sname} >= 128\n";
    print OUT "2:\t.long\t__NR_${sname}\n";
    print OUT "#endif\n";
    close(OUT);
}

1;
