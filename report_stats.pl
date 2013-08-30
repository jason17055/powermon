#!/usr/bin/perl -I/srv/lab-auditor/lib

use strict;
use warnings;
use DBI;
use Date::Parse;
use POSIX "strftime";
our %Config;
require "/srv/lab-auditor/etc/Config.pl";

my $dbh = DBI->connect($Config{'db_dsn'},
			$Config{'db_user'}, $Config{'db_pass'},
		{ RaiseError => 1, AutoCommit => 0 });

printf "%-12s %5s %5s %5s %5s\n",
		"", "Awake", "Blank", "Sleep", "Off";

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
my $last_host = "";
while (my $row = $sth->fetchrow_arrayref)
{
	my ($host_name, $period, $time0, $time1, $time2, $time3) = @$row;
	print "-- ".uc($host_name)." --\n" if $last_host ne $host_name;
	$last_host = $host_name;

	printf "%-12s %5d %5d %5d %5d\n",
		$period, $time0, $time1, $time2, $time3;

	delete $warnings{$host_name};
	if ($time2 == 0 && $time1 > 1000) {
		$warnings{$host_name} = "Unable to sleep.";
	}
}
$dbh->disconnect;

print "\n";
foreach my $host (sort keys %warnings) {
	print uc($host).": ".$warnings{$host}."\n";
}
