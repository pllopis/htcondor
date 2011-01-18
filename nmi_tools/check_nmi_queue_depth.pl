#!/usr/bin/env perl
use strict;
use warnings;
use Getopt::Long;


my $THRESHOLD;
my $SEND_EMAIL;
GetOptions("threshold=i" => \$THRESHOLD,
           "send-email=s" => \$SEND_EMAIL);

if(not $THRESHOLD) {
    print STDERR "Usage: $0 --threshold <queue depth threshold> [--send-email <recipients>]\n";
    exit 1;
}

my @out = `condor_q -format '%s\n' nmi_target_platform | sort | uniq -c | sort -nr`;

my @email;
my @platforms;
foreach my $line (@out) {
    if($line =~ /^\s*(\d+)\s*(\S+)/) {
        if($1 > $THRESHOLD) {
            push @email, $line;
            push @platforms, $2;
        }
    }
}

push @email, "\n\n\nIndividual platform status:\n";

my @missing;
foreach my $platform (@platforms) {
    my $output = `condor_status -const 'nmi_platform=="$platform"'`;
    $output =~ s/\n\n/\n/g;
    
    # If we get nothing back (except whitespace) then the machine is probably not in 
    # the pool at all.  We want to alert about these.
    if($output !~ /\S/) {
        push @missing, $platform;
        $output = "No output from condor_status, this platform may be missing\n";
    }
    
    push @email, "Platform: $platform\n";
    push @email, "$output\n\n";
}


if($SEND_EMAIL) {
    my $subject = "NMI Queue depths - " . scalar(@platforms) . " platforms over depth $THRESHOLD";
    if(@missing > 0) {
        $subject .= " (" . scalar(@missing) . " platforms missing?)";
    }

    if(not open(MAIL, "|/bin/mail -s \"$subject\" $SEND_EMAIL >/dev/null 2>&1")) {
        print "Results:";
        print @email;
        logger("Failed to send mail!\n");
        exit 1;
    }
    print MAIL @email;
    if(not close(MAIL)) {
        if($!) {
            logger("Error - failure while closing /bin/mail: $!");
            return 0;
        }
        else {
            logger("Error - /bin/mail returned exit status $?");
            return 0;
        }
    }
}
else {
    print @email;
}


sub logger {
    my ($msg) = @_;
    print STDERR $msg;
}
