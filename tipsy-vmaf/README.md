`main.json` contains instructions for running a sender and receiver
pipeline that transmits the `cheetah.mp4` video file from the sender
to the receiver.  The video is always transmitted over RTP but the
underlying protocol (`tester.environ.proto`) changes: it is either UDP
(`udp`), QUIC (stream; `raw-quic` with `tester.environ.datagram` set
to `""`) or QUIC datagram (`raw-quic` with `tester.environ.datagram`
set to `--use-datagram`).  Three client migration scenarios
(`tester.environ.scenario`) are tested: `none` (baseline),
`involuntary` and `planned`.

In the baseline case, no migration is attempted: the video should
arrive to the receiver without interruptions.  When using UDP as the
transport protocol, only the baseline scenario is executed, as the
others do not make much sense in this case.

In the involuntary migration case, one of the links between the
receiver and the sender is cut (after `tester.environ.cutlink`
seconds), but the receiver (client) is not prepared for this event and
needs to migrate the connection to its other interface on-the-fly.

In the planned migration case, the receiver transitions to its second
interface (after transmitting `tester.environ.migrationtriggersize`
bytes), in this case the client is prepared for the eventual
occurrence of a migration. `tester.environ.cutlink` needs to be set so
that the video has already progressed over the duration that
corresponds to
`tester.environ.migrationtriggersize`. `tester.environ.cutlink` might
not exactly correspond to the seconds passed playing the video
(starting point might be the time when GStreamer has been started on
host1). The currently set value seems to be the lowest at which the
issue does not appear consistently for link delays up to 450 ms.

In both migration cases, `tester.environ.keepalive` is configured to
100 ms, meaning that QUIC sends keep alive messages on both of the
client's interfaces (even on the one that is not used actively at the
moment) every 100 ms. This is required for the migration cases.

Tests run automatically, except for UDP, where due to an issue, the
receiver does not stop when the video transmission ends. Here, the
gst-pipeline player window needs to be manually closed (by clicking
the X) at the end of the transmission.

In all test cases, RTP latency (`--rtp-timestamp` argument) and VMAF
(`--vmaf` using the VQMTK/CLI super-tool) scores are collected (to
`./measurements`) per video frame and also as overall statistics.

When all test cases finish the execution, these data are visualized on
multiple plots (in `./plots/fig.pdf`).  For per-frame statistics,
`datafile` type, for overall stats (mean, median), the `simple` type
is used.  Selecting a slice of data (e.g., only a specific scenario)
is done via `match`.  In the `datafile` type's case the value
belonging to the key of an axis, should appear as a key in the `group`
expression where its value specifies the specific data's column in the
data file (first column is numbered as `0`).  Titles should be
descriptive and can use field names originating from the
`tester.environ`.

To run all the test cases, install Tipsy (branch general-cmd) and
issue in this directory:

```
$ <path to Tipsy>/tipsy config && make && xdg-open plots/fig.pdf
```

To rerun all the test cases, issue in this directory:

```
$ <path to Tipsy>/tipsy config -f && make && xdg-open plots/fig.pdf
```

To redo only the plots, issue in this directory:

```
$ <path to Tipsy>/tipsy config -f -p && make plots && xdg-open plots/fig.pdf
```

To rerun only one test case, remove the files (except for
benchmark.json and the Makefile) from the specific measurement's
directory then issue:

```
$ make
```
