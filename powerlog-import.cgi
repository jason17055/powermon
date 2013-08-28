#!/usr/bin/perl -I/srv/lab-auditor/lib

use strict;
use warnings;
use CGI;
use HTML::Entities;
use URI::Escape;
use DBI;
our %Config;
require "/srv/lab-auditor/etc/Config.pl";

my $CGI = CGI->new();
if ($CGI->request_method eq "POST")
{
	eval { handle_upload() };
	if ($@)
	{
		print $CGI->header(-status => "500");
		print encode_entities("$@");
		exit 2;
	}
}
else
{
	# print a stupid form
	print $CGI->header;
	print qq|<html>
<body>
<form enctype="multipart/form-data" method="post">
<div>Host: <input type="text" name="host" value=""></div>
<div>File: <input type="file" name="file" value=""></div>
<div>
<button type="submit">Upload</button>
</div>
</form>
</body>
</html>
|;
}

sub handle_upload
{
	my $dbh = DBI->connect($Config{'db_dsn'},
				$Config{'db_user'}, $Config{'db_pass'},
			{ RaiseError => 1, AutoCommit => 0 });

	my $host = $CGI->param('host');
	my $host_info = $dbh->selectrow_arrayref('
		SELECT id FROM Host WHERE Name=?
		', {},
		$host);
	my $host_id;
	if ($host_info) {
		$host_id = $host_info->[0];
	}
	else {
		$dbh->do("
		INSERT INTO Host (Name) VALUES (?)
		", {}, $host);
		$host_id = $dbh->{mysql_insertid};
	}

	if (my $fh = $CGI->upload("file"))
	{
		while (my $line = <$fh>)
		{
			$line =~ s/\r?\n$//s;
			my $datestr = substr($line, 0, 19);
			$line = substr($line, 20);

			$dbh->do("
			INSERT INTO LogEntry (Host,Posted,Message)
			SELECT ?,?,?
			FROM dual
			WHERE (?,?,?) NOT IN (SELECT Host,Posted,Message FROM LogEntry)
			", {},
			$host_id, $datestr, $line,
			$host_id, $datestr, $line,
			);
		}
		close $fh
			or die "upload: $!\n";
	}

	$dbh->commit;

	print $CGI->header("text/plain");
	print "ok\n";
}
