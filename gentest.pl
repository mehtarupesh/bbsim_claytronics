#!/usr/bin/perl

my ($prog) = ($0 =~ m!([^/]+)$!);
use Cwd;
use Expect;
use Time::HiRes;

$verbose = 0;
$sim = `which sim`;
($sim eq "") && die("Can't find sim command");
$sim =~ s/[ \r\t\n]+$//;

$vm = `which meldvm`;
($vm eq "") && die("Can't find meldvm command");
$vm =~ s/[ \r\t\n]+$//;

print STDERR "[$sim] [$vm]\n";
$port = 5000;
$cmdargs = "";

# 
# process options
#

while ($ARGV[0] =~ /^-/) {
    $_ = shift @ARGV;
  option: 
    {
	if (/^-p/) {
	    $port = shift @ARGV;
	    last option;
	}
	if (/^-[h?]/) {
	    &usage("");
	    last option;
	}
	if (/^-v/) {
	    $verbose++;
	    last option;
	}
	&usage("Unknown option: $_\n");
    }
}

# now expect three args: meldfile to run, config to run it on, and output name for test

($#ARGV < (3-1)) && &usage("Expected 3 arguments");

$meldfile = shift @ARGV;
( ! -e $meldfile ) && &usage("Expected a meld byte code file, Could not find $meldfile");

$configfile = shift @ARGV;
( ! -e $configfile ) && &usage("Expected a config file, Could not find $configfile");

$testfile = shift @ARGV;
( -e $testfile ) && &usage("Expected a name for a test file, but $testfile already exists");

$_ = shift @ARGV;
$simargs = "";
if (/^--$/) {
    # rest of ARGV are args to simulator
    $simargs = join(' ', @ARGV);
}

my @params = ("-c", $configfile, "-n", "-i", 10);
if ($simargs ne "") {
    @params = (@params, split(' ', $simargs));
}

print STDERR "Spawning $sim\n";
my $simprocess = Expect->spawn($sim, @params) or die "Cannot spawn $sim: $!\n";
$simprocess->log_file("sim.log", "w");
Time::HiRes::sleep(0.1);
print STDERR "Spawning $vm\n";
my $vmprocess = Expect->spawn($vm, ("-p", $port, "-f", $meldfile)) or die "Cannot spawn $vm: $!\n";
while ($simprocess->expect(undef)) {
  print $simprocess->before();
}
open(F, "<sim.log") || die("Can't open sim.log");
open(G, ">$testfile") || die("Can't open $testfile");
while (<F>) {
    /Final Status/ && last;
}
print G "config: $configfile\n";
print G "meld: $meldfile\n\n";
while (<F>) {
    s/[ \t\r\n]*$//;
    print G "$_\n";
}
close(F);
close(G);

################################################################
#	say what we do
#

sub usage {
    my ($msg) = @_;
    ($msg ne "") && print STDERR "$prog:Error: $msg\n";

    print STDERR <<"EndEndEnd";

 $prog [-v] [-p <port>] [-h] prog-name config-name test-name [-- args-to-sim]

   -v:          more verbose (repeat these for more and more)
   -p <port>:	run sim on port <port>
   -h:		show this
   prog-name:	meld byte code file to run
   config-name: configuration to run it on
   test-name:	path to test file spec that is created
   --:		rest of args are passed to simulator

EndEndEnd
    exit 0;
}

