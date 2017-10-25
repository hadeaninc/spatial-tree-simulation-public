#pragma once

#include <hadean.h>

#define Sender void
#define Receiver void

#define Process HProcess
#define Channel HChannel
#define ProcessSet HProcessSet

#define Spawn HSpawn
#define Send HSend
#define Receive HReceive
#define OpenSender HOpenSender
#define OpenReceiver HOpenReceiver

#define CloseReceiver(x)
#define CloseSender(x)
#define Pid() HGetPid()
#define SELF() ((HEndpoint){.type=endpoint_pid,.pid=Pid()})
#define SIBLING(n) ((HEndpoint){.type=endpoint_job,.job=n})

const uint32_t channel_buf_size = 4096;
