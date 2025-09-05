# handshake-full-node
Container to run a Handshake Full Node v8.0.0

Includes a daily cronjob to run `hsd-rpc compacttree` randomly at 3, 4 or 5am UTC (chance of 1 in 100)

Auth ROOT listens on port 153, Resolver listens on 253

By default, runs with `--no-sig0 --no-wallet`


## Env Vars

### HSD_SYSLOG_SERVER

IP Address of a port 514 syslog listener, if omitted syslogs to stdout

### HSD_ADDITIONAL_PARAMS

Parameters added to `hsd` when started up. Specfying this var will override the default of `--no-sig0 --no-wallet`.

### HSD_LOG_LEVEL

Log level for `hsd` - if omitted, defaults to `info`


# Docker.com

https://hub.docker.com/r/jamesstevens/handshake-full-node
