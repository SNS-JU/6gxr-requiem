CC	:= g++
LIBS := $(shell pkg-config --libs --cflags gstreamer-video-1.0 gstreamer-1.0 gstreamer-net-1.0 gstreamer-rtp-1.0 )
CFLAGS	:= -O0 -ggdb -Wall -fno-omit-frame-pointer

all: gst-pipeline

gst-pipeline: gst-pipeline.cpp
	"$(CC)" $(CFLAGS) $^ $(LIBS) -o $@

clean:
	rm gst-pipeline
