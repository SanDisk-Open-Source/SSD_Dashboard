#!/bin/sh

# update-pciids.sh is licensed under the GNU General Public License
# (GPL) version 2 or above.
# 
# Copyright (C) 2008-2010 Anibal Monsalve Salazar <anibal@debian.org>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# Please read the "COPYING" file in the archive root, or visit
# http://www.gnu.org/licenses/gpl.html, for information about the GPL.
#
# This scipt is a rewrite of a script with the same name by
# Martin Mares.

set -e

#URL="http://pci-ids.ucw.cz/pci.ids"
URL="http://pciids.sourceforge.net/v2.2/pci.ids"
FILE="pci.ids"

RM="/bin/rm"
MV="/bin/mv"
SED="/bin/sed"
GREP="/bin/grep"
ECHO="/bin/echo"
CHMOD="/bin/chmod"
CHOWN="/bin/chown"
GUNZIP="/bin/gunzip"
BUNZIP2="/bin/bunzip2"
TOUCH="/usr/bin/touch"
WGET="/usr/bin/wget"
CURL="/usr/bin/curl"
LYNX="/usr/bin/lynx"

NEWFILE="$FILE.new"
OLDFILE="$FILE.old"

if [ "$1" = "-q" ]
then
    quiet="yes"
else
    quiet="no"
fi

if ! $TOUCH $NEWFILE > /dev/null 2>&1
then
    $ECHO >&2 "update-pciids: $NEWFILE is read-only"
    exit 1
fi

[ -f $NEWFILE ]     && $RM $NEWFILE
[ -f $NEWFILE.bz2 ] && $RM $NEWFILE.bz2
[ -f $NEWFILE.gz ]  && $RM $NEWFILE.gz

if [ -x $BUNZIP2 ]
then
    EXT=".bz2"
    UNZIP=$BUNZIP2
elif [ -x $GUNZIP ]
then
    EXT=".gz"
    UNZIP=$GUNZIP
else
    $ECHO >&2 "update-pciids: cannot find bunzip2 or gunzip"
    exit 1
fi

if [ -x $WGET ]
then
    $WGET -nv -O $NEWFILE$EXT $URL$EXT > /dev/null 2>&1
elif [ -x $CURL ]
then
    $CURL -o $NEWFILE$EXT $URL$EXT > /dev/null 2>&1
elif [ -x $LYNX ]
then
    $LYNX -source $URL$EXT > $NEWFILE$EXT
else
    $ECHO >&2 "update-pciids: cannot find wget, curl or lynx"
    exit 1
fi

$UNZIP < $NEWFILE$EXT > $NEWFILE
$RM $NEWFILE$EXT

if ! $GREP > /dev/null "^C " $NEWFILE
then
    $ECHO >&2 "update-pciids: missing class info, probably truncated file"
    exit 1
fi

date=$( $GREP -E "^#[[:space:]]Date:[[:space:]]*" $NEWFILE 2> /dev/null | $SED "s/^#[[:space:]]Date:[[:space:]]*//" )

if [ -z "$date" ]
then
    $ECHO >&2 "update-pciids: missing snapshot date, probably truncated file"
    exit 1
fi 

if [ -f $FILE ]
then
    $CHOWN --reference="$FILE" "$NEWFILE"
    $CHMOD --reference="$FILE" "$NEWFILE"
    [ -f $OLDFILE ] && $RM $OLDFILE
    $MV $FILE $OLDFILE
fi

$MV $NEWFILE $FILE
$TOUCH -d "$date" $FILE

if [ -f $FILE.gz ]
then
    [ -f $OLDFILE.gz ] && $RM $OLDFILE.gz
    $MV $FILE.gz $OLDFILE.gz
fi

if [ $quiet = "no" ]
then
    $ECHO "Downloaded daily snapshot dated $date"
fi

exit 0
