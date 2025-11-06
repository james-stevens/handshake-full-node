# handshake-full-node
Container to run a Handshake Full Node v8.0.0

Includes a daily cronjob to run `hsd-rpc compacttree` randomly at 3, 4 or 5am UTC (chance of 1 in 100),
so it should run approx once every 33 days.

Auth ROOT listens on port 153, Resolver listens on 253

By default, runs with `--no-sig0 --no-wallet`

# Data Directory

This container is designed to [run read-only](dkrun) and you will want your `hsd-data` to be persistant
when the container is restarted, or it will have to reclone the entire blockchain every time it restarts.

So you will need to map some persistant storage into this container at the mount point `/opt/data`.
In this directory it will create the sub-directory `hsd-data` for all the HSD data.


## Env Vars

### SYSLOG_SERVER

IP Address of a port 514 syslog listener, if omitted syslogs to stdout.

By default, it will log to `/opt/data/logs/messages`

If you are running this container through something like `systemd` and you
want it to log to `stdout`, then set `SYSLOG_STDOUT=Y`


### HSD_ADDITIONAL_PARAMS

Parameters added to `hsd` when started up.

Specfying this var will **override** the default of `--no-sig0 --no-wallet`,
so if you want to retain either (or both) of those options, while adding more, you will have to include either (or both)
of those in your definition of `HSD_ADDITIONAL_PARAMS`.


### HSD_LOG_LEVEL

Log level for `hsd` - if omitted, defaults to `info`


# Docker.com

https://hub.docker.com/r/jamesstevens/handshake-full-node


# Upgrading Handshake

If you want to upgrade the version of `hsd` in this container, or simply prefer to replace the binaries I have built
with ones you have built yourself, it's REALLY easy.

- Download the `hsd` [release](https://github.com/handshake-org/hsd/releases) of your choice
- Un-tar it onto a dev server running the same version of [Alpine Linux](https://alpinelinux.org/downloads/) as specified in my [Dockerfile](Dockerfile) (currently v3.22)
- `cd` into the `hsd-<version>` directory `tar` just created & run `./build`

Now in the same directory you should have a directory called `hsd`

- Clone this repo
- Copy your entire `hsd-<version>` into this repo 

		cd handshake-full-node; cp -a /path/to/hsd-<version> .

- Edit `Dockerfile` to change `COPY hsd-8.0.0/hsd /usr/local/hsd/` to the version you just copied in
- Run `./dkmk` to rebuild the container

If you want to be certain your new container is 100% freshly built, run `./dkmk --no-cache`

If you are using the same version (v8.0.0), but just want to use your own buuld, I recommend you remove my
`hsd-8.0.0` directory before copying in yours, so you can be certain it's your build that will go
into the container.

		cd handshake-full-node; rm -rf hsd-8.0.0; cp -a /path/to/hsd-8.0.0 .

Obviously, if you aren't changing the verion of `hsd`, you can omit the `Dockerfile` edit step.

## IMPORTANT

NOTE: As described in the v8.0.0 release notes, I include the options `--chain-migrate=4 --wallet-migrate=7` 
in the [run_hsd](bin/run_hsd) start script, so if you change to a different version, these options would 
may also need to change, by editing that start script before you rebuild the container.
