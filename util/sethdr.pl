#!/usr/bin/perl
#
# Calculate 32-bit checksum for Atari cartridge image(s).
# Adds Atari800 emulator cartridge header if missing.
# If Atari800 emulator cartridge header is detected, the checksum
# field of the header is updated.
#
# Written by Nathan Hartwell
#
# 1.0 - 12/3/2001 - Initial release
#

use integer;
use Fcntl;
use File::stat;
$| = 1;

foreach $arg (0 .. $#ARGV)
{
  $file = $ARGV[$arg];
top:
  $sb = stat($file);
  $size = $sb->size;
  $mode = $sb->mode;
  $uid = $sb->uid;
  $gid = $sb->gid;
  sysopen(ROM, $file, O_RDWR);
  if (!ROM)
  {
    goto loop;
  }
  #($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks) = stat(ROM);
  binmode(ROM);

  if ($size == 131088 or $size == 65552 or $size == 40976 or $size == 32784 or $size == 16400 or $size == 8208 or $size == 4112)
  {
    $size -= 16;
    $szK = $size / 1024;
    seek(ROM, 7, 0);
    sysread(ROM, $ct, 1);
    $csum0 = 0;
    foreach (1 .. 4)
    {
      $csum0 *= 256;
      sysread(ROM, $bv, 1);
      $csum0 += ord($bv);
    }
    seek(ROM, 16, 0);
  }
  else
  {
    sysopen(OUT, "$file$$", O_CREAT|O_RDWR, $mode);
    binmode(OUT);
    sysread(ROM, $buf, $size);
    syswrite(OUT, "CART", 4);
    foreach (1 .. 12)
    {
      syswrite(OUT, chr(0), 1);
    }
    syswrite(OUT, $buf, $size);
    close(OUT);
    close(ROM);
    unlink $file;
    rename "$file$$",$file;
    chown $uid, $gid, $file;
    goto top;
  }

  print "ROM: $file";

  $csum = 0;
  foreach (1 .. $size)
  {
    sysread(ROM, $raw, 1);
    $csum += ord($raw);
  }

  if ($csum ne $csum0)
  {
    print " - Bad Checksum";
  }
  else
  {
    print " - Checksum OK";
  }

  $valid = "\r\n";

  if ($ct ne ' ')
  {
    printf " - Current Type: %s", '@' | $ct;
  }
  print "\n\nSelect cartridge type:\n\n";
  if ($szK == 8)
  {
    print "  A) Standard 8k (800/XL/XE)\n";
    $valid = "$valid"."A";
  }
  if ($szK == 16)
  {
    print "  B) Standard 16k (800/XL/XE)\n";
    print "  C) OSS '034M' 16k (800/XL/XE)\n";
    $valid = "$valid"."BC";
  }
  if ($szK == 32)
  {
    print "  D) Standard 32k (5200)\n";
    print "  E) DB 32k (800/XL/XE)\n";
    print "  F) 2 chip 32k (5200)\n";
    $valid = "$valid"."DEF";
  }
  if ($szK == 40)
  {
    print "  G) Bounty Bob Strikes Back 40k (5200)\n";
    $valid = "$valid"."G";
  }
  if ($szK == 64)
  {
    print "  H) 8x8k D50x 64k (800/XL/XE)\n";
    print "  I) Express 64k (800/XL/XE)\n";
    print "  J) Diamond 64k (800/XL/XE)\n";
    print "  K) SpartaDOS X 64k (800/XL/XE)\n";
    $valid = "$valid"."HIJK";
  }
  if ($szK == 32)
  {
    print "  L) XEGS 32k (800/XL/XE)\n";
    $valid = "$valid"."L";
  }
  if ($szK == 64)
  {
    print "  M) XEGS 64k (800/XL/XE)\n";
    $valid = "$valid"."M";
  }
  if ($szK == 128)
  {
    print "  N) XEGS 128k (800/XL/XE)\n";
    $valid = "$valid"."N";
  }
  if ($szK == 16)
  {
    print "  O) OSS 'M091' 16k (800/XL/XE)\n";
    print "  P) 1 chip 16k (5200)\n";
    $valid = "$valid"."OP";
  }
  if ($szK == 128)
  {
    print "  Q) Atrax 128k (800/XL/XE)\n";
    $valid = "$valid"."Q";
  }
  if ($szK == 40)
  {
    print "  R) Bounty Bob Strikes Back 40k (800/XL/XE)\n";
    $valid = "$valid"."R";
  }
  if ($szK == 8)
  {
    print "  S) Standard 8k (5200)\n";
    $valid = "$valid"."S";
  }
  if ($szK == 4)
  {
    print "  T) Standard 4k (5200)\n";
    $valid = "$valid"."T";
  }
  if ($szK == 8)
  {
    print "  U) Right Slot 8k (800)\n";
    $valid = "$valid"."U";
  }
  print "\n  Just press [Enter] to keep current header value.\n\n";
  print "    Choose: ";

gkey:
  $key = getone();
  if ($key ge 'a' and $key le 'z')
  {
    $key &= chr(0x5F);
  }
  $pv = index($valid, $key);
  if ($pv == -1)
  {
    goto gkey;
  }
  if ($pv == 0 or $pv == 1)
  {
    $key = '@' | $ct;
  }
  else
  {
    $key &= chr(0x1F);
  }

  seek(ROM, 0, 0);
  syswrite(ROM, "CART", 4);
  foreach (1 .. 3)
  {
    syswrite(ROM, chr(0), 1);
  }
  if ($key eq 127)
  {
    seek(ROM, 8, 0);
  }
  else
  {
    syswrite(ROM, $key, 1);
  }
  $csum2 = swap($csum);
  foreach (1 .. 4)
  {
    $bv = $csum2 & 0xFF;
    $csum2 >>= 8;
    syswrite(ROM, chr($bv), 1);
  }
  foreach (1 .. 4)
  {
    syswrite(ROM, chr(0), 1);
  }

  if ($key and $key ne 127)
  {
    printf "%s", $key | '@';
  }
  print "\n\n";

  close(ROM);
loop:
}
exit;

sub swap {
  my $val = shift;

  $b1 = $val & 0xFF;
  $b2 = ($val & 0xFF00) >> 8;
  $b3 = ($val & 0xFF0000) >> 16;
  $b4 = ($val & 0xFF000000) >> 24;
  $flipt = ($b1 << 24) + ($b2 << 16) + ($b3 << 8) + $b4;
  return $flipt;
}

BEGIN {
        use POSIX qw(:termios_h);


        my ($term, $oterm, $echo, $noecho, $fd_stdin);


        $fd_stdin = fileno(STDIN);


        $term     = POSIX::Termios->new();
        $term->getattr($fd_stdin);
        $oterm     = $term->getlflag();


        $echo     = ECHO | ECHOK | ICANON;
        $noecho   = $oterm & ~$echo;


        sub cbreak {
            $term->setlflag($noecho);
            $term->setcc(VTIME, 1);
            $term->setattr($fd_stdin, TCSANOW);
        }


        sub cooked {
            $term->setlflag($oterm);
            $term->setcc(VTIME, 0);
            $term->setattr($fd_stdin, TCSANOW);
        }


        sub getone {
            my $key = '';
            cbreak();
            sysread(STDIN, $key, 1);
            cooked();
            return $key;
        }


    }
    END { cooked() }


