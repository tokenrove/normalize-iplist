require 'mkmf'

extension_name = 'normalize_iplist'

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']
$CFLAGS = %w(-Wall -Wextra -pedantic -std=gnu11 -O3 -g).join(' ')

create_makefile(extension_name)
