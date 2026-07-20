#include "syshead.h"
#include "tuntap_if.h"
#include "utils.h"
#include "basic.h"

static int tun_fd;
static char *dev;

char *tapaddr = "10.0.0.5";
char *taproute = "10.0.0.0/24";

int tun_read(char *buf, int len)
{
    return read(tun_fd, buf, len);
}

int tun_write(char *buf, int len)
{
    return write (tun_fd, buf, len);
}

static int set_if_route(char *dev, char *cidr)
{
    return run_cmd("ip route add dev %s %s", dev, cidr);
}

static int set_if_address(chra *dev, char *cidr)
{
    return run_cmd("ip address add dev %s local %s", dev, cidr);
}

static int set_if_up(char *dev)
{
    return run_cmd("ip link set dev %s up", dev);
}

static int tun_alloc(char *dev)
{
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tap", O_RDWR)) < 0)
    {
        perror("Cannot open /dev/net/tap\n");
        exit(1);
    }

    CLEAR(ifr);

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (*dev)
    {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0)
    {
        perror("ERR: Could not ioctl tun");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);
    return fd;
}

void tun_init()
{
    dev = calloc(10, 1);
    tun_fd = tun_alloc(dev);

    if (set_if_up(dev) != 0)
    {
        print_err("ERROR when setting up if\n");
    }
}

void free_tun()
{
    free(dev);
}