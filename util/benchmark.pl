#!/usr/bin/perl -w
# Performance tester for the Atari800 emulator
# by Piotr Fusik <fox@scene.pl>
use strict;

# defaults
my $test = '';
my $target = '';
my $cflags = '';
my $default_cflags = '-O2 -Wall';
my @features = ();
my $reference_program = 'dc.xex';  # "Drunk Chessboard"
my $frames = 3 * 60 * 50;          # 3 Atari minutes
   # note that "Drunk Chessboard" ends after 3 minutes
my $output = '';


# create 'benchmark' directory under 'src/'
sub make_directory() {
	unless (-d 'benchmark') {
		print "'benchmark' directory does not exist, creating\n";
		mkdir 'benchmark' or die "Failed to create 'benchmark' directory\n";
	}
}

# runs a command (the output goes to the console)
sub run_command(@) {
	print "Running: @_\n";
	system @_ and die "$[0] failed\n";
}

# runs a command and captures its output
sub pipe_command(@) {
	print "Running: @_\n";
	my $result = `@_`;
	die "$[0] failed\n" if $?;
	return $result;
}

# addresses of Atari hardware registers
my @hwregs = (
	0xd000 .. 0xd01f, # GTIA
	0xd200 .. 0xd21f, # POKEY (stereo)
	0xd300 .. 0xd303, # PIA
	0xd400 .. 0xd40f  # ANTIC
);
# built-in Atari programs
my %programs = (
	# yes, I really wrote them here directly in the machine language
	'blank.xex' =>
		"\xFF\xFF\x00\x06\x39\x06" .
		"\x78\xEE\x0E\xD4\xEE\x00\xD4" .
		"\x8D\x0A\xD4" x 16 .
		"\x4C\x07\x06" .
		"\xE0\x02\xE1\x02\x00\x06",
	'incD01A.xex' =>
		"\xFF\xFF\x00\x06\x39\x06" .
		"\x78\xEE\x0E\xD4\xEE\x00\xD4" .
		"\xEE\x1A\xD0" x 16 .
		"\x4C\x07\x06" .
		"\xE0\x02\xE1\x02\x00\x06",
	'flash.xex' =>
		"\xFF\xFF\x00\x06\x1B\x06" .
		"\x78\xA9\x00\x8D\x0E\xD4\x8D\x00\xD4" .
		"\x8D\x0A\xD4\x8D\x0A\xD4" .
		"\xAE\x0B\xD4\xD0\xF5" .
		"\x8D\x1A\xD0\x49\xA4" .
		"\x4C\x09\x06" .
		"\xE0\x02\xE1\x02\x00\x06",
	'ramread.xex' =>
		"\xFF\xFF\x00\x06" . pack('v', 0x060b + 3 * @hwregs) .
		"\x78\xA9\x00\x8D\x0E\xD4\x8D\x00\xD4" .
		"\xAD\xFF\x05" x @hwregs .
		"\x4C\x09\x06" .
		"\xE0\x02\xE1\x02\x00\x06",
	'ramstore.xex' =>
		"\xFF\xFF\x00\x06" . pack('v', 0x060b + 3 * @hwregs) .
		"\x78\xA9\x00\x8D\x0E\xD4\x8D\x00\xD4" .
		"\x8D\xFF\x05" x @hwregs .
		"\x4C\x09\x06" .
		"\xE0\x02\xE1\x02\x00\x06",
	'hwread.xex' =>
		"\xFF\xFF\x00\x06" . pack('v', 0x060b + 3 * @hwregs) .
		"\x78\xA9\x00\x8D\x0E\xD4\x8D\x00\xD4" .
		join('', map("\xAD" . pack('v', $_), @hwregs)) .
		"\x4C\x09\x06" .
		"\xE0\x02\xE1\x02\x00\x06",
	'hwstore.xex' =>
		"\xFF\xFF\x00\x06" . pack('v', 0x060b + 3 * @hwregs) .
		"\x78\xA9\x00\x8D\x0E\xD4\x8D\x00\xD4" .
		join('', map("\x8D" . pack('v', $_), @hwregs)) .
		"\x4C\x09\x06" .
		"\xE0\x02\xE1\x02\x00\x06"
);

# write a built-in program to a file
sub generate_program($) {
	my $program = shift;
	print "Generating $program\n";
	open XEX, ">benchmark/$program"
	and binmode XEX
	and print XEX $programs{$program}
	and close XEX
	or die "$!\n";
}

# we will be working in the src directory
if (-e 'atari.c') {
	# ok
}
elsif (-e '../src/atari.c') {
	# script was run from the util/ directory
	chdir '../src' or die "Can't chdir to '../src'\n";
}
else {
	die "atari.c not found\n";
}

# supported targets
my @targets = qw(
	basic curses ncurses pdcurses dosvga falcon svgalib sdl windx
	x11 x11-motif x11-shm x11-xview x11-xview-shm motif shm xview xview-shm
);

my $help_me = 0;

# get command line options
for (@ARGV) {
	if (/^--test=(.+)/) {
		$test = $1;
	}
	elsif (/^--target=(.+)/) {
		$target = $1;
		grep $target eq $_, @targets or die "$target is not a valid target\n";
	}
	elsif (/^--cflags=(.+)/) {
		$cflags = $1;
	}
	elsif (/^--(?:en|dis)able-\w+/) {
		push @features, $_;
	}
	elsif (/^--program=(.+)/) {
		$reference_program = $1;
	}
	elsif (/^--frames=(\d+)$/) {
		$frames = $1;
	}
	elsif (/^--output=(.+)/) {
		$output = $1;
	}
	elsif ($_ eq '--generate-programs') {
		make_directory();
		generate_program($_) for sort keys %programs;
		exit;
	}
	elsif (/^-?-h(elp)?$/) {
		$help_me = 1;
	}
	else {
		die "Unknown option: $_\n";
	}
}

# gfx-generating target: dosvga for DJGPP, windx on Win32, sdl otherwise
my $gfx_target = 'sdl';
if ($^O =~ /win|dos/i) {
	$gfx_target = `gcc -dumpmachine` =~ /djgpp/i ? 'dosvga' : 'windx';
}

# must initialize this after parsing the command line
# in order to fill in $reference_program
my %tests = (
	'default' => {
		'target' => $gfx_target,
		'cflags' => '-D DONT_DISPLAY',
		'run' => [ $reference_program, 'blank.xex' ]
	},
	'basic' => {
		'target' => 'basic',
		'run' => [ $reference_program, 'blank.xex' ]
	},
	'monitorbreak' => {
		'target' => 'basic',
		'config' => [ '--disable-monitorbreak', '--enable-monitorbreak' ],
		'run' => [ $reference_program ],
	},
	'pagedattrib' => {
		'target' => 'basic',
		'config' => [ '--disable-pagedattrib', '--enable-pagedattrib' ],
		'run' => [ $reference_program, 'ramread.xex', 'ramstore.xex', 'hwread.xex', 'hwstore.xex' ],
	},
	'cycleexact' => {
		'target' => $gfx_target,
		'cflags' => '-D DONT_DISPLAY',
		'config' => [ '--disable-cycleexact --disable-newcycleexact',
		              '--enable-cycleexact --disable-newcycleexact',
		              '--disable-cycleexact --enable-newcycleexact',
		              '--enable-cycleexact --enable-newcycleexact' ],
		'run' => [ $reference_program, 'incD01A.xex' ]
	},
	'display' => {
		'target' => $gfx_target,
		'run' => [ $reference_program, 'blank.xex', 'flash.xex' ]
	}
);

if (@ARGV == 0 || $help_me) {
	# display help and exit
	print <<EOF;
benchmark.pl tests performance of the Atari800 emulator with different
configuration options and/or different Atari programs.

Available options:
--test=<test>         Choose test (required)
--target=<target>     Choose Atari800 target for the test
--cflags="<cflags>"   Override CFLAGS (default: "-O2 -Wall" + test-specific)
--enable-<feature>    Enable Atari800 feature via "configure"
--disable-<feature>   Disable Atari800 feature via "configure"
--program=<filename>  Choose Atari program to be run (defaults to $reference_program)
--frames=<frames>     Set number of frames to be run (defaults to $frames)
--output=<filename>   Output the results to the specified file
--generate-programs   Just generate all the built-in Atari programs

Available tests:
  default       Compare chosen Atari program with one that does nothing,
                using default configuration (may be modified
                with "--enable-<feature>", "--disable-<feature>")
                (default target: $gfx_target)
  basic         Compare chosen Atari program with one that does nothing
                (default target: basic)
  monitorbreak  Compare configurations with/without MONITOR_BREAK
                (default target: basic)
  pagedattrib   Compare configurations with/without PAGED_ATTRIB
                (default target: basic)
  cycleexact    Compare configurations with/without (NEW_)CYCLE_EXACT
                (default target: $gfx_target)
  display       Compare display performance with different Atari programs
                (default target: $gfx_target)

Available Atari800 targets:
  @targets[0..8]
  @targets[9..17]

EOF
	exit;
}

unless (exists $tests{$test}) {
	die "$test is not an available test\n";
}

# autoconf stuff
unless (-e 'config.h.in') {
	print "'config.h.in' not found\n";
	run_command('autoheader');
}
unless (-e 'configure') {
	print "'configure' not found\n";
	run_command('autoconf');
}

# create our directory under 'src/'
make_directory();

# create Atari800 config file with ROM paths
unless (-r 'benchmark/atari800.cfg') {
	print "'benchmark/atari800.cfg' does not exist, creating\n";
	open CFG, '>benchmark/atari800.cfg'
	and print CFG <<EOF
Atari 800 Emulator, Version 1.4.0
OS/A_ROM=benchmark/atariosa.rom
OS/B_ROM=benchmark/atariosb.rom
XL/XE_ROM=benchmark/atarixl.rom
BASIC_ROM=benchmark/ataribas.rom
5200_ROM=benchmark/atari5200.rom
MACHINE_TYPE=Atari XL/XE
RAM_SIZE=64
EOF
	and close CFG
	or die "$!\n";
}

# check if ROMs are configured
print "Checking 'benchmark/atari800.cfg'\n";
open CFG, 'benchmark/atari800.cfg' or die "$!\n";
while (<CFG>) {
	m!^(?:XL/XE|BASIC)_ROM=(.*?)\s*$! or next;
	my $romfile = $1;
	unless (-r $romfile) {
		die <<EOF;
$romfile not found.
Place ROM files in the 'benchmark' directory or edit 'benchmark/atari800.cfg',
then re-run this script.
EOF
	}
}
close CFG;

# create output file
unless ($output) {
	my ($sec, $min, $hour, $mday, $mon, $year) = localtime;
	$output = sprintf 'benchmark/%s_%04d-%02d-%02d_%02d-%02d-%02d.txt', $test,
		$year + 1900, $mon + 1, $mday, $hour, $min, $sec;
}

print "Writing output to: $output\n";
open OUT, ">$output" or die "$!\n";

print "Running test: $test\n";
print OUT "Test: $test\n";

# get test settings
my $test_settings = $tests{$test};

$target ||= $test_settings->{'target'};

$cflags ||= exists($test_settings->{'cflags'})
          ? $default_cflags . ' ' . $test_settings->{'cflags'}
		  : $default_cflags;
$cflags .= ' -D BENCHMARK=' . $frames;
$ENV{'CFLAGS'} = $cflags;
print "Using CFLAGS: $cflags\n";
print OUT "CFLAGS=$cflags\n";

# compile each configuration
my @configs = $test_settings->{'config'} ? @{$test_settings->{'config'}} : ('');
for my $config (@configs) {
	print '-' x 76, "\n";
	# "@{[]}" trick in this print is to avoid two consecutive spaces when @features is empty
	print OUT "./configure --target=$target --disable-sound @{[@features, $config]}\n";
	run_command('sh', './configure', "--target=$target", '--disable-sound', @features, split(' ', $config));
	run_command('make', 'clean');
	run_command('make');
	# run each program
	for my $program (@{$test_settings->{'run'}}) {
		if (-r $program) {
			# ok
		}
		elsif (-r "benchmark/$program") {
			$program = "benchmark/$program";
		}
		elsif (exists $programs{$program}) {
			generate_program($program);
			$program = "benchmark/$program";
		}
		else {
			die "$program does not exist\n";
		}
		my $result = pipe_command('./atari800', '-config', 'benchmark/atari800.cfg', $program);
		print $result;
		# parse result
		$result =~ /\d+ frames emulated in ([0-9.]+) seconds/
			or die "Expected 'frames emulated in'\n";
		my $speed_msg = "$1 seconds";
		# avoid division by zero
		if ($1) {
			# assuming PAL, real Atari needs (0.02 * $frames) time
			$speed_msg .= sprintf ' (%d%% of real Atari speed)', 100 * 0.02 * $frames / $1;
		}
		print "$speed_msg\n\n";
		printf OUT "./atari800 %-23s # %s\n", $program, $speed_msg;
	}
}

print OUT "Test complete.\n";
close OUT;
