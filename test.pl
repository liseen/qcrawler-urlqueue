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
    $memd->add("host.host.host" . $i, ("$i" x 200) . "a");
    $memd->add("host.host.host" . $i, ("$i" x 200) . "b");
    $memd->add("host.host.host" . $i, ("$i" x 200) . "c");
}

my $i = 0;
while (1) {
    sleep 5;
    my $val = $memd->get("url_queue");
    if ($val) {
        $i++;
        print "$i: okay\n";
    } else {
        last;
    }
}
if ($i == 30) {
    print "all get okay\n"
}

sleep 10;

my $val = $memd->get("url_queue");
if (!$val) {
    print "okay no url found\n"
} else {
    print "error\n";
}
