package KernelWedge;

use strict;
use warnings;

BEGIN {
	use Exporter ();
	our @ISA = qw(Exporter);
	our @EXPORT_OK = qw(CONTROL_FIELDS CONFIG_DIR
			    MODULE_FILENAME_RE
			    read_package_lists
			    for_each_package);
}

use constant CONTROL_FIELDS => qw(
	Package Package-Type Provides Depends Architecture Kernel-Version
	Section Priority Description
);

use constant CONFIG_DIR => $ENV{KW_CONFIG_DIR};
if (!defined(CONFIG_DIR)) {
	die "Required environment variable \$KW_CONFIG_DIR is not defined";
}

use constant MODULE_FILENAME_RE => '\.ko(?:\.(?:xz|zstd))?$';

sub read_package_lists {
	my @packages = ();

	open(LIST, CONFIG_DIR . "/package-list") || die "package-list: $!";
	my $field;
	my %pkg;
	while (<LIST>) {
		chomp;
		next if /^#/;

		if (/^(\S+):\s*(.*)/) {
			$field=$1;
			my $val=$2;
			if (! grep { $field =~ /^\Q$_\E(_.+)?$/ } CONTROL_FIELDS) {
				die "unknown field, $field";
			}
			$pkg{$field}=$val;
		}
		elsif (/^$/) {
			if (%pkg) {
				push @packages, {%pkg};  # reference to a *copy* of %pkg
				%pkg=();
			}
		}
		elsif (/^(\s+.*)/) {
			# continued field
			$pkg{$field}.="\n".$1;
		}
	}
	if (%pkg) {
		push @packages, \%pkg;
	}
	close LIST;

	return [@packages];
}

sub for_each_package {
	my ($packages, $versions, $fn) = @_;

	foreach my $ver (@$versions) {
		my ($arch, $kernelversion, $flavour) = @$ver;
		foreach my $pkg (@$packages) {
			# Used to get a field of the package, looking first for
			# architecture-specific fields.
			my $package = sub {
				my $field=shift;
				return $pkg->{$field."_".$flavour}
				if exists $pkg->{$field."_".$flavour};
				return $pkg->{$field."_".$arch."_".$flavour}
				if exists $pkg->{$field."_".$arch."_".$flavour};
				return $pkg->{$field."_".$arch}
				if exists $pkg->{$field."_".$arch};
				return $pkg->{$field}
				if exists $pkg->{$field};
				return undef;
			};

			# Check for a modules list file for this architecture and
			# package.
			my $modlistdir="";
			if (-d (CONFIG_DIR . "/modules/$arch-$flavour")) {
				$modlistdir = CONFIG_DIR . "/modules/$arch-$flavour";
			}
			elsif (-d (CONFIG_DIR . "/modules/$flavour")) {
				$modlistdir = CONFIG_DIR . "/modules/$flavour";
			}
			else {
				$modlistdir = CONFIG_DIR . "/modules/$arch";
			}

			next unless -e "$modlistdir/".$package->("Package");

			$fn->($arch, $kernelversion, $flavour, $modlistdir,
			      $package);
		}
	}
}

1;
