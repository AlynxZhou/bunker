Bunker
======

A simple HTTP server with daemonize and CGI support. (HTTP 1.0 only.)
---------------------------------------------------------------------

# Usage:

```
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make
$ ./bin/bunker -c ../docs -p 8080 -l ./bunker.log
```

Then visit `http://localhost:8080/`.

# License

Apache-2.0
