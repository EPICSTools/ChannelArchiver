#!/usr/bin/perl
#
# Based on Greg Lawson's relocate_arch_data.pl
#
# Reads the mailbox dir, copies new data,
# and re-indexes.
#
# For the copy to work via scp,
# key pairs need to be configured.
# Assume this is the data server computer that
# needs to use scp to pull data off archive computers.
#
# Assume the script runs as user 'xfer'
# and needs to pull data from 'arch1':
#
# Once only:
# ssh-keygen -t dsa
#
# Copy public key over:
# scp ~/.ssh/id_dsa.pub xfer@arch1:~/.ssh/xx
#
# Configure public key on arch1:
# ssh arch1
# cd .ssh/
# cat xx >>authorized_keys 
# rm xx
# chmod 600 authorized_keys 
#
# From now on, you should be able to go to 'arch1'
# without being queried for a pw.

BEGIN
{
    push(@INC, 'scripts' );
    push(@INC, '/arch/scripts' );
}


use English;
use strict;
use vars qw($opt_d $opt_h $opt_c $opt_n);
use Cwd;
use File::Path;
use Getopt::Std;
use Data::Dumper;
use Sys::Hostname;
use archiveconfig;

# Very bad, not sure how to better solve this:
# At the SNS, the archive computer might consider itself 'ics-srv-archive1',
# and we are to copy data from 'ics-srv-archive1:/arch/....' to the server.
# But from the data server machine, it's accessible as 'arch1'.
# So we need to translate some addresses:
my (%host_hacks) =
(
    'ics-srv-archive1' => 'arch1',
    'ics-srv-archive2' => 'arch2',
    'ics-srv-archive3' => 'arch3',
);

# Globals, Defaults
my ($config_name) = "archiveconfig.xml";
my ($path) = cwd();

sub usage()
{
    print("USAGE: update_server [options]\n");
    print("\n");
    print("Reads $config_name, checks <mailbox> directory\n");
    print("for new data.\n");
    print("Copied data meant for this server,\n");
    print("then performs an index update.\n");
    print("\n");
    print("Options:\n");
    print(" -h          : help\n");
    print(" -c <config> : Use given config file instead of $config_name\n");
    print(" -d          : debug\n");
    print(" -n          : nop, don't run any commands, just print them\n");
}

# Configuration info filled by parse_config_file
my ($config);

sub check_mailbox()
{
    my ($updates) = 0;
    my ($entry);
    return 0 unless exists($config->{mailbox});
    chdir($config->{mailbox});
    mkpath("done");
    foreach $entry ( <*> )
    {
        next if ($entry eq 'done');
	print("Mailbox file '$entry':\n") if ($opt_d);
	open(MB, "$entry") or die "Cannot open '$entry'\n";
	while (<MB>)
        {
            chomp;
            print("$_\n") if ($opt_d);
            my ($info, $src, $dst) = split(/\s+/);
            my ($src_host, $src_dir) = split(/:/, $src);
            my ($dst_host, $dst_dir) = split(/:/, $dst);
            if ($info eq "new" and defined($src_host) and is_localhost($src_host))
            {   # Data already here, we need to update
                ++$updates;
            }
            elsif ($info eq "copy" and defined($dst_host) and is_localhost($dst_host))
            {   # Copy data here, then update.
                # Careful with 'scp -r':
                # It will create the target dir, e.g. 2006/03_01.
                # But, if the target dir already exists,
                # it will create 2006/03_01/03_01!!
                # -> don't use -r
		$src_host = $host_hacks{$src_host} if (defined($host_hacks{$src_host}));
                my ($cmd) = "mkdir -p $dst_dir && scp $src_host:$src_dir/* $dst_dir";
                print("$cmd\n");
                system($cmd) unless ($opt_n);
                ++$updates;
            }
        }
        close(MB);
        # rename
        rename($entry, "done/$entry");
    } 
    return $updates;
} 

# The main code ==============================================

# Parse command-line options
if (!getopts("dhc:n") ||  $opt_h)
{
    usage();
    exit(0);
}
$config_name = $opt_c if (length($opt_c) > 0);
$config = parse_config_file($config_name, $opt_d);

die "Has to run in $config->{root}\n" unless ($path eq $config->{root});

my ($updates) = check_mailbox();
chdir($path);
if ($updates)
{
    my ($cmd) = "perl scripts/update_indices.pl";
    print("$cmd\n");
    system($cmd) unless ($opt_n);
}

