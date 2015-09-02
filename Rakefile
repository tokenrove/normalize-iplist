
require 'rubygems'
require 'rake'

require 'rubygems/package_task'
require 'rake/extensiontask'
require 'rake/testtask'
require 'rake/clean'

EXT_CONF = 'ext/extconf.rb'
MAKEFILE = 'ext/Makefile'
MODULE = 'ext/normalize_iplist.so'
SRC = Dir.glob('ext/*.c')
SRC << MAKEFILE

CLEAN.include ['ext/*.o', 'ext/depend', MODULE]
CLOBBER.include ['config.save', 'ext/mkmf.log', MAKEFILE]

file MAKEFILE => EXT_CONF do |t|
  Dir::chdir(File::dirname(EXT_CONF)) do
    unless sh "ruby #{File::basename(EXT_CONF)}"
      $stderr.puts "Failed to run extconf"
      break
    end
  end
end
file MODULE => SRC do |t|
  Dir::chdir(File::dirname(EXT_CONF)) do
    unless sh "make"
      $stderr.puts "make failed"
      break
    end
  end
end
desc "Build the native library"
task :build => MODULE

Rake::TestTask.new do |t|
  t.libs << "ext"
  t.test_files = FileList['test/test*.rb']
  t.verbose = true
end
