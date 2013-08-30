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

my $sth = $dbh->prepare("
	SELECT t1.Host,LastData,FirstData,LastStat
	FROM (
		SELECT Host,MAX(DATE(Posted)) AS LastData,MIN(DATE(Posted)) AS FirstData
		FROM LogEntry
		GROUP BY Host
		) t1
	LEFT JOIN (
		SELECT Host,MAX(PeriodStart) AS LastStat
		FROM PowerStats
		WHERE Period='D'
		GROUP BY Host
		) t2
		ON t2.Host=t1.Host
	ORDER BY Host
	");
$sth->execute;
while (my $row = $sth->fetchrow_arrayref)
{
	my $host_id = $row->[0];
	my $t = str2time($row->[1]);
	my $d = $row->[3] ? str2time($row->[3])+86400 : str2time($row->[2]);
	while ($d + 86400 <= $t) {
		process_host($row->[0], $d, $d+86400);
		$d += 86400;
	}
}

sub process_host
{
	my ($host_id, $period_start, $period_end) = @_;

print "doing host $host_id\n";
print "  start = ".strftime('%Y-%m-%d %H:%M:%S', localtime($period_start))."\n";
print "  end = ".strftime('%Y-%m-%d %H:%M:%S', localtime($period_end))."\n";
	my $sth = $dbh->prepare("
		SELECT Posted,Message
		FROM LogEntry
		WHERE Host=?
		AND Posted >= ?
		AND Posted < ?
		");
	$sth->execute($host_id,
		strftime('%Y-%m-%d', localtime($period_start)),
		strftime('%Y-%m-%d', localtime($period_end))
		);
	my $st = 'Awake';
	my $last_time = 0;
	my %times;
	while (my $row = $sth->fetchrow_arrayref)
	{
		my $t = str2time($row->[0]) - $period_start;
		my $mess = $row->[1];
		my $elapsed = $t - $last_time;
		$last_time = $t;

		if ($mess =~ /^startup/) {
			$times{'Off'} += $elapsed;
			$st = "Awake";
		}
		elsif ($mess =~ /^Shutdown/) {
			$times{$st} += $elapsed;
			$st = "Off";
		}
		elsif ($mess =~ /^Still alive/) {
			if ($st eq 'Off') {
				$st = 'Awake';
			}
			$times{$st} += $elapsed;
			$st = 'Awake' unless ($st =~ /Awake|ScreenOff/);
		}
		elsif ($mess =~ /^The monitor is on/) {
			$times{$st} += $elapsed;
			$st = 'Awake';
		}
		elsif ($mess =~ /^The monitor is off/) {
			$times{$st} += $elapsed;
			$st = 'ScreenOff';
		}
		elsif ($mess =~ /^Entering standby/) {
			$times{$st} += $elapsed;
			$st = 'Sleep';
		}
		elsif ($mess =~ /^(Resuming from standby|The system is waking due to user activity)/) {
			$times{$st} += $elapsed;
			$st = 'Awake' unless ($st =~ /Awake|ScreenOff/);
		}
		else {
			print "$mess\n";
		}
	}

	printf " * TimeAwake     %6.1f\n", ($times{Awake}||0)/60;
	printf " * TimeScreenOff %6.1f\n", ($times{ScreenOff}||0)/60;
	printf " * TimeSleep     %6.1f\n", ($times{Sleep}||0)/60;
	printf " * TimeOff       %6.1f\n", ($times{Off}||0)/60;

	$dbh->do("INSERT INTO PowerStats
		(Host,Period,PeriodStart,
		TimeAwake,TimeScreenOff,TimeSleep,TimeOff)
		VALUES (
		?,'D',?,?,?,?,?
		)
		", {},
		$host_id, 
		strftime('%Y-%m-%d', localtime($period_start)),
		$times{Awake} || 0,
		$times{ScreenOff} || 0,
		$times{Sleep} || 0,
		$times{Off} || 0,
		);
}
