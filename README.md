# REQUIEM: Research on QUIC client mobility

This repository contains work mainly supported by the
[REQUIEM](https://bit.ly/tki-6gxr-requiem)
[open-call](https://www.6g-xr.eu/open-calls/oc1-results/) of the
[6G-XR](https://www.6g-xr.eu/) EU project.

# test-in-screen

The main entry point is the `test-in-screen` script, which runs one
measurement in a mininet-based network inside a GNU Screen instance.
See `test-in-screen --help` for details.

# Dependencies

`test-in-screen` relies on our forks of
[quinn](https://github.com/SNS-JU/6gxr-quinn),
[gst-plugins-rs](https://github.com/SNS-JU/6gxr-gst-plugins-rs), and
[latency-clock](https://github.com/SNS-JU/6gxr-latency-clock).

In a Debian system the following command stats the installation of the
dependencies:
```shell
apt install gnuplot-nox screen mininet
```
[k8s/Dockerfile](k8s/Dockerfile) shows the rest of installation
process.

# Executing multiple measurements

Directory [tipsy-requiem](tipsy-requiem) and other directories with
the prefix `tipsy-` contains configuration files for running
`test-in-screen` with different configs in batch mode.  These require
the `general-cmd` branch of
[tipsy](https://github.com/hsnlab/tipsy/tree/general-cmd) to be
installed.
