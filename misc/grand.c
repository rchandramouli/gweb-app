/* GET 256 random value from /dev/urandom */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static char *base62 =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.";
    
int main (int argc, char *argv[])
{
    int fd, nr;
    uint32_t random;
    char random_base62[63], ch;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    /* Generate BizHash bucket */
    printf("{\n");
    for (nr = 0; nr < 256; nr++) {
        read(fd, &random, sizeof(random));
        printf("0x%08x, ", random);
        if (((nr + 1) % 8) == 0)
            printf("\n");
    }
    printf("};\n");

    /* Generate Base62 random string */
    for (nr = 0; nr < 62; nr++)
        random_base62[nr] = base62[nr];
    for (nr = 0; nr < 61; nr++) {
        read(fd, &random, sizeof(random));
        random = nr + (random % (62 - nr));
        ch = random_base62[nr];
        random_base62[nr] = random_base62[random];
        random_base62[random] = ch;
    }
    random_base62[62] = '\0';
    printf("\n[%s]\n", random_base62);
    
    close(fd);

    return 0;
}
