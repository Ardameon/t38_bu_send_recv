#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main()
{
    int fd = 0;

    fd = open("./file", O_CREAT | O_RDONLY);
    if(fd < 0)
    {
        perror("open() error");
    }

    /* Hello github */

    sleep(10);

    if(fd > 0) close(fd);

    return 0;
}
