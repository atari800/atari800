#!/bin/perl
#
# Inserts version number and command line options into readme.html
#
# Copyright (C) 2002  Krzysztof Nikiel
#

die ("usage $0 <readme.in file> <usage.txt file> <version header>\n",
     "\tadditional options are read from stdin\n",
    ) if ($#ARGV < 2);

open (README, "<$ARGV[0]") || die "can't open $ARGV[0]";
open (USAGE, "<$ARGV[1]") || die "can't open $ARGV[1]";
open (VHEAD, "<$ARGV[2]") || die "can't open $ARGV[2]";

@opts = ();
@usagetab = ();

# get version number
$version = "";
while (<VHEAD>)
{
  chomp;

  if (/\s+(\d+.\d+\.\d+)"$/) {$version = $1}
}
if ($version eq "") {die "can't find version number in $ARGV[2]"}

# parse usage file
#
# find global options
while (<USAGE>)
{
  chomp;
  
  #  print "*$_\n";
  if (/^Options/) {
    while (<USAGE>) {
    if (/^-------/) {last}
    else {die "malformed Oprions header($_)\n"};
    }
    last;
  };
}

# read global options
while (<USAGE>)
{
  chomp;
  
  # print "$_\n";
  if (/^\s*-/) {addopt($_);} 
  elsif (/^\s*$/) {}
  else {last};
}

# read more options
while (<stdin>)
{
  chomp;
  
  if (/^\s*-/) {addopt($_)}
}

# merge options with pattern file
while (<README>)
{
  chomp;
  
  if (/\$A800VERSION/) {s/\$A800VERSION/$version/g}
  
  if (/^\$HELPMENU/) {
    foreach $line (@usagetab) {
      print "$line\n";
    }    
  }
  else {
    print "$_\n";
  }
}

sub addopt
{
  my $test;
  my $opt;
  my $dscr;

  $opt = "";
  if (/^\s*(\S+.*\S+)\s{2,}(\S+.*\S+)\s*$/) {
    $opt = $1;
    $dscr = $2;
  } elsif (/^\s*(\S+.*\S+)\t+(\S+.*\S+)\s*$/) {
    $opt = $1;
    $dscr = $2;
  } else {die "can't parse \'$_\'"}
  
  $opt =~ s/</&lt/g;
  
  for $test (@opts) {
    if ($test eq $opt) {
      $opt = "";
      last;
    }
  }
  if ($opt ne "") {
    push (@opts, $opt);
    push (@usagetab, "<li> <kbd>$opt</kbd> $dscr");
    #    print "<li> <kbd>$opt</kbd> $dscr\n";
  }
}

