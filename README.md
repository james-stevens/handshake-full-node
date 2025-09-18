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


# Upgrading Handshake

If you want to upgrade `hsd`, or simply prefer to replace the binaries I have built
with ones you have built yourself, it's REALLY easy.

- Download the `hsd` [release](https://github.com/handshake-org/hsd/releases) of your choice
- Un-tar it onto a dev server running the same version of [Alpine Linux](https://alpinelinux.org/downloads/) as specified in my (Dockerfile)[Dockerfile] (currently v3.22)
- cd into the `hsd-<version>` directory `tar` just created & run `./build`

Now in the same directory you should have a directory called `hsd`

- In a clone of this repo, remove the directory `hsd` and replace it with yor new `hsd` directory
- In the repo clone now run `./dkmk` to rebuild the container

If you want to be certain your new container is 100% freshly built, run `./dkmk --no-cache`
