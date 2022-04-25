#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

#define SHARED_MEM_NAME "bakery_shared_mem"

typedef struct bakerSync{
    char supply[2];
    sem_t chef_checked[6];
    sem_t dessert_available;
    sem_t delivery;
}BakerSync;


int get_arguments(int argc, char const *argv[], char **input_file);
BakerSync* init_shared_mem();
int chef0(BakerSync *baker);
int chef1(BakerSync *baker);
int chef2(BakerSync *baker);
int chef3(BakerSync *baker);
int chef4(BakerSync *baker);
int chef5(BakerSync *baker);


int main(int argc, char const *argv[])
{
    char *input_file;
    BakerSync *bakerSync;
    pid_t chef_pid[6];

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


    while(1){
        //TODO ADD SUPPLY
        for(int i = 0; i < 6; ++i)
            sem_post(&bakerSync->chef_checked[i]); //TODO CHECK IF THIS IS RIGHT
        sem_post(&bakerSync->delivery);

        sem_wait(&bakerSync->dessert_available);
    }


    return 0;
}

int _chef_work(BakerSync *baker, const char demand[2], int chef_num){
    int dessert_count = 0;

    while(1){
        sem_wait(&baker->chef_checked[chef_num]);
        sem_wait(&baker->delivery);
        if((baker->supply[0] == demand[0] && baker->supply[1] == demand[1]) || 
           (baker->supply[0] == demand[1] && baker->supply[1] == demand[0]))
        {
            dessert_count++;
            baker->supply[0] = '\0';
            baker->supply[1] = '\0';
            sem_post(&baker->dessert_available);
        }

        else{
            sem_post(&baker->delivery);
        }   
    }

    return dessert_count;
}

//CHEFS
int chef0(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'W', 'S'};
    
    dessert_count = _chef_work(baker, demand, 0);

    return dessert_count;
}

int chef1(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'W', 'F'};
    
    dessert_count = _chef_work(baker, demand, 1);

    return dessert_count;
}

int chef2(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'F', 'S'};
    
    dessert_count = _chef_work(baker, demand, 2);

    return dessert_count;
}

int chef3(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'F', 'M'};
    
    dessert_count = _chef_work(baker, demand, 3);

    return dessert_count;
}

int chef4(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'M', 'W'};
    
    dessert_count = _chef_work(baker, demand, 4);

    return dessert_count;
}

int chef5(BakerSync *baker){
    int dessert_count = 0;
    char demand[2] = {'M', 'S'};
    
    dessert_count = _chef_work(baker, demand, 5);

    return dessert_count;
}
//CHEFS END

//CHEF HELPER


int get_arguments(int argc, char const *argv[], char **input_file){
    input_file = NULL;
    for (int i = 1; i < argc; i++){
        if (strcmp(argv[i], "-i") == 0){
            input_file = argv[i + 1];
        }
    }

    if(input_file == NULL)
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

    if(sem_init(&baker->delivery, 1, 0) < 0){
        perror("sem_init");
        return -1;
    }

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
}