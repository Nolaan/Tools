#!/usr/bin/perl
use strict;
use warnings;
use POSIX;
use File::Pid;
use MIME::Lite;
use 5.10.0;


###################################
#    Librairie daemonize
###################################
# make "mydaemon.log" file in /var/log/ with "chown root:adm mydaemon"

my $daemonName    = "ipreporter";
#
my $dieNow        = 0;                                     # used for "infinte loop" construct - allows daemon mode to gracefully exit
my $sleepMainLoop = 1800;                                    # number of seconds to wait between "do something" execution after queue is clear
my $logging       = 1;                                     # 1= logging is on
my $logFilePath   = "/var/log/";                           # log file path
my $logFile       = $logFilePath . $daemonName . ".log";
my $pidFilePath   = "/var/run/";                           # PID file path
my $pidFile       = $pidFilePath . $daemonName . ".pid";

# daemonize
use POSIX qw(setsid);
chdir '/';
umask 0;
defined( my $pid = fork ) or die "Can't fork: $!";
exit if $pid;
if(!$pid) {
    close(STDIN);
# open(STDIN,  '/dev/null')  or die "Can't read /dev/null: $!";
    close(STDOUT);
# open(STDOUT, '>>/dev/null') or die "Can't write to /dev/null: $!";
    close(STDERR);
# open(STDERR, '>>/dev/null') or die "Can't write to /dev/null: $!";
}

# dissociate this process from the controlling terminal that started it and stop being part
# of whatever process group this process was a part of.
POSIX::setsid() or die "Can't start a new session.";

# callback signal handler for signals.
$SIG{INT} = $SIG{TERM} = $SIG{HUP} = \&signalHandler;
$SIG{PIPE} = 'ignore';

# create pid file in /var/run/
my $pidfile = File::Pid->new( { file => $pidFile, } );

$pidfile->write or die "Can't write PID file, /dev/null: $!";

# turn on logging
if ($logging) {
    open LOG, ">>$logFile";
    select((select(LOG), $|=1)[0]); # make the log file "hot" - turn off buffering
}


# "infinite" loop where some useful process happens
until ($dieNow) {

    # TODO: put your custom code here!
    
    logEntry("Entering the main loop!");
    main();
    logEntry("Exiting the main loop!");

    sleep($sleepMainLoop);
}

# add a line to the log file
sub logEntry {
    my ($logText) = @_;
    my ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) = localtime(time);
    my $dateTime = sprintf "%4d-%02d-%02d %02d:%02d:%02d", $year + 1900, $mon + 1, $mday, $hour, $min, $sec;
    if ($logging) {
        print LOG "$dateTime $logText\n";
    }
}



###################################
#    FIN librairie daemonize
###################################



###################################
#    Boucle principale
###################################

sub main {

# Variables globale
    logEntry("Getting IP Address");
    my $ip=`wget http://ipecho.net/plain -O - -q`;
    logEntry("Ip fetched");
    my $ipfile = "/home/nolaan/.public_ip";
    my $last_ip="0";
    my $ip_changed = 0;
    my $is_connected = ( $ip =~ m/\d{1,3}\./ );

    if ( -e $ipfile) {
        # $last_ip=`cat $ipfile`;
        open(FILE,"<$ipfile") or die "Fucking cant open dat";

        while (<FILE>){
            if ( $_ =~ m/([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})/) {
                $last_ip = $1;
            }
        }

    }


# Comparaison des IP
    if ( $ip ne $last_ip ){
        `echo $ip > $ipfile`;
        $ip_changed = 1;
        logEntry("Ip a changé et sauvegardé.");
    } else {
        logEntry("Rien a bougé");
    }

    # logEntry("IP recupérée : $ip\n") ;
    logEntry("IP recupérée : $ip\n");

# Diffusion de l'adresse IP

    if ( $ip_changed and $is_connected ) {

        logEntry("Mailing nolaan") ;
        my $Message = new MIME::Lite 
        From =>'root@gideon-boot', 
        To =>'nolaan.d@gmail.com', 
        Subject =>'Ya eu un changement IP pour gideon-boot...', 
        Type =>'TEXT', 
        Data =>"La nouvelle ip est : $ip";
        $Message->send;

    }

}

# catch signals and end the program if one is caught.
sub signalHandler {
    $dieNow = 1;    # this will cause the "infinite loop" to exit
}

# do this stuff when exit() is called.
END {
    logEntry("Received die signal, closing all!");
    if ($logging) { close LOG }
    $pidfile->remove if defined $pidfile;
}

