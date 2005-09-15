#!/usr/bin/env perl

###########################################################################
# 09/25/04: bgietzel : script to run the Condor test suite 
#
# - Get the buildIDs from the nightly test file, and reset the file
# - For each buildID passed 
#    - Generate the submit file and helper files
#    - run nmi_submit 
###########################################################################

#use strict;
use DBI;
use File::Copy;
use Cwd;
use Getopt::Long;
use File::Basename;
my $LIBDIR;
BEGIN {
    my $dir1 = dirname($0);
    my $dir2 = dirname($dir1);
    if( $dir1 eq $dir2 ) {
	$LIBDIR = $dir2 . "/../lib";
    } else {
	$LIBDIR = $dir2 . "/lib";
    }
}
use lib "$LIBDIR";
use CondorGlue;

use vars qw/ $opt_help $opt_nightly $opt_tag $opt_module $opt_notify $opt_remote_declare_args/;

$ENV{PATH} = "/nmi/bin:/usr/local/condor/bin:/usr/local/condor/sbin:"
             . $ENV{PATH};

GetOptions (
    'help'            => $opt_help,
    'nightly'         => $opt_nightly,
    'tag=s'           => $opt_tag,
    'module=s'        => $opt_module,
    'notify'          => $opt_notify,
    'platforms=s'     => $opt_platforms,
    'buildid=s'       => $opt_buildid,
    'test-src=s'      => $opt_test_src,
    'nmi-glue=s'      => $opt_nmi_glue,
    'remote_declare_args=s'   => $opt_remote_declare_args,
    'workspace'       => $opt_workspace
);



# database parameters
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";
my $TASK_TABLE = "Task";

my $notify;
my $remote_declare_args;
my %ids;
my @platforms;
my $gid;

#format of test_ids file
#109 BUILD-V6_6-branch-2004-9-27 V6_6_BUILD
my $workspace = "/tmp/condor_test." . "$$";

if ( defined($opt_help) ) {
    print_usage();
    exit 0;
}

if ( defined($opt_notify) ) {
    $notify = "$opt_notify";
}
else {
    $notify = "condor-build\@cs.wisc.edu";
}

# use input remote_declare_args value for the list of tests to run, or default 
# to quick (all tests complete in 5 minutes or less).
if ( defined($opt_remote_declare_args) ) {
    $remote_declare_args = "$opt_remote_declare_args";
}
else {
    $remote_declare_args = "quick";
}

my $cwd = &getcwd();
my $NIGHTLY_IDS_FILE = "$cwd/test_ids";

mkdir($workspace) || die "Can't create workspace $workspace: $!\n";
chdir($workspace) || die "Can't chdir($workspace): $!\n";

# This is a bit of a hack. Needed to make workspace build work.
if (defined $opt_workspace) {
  $opt_tag = 'workspace_build_1.2.3';
  $opt_module = 'Condor';
}

if (defined $opt_buildid) {
    if (not defined($opt_tag) ) {
        print "ERROR: You need to specify --tag while using --buildid\n";
        print_usage();
        exit 1;
    }
    $ids{$opt_buildid} = $opt_tag;
    if (not defined $opt_module) {
        print "ERROR: You need to specify --module while using --buildid\n";
        print_usage();
        exit 1;
    }
    $ids{$opt_buildid} .= " $opt_module";
}
elsif (defined $opt_nightly) {
    %ids = &get_nightlyids();
    # Check that the $NIGHTLY_IDS_FILE exists
    die "$NIGHTLY_IDS_FILE does not exist.\n" if (not -f $NIGHTLY_IDS_FILE);
}
else {
    print "ERROR: Specify --buildid=<run id to test> or --nightly\n";
    print_usage();
    exit 1;
}

while ( my($id, $tag_module_string) = each(%ids) ) {
    if (defined ($opt_platforms)) {
        @platforms = split (/\s*\,\s*/, $opt_platforms);
    }
    else {
        @platforms = &get_platforms($id);
    }
    $gid = &get_gid($id);
    my $cmdfile = &generate_cmdfile($id, $tag_module_string, @platforms);
    print "Submitting condor test suite run for build $id ($tag_module_string) ...\n";
    CondorGlue::run( "/nmi/bin/nmi_submit $cmdfile", 0 );
} 
chdir($cwd);
system("rm -rf $workspace");
exit 0;


sub generate_cmdfile() {
    my ($id, $tag_module_string, @platforms) = @_;
    
    my @platforms = @platforms;

    print "tagmodule string is $tag_module_string\n";
    #get tag and module
    my ($tag, $module) = split(/ /, $tag_module_string); 
    print "++tag is $tag\n";
    print "++module is $module\n";

    my $cmdfile = "condor_cmdfile-$tag";
    my $gluefile = "condor_test.src";
    my $runidfile = "input_build_runid.src";
    my $src_file = "test_src.src";

    # generate the test glue file - may be symlinked eventually
    if (defined $opt_nmi_glue) {
        # SCP the glue from different location
        if ( not -d "$opt_nmi_glue/test" ) {
            print "You should have glue in directory $opt_nmi_glue/test\n";
            die "This doesnot seem to exist or --nmi-glue not in standard format\n";
        }
        open(GLUEFILE, ">$gluefile") || 
	    die "Can't open $gluefile for writing: $!\n";
        print GLUEFILE "method = scp\n";
        print GLUEFILE "scp_file = $opt_nmi_glue\n";
        print GLUEFILE "recursive = true\n";     
        close GLUEFILE;
    }
    else {
        # Checkout the glue from the tag
        CondorGlue::makeFetchFile( $gluefile, "nmi_glue/test", $tag );
    }

    # generate the runid input file
    open(RUNIDFILE, ">$runidfile") || 
	die "Can't open $runidfile for writing: $!\n";
    print RUNIDFILE "method = nmi\n";
    print RUNIDFILE "input_runids = $id\n";
    print RUNIDFILE "untar_results = false\n";     
    close RUNIDFILE;

    # Generate the cmdfile
    open(CMDFILE, ">$cmdfile") || die "Can't open $cmdfile for writing: $!\n";
    print CMDFILE "run_type = test\n";
    
    CondorGlue::printIdentifiers( *CMDFILE, $tag );

    if (defined $opt_test_src) {
        generate_test_src_input($src_file);
        print CMDFILE "inputs = $runidfile, $gluefile, $src_file\n";
        print CMDFILE "pre_all_args = --src=$opt_test_src\n";
    }
    else {
        print CMDFILE "inputs = $runidfile, $gluefile\n";
    }

    print CMDFILE "pre_all = nmi_glue/test/pre_all\n";
    print CMDFILE "platform_pre = nmi_glue/test/platform_pre\n";
    print CMDFILE "remote_pre_declare = nmi_glue/test/remote_pre_declare\n";
    print CMDFILE "remote_declare = nmi_glue/test/remote_declare\n";
    # select scope of testsuite run 
    # all takes 30 minutes
    print CMDFILE "remote_declare_args =  $remote_declare_args\n";
    print CMDFILE "remote_pre = nmi_glue/test/remote_pre\n";
    print CMDFILE "remote_task = nmi_glue/test/remote_task\n";
    print CMDFILE "remote_post = nmi_glue/test/remote_post\n";
    print CMDFILE "\n";
    print CMDFILE "remote_task_timeout = 20\n";
    print CMDFILE "\n";

    print CMDFILE "platforms = ";
    foreach $platform (@platforms) { 
        print CMDFILE "$platform,";
    }
    print CMDFILE "\n";

    CondorGlue::printTestingPrereqs( *CMDFILE );

    print CMDFILE "notify = $notify\n";
    print CMDFILE "priority = 1\n";
    close CMDFILE;
    return $cmdfile;
}


sub generate_test_src_input() {
    my ($src_file) = @_;

    # generate the runid input file
    open(SRCFILE, ">$src_file") || 
	die "Can't open $src_file for writing: $!\n";
    print SRCFILE "method = scp\n";
    print SRCFILE "scp_file = $opt_test_src\n";
    close SRCFILE;
}


sub get_nightlyids() {
    print "Getting build ids and source to run against ...\n";

    my %ids;

    open(IDS, "<", "$NIGHTLY_IDS_FILE") ||
	die "Can't read nightly ids file $NIGHTLY_IDS_FILE: $!\n";
    while (<IDS>) {
        chomp($_);
        my @id = split /\s/, $_;
        $ids{$id[0]} = "$id[1]" . " " . "$id[2]"; 
    }
    # truncate file
    #seek(IDS, 0, 0) || die "can't seek to $NIGHTLY_IDS_FILE: $!\n"; 
    #print IDS "" || die "can't print to $NIGHTLY_IDS_FILE: $!\n";
    #truncate(IDS, tell(IDS)) || die "can't truncate $NIGHTLY_IDS_FILE: $!\n";
    close(IDS) || die "can't close $NIGHTLY_IDS_FILE: $!\n";

    return %ids;
}

sub get_gid() {
    my ($runid) = @_;

    my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") ||
             die "Could not connect to database: $!\n";
    my $cmd_str = qq/SELECT gid from $RUN_TABLE WHERE runid='$runid'/;
    print "$cmd_str\n";

    my $handle = $db->prepare("$cmd_str");
    $handle->execute();
    while ( my $row = $handle->fetchrow_hashref() ) {
        $gid = $row->{'gid'};
        print "gid = $gid\n";
    }
    $handle->finish();
    $db->disconnect;

    return $gid;
}

sub get_platforms() {
    my ($runid) = @_;

    my @platforms = ();
    my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") ||
             die "Could not connect to database: $!\n";
    my $cmd_str = qq/SELECT DISTINCT platform from $TASK_TABLE WHERE runid='$runid'/;
    print "$cmd_str\n";

    my $handle = $db->prepare("$cmd_str");
    $handle->execute();
    while ( my $row = $handle->fetchrow_hashref() ) {
        $platform = $row->{'platform'};
        push(@platforms, $platform);
    }
    $handle->finish();    
    $db->disconnect;

    # strip out local platform entry
    @platforms = grep !/local/, @platforms;

    return @platforms;
}

sub print_usage() {
    print <<END_USAGE;

--help
This screen

--nightly (default)
This pulls the nightly tags file from http://www.cs.wisc.edu/condor/nwo-build-tags and submits builds for all the tags

--buildid 
build runid to test

--platforms
Comma sperated list of platforms to run tests on

--tag
condor source code tag to be fetched from cvs

--module
condor source code module to be fetched from cvs

--notify
List of users to be notified about the results

--test-src
Source to test against

--nmi_glue=<nmi_glue directory for condor containing customised glue>

--remote_declare_args
List of Condor testsuite tests to run. Defaults to "quick" (all tests run in 5 minutes or less) 

END_USAGE
}

