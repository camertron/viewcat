#ifndef HESCAPE_H
#define HESCAPE_H

#include <sys/types.h>
#include <stdint.h>
#include "ruby.h"

/*
 * Replace characters according to the following rules.
 * Note that this function can handle only ASCII-compatible string.
 *
 * " => &quot;
 * & => &amp;
 * ' => &#39;
 * < => &lt;
 * > => &gt;
 *
 * @return size of dest. If it's larger than len, dest is required to be freed.
 */
extern VALUE rb_escape_html(VALUE str);

#endif
