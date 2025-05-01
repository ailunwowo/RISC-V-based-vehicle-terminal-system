#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#define CMD_STEPMOTOR_A _IOW('L', 0, unsigned long)
#define CMD_STEPMOTOR_B _IOW('L', 1, unsigned long)
#define CMD_STEPMOTOR_C _IOW('L', 2, unsigned long)
#define CMD_STEPMOTOR_D _IOW('L', 3, unsigned long)

#define HIGH 1
#define LOW 0

int fd;

void step_motor_off(void) {
    ioctl(fd, CMD_STEPMOTOR_A, LOW);
    ioctl(fd, CMD_STEPMOTOR_B, LOW);
    ioctl(fd, CMD_STEPMOTOR_C, LOW);
    ioctl(fd, CMD_STEPMOTOR_D, LOW);
}

void step_turn(unsigned long step, unsigned long delay) {
    switch(step % 8) {
        case 0:  // A
            ioctl(fd, CMD_STEPMOTOR_A, HIGH);
            ioctl(fd, CMD_STEPMOTOR_B, LOW);
            ioctl(fd, CMD_STEPMOTOR_C, LOW);
            ioctl(fd, CMD_STEPMOTOR_D, LOW);
            break;
        case 1:  // AB
            ioctl(fd, CMD_STEPMOTOR_A, HIGH);
            ioctl(fd, CMD_STEPMOTOR_B, HIGH);
            ioctl(fd, CMD_STEPMOTOR_C, LOW);
            ioctl(fd, CMD_STEPMOTOR_D, LOW);
            break;
        case 2:  // B
            ioctl(fd, CMD_STEPMOTOR_A, LOW);
            ioctl(fd, CMD_STEPMOTOR_B, HIGH);
            ioctl(fd, CMD_STEPMOTOR_C, LOW);
            ioctl(fd, CMD_STEPMOTOR_D, LOW);
            break;
        case 3:  // BC
            ioctl(fd, CMD_STEPMOTOR_A, LOW);
            ioctl(fd, CMD_STEPMOTOR_B, HIGH);
            ioctl(fd, CMD_STEPMOTOR_C, HIGH);
            ioctl(fd, CMD_STEPMOTOR_D, LOW);
            break;
        case 4:  // C
            ioctl(fd, CMD_STEPMOTOR_A, LOW);
            ioctl(fd, CMD_STEPMOTOR_B, LOW);
            ioctl(fd, CMD_STEPMOTOR_C, HIGH);
            ioctl(fd, CMD_STEPMOTOR_D, LOW);
            break;
        case 5:  // CD
            ioctl(fd, CMD_STEPMOTOR_A, LOW);
            ioctl(fd, CMD_STEPMOTOR_B, LOW);
            ioctl(fd, CMD_STEPMOTOR_C, HIGH);
            ioctl(fd, CMD_STEPMOTOR_D, HIGH);
            break;
        case 6:  // D
            ioctl(fd, CMD_STEPMOTOR_A, LOW);
            ioctl(fd, CMD_STEPMOTOR_B, LOW);
            ioctl(fd, CMD_STEPMOTOR_C, LOW);
            ioctl(fd, CMD_STEPMOTOR_D, HIGH);
            break;
        case 7:  // DA
            ioctl(fd, CMD_STEPMOTOR_A, HIGH);
            ioctl(fd, CMD_STEPMOTOR_B, LOW);
            ioctl(fd, CMD_STEPMOTOR_C, LOW);
            ioctl(fd, CMD_STEPMOTOR_D, HIGH);
            break;
    }
    usleep(delay);
    step_motor_off();
}

void step_motor_num(int direction, unsigned long steps, unsigned long speed) {
    static unsigned long current_step = 0;
    unsigned long i;
    
    for(i = 0; i < steps; i++) {
        if(direction) {
            current_step++;
        } else {
            current_step--;
        }
        step_turn(current_step, speed);
    }
}

int main(int argc, char *argv[]) {
    char *step_motor = "/dev/step_motor";
    int direction;
    unsigned long steps, speed;
    
    if(argc < 4) {
        printf("Usage: %s <direction> <steps> <speed>\n", argv[0]);
        printf("Direction: 1 (CW) or 0 (CCW)\n");
        printf("Steps: number of steps to move\n");
        printf("Speed: delay between steps in microseconds (3000-20000)\n");
        printf("Example: %s 1 4076 3080\n", argv[0]);
        return 1;
    }
    
    if((fd = open(step_motor, O_RDWR | O_NONBLOCK)) < 0) {
        printf("Failed to open %s\n", step_motor);
        return -1;
    }
    
    direction = atoi(argv[1]);
    steps = atol(argv[2]);
    speed = atol(argv[3]);
    
    printf("Moving motor: Direction=%s, Steps=%lu, Speed=%lu us/step\n",
           direction ? "CW" : "CCW", steps, speed);
    
    step_motor_num(direction, steps, speed);
    
    close(fd);
    return 0;
}