#!/usr/bin/env perl

use warnings;
use strict;

use Benchmark qw(:all) ;

use Cache::Memcached;

my $server_str = shift or
    warn "no servers gived, use localhost:19854\n"
if (!$server_str) {
    $server_str = "localhost:19854";
}

my $memd = new Cache::Memcached {
    'servers' => [ $server_str ]
};

timethese(0, {
        'test1' => sub {
            for my $i (1..100) {
                $memd->add("host.host.host" . $i, ("$i" x 1024) . "a");
            }
        }
     });

timethese(10000, {
        'test1' => sub {
            my $val = $memd->get("urlqueue");
        }
     });

