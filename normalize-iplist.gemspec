Gem::Specification.new do |s|
  s.name = "normalize-iplist"
  s.version = "0.6"
  s.email = "julian.squires@adgear.com"
  s.summary = "IP list normalization/serialization"
  s.files = ["Rakefile", "README.rdoc", "ext/normalize_iplist.c", "ext/extconf.rb"]
  s.required_ruby_version = '>= 2.2.0'
  s.extensions = "ext/extconf.rb"
  s.author = "Julian Squires"
  s.rubyforge_project = "None"
  s.description = "IP list normalization/serialization."
end
