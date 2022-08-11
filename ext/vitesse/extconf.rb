
require 'mkmf'

extension_name = "vitesse"
dir_config(extension_name)

$CFLAGS << " -fPIC -O3 "
$srcs = ["vitesse.c", "hescape.c", "fast_blank.c"]

create_makefile(extension_name)
