file "ext/vitesse/Makefile" do
  Dir.chdir("ext/vitesse") do
    system "ruby extconf.rb"
  end
end

task build: ["ext/vitesse/Makefile"] do
  Dir.chdir("ext/vitesse") do
    system "make"
  end
end
