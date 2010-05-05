#!/usr/bin/env perl

use warnings;
use strict;

use Benchmark qw(:all) ;

use Cache::Memcached;

my $server_str = shift or
    warn "no servers gived, use localhost:19854\n";

if (!$server_str) {
    $server_str = "localhost:19854";
}

my $memd = new Cache::Memcached {
    'servers' => [ $server_str ]
};

for my $i (1..10) {
    $memd->add("host.host.host" . $i % 10, "a". ("$i" x 200) . "a");
}

for my $i (1..10) {
    my $val = $memd->get("url_queue");
    if ($val) {
        print "$i: okay\n";
        print $val . "\n";
    } else {

    }
}

sleep 10;

my $val = $memd->get("url_queue");
if (!$val) {
    print "okay no url found\n"
} else {
    print "error\n";
}
