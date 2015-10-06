require 'mkmf'

extension_name = 'normalize_iplist'

RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC'] if ENV['CC']
$CFLAGS = %w(-Wall -Wextra -std=gnu99 -fms-extensions -O3 -g).join(' ')

create_makefile(extension_name)
