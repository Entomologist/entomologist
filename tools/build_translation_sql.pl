#!/usr/bin/perl

if ($#ARGV + 1 != 1)
{
  print "Usage: $0 <path to mantis lang dir>\n";
  exit(1);
}

my $path = $ARGV[0];

opendir(TRANS, $path );
@files = readdir(TRANS);
closedir(TRANS);

print "CREATE TABLE translations (local TEXT, entomologist TEXT);\n";
# Extra fields just to make sure everything works out
print "INSERT INTO translations (local, entomologist) VALUES (\"Id\", \"id\");\n";

foreach $file(@files)
{
  if ($file =~ /string_*/)
  {
    parse_file("$path/$file");
  }
}
exit(0);

sub parse_file
{
  my $lang_file = $_[0];
  open FILE, $lang_file or die $!;
  while (<FILE>)
  {
    $line = $_;
    chomp $line;
    if ($line =~ m/^\$s_email_project\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"project\");\n";
    }
    elsif ($line =~ m/^\$s_id\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"id\");\n";
    }
    elsif ($line =~ m/^\$s_assigned_to\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"assigned_to\");\n";
    }
    elsif ($line =~ m/^\$s_reporter\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"reporter\");\n";
    }
    elsif ($line =~ m/^\$s_priority\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"priority\");\n";
    }
    elsif ($line =~ m/^\$s_severity\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"severity\");\n";
    }
    elsif ($line =~ m/^\$s_category\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"category\");\n";
    }
    elsif ($line =~ m/^\$s_updated\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"updated\");\n";
    }
    elsif ($line =~ m/^\$s_summary\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"summary\");\n";
    }
    elsif ($line =~ m/^\$s_status\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"status\");\n";
    }
    elsif ($line =~ m/^\$s_reproducibility\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"reproducibility\");\n";
    }
    elsif ($line =~ m/^\$s_os\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"os\");\n";
    }
    elsif ($line =~ m/^\$s_os_version\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"os_version\");\n";
    }
    elsif ($line =~ m/^\$s_product_version\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"product_version\");\n";
    }
    elsif ($line =~ m/^\$s_resolution\s*=\s*\'(.*)\'.*$/)
    {
      print "INSERT INTO translations (local, entomologist) VALUES (\"$1\", \"resolution\");\n";
    }                                                                                                                                                        
  }
}
