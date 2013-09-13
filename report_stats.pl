#!/usr/bin/perl -I/srv/lab-auditor/lib

use strict;
use warnings;
use DBI;
use Date::Parse;
use POSIX "strftime";
use Date::Parse;
use Getopt::Long;
our %Config;
require "/srv/lab-auditor/etc/Config.pl";

my $do_summary = 1;
my $do_detail = 1;
GetOptions(
	"summary!" => \$do_summary,
	"detail!" => \$do_detail,
	) or exit 2;

my $dbh = DBI->connect($Config{'db_dsn'},
			$Config{'db_user'}, $Config{'db_pass'},
		{ RaiseError => 1, AutoCommit => 0 });

if ($do_detail) {
	printf "%-12s %5s %5s %5s %5s\n",
		"", "Awake", "Blank", "Sleep", "Off";
}

my %warnings;
my $sth = $dbh->prepare("
	SELECT Host.Name,
		PeriodStart,
		TimeAwake,
		TimeScreenOff,
		TimeSleep,
		TimeOff
	FROM Host
	JOIN PowerStats ON PowerStats.Host=Host.id
	WHERE Period='D'
	ORDER BY Host.Name,PeriodStart
	");
$sth->execute;
my $cur_host = { name => "" };
my $cur_time = time;
while (my $row = $sth->fetchrow_arrayref)
{
	my ($host_name, $period, $time0, $time1, $time2, $time3) = @$row;
	if ($cur_host->{name} ne $host_name) {
		$cur_host = { name => $host_name };
		print "-- ".uc($host_name)." --\n" if $do_detail;
	}

	if ($do_detail) {
	printf "%-12s %5d %5d %5d %5d\n",
		$period, $time0, $time1, $time2, $time3;
	}

	$cur_host->{screen_off} += $time1;
	$cur_host->{asleep} += $time2;

	my $period_t = str2time($period);
	delete $warnings{$host_name};
	if ($period_t + 4*86400 < $cur_time) {
		$warnings{$host_name} = "No data since $period.";
	}
	elsif ($cur_host->{asleep} == 0 && $cur_host->{screen_off} > 86400) {
		$warnings{$host_name} = "Unable to sleep.";
	}
}
$dbh->disconnect;

print "\n" if $do_detail;

if ($do_summary) {
foreach my $host (sort keys %warnings) {
	print uc($host).": ".$warnings{$host}."\n";
}
}
