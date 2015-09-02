require 'mkmf'

extension_name = 'normalize_iplist'

$CFLAGS = %w(-Wall -Wextra -pedantic -std=gnu99 -O3 -g).join(' ')

create_makefile(extension_name)
