/* GET 256 random value from /dev/urandom */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_SYMBOLS  (64)

static char *base64 =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.";
    
int main (int argc, char *argv[])
{
    int fd, nr;
    uint32_t random;
    char random_base64[MAX_SYMBOLS+1], ch;

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

    /* Generate Base64 random string */
    for (nr = 0; nr < MAX_SYMBOLS; nr++)
        random_base64[nr] = base64[nr];
    for (nr = 0; nr < MAX_SYMBOLS-1; nr++) {
        read(fd, &random, sizeof(random));
        random = nr + (random % (MAX_SYMBOLS - nr));
        ch = random_base64[nr];
        random_base64[nr] = random_base64[random];
        random_base64[random] = ch;
    }
    random_base64[MAX_SYMBOLS] = '\0';
    printf("\n[%s]\n", random_base64);
    
    close(fd);

    return 0;
}
