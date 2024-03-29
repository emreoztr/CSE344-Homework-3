#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SHARED_MEM_NAME "bakery_shared_mem"
#define NO_EINTR(stmt) while((stmt) < 0 && errno == EINTR);

typedef struct bakerSync{
    char supply[2];
    sem_t chef_checked[6];
    sem_t walnut, flour, milk, sugar;
    sem_t mutex;
    sem_t dessert_available;

    int shm_fd;

    int is_milk, is_flour, is_walnut, is_sugar;
}BakerSync;

sig_atomic_t interrupted = 0;

void sigint_handler(int sig){
    if(sig == SIGINT)
        interrupted = 1;
}

int get_arguments(int argc, char *argv[], char **input_file);
BakerSync* init_shared_mem();
int chef0(BakerSync *baker);
int chef1(BakerSync *baker);
int chef2(BakerSync *baker);
int chef3(BakerSync *baker);
int chef4(BakerSync *baker);
int chef5(BakerSync *baker);

void pusher0(BakerSync *baker);
void pusher1(BakerSync *baker);
void pusher2(BakerSync *baker);
void pusher3(BakerSync *baker);


int main(int argc, char *argv[])
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);

    char *input_file;
    BakerSync *bakerSync;
    pid_t chef_pid[6];
    pid_t pusher[4];
    int input_fd;
    int read_bytes;
    char buffer[3];

    char *ingredient1;
    char *ingredient2;

    int status;
    int dessert_count = 0;

    if(get_arguments(argc, argv, &input_file) < 0){
        return -1;
    }

    bakerSync = init_shared_mem();
    if(bakerSync == NULL){
        return -1;
    }

    chef_pid[0] = fork();
    if(chef_pid[0] == 0){
        return chef0(bakerSync);
    }
    else if(chef_pid[0] < 0){
        return -1;
    }

    chef_pid[1] = fork();
    if(chef_pid[1] == 0){
        return chef1(bakerSync);
    }
    else if(chef_pid[1] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[2] = fork();
    if(chef_pid[2] == 0){
        return chef2(bakerSync);
    }
    else if(chef_pid[2] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[3] = fork();
    if(chef_pid[3] == 0){
        return chef3(bakerSync);
    }
    else if(chef_pid[3] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[4] = fork();
    if(chef_pid[4] == 0){
        return chef4(bakerSync);
    }
    else if(chef_pid[4] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[5] = fork();
    if(chef_pid[5] == 0){
        return chef5(bakerSync);
    }
    else if(chef_pid[5] < 0){
        //TODO wait children
        return -1;
    }

    pusher[0] = fork();
    if(pusher[0] == 0){
        pusher0(bakerSync);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[0] < 0){
        return -1;
    }

    pusher[1] = fork();
    if(pusher[1] == 0){
        pusher1(bakerSync);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[1] < 0){
        return -1;
    }

    pusher[2] = fork();
    if(pusher[2] == 0){
        pusher2(bakerSync);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[2] < 0){
        return -1;
    }

    pusher[3] = fork();
    if(pusher[3] == 0){
        pusher3(bakerSync);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[3] < 0){
        return -1;
    }

    input_fd = open(input_file, O_RDONLY);
    if(input_fd < 0){
        //TODO wait children
        perror("Error opening input file: ");
        return -1;
    }

    while(1){
        if(interrupted)
            break;
        NO_EINTR(read_bytes = read(input_fd, buffer, 3));
        if(read_bytes < 0){
            perror("Error reading input file: ");
            break;
        }
        else if(read_bytes == 0 || read_bytes == 1){
            break;
        }

        if(interrupted)
            break;

        bakerSync->supply[0] = buffer[0];
        bakerSync->supply[1] = buffer[1];

        if(bakerSync->supply[1] == 'M')
            ingredient2 = "milk";
        else if(bakerSync->supply[1] == 'F')
            ingredient2 = "flour";
        else if(bakerSync->supply[1] == 'W')
            ingredient2 = "walnut";
        else if(bakerSync->supply[1] == 'S')
            ingredient2 = "sugar";

        if(bakerSync->supply[0] == 'M')
            ingredient1 = "milk";
        else if(bakerSync->supply[0] == 'F')
            ingredient1 = "flour";
        else if(bakerSync->supply[0] == 'W')
            ingredient1 = "walnut";
        else if(bakerSync->supply[0] == 'S')
            ingredient1 = "sugar";

        

        if(bakerSync->supply[0] == 'M' || bakerSync->supply[1] == 'M')
            sem_post(&bakerSync->milk);
        if(bakerSync->supply[0] == 'F' || bakerSync->supply[1] == 'F')
            sem_post(&bakerSync->flour);
        if(bakerSync->supply[0] == 'W' || bakerSync->supply[1] == 'W')
            sem_post(&bakerSync->walnut);
        if(bakerSync->supply[0] == 'S' || bakerSync->supply[1] == 'S')
            sem_post(&bakerSync->sugar);

        printf("the wholesaler (pid %d) delivers %s and %s (array: %c , %c)\n", getpid(), ingredient1, ingredient2, bakerSync->supply[0], bakerSync->supply[1]);
        sem_post(&bakerSync->mutex);

        printf("the wholesaler (pid %d) is waiting for the dessert (array: %c , %c)\n", getpid(), bakerSync->supply[0], bakerSync->supply[1]);
        sem_wait(&bakerSync->dessert_available);
        printf("the wholesaler (pid %d) has obtained the dessert and left (array: %c , %c)\n", getpid(), bakerSync->supply[0], bakerSync->supply[1]);
        if(interrupted)
            break;
    }

    for(int i = 0; i < 6; i++){
        kill(chef_pid[i], SIGINT);
        waitpid(chef_pid[i], &status, 0);
        dessert_count += WEXITSTATUS(status);
    }
    
    for(int i = 0; i < 4; i++){
        kill(pusher[i], SIGINT);
        wait(NULL);
    }

    if(close(input_fd) < 0){
        perror("Error closing input file: ");
    }
    if(shm_unlink(SHARED_MEM_NAME) < 0){
        perror("shm_unlink");
    }
    if(munmap(bakerSync, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    

    printf("the wholesaler (pid %d) is done (total desserts: %d)\n", getpid(), dessert_count);

    exit(EXIT_SUCCESS);
}

int _chef_work(BakerSync *baker, const char demand[2], int chef_num){
    int dessert_count = 0;
    int pid = getpid();
    char *ingredient1;
    char *ingredient2;
    char supply_bak[2];

    if(demand[0] == 'M'){
        ingredient1 = "milk";
    }
    else if(demand[0] == 'F'){
        ingredient1 = "flour";
    }
    else if(demand[0] == 'W'){
        ingredient1 = "walnut";
    }
    else if(demand[0] == 'S'){
        ingredient1 = "sugar";
    }

    if(demand[1] == 'M'){
        ingredient2 = "milk";
    }
    else if(demand[1] == 'F'){
        ingredient2 = "flour";
    }
    else if(demand[1] == 'W'){
        ingredient2 = "walnut";
    }
    else if(demand[1] == 'S'){
        ingredient2 = "sugar";
    }

    while(1){
        printf("chef%d (pid %d) is waiting for %s and %s\n", chef_num, pid, ingredient1, ingredient2);
        sem_wait(&baker->chef_checked[chef_num]);
        if(interrupted)
            break;
        sem_wait(&baker->mutex);
        if(interrupted)
            break;
        if((baker->supply[0] == demand[0] && baker->supply[1] == demand[1]) || 
           (baker->supply[0] == demand[1] && baker->supply[1] == demand[0]))
        {
            dessert_count++;
            supply_bak[0] = baker->supply[0];
            baker->supply[0] = '-';
            printf("chef%d (pid %d) has taken the %s (array: %c , %c)\n", chef_num, pid, (supply_bak[0] == (ingredient1[0] + 'A' - 'a')) ? ingredient1 : ingredient2, baker->supply[0], baker->supply[1]);
            
            supply_bak[1] = baker->supply[1];
            baker->supply[1] = '-';
            printf("chef%d (pid %d) has taken the %s (array: %c , %c)\n", chef_num, pid, (supply_bak[1] == ingredient2[0] + 'A' - 'a') ? ingredient2 : ingredient1, baker->supply[0], baker->supply[1]);
            
            sem_post(&baker->dessert_available);
        }
    }

    printf("chef%d (pid %d) is exiting\n", chef_num, pid);

    return dessert_count;
}

//CHEFS
int chef0(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'W', 'S'};
    
    dessert_count = _chef_work(baker, demand, 0);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }

    return dessert_count;
}

int chef1(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'W', 'F'};
    
    dessert_count = _chef_work(baker, demand, 1);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }

    return dessert_count;
}

int chef2(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'F', 'S'};
    
    dessert_count = _chef_work(baker, demand, 2);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }

    return dessert_count;
}

int chef3(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'F', 'M'};
    
    dessert_count = _chef_work(baker, demand, 3);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }

    return dessert_count;
}

int chef4(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'M', 'W'};
    
    dessert_count = _chef_work(baker, demand, 4);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }

    return dessert_count;
}

int chef5(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'M', 'S'};
    
    dessert_count = _chef_work(baker, demand, 5);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }

    return dessert_count;
}
//CHEFS END

//CHEF HELPER
void pusher0(BakerSync *baker){
    while(1){
        sem_wait(&baker->milk);
        if(interrupted)
            break;
        sem_wait(&baker->mutex);
        if(interrupted)
            break;
        if(baker->is_flour){
            baker->is_flour = 0;
            sem_post(&baker->chef_checked[3]);
        }
        else if(baker->is_walnut){
            baker->is_walnut = 0;
            sem_post(&baker->chef_checked[4]);
        }
        else if(baker->is_sugar){
            baker->is_sugar = 0;
            sem_post(&baker->chef_checked[5]);
        }
        else{
            baker->is_milk = 1;
        }
        sem_post(&baker->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
}

void pusher1(BakerSync *baker){
    while(1){
        sem_wait(&baker->flour);
        if(interrupted)
            break;
        sem_wait(&baker->mutex);
        if(interrupted)
            break;
        if(baker->is_milk){
            baker->is_milk = 0;
            sem_post(&baker->chef_checked[3]);
        }
        else if(baker->is_walnut){
            baker->is_walnut = 0;
            sem_post(&baker->chef_checked[1]);
        }
        else if(baker->is_sugar){
            baker->is_sugar = 0;
            sem_post(&baker->chef_checked[2]);
        }
        else{
            baker->is_flour = 1;
        }
        sem_post(&baker->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
}

void pusher2(BakerSync *baker){
    while(1){
        sem_wait(&baker->walnut);
        if(interrupted)
            break;
        sem_wait(&baker->mutex);
        if(interrupted)
            break;
        if(baker->is_milk){
            baker->is_milk = 0;
            sem_post(&baker->chef_checked[4]);
        }
        else if(baker->is_flour){
            baker->is_flour = 0;
            sem_post(&baker->chef_checked[1]);
        }
        else if(baker->is_sugar){
            baker->is_sugar = 0;
            sem_post(&baker->chef_checked[0]);
        }
        else{
            baker->is_walnut = 1;
        }
        sem_post(&baker->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
}

void pusher3(BakerSync *baker){
    while(1){
        sem_wait(&baker->sugar);
        if(interrupted)
            break;
        sem_wait(&baker->mutex);
        if(interrupted)
            break;
        if(baker->is_milk){
            baker->is_milk = 0;
            sem_post(&baker->chef_checked[5]);
        }
        else if(baker->is_flour){
            baker->is_flour = 0;
            sem_post(&baker->chef_checked[2]);
        }
        else if(baker->is_walnut){
            baker->is_walnut = 0;
            sem_post(&baker->chef_checked[0]);
        }
        else{
            baker->is_sugar = 1;
        }
        sem_post(&baker->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
}
//HELPER END

int get_arguments(int argc, char *argv[], char **input_file){
    *input_file = NULL;
    for (int i = 1; i < argc; i++){
        if (strcmp(argv[i], "-i") == 0){
            *input_file = argv[i + 1];
        }
    }

    if(*input_file == NULL)
        return -1;

    return 0;
}

int _sem_initializations(BakerSync *baker){
    for(int i = 0; i < 6; ++i){
        if(sem_init(&baker->chef_checked[i], 1, 0) < 0){
            perror("sem_init");
            return -1;
        }
    }

    if(sem_init(&baker->dessert_available, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

    if(sem_init(&baker->mutex, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

    if(sem_init(&baker->milk, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

    if(sem_init(&baker->sugar, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

    if(sem_init(&baker->walnut, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

    if(sem_init(&baker->flour, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

    baker->is_flour = 0;
    baker->is_milk = 0;
    baker->is_sugar = 0;
    baker->is_walnut = 0;

    return 0;
}

BakerSync* init_shared_mem(){
    int shm_fd;
    BakerSync *baker;

    shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    if(shm_fd < 0){
        perror("shm_open");
        return NULL;
    }

    if(ftruncate(shm_fd, sizeof(BakerSync)) < 0){
        perror("ftruncate");
        return NULL;
    }

    baker = mmap(NULL, sizeof(BakerSync), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(baker == MAP_FAILED){
        perror("mmap");
        return NULL;
    }

    if(_sem_initializations(baker) < 0){
        if(shm_unlink(SHARED_MEM_NAME) < 0)
            perror("shm_unlink");
        if(munmap(baker, sizeof(BakerSync)) < 0)
            perror("munmap");
        return NULL;
    }

    baker->shm_fd = shm_fd;

    return baker;
}