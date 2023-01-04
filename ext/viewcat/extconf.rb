require 'mkmf'

$CFLAGS << " -fPIC -O3 -msse4 "
$srcs = ["viewcat.c", "hescape.c", "fast_blank.c"]

create_makefile("viewcat/viewcat")
