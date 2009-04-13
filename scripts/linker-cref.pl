#!/usr/bin/perl

local $use = 0;
local $sym = "";
local %SYM = ();

local $all = (0 <= $#ARGV && "-a" eq $ARGV[0]);

foreach(`make LDFLAGS=-Wl,--cref`)
#foreach(`make DEBUG=0 NO_LOG_DEBUG=1 NO_LOG_INFO=1 NO_LOG_WARN=1 NO_LOG_ERROR=1 LDFLAGS=-Wl,--cref`)
{
  chomp;
  if (m#^Cross Reference Table#s)
  {
    $use = 1;
  }
  elsif(m#^make\[1\]\:#s)
  {
    $use = 0;
  }
  elsif($use && "" ne $_ && !m#^Symbol\s+File$#s)
  {
    @_ = split(/\s+/, $_);
    ($#_ < 1 || 1 < $#_) && die("Oops: split has $#_ on '$_'!");
    $sym = $_[0] if ("" ne $_[0]);
    $SYM{$sym} .= " " if defined($SYM{$sym});
    $SYM{$sym} .= $_[1];
  }
}

%REF = ();

foreach $sym(keys %SYM)
{
  foreach(split(/ /, $SYM{$sym}))
  {
    $REF{$_} .= " " if defined($REF{$_});
    $REF{$_} .= $sym;
  }
}

print "Static Candidates\n";
print "=================\n";

foreach(sort keys %REF)
{
  my $m = $_;
  foreach(sort split(/ /, $REF{$_}))
  {
    @_ = split(/ /, $SYM{$_});
    if (0 == $#_)
    {
      if ($all || !($_[0] =~ m#/lib#s))
      {
        if ("" ne $m)
        {
          print "\n$m\n";
          $m = "";
        }
        print "\t$_\n"
      }
    }
  }
}

print "\n";
print "Symbols in Files\n";
print "================\n";

foreach(sort keys %REF)
{
  print "$_\n";
  foreach(sort split(/ /, $REF{$_}))
  {
    print "\t$_\n";
  }
}

print "\n";
print "Files using Symbols\n";
print "===================\n";

foreach(sort keys %SYM)
{
  print "$_\n";
  foreach(sort split(/ /, $SYM{$_}))
  {
    print "\t$_\n";
  }
}

