
require 'mkmf'

$CFLAGS << " -fPIC -O3 "
$srcs = ["vitesse.c", "hescape.c", "fast_blank.c"]

create_makefile("vitesse")
