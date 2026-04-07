package KernelWedge;

use strict;
use warnings;

BEGIN {
	use Exporter ();
	our @ISA = qw(Exporter);
	our @EXPORT_OK = qw(CONTROL_FIELDS CONFIG_DIR
			    MODULE_FILENAME_RE
			    read_package_lists
			    for_each_package
			    package_enabled);
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
			if ((! grep { $field =~ /^\Q$_\E(_.+)?$/ } CONTROL_FIELDS)
			    && $field !~ /^Flavour_.+$/) {
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

sub _package_enabled {
	my ($pkg, $arch, $flavour) = @_;

	# The actual arch/flavour must not match any negative entries
	# (leading '!'), and if there are any positive entries then
	# the arch/flavour must match one of them.
	for (['Architecture',  $arch],
	     ["Flavour_$arch", $flavour]) {
		my ($field, $value) = @$_;
		my @words = split(/\s+/, $pkg->{$field} || '');
		return 0 if grep /^!$value$/, @words;
		return 0 if (grep /^[^!]/, @words) && (!grep /^$value$/, @words);
	}
	return 1;
}

sub package_enabled {
	my ($packages, $name, $arch, $flavour) = @_;

	my @matches = grep { $_->{'Package'} eq $name } @$packages;
	return @matches == 1 && _package_enabled($matches[0], $arch, $flavour);
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

			if (_package_enabled($pkg, $arch, $flavour)) {
				$fn->($arch, $kernelversion, $flavour, $package);
			}
		}
	}
}

1;
