/* Copyright (C) 2024 Stelard Actek (based on virtjs by Sam Bazley)
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <libevdev/libevdev.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static void sigint(int signum)
{
	(void) signum;
}

struct vbtn {
	signed int max;
	signed int thresh;
	signed int state;
};

int main(int argc, char *argv[])
{
	int rfd = 0, uifd = 0;
	int rc;
	struct libevdev *real = NULL, *virt = NULL;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s /dev/input/event\n", argv[0]);
		return -1;
	}

	if ((rfd = open(argv[1], O_RDONLY | O_NONBLOCK)) == -1) {
		perror(argv[1]);
		return errno;
	}

	rc = libevdev_new_from_fd(rfd, &real);
	if (rc < 0) {
		fprintf(stderr, "Failed to init libevdev: %s\n", strerror(-rc));
		return -1;
	}

	const char *name = libevdev_get_name(real);
	unsigned short vendor = libevdev_get_id_vendor(real);
	unsigned short product = libevdev_get_id_product(real);

	printf("%s (%04x:%04x)\n",
			name,
			vendor,
			product);

	if (libevdev_has_event_type(real, EV_KEY)) {
		fprintf(stderr, "This device has buttons\n");
		goto end;
	}

	if (!libevdev_has_event_type(real, EV_ABS)) {
		fprintf(stderr, "This device does not have absolute axes\n");
		goto end;
	}

	if ((uifd = open("/dev/uinput", O_RDWR)) == -1) {
		perror("/dev/uinput");
		goto end;
	}

	// Create virtual device named "Virtual <device name>", cloning vendor and product, and setting version to 1
	struct uinput_user_dev vdev;
	memset(&vdev, 0, sizeof(vdev));
	vdev.id.bustype = BUS_VIRTUAL;
	vdev.id.vendor = vendor;
	vdev.id.product = product;
	vdev.id.version = 1;
	//strncpy(vdev.name, "Virtual Joystick", UINPUT_MAX_NAME_SIZE);
	int size = snprintf(NULL, 0, "Virtual %s", name);
	char * vname = malloc(size + 1);
	sprintf(vname, "Virtual %s", name);
	strncpy(vdev.name, vname, UINPUT_MAX_NAME_SIZE);
	free(vname);

	// Count absolute axes
	unsigned short abs_cnt = 0;
	for (int i = 0; i < ABS_CNT; i++) {
		if (libevdev_has_event_code(real, EV_ABS, i)) {
			abs_cnt++;
		}
	}

	// Describe virtual device
	ioctl(uifd, UI_SET_EVBIT, EV_SYN);
	ioctl(uifd, UI_SET_EVBIT, EV_KEY);
	for (int i = 0; i < abs_cnt; i++) {
		ioctl(uifd, UI_SET_KEYBIT, BTN_TRIGGER + i);
		printf("Adding button %d code: %d\n", i, BTN_TRIGGER + i);
	}
	ioctl(uifd, UI_SET_EVBIT, EV_ABS);
	ioctl(uifd, UI_SET_EVBIT, EV_MSC);
	ioctl(uifd, UI_SET_MSCBIT, MSC_SCAN);

	struct vbtn vbtns[ABS_CNT];
	memset(vbtns, 0, sizeof vbtns);

	int j = 0;
	for (int i = 0; i < ABS_CNT; i++) {
		if (libevdev_has_event_code(real, EV_ABS, i)) {
			ioctl(uifd, UI_SET_ABSBIT, i);
			vdev.absmin[j] = libevdev_get_abs_minimum(real, i);
			vdev.absmax[j] = libevdev_get_abs_maximum(real, i);
			vdev.absflat[j] = libevdev_get_abs_flat(real, i);
			vdev.absfuzz[j] = libevdev_get_abs_fuzz(real, i);

			vbtns[j].max = vdev.absmax[j];
			vbtns[j].thresh = vdev.absmax[j] * 0.1;
			printf("VBtn %d max: %d, thresh: %d\n", j, vbtns[j].max, vbtns[j].thresh);

			j++;
		}
	}

	write(uifd, &vdev, sizeof(vdev));

	int err;
	if ((err = ioctl(uifd, UI_DEV_CREATE)) != 0) {
		fprintf(stderr, "Failed to create device\n");
		goto end;
	}

	signal(SIGINT, sigint);

	struct pollfd fds [] = {
		{rfd, POLLIN, 0},
		{STDOUT_FILENO, POLLHUP, 0},
	};

	// Begin polling real device
	while (poll(fds, 2, -1) > 0) {
		if (fds[0].revents & POLLIN) {
			struct input_event ev;

			rc = libevdev_next_event(real, LIBEVDEV_READ_FLAG_BLOCKING, &ev);
			while (rc == 0) {
				//printf("Code: %d, Type: %d, Value: %d\n", ev.code, ev.type, ev.value);
				
				if (ev.type == EV_ABS) {
					// Update button state
					signed int new_state = ev.value > vbtns[ev.code].thresh ? 1 : 0;
					if (vbtns[ev.code].state != new_state) {
						vbtns[ev.code].state = new_state;
						//printf("VBtn %d state: %d\n", ev.code, new_state);

						struct input_event btn_ev;
						btn_ev.code = BTN_TRIGGER + ev.code;
						btn_ev.time = ev.time;
						btn_ev.type = EV_KEY;
						btn_ev.value = new_state;
						write(uifd, &btn_ev, sizeof(btn_ev));
					}
				}

				// Repeat cloned device state
				write(uifd, &ev, sizeof(ev));

				rc = libevdev_next_event(real, LIBEVDEV_READ_FLAG_BLOCKING, &ev);
			}
		}

		// Break on error or hangup
		if (fds[1].revents & (POLLERR | POLLHUP)) {
			break;
		}
	}

	errno = 0;
end:
	if (real) {
		libevdev_free(real);
	}

	if (virt) {
		libevdev_free(virt);
	}

	if (rfd) {
		close(rfd);
	}

	if (uifd) {
		close(uifd);
	}

	return errno;
}
