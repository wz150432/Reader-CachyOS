// Small desktop shortcut client: no GUI initialization or persistent process.
#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static long long monotonic_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int wait_writable(int fd, long long deadline)
{
    for (;;) {
        const long long remaining = deadline - monotonic_ms();
        if (remaining <= 0) return 0;
        struct pollfd descriptor = {fd, POLLOUT, 0};
        const int result = poll(&descriptor, 1, (int)remaining);
        if (result > 0) return (descriptor.revents & POLLOUT) != 0;
        if (result == 0 || errno != EINTR) return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "--toggle-hide") != 0
        && strcmp(argv[1], "--hide") != 0 && strcmp(argv[1], "--show") != 0))
        return 2;
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime || !*runtime) return 1;
    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    const int length = snprintf(address.sun_path, sizeof address.sun_path,
                                "%s/reader-control.sock", runtime);
    if (length < 0 || (size_t)length >= sizeof address.sun_path) return 1;
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return 1;
    int result = 1;
    const long long deadline = monotonic_ms() + 500;
    if (connect(fd, (const struct sockaddr *)&address, sizeof address) != 0) {
        if (errno != EINPROGRESS || !wait_writable(fd, deadline)) goto done;
        int error = 0;
        socklen_t size = sizeof error;
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) != 0 || error) goto done;
    }
    const char *command = argv[1] + 2;
    size_t remaining = strlen(command);
    while (remaining) {
        const ssize_t sent = send(fd, command, remaining, MSG_NOSIGNAL);
        if (sent > 0) {
            command += sent;
            remaining -= (size_t)sent;
        } else if (sent < 0 && errno == EINTR) {
            if (monotonic_ms() >= deadline) goto done;
        } else if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (!wait_writable(fd, deadline)) goto done;
        } else {
            goto done;
        }
    }
    result = 0;
done:
    close(fd);
    return result;
}
