## Install

Minimal k8s install:
```shell
kubectl apply -f rawquic.yaml
```

This includes:
- `rawquic-server` service
- `rawquic-server` pod with 2 containers:
  - `gst`: will run the gstreamer pipeline
  - `debug`: debugging tools from [l7mp/net-debug](https://github.com/l7mp/net-debug)

## Usage

- Check load balancer status and IP
```shell
kubectl get svc rawquic-server -o wide
```

We will use this address in the client as the `address` parameter:

- Set `port=5000` in the gst pipeline `quinnquicsink`. Otherwise it will use a random port and the LB will not work.
