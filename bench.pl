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
my $i = 0;
timethese(10000, {
        'test1' => sub {
            $memd->add("host.host.host" . $i % 500, ("1" x 1000) . "$i");
            $i++;
        }
     });

timethese(10000, {
        'test2' => sub {
            my $val = $memd->get("url_queue");
        }
     });

