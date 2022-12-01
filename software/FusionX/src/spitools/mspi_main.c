#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define FRAME_MAX_SIZE      8
#define SYSFS_GPIO_DIR  "/sys/class/gpio"
#define MAX_BUF         (64)
#define GPIO_HIGH       (1)
#define GPIO_LOW        (0)

/**
 * mode: 0 -- CPOL=0,CPHA=0
 *       1 -- CPOL=0,CPHA=1
 *       2 -- CPOL=1,CPHA=0
 *       3 -- CPOL=1,CPHA=1
 */
static uint8_t  mode  = 1;

/**
 * bits: 1 ~ 16
 */
static uint8_t  bits  = 8;

/**
 * speed: <= 72MHz
 */
static uint32_t speed = 30 * 1000 * 1000;

/**
 * device: 0/1
 */
static const char *device = "/dev/spidev0.0";

static uint16_t delay = 0;

static int spi_fd = -1;

// Number of iterations for loopback test.
static int iterations = 1000000;

// Debug flag.
static int debug = 0;


/**
 * function: gpio export
 */
int ss_gpio_export(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);

    return 0;
}
 
/**
 * function: gpio unexport
 */
int ss_gpio_unexport(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

/**
 * function:  gpio set direction
 * parameter: out_flag = 1 -- out
 *            out_flag = 0 -- in
 */
int ss_gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/direction");
        return fd;
    }

    if (out_flag)
        write(fd, "out", 4);
    else
        write(fd, "in", 3);

    close(fd);
    return 0;
}
 
/**
 * function:  gpio set value
 * parameter: value = 1 -- high
 *            value = 0 -- low
 */
int ss_gpio_set_value(unsigned int gpio, unsigned int value)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/set-value");
        return fd;
    }

    if (value)
        write(fd, "1", 2);
    else
        write(fd, "0", 2);

    close(fd);
    return 0;
}


/**
 * function: gpio get value
 */
int ss_gpio_get_value(unsigned int gpio, unsigned int *value)
{
    int fd;
    char buf[MAX_BUF];
    char ch;

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        perror("gpio/get-value");
        return fd;
    }

    read(fd, &ch, 1);

    if (ch != '0') {
        *value = 1;
    } else {
        *value = 0;
    }

    close(fd);
    return 0;
}
 
 
/**
 * function:  gpio_set edge
 * parameter: edge = "none", "rising", "falling", "both"
 */
int ss_gpio_set_edge(unsigned int gpio, char *edge)
{
    int fd;
    char buf[MAX_BUF];
 
    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/set-edge");
        return fd;
    }

    write(fd, edge, strlen(edge) + 1);
    close(fd);
    return 0;
}

/**
 * function: gpio open fd
 */
int ss_gpio_open(unsigned int gpio)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_RDONLY | O_NONBLOCK );
    if (fd < 0) {
        perror("gpio/fd_open");
    }
    return fd;
}
 
/**
 * function: gpio close fd
 */
int ss_gpio_close(int fd)
{
    return close(fd);
}


/**
 * function: open mspi device
 */
int ss_mspi_open(void)
{
    int ret = 0;
    uint8_t lsb;

    if (spi_fd >= 0)
        return -1;

    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0)
    {
        printf("can't open %s\n", device);
        return -1;
    }

    //set mspi mode
    ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    if (ret < 0)
    {
        printf("can't set spi mode\n");
        close(spi_fd);
        return -1;
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
    if (ret < 0)
    {
        printf("can't get spi mode\n");
        close(spi_fd);
        return -1;
    }

    //set bits per word
    ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret < 0)
    {
        printf("can't set bits per word\n");
        close(spi_fd);
        return -1;
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret < 0)
    {
        printf("can't get bits per word\n");
        close(spi_fd);
        return -1;
    }

    //set mspi speed hz
    ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret < 0)
    {
        printf("can't set max speed hz\n");
        close(spi_fd);
        return -1;
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret < 0)
    {
        printf("can't get max speed hz\n");
        close(spi_fd);
        return -1;
    }

    //ret = ioctl(spi_fd, SPI_IOC_WR_LSB_FIRST, &lsb);
    //if (ret < 0)
    //{
    //    printf("mspi set lsb first falid!\n");
    //    close(spi_fd);
    //    return -1;
    //}

    ret = ioctl(spi_fd, SPI_IOC_RD_LSB_FIRST, &lsb);
    if (ret < 0)
    {
        printf("mspi get lsb first falid!\n");
        close(spi_fd);
        return -1;
    }

    printf("mspi mode: %d\n", mode);
    printf("mspi bits per word: %d\n", bits);
    printf("mspi speed: %d Hz\n", speed);
    printf("mspi transmit is lsb first: %d\n", lsb);

    return 0;
}


/**
 * function: close mspi device
 */
int ss_mspi_close(void)
{
    if (spi_fd < 0)
        return -1;

    close(spi_fd);
    spi_fd = -1;

    printf("close %s success\n", device);

    return 0;
}

/**
 * function: mspi send data
 */
int ss_mspi_write(uint8_t *wr_buf, int size)
{
    int i, j, ret;
    uint16_t tx[FRAME_MAX_SIZE] = {0};  //max 8
    int frame_cnt = size / sizeof(tx), data_cnt = size % sizeof(tx);

    if (spi_fd < 0)
        return -1;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = NULL,
        .len = sizeof(tx),
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    for (i = 0; i < frame_cnt; i ++)
    {
        tr.len = sizeof(tx);
        memcpy((uint8_t *)tx, (uint8_t *)wr_buf + i * sizeof(tx), sizeof(tx));
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }
    }

    if (data_cnt)
    {
        tr.len = data_cnt;
        memset((uint8_t *)tx, 0x0, sizeof(tx));
        memcpy((uint8_t *)tx, (uint8_t *)wr_buf + size - data_cnt, data_cnt);
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }
    }

    return 0;
}


/**
 * function: mspi read data
 */
int ss_mspi_read(uint8_t *rd_buf, int size)
{
    int i, j, ret;
    uint16_t rx[FRAME_MAX_SIZE] = {0};
    int frame_cnt = size / sizeof(rx), data_cnt = size % sizeof(rx);

    if (spi_fd < 0)
        return -1;

    struct spi_ioc_transfer tr = {
        .tx_buf = NULL,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(rx),
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    for (i = 0; i < frame_cnt; i ++)
    {
        tr.len = sizeof(rx);
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }
        memcpy((uint8_t *)rd_buf + i * sizeof(rx), (uint8_t *)rx, sizeof(rx));
    }

    if (data_cnt)
    {
        tr.len = data_cnt;
        memset((uint8_t *)rx, 0x0, sizeof(rx));
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }
        memcpy((uint8_t *)rd_buf + size - data_cnt, (uint8_t *)rx, data_cnt);
    }

    return 0;
}

/**
 * function: mspi lookback test
 */
int ss_mspi_lookback(uint8_t *wr_buf, uint8_t *rd_buf, int size, uint8_t loopTest)
{
    int i, j, ret;
    uint16_t rx[FRAME_MAX_SIZE+2] = {0};
    uint16_t tx[FRAME_MAX_SIZE+2] = {0};
    int frame_cnt = size / sizeof(tx), data_cnt = size % sizeof(tx);
    printf("frame count: %d, data count: %d\n", frame_cnt, data_cnt);

    if (spi_fd < 0)
        return -1;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    // Loop Test sends a repeated pattern to the CPLD and expects to get the same data echoed back. 
    if(!loopTest)
    {
        memcpy((uint8_t *)tx, (uint8_t *)wr_buf + i * sizeof(tx), sizeof(tx));
        for (i = 0; i < frame_cnt; i ++)
        {
            tr.len = sizeof(tx);
            memcpy((uint8_t *)tx, (uint8_t *)wr_buf + i * sizeof(tx), sizeof(tx));
            printf("mspi tx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);

            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            memcpy((uint8_t *)rd_buf + i * sizeof(rx), (uint8_t *)rx, sizeof(rx));
            printf("mspi rx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
        }

        if(data_cnt)
        {
            memset((uint8_t *)tx, 0x0, sizeof(tx));
            memset((uint8_t *)rx, 0x0, sizeof(rx));
            memcpy((uint8_t *)tx, (uint8_t *)wr_buf + size - data_cnt, data_cnt);
            printf("mspi tx_buf(dc): %04x %04x %04x %04x %04x %04x %04x %04x\n", tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);

            for(int idx2=0; idx2 < data_cnt; idx2 += 4)
            {
                if(data_cnt - idx2 > 2)
                {
                    tr.len = 4;
                } else
                { 
                    tr.len = 2;
                }
                ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
                if (ret < 0)
                {
                    printf("can't send spi message");
                    return -1;
                }
                if(data_cnt - idx2 > 2)
                {
                    tr.tx_buf += 4;
                    tr.rx_buf += 4;
                } else
                { 
                    tr.tx_buf += 2;
                    tr.rx_buf += 2;
                }
            }
            // Shift out the last loop message.
            tr.len = (data_cnt > 2 ? 4 : 4);
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            memcpy((uint8_t *)rd_buf + size - data_cnt, (uint8_t *)rx, data_cnt);
            printf("mspi rx_buf(dc): %04x %04x %04x %04x %04x %04x %04x %04x\n", rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
        }
    } else
    {
        uint32_t errorCount = 0;
        struct timeval start, stop;

        memset((uint8_t *)tx, 0x0, sizeof(tx));
        memset((uint8_t *)rx, 0x0, sizeof(rx));

        // Send the command to switch CPLD into loopback test mode.
        uint16_t setmodeBuf[2] = {0xFE, 0x00};
        tr.len = 2;
        memcpy((uint8_t *)tx, (uint8_t *)setmodeBuf, 2);
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }

        gettimeofday(&start, NULL);
        for(int idx=0; idx < iterations; idx++)
        {
            memcpy((uint8_t *)tx, (uint8_t *)wr_buf + size - data_cnt, data_cnt);
            if(debug) printf("mspi tx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);
            for(int idx2=0; idx2 < data_cnt; idx2 += 4)
            {
                if(data_cnt - idx2 > 2)
                {
                    tr.len = 4;
                } else
                { 
                    tr.len = 2;
                }
                ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
                if (ret < 0)
                {
                    printf("can't send spi message");
                    return -1;
                }
                if(data_cnt - idx2 > 2)
                {
                    tr.tx_buf += 4;
                    tr.rx_buf += 4;
                } else
                { 
                    tr.tx_buf += 2;
                    tr.rx_buf += 2;
                }
            }
            // Shift out the last loop message.
            tr.len = 2;
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            tr.tx_buf = (unsigned long)tx;
            tr.rx_buf = (unsigned long)rx;

            // Check data in = data out, shifted by one as the return data is tx-1.
            memcpy((uint8_t *)rd_buf + size - data_cnt, (uint8_t *)rx, data_cnt+2);
            //if(memcmp((uint8_t *)tx, (uint8_t *)rx+2, data_cnt) != 0)
            if(memcmp((uint8_t *)tx, (uint8_t *)rd_buf + size - data_cnt +2, data_cnt) != 0)
            {
                errorCount++;
            }
            //for(uint32_t idx2=0; idx2 < data_cnt; idx2++)
            //{
            //    if(tx[idx2] != rx[idx2+1])
            //        errorCount++;
            //}
            if(debug) printf("mspi rx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7], rx[8]);
        }
        gettimeofday(&stop, NULL);
        long totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
        float bytesSec = (float)((data_cnt+2)*iterations*2)/((float)totalTime/1000000);
        printf("Loop mode errorCount: %d, time=%ldms, %8.02fbytes/sec\n", errorCount, totalTime/1000, bytesSec );
    }

    return 0;
}

int ss_mspi_screenblank(void)
{
    int i, j, ret;
    uint16_t rx[FRAME_MAX_SIZE+2] = {0};
    uint16_t tx[FRAME_MAX_SIZE+2] = {0};
    unsigned int READY;

    if (spi_fd < 0)
        return -1;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    // Initialise GPIO.
    ss_gpio_export(12);
    ss_gpio_set_dir(12, 0);
    
    memset((uint8_t *)tx, 0x0, sizeof(tx));
    memset((uint8_t *)rx, 0x0, sizeof(rx));
    for(uint8_t dispchar=0x00; dispchar < 255; dispchar++)
    {
    for(uint16_t idx=0xd800, idxd=0xd000; idx < 0xdc00; idx++, idxd++)
    {
        for(int idx2=0; idx2 < 2; idx2++)
        {
            // CPLD FSM Ready?
            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tx[0] = (idx2 == 0 ? idx : idxd);
            tx[1] = (idx2 == 0 ? 0x1871 : 0x1800 | dispchar);
            tr.len = 4;
            //printf("mspi tx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);

            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }
            //printf("mspi rx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7], rx[8]);

            //do {
            //    ss_gpio_get_value(12, &READY);
            //} while(READY == 0);
            //tx[0] = 0x0000;
            //rx[0] = 0xffff;
            //tr.len = 2;
            //while(rx[0] & 0x8000)
           // {
           //     ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
           //     if (ret < 0)
           //     {
           //         printf("can't send spi message");
           //         return -1;
           //     }
           // }
        }
    }

    // Read back test.
    for(uint16_t idx=0xd000; idx < 0xd400; idx++)
    {
            // CPLD FSM Ready?
            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tx[0] = idx; 
            tx[1] = 0x2000;
            tr.len = 4;
            //printf("mspi tx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);

            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tr.len = 2;
            memset((uint8_t *)tx, 0x0, sizeof(tx));
            memset((uint8_t *)rx, 0x0, sizeof(rx));
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }
            if((rx[0]&0x00ff) != dispchar)
            {
                //printf("mspi tx_buf: %04x %04x %04x %04x %04x %04x %04x %04x\n", tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);
                printf("%04x:%04x->%02x\n", idx, rx[0], dispchar);
            }
    }
    }

    return 0;
}

int ss_mspi_memorydump(void)
{
    int ret;
    uint16_t rx[FRAME_MAX_SIZE+2] = {0};
    uint16_t tx[FRAME_MAX_SIZE+2] = {0};
    unsigned int READY;
    uint8_t  displayWidth = 32;
    uint32_t pnt          = 0x0000;
    uint32_t endAddr      = 0x0000 + 0xE000;
    uint32_t addr         = 0x0000;
    uint32_t i = 0;
    //uint32_t data;
    int8_t   keyIn;
    int      result       = -1;
    char c = 0;

    if (spi_fd < 0)
        return -1;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    // Initialise GPIO.
    ss_gpio_export(12);
    ss_gpio_set_dir(12, 0);
    
    while (1)
    {
        printf("%08lX", addr); // print address
        printf(":  ");

        // print hexadecimal data
        for (i=0; i < displayWidth; )
        {
            // CPLD FSM Ready?
            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tx[0] = pnt+i; 
            tx[1] = 0x2000;
            tr.len = 4;
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tr.len = 2;
            memset((uint8_t *)tx, 0x0, sizeof(tx));
            memset((uint8_t *)rx, 0x0, sizeof(rx));
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }
            if(pnt+i < endAddr)
                printf("%02X", (rx[0]&0x00ff));
            else
                printf("  ");
            i++;
            fputc((char)' ', stdout);
        }

        // print ascii data
        printf(" |");

        // print single ascii char
        for (i=0; i < displayWidth; i++)
        {
            // CPLD FSM Ready?
            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tx[0] = pnt+i; 
            tx[1] = 0x2000;
            tr.len = 4;
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            do {
                ss_gpio_get_value(12, &READY);
            } while(READY == 0);

            tr.len = 2;
            memset((uint8_t *)tx, 0x0, sizeof(tx));
            memset((uint8_t *)rx, 0x0, sizeof(rx));
            ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
            if (ret < 0)
            {
                printf("can't send spi message");
                return -1;
            }

            if ((pnt+i < endAddr) && ((rx[0]&0x00ff) >= ' ') && ((rx[0]&0x00ff) <= '~'))
                fputc((char)(rx[0]&0x00ff), stdout);
            else
                fputc((char)' ', stdout);
        }

        printf("|\r\n");
        fflush(stdout);

        // Move on one row.
        pnt  += displayWidth;
        addr += displayWidth;

        // End of buffer, exit the loop.
        if(pnt >= (0x0000 + 0xE000))
        {
            break;
        }
    }

    // Normal exit, return -1 to show no key pressed.
memoryDumpExit:
    return 0;
}

int ss_mspi_memoryspeed(void)
{
    int ret;
    uint16_t rx[FRAME_MAX_SIZE+2] = {0};
    uint16_t tx[FRAME_MAX_SIZE+2] = {0};
    unsigned int READY;
    uint32_t pnt          = 0x0000;
    uint32_t endAddr      = 0x0000 + 0xE000;
    uint32_t addr         = 0x0000;
    uint32_t i = 0;
    //uint32_t data;
    int8_t   keyIn;
    int      result       = -1;
    char c = 0;
    struct timeval start, stop;

    if (spi_fd < 0)
        return -1;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    // Initialise GPIO.
    ss_gpio_export(12);
    ss_gpio_set_dir(12, 0);
    
    pnt = 0x1000;
    memset((uint8_t *)tx, 0x0, sizeof(tx));
    gettimeofday(&start, NULL);
    for(i=0x1000; i < 0xD000; i++)
    {
        // CPLD FSM Ready?
    //    do {
    //        ss_gpio_get_value(12, &READY);
    //    } while(READY == 0);

        if(i == 0)
        {
            tx[0] = i; 
            tx[1] = 0x2000;
            tr.len = 4;
        } else
        {
            tx[0] = i; 
            tx[1] = 0x2100;
            tr.len = 2;
        }
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }

 //       do {
 //           ss_gpio_get_value(12, &READY);
 //       } while(READY == 0);
//
//        tr.len = 2;
//        //memset((uint8_t *)rx, 0x0, sizeof(rx));
//        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
//        if (ret < 0)
//        {
//            printf("can't send spi message");
//            return -1;
//        }
    }
    gettimeofday(&stop, NULL);
    long totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    float bytesSec = (float)(0xC000)/((float)totalTime/1000000);
    printf("RAM 0x1000:0xD000, time=%ldms, %8.02fbytes/sec\n", totalTime/1000, bytesSec );

    gettimeofday(&start, NULL);
    for(i=0xD000; i < 0xE000; i++)
    {
        // CPLD FSM Ready?
        do {
            ss_gpio_get_value(12, &READY);
        } while(READY == 0);

        tx[0] = i; 
        tx[1] = 0x2000;
        tr.len = 4;
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }

        do {
            ss_gpio_get_value(12, &READY);
        } while(READY == 0);

        tr.len = 2;
        memset((uint8_t *)rx, 0x0, sizeof(rx));
        ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0)
        {
            printf("can't send spi message");
            return -1;
        }
    }
    gettimeofday(&stop, NULL);
    totalTime = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
    bytesSec = (float)(0x1000)/((float)totalTime/1000000);
    printf("VRAM 0xD000:0xE000, time=%ldms, %8.02fbytes/sec\n", totalTime/1000, bytesSec );

    return 0;
}

int main(int argc, char *argv[])
{
    int i, ret, len;
    uint8_t tmp_buf[64];
    uint8_t wr_buf[64], rd_buf[64];
    uint8_t loopTest = 0;

    if (argc < 3)
    {
        printf("please inpu such as: ./mspi_main bits string\n");
        printf("eg: ./mspi_main 8 FF [speed MHz] [clock mode 0-3] [debug] [loop] [loopback test iterations]\n");
        printf("eg: ./mspi_main 9 01FF [speed MHz] [clock mode 0-3] [debug] [loop] [loopback test iterations]\n");
        //              0       1  2       3             4             5      6               7
        return -1;
    }

    printf("mspi test start!\n");

    //set mspi transmit bits
    bits = atoi(argv[1]);

    // Loop test?
    if(argc >= 6 && strcasecmp(argv[6], "loop") == 0)
    {
        loopTest = 1;
        printf("loop test enabled#n");
    }

    // Speed override?
    if(argc >= 4)
    {
        speed = atoi(argv[3]) * 1000 * 1000;
        printf("speed override: %dHz\n", speed);
    }
    if(speed == 0)
    {
        printf("Speed invalid (%d), cannot run test\n", speed);
        return(-1);
    }
    if(argc >= 5)
    {
        mode = atoi(argv[4]);
        printf("mode override: %d\n", mode);
    }
    if(mode >= 4)
    {
        printf("Mode invalid (%d), cannot run test\n", mode);
        return(-1);
    }
    if(argc >= 8)
    {
        iterations = atoi(argv[7]);
        printf("iteration override: %d\n", iterations);
    }
    if(iterations == 0)
    {
        printf("Iterations invalid (%d), cannot run test\n", iterations);
        return(-1);
    }
    if(argc >= 6)
    {
        debug = atoi(argv[5]);
    }

    ret = ss_mspi_open();
    if (ret < 0)
        return -1;

    memset(wr_buf, 0x0, sizeof(wr_buf));
    memset(rd_buf, 0x0, sizeof(rd_buf));
    memset(tmp_buf, 0x0, sizeof(rd_buf));

    //receive shell '0'~'F' string and convert to char data
    uint8_t *cp = argv[2];
    uint8_t tmp = 0;
    if(strcasecmp(cp, "BLANK") == 0)
    {
        ret = ss_mspi_screenblank();
        if (ret < 0)
        {
            ss_mspi_close();
            return -1;
        }
    } else
    if(strcasecmp(cp, "DUMP") == 0)
    {
        ret = ss_mspi_memorydump();
        if (ret < 0)
        {
            ss_mspi_close();
            return -1;
        }
    } else
    if(strcasecmp(cp, "RAMSPEED") == 0)
    {
        ret = ss_mspi_memoryspeed();
        if (ret < 0)
        {
            ss_mspi_close();
            return -1;
        }
    } else
    {
        for (i = 0; *cp; i ++, cp ++)
        {
            tmp = *cp - '0';
            if (tmp > 9)
                tmp -= ('A' - '0') - 10;
            if (tmp > 15)
                tmp -= ('a' - 'A');
            if (tmp > 15)
            {
                printf("invalid hex data on %c!\n", *cp);
                ss_mspi_close();
                return -1;
            }

            if (i % 2)
                tmp_buf[i / 2] |= tmp;
            else
                tmp_buf[i / 2]  = tmp << 4;
        }
        len = i / 2;
        printf("test len: %d, bits: %d, in data: %s\n", len, bits, argv[2]);

        if(loopTest && (len % 4 != 0 && len != 2))
        {
            printf("Invalid data length, either 16 bit single value or single/multiple 32bit values required.t\n", iterations);
            return(-1);
        }

        //save data to an array
        if (bits <= 8)
        {
            //save data to uint8_t
            for (i = 0; i < len; i ++)
            {
                wr_buf[i] = tmp_buf[i];
            }
        }
        else
        {
            //need to convert to uint16_t, high bits save to high address
            for (i = 0; i < len; i += 2)
            {
                wr_buf[i + 1] = tmp_buf[i];
                wr_buf[i + 0] = tmp_buf[i + 1];
            }
        }

        for (i = 0; i < len; i ++)
        {
            if (!(i % 8))
                printf("input data: ");
            printf("0x%02x ", wr_buf[i]);
        }
        //printf("\n\n");

        ret = ss_mspi_lookback(wr_buf, rd_buf, len, loopTest);
        if (ret < 0)
        {
            ss_mspi_close();
            return -1;
        }

        for (i = 0; i < len; i ++)
        {
            if (!(i % 8))
                printf("\noutput data: ");
            printf("0x%02x ", rd_buf[i]);
        }
        printf("\n");
    }

    ss_mspi_close();

    return 0;
}

