#include <mserv.h>
#include <fcntl.h>

void fd_set_nonblock(int fd)
{
        int oldflags = fcntl(fd, F_GETFL);

        if (!(oldflags & O_NONBLOCK))
                fcntl(fd, F_SETFL, oldflags | O_NONBLOCK);
}