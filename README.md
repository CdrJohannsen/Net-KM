# Net-KM

A virtual KVM without V

Net-KM sends keystrokes and mouse events from your server (the machine that has the HIDs connected) to a client.
Only use this inside a safe network and don't type sensitive information as this programm is not safe at all.

## Build

Note that this is _very_ experimental

```bash
git clone https://github.com/CdrJohannsen/Net-KM
cd Net-KM
cmake -B build -S .
cmake --build build
```

The executables `net-km-server` and `net-km-client` will be in `build/`

## Usage

### On the server:

```bash
path/to/net-km-server [/dev/input/...]
```

The devices can be found at `/dev/input/by-id/` and are probably called something like `usb-<name of your device>-event-mouse` and `usb-<name of your device>-event-kbd` there are other devices with similar names that will probably not work

### On the client:

```bash
path/to/net-km-client <server ip>
```

## Allowing rootless access to /dev/uinput

In order for the client to write keyboard and mouse events, it needs access to `/dev/uinput` and have the uinput kernel module loaded.

In order to do that, you will need to create a group for all users that should be allowed to use the client

```bash
sudo groupadd uinput
sudo usermod -a -G uinput <username>
```

Then you'll need to give users of the `uinput` group read and write access to `/dev/uinput`.
Write this line to `/etc/udev/rules.d/99-input.rules`

```
KERNEL=="uinput",GROUP="uinput",MODE:="0660"
```

Lastly if you want the uinput kernel module to always be loaded, add this line to `/etc/modules-load.d/uinput.conf`

```
uinput
```
