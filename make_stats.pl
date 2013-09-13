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
	SELECT t1.Host,LastData,FirstData,LastStat,h.Name,h.LastPowerState,h.LastPowerTime
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
	JOIN Host h
		ON h.id=t1.Host
	ORDER BY Host
	");
$sth->execute;
while (my $row = $sth->fetchrow_arrayref)
{
	my $host_id = $row->[0];
	my $t = str2time($row->[1]);
	my $d = str2time($row->[2]);
	my $host_name = $row->[4];
	if (!$d) {
		warn("Invalid datetime '$row->[2]' (host $row->[4])\n");
		next;
	}
	my $last_state = $row->[5] || 'Off';
	my $last_time = $row->[6] ? str2time($row->[6]) : $d;
	my $count = 0;
	while ($last_time + 86400 <= $t) {
		$last_state = process_host_day($row->[0], $last_time, $last_state);
		$last_time += 86400;
		$count++;
	}

	if ($count)
	{
printf "TIME=%s, STATE=%s\n", strftime('%Y-%m-%d %H:%M:%S', localtime($last_time)), $last_state;

	$dbh->do("
		UPDATE Host
		SET LastPowerState=?, LastPowerTime=?
		WHERE id=?
		", {},
		$last_state,
		strftime('%Y-%m-%d %H:%M:%S', localtime($last_time)),
		$host_id,
		);
	$dbh->do("
		DELETE FROM LogEntry
		WHERE Host=?
		AND Posted<?
		", {},
		$row->[0],
		strftime('%Y-%m-%d', localtime($last_time)),
		);
	}
}

sub process_host_day
{
	my ($host_id, $period_start, $st) = @_;

my $period_end = $period_start + 86400;
print "doing host $host_id\n";
print "  start = ".strftime('%Y-%m-%d %H:%M:%S', localtime($period_start))."\n";
print "  end = ".strftime('%Y-%m-%d %H:%M:%S', localtime($period_end))."\n";
die if $period_start < 86400*365;
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
	my $last_time = 0;
	my %times;
	while (my $row = $sth->fetchrow_arrayref)
	{
		my $t = str2time($row->[0]) - $period_start;
		my $mess = $row->[1];
		my $elapsed = $t - $last_time;

		if ($mess =~ /^startup/) {
			if ($st eq 'Off' && $elapsed < 300) {
				# less than five minutes off, looks like a reboot
				$st = 'Awake';
			}
			$times{$st} += $elapsed;
			$st = "Awake";
		}
		elsif ($mess =~ /^Shutdown/) {
			$times{$st} += $elapsed;
			$st = "Off";
		}
		elsif ($mess =~ /^Still alive/) {
			next unless ($st =~ /Awake|ScreenOff/);
			$times{$st} += $elapsed;
		}
		elsif ($mess =~ /^The monitor is on/) {
			next unless ($st =~ /Awake|ScreenOff/);
			$times{$st} += $elapsed;
			$st = 'Awake';
		}
		elsif ($mess =~ /^The monitor is off/) {
			next unless ($st =~ /Awake|ScreenOff/);
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
		$last_time = $t;
	}

	my $final_elapsed = 86400-$last_time;
	$times{$st} += $final_elapsed;

	printf " * TimeAwake     %6.1f\n", ($times{Awake}||0)/60;
	printf " * TimeScreenOff %6.1f\n", ($times{ScreenOff}||0)/60;
	printf " * TimeSleep     %6.1f\n", ($times{Sleep}||0)/60;
	printf " * TimeOff       %6.1f\n", ($times{Off}||0)/60;

	$dbh->do("REPLACE INTO PowerStats
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
	return $st;
}
