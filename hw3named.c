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
    int is_milk, is_flour, is_walnut, is_sugar;
}BakerSync;

typedef struct namedSemaphores{
    sem_t *chef_checked[6];
    sem_t *walnut, *flour, *milk, *sugar;
    sem_t *mutex;
    sem_t *dessert_available;
}NamedSemaphores;

sig_atomic_t interrupted = 0;

void sigint_handler(int sig){
    if(sig == SIGINT)
        interrupted = 1;
}

int get_arguments(int argc, char *argv[], char **input_file, char *semaphore_names[12]);
BakerSync* init_shared_mem();
int open_semaphores(NamedSemaphores *semaphores, char *semaphore_names[12]);
void close_semaphores(NamedSemaphores *semaphores);
void unlink_semaphores(char *semaphore_names[12]);

int chef0(BakerSync *baker, NamedSemaphores *namedSemaphores);
int chef1(BakerSync *baker, NamedSemaphores *namedSemaphores);
int chef2(BakerSync *baker, NamedSemaphores *namedSemaphores);
int chef3(BakerSync *baker, NamedSemaphores *namedSemaphores);
int chef4(BakerSync *baker, NamedSemaphores *namedSemaphores);
int chef5(BakerSync *baker, NamedSemaphores *namedSemaphores);

void pusher0(BakerSync *baker, NamedSemaphores *namedSemaphores);
void pusher1(BakerSync *baker, NamedSemaphores *namedSemaphores);
void pusher2(BakerSync *baker, NamedSemaphores *namedSemaphores);
void pusher3(BakerSync *baker, NamedSemaphores *namedSemaphores);


int main(int argc, char *argv[])
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);
    char *semaphore_names[] = {"chef_checked0", "chef_checked1", "chef_checked2", "chef_checked3", "chef_checked4", "chef_checked5", "walnut", "flour", "milk", "sugar", "mutex", "dessert_available"};

    char *input_file;
    BakerSync *bakerSync;
    NamedSemaphores named_semaphores;
    pid_t chef_pid[6];
    pid_t pusher[4];
    int input_fd;
    int read_bytes;
    char buffer[3];

    char *ingredient1;
    char *ingredient2;

    int status;
    int dessert_count = 0;

    if(get_arguments(argc, argv, &input_file, semaphore_names) < 0){
        return -1;
    }

    bakerSync = init_shared_mem();
    if(bakerSync == NULL){
        return -1;
    }

    if(open_semaphores(&named_semaphores, semaphore_names) < 0){
        return -1;
    }

    chef_pid[0] = fork();
    if(chef_pid[0] == 0){
        return chef0(bakerSync, &named_semaphores);
    }
    else if(chef_pid[0] < 0){
        return -1;
    }

    chef_pid[1] = fork();
    if(chef_pid[1] == 0){
        return chef1(bakerSync, &named_semaphores);
    }
    else if(chef_pid[1] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[2] = fork();
    if(chef_pid[2] == 0){
        return chef2(bakerSync, &named_semaphores);
    }
    else if(chef_pid[2] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[3] = fork();
    if(chef_pid[3] == 0){
        return chef3(bakerSync, &named_semaphores);
    }
    else if(chef_pid[3] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[4] = fork();
    if(chef_pid[4] == 0){
        return chef4(bakerSync, &named_semaphores);
    }
    else if(chef_pid[4] < 0){
        //TODO wait children
        return -1;
    }

    chef_pid[5] = fork();
    if(chef_pid[5] == 0){
        return chef5(bakerSync, &named_semaphores);
    }
    else if(chef_pid[5] < 0){
        //TODO wait children
        return -1;
    }

    pusher[0] = fork();
    if(pusher[0] == 0){
        pusher0(bakerSync, &named_semaphores);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[0] < 0){
        return -1;
    }

    pusher[1] = fork();
    if(pusher[1] == 0){
        pusher1(bakerSync, &named_semaphores);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[1] < 0){
        return -1;
    }

    pusher[2] = fork();
    if(pusher[2] == 0){
        pusher2(bakerSync, &named_semaphores);
        exit(EXIT_SUCCESS);
    }
    else if(pusher[2] < 0){
        return -1;
    }

    pusher[3] = fork();
    if(pusher[3] == 0){
        pusher3(bakerSync, &named_semaphores);
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
            sem_post(named_semaphores.milk);
        if(bakerSync->supply[0] == 'F' || bakerSync->supply[1] == 'F')
            sem_post(named_semaphores.flour);
        if(bakerSync->supply[0] == 'W' || bakerSync->supply[1] == 'W')
            sem_post(named_semaphores.walnut);
        if(bakerSync->supply[0] == 'S' || bakerSync->supply[1] == 'S')
            sem_post(named_semaphores.sugar);

        printf("the wholesaler (pid %d) delivers %s and %s (array: %c , %c)\n", getpid(), ingredient1, ingredient2, bakerSync->supply[0], bakerSync->supply[1]);
        sem_post(named_semaphores.mutex);

        printf("the wholesaler (pid %d) is waiting for the dessert (array: %c , %c)\n", getpid(), bakerSync->supply[0], bakerSync->supply[1]);
        sem_wait(named_semaphores.dessert_available);
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
    
    close_semaphores(&named_semaphores);
    unlink_semaphores(semaphore_names);

    printf("the wholesaler (pid %d) is done (total desserts: %d)\n", getpid(), dessert_count);

    exit(EXIT_SUCCESS);
}

int _chef_work(BakerSync *baker, NamedSemaphores *namedSemaphores, const char demand[2], int chef_num){
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
        sem_wait(namedSemaphores->chef_checked[chef_num]);
        if(interrupted)
            break;
        sem_wait(namedSemaphores->mutex);
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
            
            sem_post(namedSemaphores->dessert_available);
        }
    }

    printf("chef%d (pid %d) is exiting\n", chef_num, pid);

    return dessert_count;
}

//CHEFS
int chef0(BakerSync *baker, NamedSemaphores *namedSemaphores){
    int dessert_count = 0;
    char demand[2] = {'W', 'S'};
    
    dessert_count = _chef_work(baker, namedSemaphores, demand, 0);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
    return dessert_count;
}

int chef1(BakerSync *baker, NamedSemaphores *namedSemaphores){
    int dessert_count = 0;
    char demand[2] = {'W', 'F'};
    
    dessert_count = _chef_work(baker, namedSemaphores, demand, 1);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
    return dessert_count;
}

int chef2(BakerSync *baker, NamedSemaphores *namedSemaphores){
    int dessert_count = 0;
    char demand[2] = {'F', 'S'};
    
    dessert_count = _chef_work(baker, namedSemaphores, demand, 2);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
    return dessert_count;
}

int chef3(BakerSync *baker, NamedSemaphores *namedSemaphores){
    int dessert_count = 0;
    char demand[2] = {'F', 'M'};
    
    dessert_count = _chef_work(baker, namedSemaphores, demand, 3);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
    return dessert_count;
}

int chef4(BakerSync *baker, NamedSemaphores *namedSemaphores){
    int dessert_count = 0;
    char demand[2] = {'M', 'W'};
    
    dessert_count = _chef_work(baker, namedSemaphores, demand, 4);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
    return dessert_count;
}

int chef5(BakerSync *baker, NamedSemaphores *namedSemaphores){
    int dessert_count = 0;
    char demand[2] = {'M', 'S'};
    
    dessert_count = _chef_work(baker, namedSemaphores, demand, 5);

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
    return dessert_count;
}
//CHEFS END

//CHEF HELPER
void pusher0(BakerSync *baker, NamedSemaphores *namedSemaphores){
    while(1){
        sem_wait(namedSemaphores->milk);
        if(interrupted)
            break;
        sem_wait(namedSemaphores->mutex);
        if(interrupted)
            break;
        if(baker->is_flour){
            baker->is_flour = 0;
            sem_post(namedSemaphores->chef_checked[3]);
        }
        else if(baker->is_walnut){
            baker->is_walnut = 0;
            sem_post(namedSemaphores->chef_checked[4]);
        }
        else if(baker->is_sugar){
            baker->is_sugar = 0;
            sem_post(namedSemaphores->chef_checked[5]);
        }
        else{
            baker->is_milk = 1;
        }
        sem_post(namedSemaphores->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
}

void pusher1(BakerSync *baker, NamedSemaphores *namedSemaphores){
    while(1){
        sem_wait(namedSemaphores->flour);
        if(interrupted)
            break;
        sem_wait(namedSemaphores->mutex);
        if(interrupted)
            break;
        if(baker->is_milk){
            baker->is_milk = 0;
            sem_post(namedSemaphores->chef_checked[3]);
        }
        else if(baker->is_walnut){
            baker->is_walnut = 0;
            sem_post(namedSemaphores->chef_checked[1]);
        }
        else if(baker->is_sugar){
            baker->is_sugar = 0;
            sem_post(namedSemaphores->chef_checked[2]);
        }
        else{
            baker->is_flour = 1;
        }
        sem_post(namedSemaphores->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
}

void pusher2(BakerSync *baker, NamedSemaphores *namedSemaphores){
    while(1){
        sem_wait(namedSemaphores->walnut);
        if(interrupted)
            break;
        sem_wait(namedSemaphores->mutex);
        if(interrupted)
            break;
        if(baker->is_milk){
            baker->is_milk = 0;
            sem_post(namedSemaphores->chef_checked[4]);
        }
        else if(baker->is_flour){
            baker->is_flour = 0;
            sem_post(namedSemaphores->chef_checked[1]);
        }
        else if(baker->is_sugar){
            baker->is_sugar = 0;
            sem_post(namedSemaphores->chef_checked[0]);
        }
        else{
            baker->is_walnut = 1;
        }
        sem_post(namedSemaphores->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
}

void pusher3(BakerSync *baker, NamedSemaphores *namedSemaphores){
    while(1){
        sem_wait(namedSemaphores->sugar);
        if(interrupted)
            break;
        sem_wait(namedSemaphores->mutex);
        if(interrupted)
            break;
        if(baker->is_milk){
            baker->is_milk = 0;
            sem_post(namedSemaphores->chef_checked[5]);
        }
        else if(baker->is_flour){
            baker->is_flour = 0;
            sem_post(namedSemaphores->chef_checked[2]);
        }
        else if(baker->is_walnut){
            baker->is_walnut = 0;
            sem_post(namedSemaphores->chef_checked[0]);
        }
        else{
            baker->is_sugar = 1;
        }
        sem_post(namedSemaphores->mutex);
    }

    if(munmap(baker, sizeof(BakerSync)) < 0){
        perror("munmap");
    }
    close_semaphores(namedSemaphores);
}
//HELPER END

int get_arguments(int argc, char *argv[], char **input_file, char *semaphore_names[12]){
    *input_file = NULL;
    for (int i = 1; i < argc; i++){
        if (strcmp(argv[i], "-i") == 0){
            *input_file = argv[i + 1];
        }
        else if(strcmp(argv[i], "-n") == 0){
            i++;
            for (int j = 0; j < 12; j++){
                if(j + i == argc || strcmp(argv[i + j], "-i") == 0) {
                    i += (j - 1);
                    break;
                }
                semaphore_names[j] = argv[i + j];
            }
        }
    }

    if(*input_file == NULL)
        return -1;

    return 0;
}

int _sem_initializations(BakerSync *baker){
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

    return baker;
}



int open_semaphores(NamedSemaphores *semaphores, char *semaphore_names[12]){
    for(int i = 0; i < 6; ++i){
        semaphores->chef_checked[i] = sem_open(semaphore_names[i], O_CREAT | O_RDWR, 0666, 0);
        if(semaphores->chef_checked[i] == SEM_FAILED){
            perror("sem_open");
            return -1;
        }
    }

    semaphores->walnut = sem_open(semaphore_names[6], O_CREAT | O_RDWR, 0666, 0);
    if(semaphores->walnut == SEM_FAILED){
        perror("sem_open");
        return -1;
    }

    semaphores->flour = sem_open(semaphore_names[7], O_CREAT | O_RDWR, 0666, 0);
    if(semaphores->flour == SEM_FAILED){
        perror("sem_open");
        return -1;
    }

    semaphores->milk = sem_open(semaphore_names[8], O_CREAT | O_RDWR, 0666, 0);
    if(semaphores->milk == SEM_FAILED){
        perror("sem_open");
        return -1;
    }

    semaphores->sugar = sem_open(semaphore_names[9], O_CREAT | O_RDWR, 0666, 0);
    if(semaphores->sugar == SEM_FAILED){
        perror("sem_open");
        return -1;
    }

    semaphores->dessert_available = sem_open(semaphore_names[10], O_CREAT | O_RDWR, 0666, 0);
    if(semaphores->dessert_available == SEM_FAILED){
        perror("sem_open");
        return -1;
    }

    semaphores->mutex = sem_open(semaphore_names[11], O_CREAT | O_RDWR, 0666, 0);
    if(semaphores->mutex == SEM_FAILED){
        perror("sem_open");
        return -1;
    }

    return 0;
}

void close_semaphores(NamedSemaphores *semaphores){
    for(int i = 0; i < 6; ++i){
        if(sem_close(semaphores->chef_checked[i]) < 0){
            perror("sem_close");
        }
    }

    if(sem_close(semaphores->walnut) < 0){
        perror("sem_close");
    }

    if(sem_close(semaphores->flour) < 0){
        perror("sem_close");
    }

    if(sem_close(semaphores->milk) < 0){
        perror("sem_close");
    }

    if(sem_close(semaphores->sugar) < 0){
        perror("sem_close");
    }   

    if(sem_close(semaphores->dessert_available) < 0){
        perror("sem_close");
    }

    if(sem_close(semaphores->mutex) < 0){
        perror("sem_close");
    }
}

void unlink_semaphores(char *semaphore_names[12]){
    for(int i = 0; i < 12; ++i){
        if(sem_unlink(semaphore_names[i]) < 0){
            perror("sem_unlink");
        }
    }
}

