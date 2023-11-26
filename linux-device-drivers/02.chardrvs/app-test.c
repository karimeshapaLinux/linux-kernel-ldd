#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/poll.h>

#define STD_ON	(1)
#define STD_OFF	(0)

#define IOCTL_TEST	(STD_ON)
#define POLL_TEST	(STD_OFF)

#define WR_VALUE 	_IOW('c', 1, int *) /* cmd 1: Set NR of users */
#define RD_VALUE 	_IOR('c', 2, int *) /* cmd 2: Get NR of users */

int main()
{
#if (IOCTL_TEST == STD_ON)
	int ioctl_fd, ioret;
	int wr_num = 2;
	int rd_val = 0;
	ioctl_fd = open("/dev/chardrvs", O_RDWR);
	if (ioctl_fd < 0) {
		printf("Can't open chardrvs driver file\n");
		return 0;
	}
	ioret = ioctl(ioctl_fd, WR_VALUE, &wr_num);
	if (ioret < 0) {
		printf("Permissions denied...\n");
		goto close;
	} else {
		ioctl(ioctl_fd, RD_VALUE, &rd_val);
		printf("rd_val %d\n", rd_val);
	}

close:
	close(ioctl_fd);
#endif

#if (POLL_TEST == STD_ON)
	struct pollfd fds;
	int poll_fd;
	int ret;

	poll_fd = open("/dev/chardrvs", O_WRONLY);
	fds.fd = poll_fd;
	fds.events = POLLOUT;

	ret = poll(&fds, 1, 5000);
	if (fds.revents & POLLOUT)
		printf ("chardrvs file is writable\n");
	close(poll_fd);
#endif

	return 0;
}

