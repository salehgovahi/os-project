#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include "shared_data.h"
#include "load_data.h"
#include "process.h"

void deep_copy_stores(Store *destination, const Store *source, int store_count)
{
    for (int i = 0; i < store_count; i++)
    {
        strcpy(destination[i].store_name, source[i].store_name);
        destination[i].category_count = source[i].category_count;

        for (int j = 0; j < source[i].category_count; j++)
        {
            strcpy(destination[i].categories[j].category_name, source[i].categories[j].category_name);
            destination[i].categories[j].product_count = source[i].categories[j].product_count;

            for (int k = 0; k < source[i].categories[j].product_count; k++)
            {
                strcpy(destination[i].categories[j].products[k].name, source[i].categories[j].products[k].name);
                destination[i].categories[j].products[k].price = source[i].categories[j].products[k].price;
                destination[i].categories[j].products[k].entity = source[i].categories[j].products[k].entity;
                destination[i].categories[j].products[k].score = source[i].categories[j].products[k].score;
            }
        }
    }
}

int main()
{
    Store stores[MAX_STORES];
    const char *base_path = "Dataset";
    const char *output_directory = "Output";

    mkdir(output_directory, 0755);

    int store_count = load_dataset(stores, base_path);

    int shm_fd = shm_open("/global_shared_memory", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(GlobalSharedMemory)) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    global_shared_memory = mmap(NULL, sizeof(GlobalSharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (global_shared_memory == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&global_shared_memory->mutex, &mutex_attr);

    deep_copy_stores(global_shared_memory->stores, stores, store_count);

    while (user_process(store_count,output_directory));

    pthread_mutex_destroy(&global_shared_memory->mutex);
    munmap(global_shared_memory, sizeof(GlobalSharedMemory));
    shm_unlink("/global_shared_memory");

    return 0;
}