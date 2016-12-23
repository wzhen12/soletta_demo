/*
 * This file is part of the Soletta (TM) Project
 *
 * Copyright (C) 2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <limits.h>
#include <errno.h>

#include <sol-iio.h>
#include <sol-log.h>
#include <sol-mainloop.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#define UNIX_DOMAIN "/tmp/UNIX.domain"

struct iio_light_data {
    struct sol_iio_channel *light;
};

static struct sol_timeout *timeout;

static bool
iio_light_reader_cb(void *data)
{
    int r;
    char buffer[128];
    struct iio_light_data *light_data = data;
    double value;
    unsigned int intvalue;
    int connect_fd;
    int ret;
    static struct sockaddr_un srv_addr;
    printf("in iio_light_reader_cb\n");
    //creat unix socket
    connect_fd=socket(PF_UNIX,SOCK_STREAM,0);
    if(connect_fd<0)
    {
        printf("cannot create communication socket\n");
        return false;
    }

    r = sol_iio_read_channel_value(light_data->light, &value);
    printf("r = %d value = %lf\n", r, value);
    SOL_INT_CHECK_GOTO(r, < 0, error);

    memset(buffer, 0, 128);
    srv_addr.sun_family=AF_UNIX;
    strcpy(srv_addr.sun_path,UNIX_DOMAIN);
    //connect server
    ret=connect(connect_fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));
    if(ret==-1)
    {
        printf("cannot connect to the server");
        return true;
    }

    intvalue = (unsigned int)value;
    sprintf(buffer,"%d", intvalue);
    //send info server
    write(connect_fd,buffer,strlen(buffer));
    close(connect_fd);

    return true;

error:
    SOL_WRN("Could not read channel buffer values");

    return false;
}

int
main(int argc, char *argv[])
{
    struct sol_iio_device *device = NULL;
    struct sol_iio_config iio_config = {};
    struct sol_iio_channel_config channel_config = SOL_IIO_CHANNEL_CONFIG_INIT;
    struct iio_light_data light_data = {};
    int device_id;

    sol_init();

    device_id = sol_iio_address_device("/sys/bus/iio/devices/iio:device0");
    printf("device_id = %d\n", device_id);
    SOL_INT_CHECK_GOTO(device_id, < 0, error_iio);

    SOL_SET_API_VERSION(iio_config.api_version = 1; )

    iio_config.data = &light_data;
    iio_config.buffer_size = -1;
    device = sol_iio_open(device_id, &iio_config);
    SOL_NULL_CHECK_GOTO(device, error_iio);

    light_data.light = sol_iio_add_channel(device, "in_proximity",
        &channel_config);
    printf("open in_proximity");
    SOL_NULL_CHECK_GOTO(light_data.light, error_iio);

    sol_iio_device_start_buffer(device);

    timeout = sol_timeout_add(1000, iio_light_reader_cb, &light_data);

    sol_run();

    if (timeout)
        sol_timeout_del(timeout);

    sol_iio_close(device);

    sol_shutdown();
    return 0;

error_iio:
    if (device) {
        /* Must do sol_iio_close(device), it will disable IIO buffer.
         * If not, the trigger, buffer size and buffer enable cannot be set
         * on the next launch.
         */
        sol_iio_close(device);
    }

    if (iio_config.trigger_name) {
        free((char *)iio_config.trigger_name);
        iio_config.trigger_name = NULL;
    }

    sol_shutdown();
    return -1;
}
