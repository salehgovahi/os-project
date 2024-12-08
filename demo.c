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

#define MAX_PRODUCTS 653
#define MAX_CATEGORIES 8
#define MAX_STORES 3
#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 256
#define MAX_ORDER_ITEMS 256
#define MAX_THRESHOLD 1000000.0f

typedef struct
{
    char name[MAX_NAME_LENGTH];
    float price;
    int entity;
    float score;
    int rating_count; // New field for rating count
    char last_modified[50];
} Product;

typedef struct
{
    char category_name[MAX_NAME_LENGTH];
    Product products[MAX_PRODUCTS];
    int product_count;
} Category;

typedef struct
{
    char store_name[MAX_NAME_LENGTH];
    Category categories[MAX_CATEGORIES];
    int category_count;
} Store;

typedef struct
{
    char product_name[MAX_NAME_LENGTH];
    int quantity;
} OrderItem;

typedef struct
{
    char username[MAX_NAME_LENGTH];
    OrderItem order_list[MAX_ORDER_ITEMS];
    int order_count;
    float price_threshold;
} UserOrder;

typedef struct
{
    Store *stores;
    int store_count;
    UserOrder *user_order;
} ThreadData;


pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t score_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t final_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t score_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t final_cond = PTHREAD_COND_INITIALIZER;

int order_thread_should_run = 0;
int score_thread_should_run = 0;
int final_thread_should_run = 0;

/// Functions:
int parse_product(FILE *file, Product *product)
{
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "Name: %[^\n]", product->name) == 1)
            continue;
        if (sscanf(line, "Price: %f", &product->price) == 1)
            continue;
        if (sscanf(line, "Score: %f", &product->score) == 1)
            continue;
        if (sscanf(line, "Entity: %d", &product->entity) == 1)
            continue;
        if (sscanf(line, "Rating Count: %d", &product->rating_count) == 1)
            continue; // Add this line
        if (sscanf(line, "Last Modified: %[^\n]", product->last_modified) == 1)
            continue;
    }

    // Set default rating count if not found in the file
    if (product->rating_count == 0) {
        product->rating_count = 1; 
    }

    return 0;
}
int load_dataset(Store *stores, const char *base_path)
{
    DIR *base_dir = opendir(base_path);
    if (!base_dir)
    {
        perror("Failed to open dataset directory");
        return -1;
    }

    struct dirent *store_entry;
    int store_count = 0;

    while ((store_entry = readdir(base_dir)) != NULL)
    {
        if (store_entry->d_type == DT_DIR && store_entry->d_name[0] != '.')
        {
            if (store_count >= MAX_STORES)
            {
                fprintf(stderr, "Warning: Maximum store limit reached. Some stores may not be loaded.\n");
                break;
            }
            snprintf(stores[store_count].store_name, sizeof(stores[store_count].store_name), "%s", store_entry->d_name);

            char store_path[MAX_PATH_LENGTH];
            snprintf(store_path, sizeof(store_path), "%s/%s", base_path, store_entry->d_name);

            DIR *store_dir = opendir(store_path);
            if (!store_dir)
            {
                perror("Failed to open store directory");
                continue;
            }

            stores[store_count].category_count = 0;

            struct dirent *category_entry;
            while ((category_entry = readdir(store_dir)) != NULL)
            {
                if (category_entry->d_type == DT_DIR && category_entry->d_name[0] != '.')
                {
                    if (stores[store_count].category_count >= MAX_CATEGORIES)
                    {
                        fprintf(stderr, "Warning: Maximum category limit reached in store %s.\n", stores[store_count].store_name);
                        break;
                    }

                    snprintf(stores[store_count].categories[stores[store_count].category_count].category_name,
                             sizeof(stores[store_count].categories[stores[store_count].category_count].category_name),
                             "%s", category_entry->d_name);

                    char category_path[MAX_PATH_LENGTH];

                    if (snprintf(category_path, sizeof(category_path), "%s/%s", store_path, category_entry->d_name) >= sizeof(category_path))
                    {
                        fprintf(stderr, "Warning: Category path too long, skipping: %s/%s\n", store_path, category_entry->d_name);
                        continue;
                    }

                    DIR *category_dir = opendir(category_path);
                    if (!category_dir)
                    {
                        perror("Failed to open category directory");
                        continue;
                    }

                    stores[store_count].categories[stores[store_count].category_count].product_count = 0;

                    struct dirent *product_entry;
                    while ((product_entry = readdir(category_dir)) != NULL)
                    {
                        if (product_entry->d_type == DT_REG && strstr(product_entry->d_name, ".txt") != NULL)
                        {
                            char product_file_path[MAX_PATH_LENGTH];

                            if (snprintf(product_file_path, sizeof(product_file_path), "%s/%s", category_path, product_entry->d_name) >= sizeof(product_file_path))
                            {
                                fprintf(stderr, "Warning: Product file path too long, skipping: %s/%s\n", category_path, product_entry->d_name);
                                continue;
                            }

                            FILE *file = fopen(product_file_path, "r");
                            if (file)
                            {
                                Product product;
                                if (parse_product(file, &product) == 0)
                                {
                                    stores[store_count].categories[stores[store_count].category_count].products[stores[store_count].categories[stores[store_count].category_count].product_count++] = product;
                                }
                                fclose(file);
                            }
                        }
                    }
                    closedir(category_dir);
                    stores[store_count].category_count++;
                }
            }
            closedir(store_dir);
            store_count++;
        }
    }

    closedir(base_dir);
    return store_count;
}

UserOrder get_user_order()
{
    UserOrder user_order;

    // Get username
    printf("Username: ");
    fgets(user_order.username, sizeof(user_order.username), stdin);
    user_order.username[strcspn(user_order.username, "\n")] = 0;

    // Initialize order count
    user_order.order_count = 0;

    printf("Enter your order list (product_name quantity), type 'done' when finished:\n");

    while (user_order.order_count < MAX_ORDER_ITEMS)
    {
        char line[256];
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            break;
        }
        line[strcspn(line, "\n")] = 0;

        // Check if user is done
        if (strcmp(line, "done") == 0)
        {
            break;
        }

        // Find last space to separate product name from quantity
        char *last_space = strrchr(line, ' ');
        if (last_space == NULL)
        {
            printf("Invalid input format. Please use format: 'product name quantity'\n");
            continue;
        }

        // Parse quantity
        int quantity;
        if (sscanf(last_space + 1, "%d", &quantity) != 1)
        {
            printf("Invalid quantity. Please enter a valid number.\n");
            continue;
        }

        // Get product name length
        int name_length = last_space - line;
        if (name_length >= MAX_NAME_LENGTH)
        {
            printf("Product name is too long. Maximum length is %d characters.\n", MAX_NAME_LENGTH - 1);
            continue;
        }

        // Copy and process product name
        strncpy(user_order.order_list[user_order.order_count].product_name, line, name_length);
        user_order.order_list[user_order.order_count].product_name[name_length] = '\0';

        // Trim spaces
        char *start = user_order.order_list[user_order.order_count].product_name;
        char *end = start + strlen(start) - 1;
        while (*start && isspace(*start))
            start++;
        while (end > start && isspace(*end))
            *end-- = '\0';
        memmove(user_order.order_list[user_order.order_count].product_name, start, strlen(start) + 1);

        // Set quantity and increment counter
        user_order.order_list[user_order.order_count].quantity = quantity;
        user_order.order_count++;

        printf("Added: '%s' (Quantity: %d)\n",
               user_order.order_list[user_order.order_count - 1].product_name,
               user_order.order_list[user_order.order_count - 1].quantity);
    }

    // Get price threshold
    printf("Price threshold (default is %.2f): ", MAX_THRESHOLD);
    char input_buffer[256];
    fgets(input_buffer, sizeof(input_buffer), stdin);

    if (input_buffer[0] == '\n')
    {
        user_order.price_threshold = MAX_THRESHOLD;
        printf("Using default price threshold: %.2f\n", MAX_THRESHOLD);
    }
    else
    {
        if (sscanf(input_buffer, "%f", &user_order.price_threshold) != 1 ||
            user_order.price_threshold <= 0)
        {
            user_order.price_threshold = MAX_THRESHOLD;
            printf("Invalid threshold. Using default: %.2f\n", MAX_THRESHOLD);
        }
    }

    return user_order;
}
// Thread functions
void *process_orders(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    while (1)
    {
        pthread_mutex_lock(&order_mutex);
        while (!order_thread_should_run)
        {
            pthread_cond_wait(&order_cond, &order_mutex);
        }

        printf("Order thread %lu woke up and processing...\n", pthread_self());

        // Your order processing logic here
        for (int i = 0; i < data->store_count; i++)
        {
            printf("Processing orders for store: %s\n", data->stores[i].store_name);
            // Add your order processing logic here
        }

        order_thread_should_run = 0; // Reset the flag
        pthread_mutex_unlock(&order_mutex);
    }

    return NULL;
}

void *process_scores(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    while (1)
    {
        pthread_mutex_lock(&score_mutex);
        while (!score_thread_should_run)
        {
            pthread_cond_wait(&score_cond, &score_mutex);
        }

        printf("Score thread %lu woke up and processing...\n", pthread_self());

        // Your score processing logic here
        for (int i = 0; i < data->store_count; i++)
        {
            printf("Processing scores for store: %s\n", data->stores[i].store_name);
            // Add your score processing logic here
        }

        score_thread_should_run = 0; // Reset the flag
        pthread_mutex_unlock(&score_mutex);
    }

    return NULL;
}

void *process_final(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    while (1)
    {
        pthread_mutex_lock(&final_mutex);
        while (!final_thread_should_run)
        {
            pthread_cond_wait(&final_cond, &final_mutex);
        }

        printf("Final thread %lu woke up and processing...\n", pthread_self());

        // Your final processing logic here
        for (int i = 0; i < data->store_count; i++)
        {
            printf("Final processing for store: %s\n", data->stores[i].store_name);
            // Add your final processing logic here
        }

        final_thread_should_run = 0; // Reset the flag
        pthread_mutex_unlock(&final_mutex);
    }

    return NULL;
}

// Functions to wake up threads
void wake_order_thread()
{
    pthread_mutex_lock(&order_mutex);
    order_thread_should_run = 1;
    pthread_cond_signal(&order_cond);
    pthread_mutex_unlock(&order_mutex);
}

void wake_score_thread()
{
    pthread_mutex_lock(&score_mutex);
    score_thread_should_run = 1;
    pthread_cond_signal(&score_cond);
    pthread_mutex_unlock(&score_mutex);
}

void wake_final_thread()
{
    pthread_mutex_lock(&final_mutex);
    final_thread_should_run = 1;
    pthread_cond_signal(&final_cond);
    pthread_mutex_unlock(&final_mutex);
}

int main()
{
    Store stores[MAX_STORES];
    const char *base_path = "Dataset";
    const char *output_directory = "Output";

    mkdir(output_directory, 0755);
    int store_count = load_dataset(stores, base_path);

    UserOrder user_order = get_user_order();

    pid_t user_pid = fork();
    if (user_pid < 0)
    {
        perror("Error forking user process");
        exit(EXIT_FAILURE);
    }

    if (user_pid == 0)
    {
        printf("%s create PID: %d\n", user_order.username, getpid());
        user_pid = getpid();

        // Create thread data
        ThreadData thread_data = {
            .stores = stores,
            .store_count = store_count,
            .user_order = &user_order};

        pthread_t order_thread_id;
        pthread_t score_thread_id;
        pthread_t final_thread_id;

        // Create threads
        pthread_create(&order_thread_id, NULL, process_orders, &thread_data);
        pthread_create(&score_thread_id, NULL, process_scores, &thread_data);
        pthread_create(&final_thread_id, NULL, process_final, &thread_data);

        printf("PID %d create thread for Orders TID: %lu\n", getpid(), order_thread_id);
        printf("PID %d create thread for Score TID: %lu\n", getpid(), score_thread_id);
        printf("PID %d create thread for Final TID: %lu\n", getpid(), final_thread_id);

        for (int m = 0; m < user_order.order_count; m++)
        {
            for (int i = 0; i < store_count; i++)
            {
                pid_t store_pid = fork();
                if (store_pid < 0)
                {
                    perror("Error forking store process");
                    exit(EXIT_FAILURE);
                }

                if (store_pid == 0)
                {
                    store_pid = getpid();
                    printf("PID %d create child for %s PID: %d\n", user_pid, stores[i].store_name, getpid());

                    for (int j = 0; j < stores[i].category_count; j++)
                    {
                        pid_t category_pid = fork();
                        if (category_pid < 0)
                        {
                            perror("Error forking category process");
                            exit(EXIT_FAILURE);
                        }

                        if (category_pid == 0)
                        {
                            printf("PID %d create child for %s PID: %d\n", store_pid, stores[i].categories[j].category_name, getpid());

                            for (int k = 0; k < stores[i].categories[j].product_count; k++)
                            {
                                char log_file_path[MAX_PATH_LENGTH];
                                snprintf(log_file_path, sizeof(log_file_path), "%s/log_%s", output_directory, user_order.username);
                                pthread_t threads[MAX_PRODUCTS];
                                int thread_count = 0;
                            }

                            exit(0);
                        }
                    }

                    while (wait(NULL) > 0)
                        ;
                    exit(0);
                }
            }

            while (wait(NULL) > 0)
                ;
        }

        // // Example of controlling threads
        // sleep(2); // Wait for 2 seconds

        // // Wake up order thread
        // wake_order_thread();
        // printf("Signaled order thread to wake up\n");
        // sleep(2);

        // // Wake up score thread
        // wake_score_thread();
        // printf("Signaled score thread to wake up\n");
        // sleep(2);

        // // Wake up final thread
        // wake_final_thread();
        // printf("Signaled final thread to wake up\n");
        // sleep(2);

        // Clean up
        pthread_join(order_thread_id, NULL);
        pthread_join(score_thread_id, NULL);
        pthread_join(final_thread_id, NULL);

        pthread_mutex_destroy(&order_mutex);
        pthread_cond_destroy(&order_cond);
        pthread_mutex_destroy(&score_mutex);
        pthread_cond_destroy(&score_cond);
        pthread_mutex_destroy(&final_mutex);
        pthread_cond_destroy(&final_cond);

        return 0;
    }

    waitpid(user_pid, NULL, 0);
    return 0;
}
