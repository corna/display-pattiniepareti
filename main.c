#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <pthread.h>

#include "codes.h"

#define COLON 1
#define DOT 2
#define DEGREES 4

#define SPI_SPEED 250000
#define MASTER1_PATH "/sys/bus/w1/devices/w1_bus_master1"
#define MASTER2_PATH "/sys/bus/w1/devices/w1_bus_master2"
#define SPI_PATH "/dev/spidev0.1"
#define PWM_PATH "/sys/class/pwm/pwmchip0/pwm0/"

#define TIME_DURATION 5
#define DATA_DURATION 3
#define TEXT_DURATION 1

#define POWERSAVE_START (23 * 60 + 30)  //23:30
#define POWERSAVE_END (8 * 60)          //8:00

#define BRIGHTNESS_FULL 255
#define BRIGHTNESS_POWERSAVE 2

#define SHOW_ICE_TEMP_START 250
#define SHOW_ICE_TEMP_END 120

#define PATH_SIZE 128

char *temp_msgs[] = { " out", " ICE" };

typedef struct
{
    char path[PATH_SIZE];
    long temperature;
    bool ended;
} temp_data;

bool getW1SlavePath(const char *master_path, char *first_slave_path)
{
    bool status = false;
    FILE *fd;
    char path[PATH_SIZE];
    unsigned int len;

    strncpy(path, master_path, PATH_SIZE - 1);
    strncat(path, "/w1_master_slaves", PATH_SIZE - 1);
    fd = fopen(path, "r");

    if (fd > 0)
    {
        strncpy(first_slave_path, master_path, PATH_SIZE - 1);
        strncat(first_slave_path, "/", PATH_SIZE - 1);
        len = strlen(first_slave_path);

        if (fgets(first_slave_path + len, PATH_SIZE - len - 1, fd)) {

            len = strlen(first_slave_path);
            if (first_slave_path[len - 1] == '\n')
                first_slave_path[len - 1] = '\0';

            strncat(first_slave_path, "/w1_slave", PATH_SIZE - 1);
            status = true;
        }

        fclose(fd);
    }

    return status;
}

bool displayWrite(const char text[4], const char extra, const char* filename)
{
    static int fd = 0;
    uint8_t out[4];

    for (unsigned int i = 0; i < 4; i++)
    {
        if (isupper(text[i]))
            out[3 - i] = alphabet_upper[text[i] - 'A'];
        else if (islower(text[i]))
            out[3 - i] = alphabet_lower[text[i] - 'a'];
        else if (isdigit(text[i]))
            out[3 - i] = digits[text[i] - '0'];
        else if (text[i] == '-')
            out[3 - i] = 0x40;
        else if (text[i] == '+' && i == 0)
            out[3 - i] = 0xC0;
        else if (text[i] == '_')
            out[3 - i] = 0x08;
        else
            out[3 - i] = 0x00;
    }

    if (extra & COLON)
        out[2] |= (1 << 7);

    if (extra & DEGREES)
        out[1] |= (1 << 7);

    if (extra & DOT)
        out[0] |= (1 << 7);

    if (fd <= 0)
    {
        fd = open(filename, O_WRONLY);

        if (fd > 0)
        {
            if (!ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, SPI_SPEED))
                printf("Can't set SPI speed to %d", SPI_SPEED);
        }
    }

    if (fd > 0)
    {
        if (write(fd, out, 4) == 4)
            return true;
        else
            return false;
    }
    else
        return false;
}

bool displayBrightness(uint8_t brightness)
{
    int fd = open(PWM_PATH "period", O_RDONLY);
    bool success = false;
    char buf[16];
    char *end;
    unsigned int period;

    if (fd > 0)
    {
        if (read(fd, buf, 15) > 0)
        {
            period = strtol(buf, &end, 10);
            success = (buf != end);
        }

        close(fd);
    }

    if (success)
    {
        success = false;
        fd = open(PWM_PATH "duty_cycle", O_WRONLY);

        if (fd > 0)
        {
            int len = sprintf(buf, "%u", period * brightness / 255);

            if (len > 0)
            {
                success = (write(fd, buf, len) == len);
            }

            close(fd);
        }
    }

    return success;
}

void *readTemp(void *data)
{
    int fd = open(((temp_data *)data)->path, O_RDONLY);
    char buf[128];
    char *pos;
    char *endpos;

    ((temp_data *)data)->temperature = LONG_MAX;

    if (fd > 0)
    {
        if (read(fd, buf, 127) > 0)
        {
            pos = strstr(buf, "YES");

            if (pos)
            {
                pos = strstr(pos, "t=");

                if (pos)
                {
                    pos += 2;
                    ((temp_data *)data)->temperature = strtol(pos, &endpos, 10);

                    if (endpos != pos)
                    {
                        close(fd);
                        ((temp_data *)data)->ended = true;
                        return NULL;
                    }
                }
            }

            close(fd);
        }
    }

    ((temp_data *)data)->ended = true;
    return NULL;
}

bool powerSave()
{
    time_t rawtime;
    char buffer[8];
    char *end;
    long minutes;

    time(&rawtime);
    strftime(buffer, 7, "%H", localtime(&rawtime));
    minutes = strtol(buffer, &end, 10) * 60;

    if (end != buffer)
    {
        strftime(buffer, 7, "%M", localtime(&rawtime));
        minutes += strtol(buffer, &end, 10);

        if (end != buffer)
        {
            return (minutes < POWERSAVE_END || minutes > POWERSAVE_START);
        }
    }

    return false;
}

bool showIceTemp()
{
    time_t rawtime;
    char buffer[8];
    char *end;
    long days;

    time(&rawtime);
    strftime(buffer, 7, "%j", localtime(&rawtime));
    days = strtol(buffer, &end, 10);

    if (end != buffer)
    {
        return (days < SHOW_ICE_TEMP_END || days > SHOW_ICE_TEMP_START);
    }

    return false;
}

int main(void)
{
    int temp_num;
    char str[8];
    time_t rawtime;
    pthread_t temp_pthreads[2];
    temp_data temp_datas[2];

    while(true)
    {
        uint8_t colon_on = COLON;

        getW1SlavePath(MASTER1_PATH, temp_datas[0].path);
        getW1SlavePath(MASTER2_PATH, temp_datas[1].path);

        if (powerSave())
            displayBrightness(BRIGHTNESS_POWERSAVE);
        else
            displayBrightness(BRIGHTNESS_FULL);

        if (showIceTemp())
            temp_num = 2;
        else
            temp_num = 1;

        for (unsigned int i = 0; i < temp_num; i++)
        {
            temp_datas[i].ended = false;

            if (pthread_create(&temp_pthreads[i], NULL, readTemp, &temp_datas[i])) {
                fprintf(stderr, "Error creating temperature thread %d\n", i);
                temp_datas[i].ended = true;
                temp_datas[i].temperature = LONG_MAX;
            }
        }

        for (unsigned int i = 0; i < TIME_DURATION * 2; i++)
        {
            struct timespec half_sec = {0, 500000000};

            time(&rawtime);
            strftime (str, 7, "%H%M", localtime(&rawtime));
            displayWrite(str, colon_on, SPI_PATH);
            colon_on = colon_on == COLON ? 0 : COLON;

            nanosleep(&half_sec, NULL);
        }

        time(&rawtime);
        strftime (str, 7, "%d%m", localtime(&rawtime));
        displayWrite(str, DOT, SPI_PATH);

        sleep(DATA_DURATION);

        for (unsigned int i = 0; i < temp_num; i++)
        {
            displayWrite(temp_msgs[i], 0, SPI_PATH);

            sleep(TEXT_DURATION);

            if (temp_datas[i].ended == true)
            {
                pthread_join(temp_pthreads[i], NULL);
            }
            else
            {
                fprintf(stderr, "Thread %d has not ended yet, canceling\n", i);

                if (pthread_cancel(temp_pthreads[i]) != 0)
                {
                    fprintf(stderr, "Thread %d can't be canceled\n", i);
                }

                temp_datas[i].temperature = LONG_MAX;
            }

            if (temp_datas[i].temperature != LONG_MAX)
            {
                sprintf(str, "%+04ld", temp_datas[i].temperature / 100);

                if (str[1] == '0')
                    str[1] = ' ';

                displayWrite(str, DEGREES, SPI_PATH);
            }
            else
                displayWrite("EEEE", 0, SPI_PATH);

            sleep(DATA_DURATION);
        }

    }

    return 0;
}
