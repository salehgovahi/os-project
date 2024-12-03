#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
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
    float score;
    int entity;
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
    Product *product;
    const char *store_name;
    const char *category_name;
    FILE *log_file;
    float price_threshold;
    int quantity;
    long int thread_id;
    long int process_id;
} ProductContext;

int parse_product(FILE *file, Product *product)
{
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "Name: %[^\n]", product->name) == 1)
            continue;
        if (sscanf(line, "Price: %f", &product->price) == 1)
            continue;
        if (sscanf(line, "Score: %f", &product->score) == 1)
            continue;
        if (sscanf(line, "Entity: %d", &product->entity) == 1)
            continue;
        if (sscanf(line, "Last Modified: %[^\n]", product->last_modified) == 1)
            continue;
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

void *process_product(void *arg)
{
    ProductContext *context = (ProductContext *)arg;
    Product *product = context->product;
    const char *store_name = context->store_name;
    const char *category_name = context->category_name;

    float total_price = product->price * context->quantity;

    if (total_price <= context->price_threshold || context->price_threshold < 0)
    {
        if (context->log_file)
        {
            fprintf(context->log_file, "Thread ID: %ld, PID: %ld, Store: %s, Category: %s, Product: %s, Quantity: %d, Total Price: %.2f\n",
                    context->thread_id, context->process_id, store_name, category_name, product->name, context->quantity, total_price);
        }
    }

    free(context);
    return NULL;
}


void *find_product(void *arg)
{
    ProductContext *context = (ProductContext *)arg;
    Product *product = context->product;
    const char *store_name = context->store_name;
    const char *category_name = context->category_name;

    float total_price = product->price * context->quantity;

    if (total_price <= context->price_threshold || context->price_threshold < 0)
    {
        if (context->log_file)
        {
            fprintf(context->log_file, "Store: %s, Category: %s, Product: %s, Quantity: %d, Total Price: %.2f\n",
                    store_name, category_name, product->name, context->quantity, total_price);
        }
    }

    free(context);
    return NULL;
}

void *process_orders(void *arg)
{
    printf("Processing orders in thread ID: %lu\n", pthread_self());
    return NULL;
}

void *process_scores(void *arg)
{
    printf("Processing scores in thread ID: %lu\n", pthread_self());
    return NULL;
}

void *process_final(void *arg)
{
    printf("Processing final in thread ID: %lu\n", pthread_self());
    return NULL;
}

int main()
{
    Store stores[MAX_STORES];
    const char *base_path = "Dataset";
    const char *output_directory = "Output";
    const char *log_file_path = "a.txt";

    mkdir(output_directory, 0755);
    int store_count = load_dataset(stores, base_path);

    UserOrder user_order;
    printf("Username: ");
    fgets(user_order.username, sizeof(user_order.username), stdin);
    user_order.username[strcspn(user_order.username, "\n")] = 0;

    printf("Enter your order list (product_name quantity), type 'done' when finished:\n");
    user_order.order_count = 0;

    while (user_order.order_count < MAX_ORDER_ITEMS)
    {
        char line[256];
        fgets(line, sizeof(line), stdin);
        if (strcmp(line, "done\n") == 0)
        {
            break;
        }
        sscanf(line, "%s %d", user_order.order_list[user_order.order_count].product_name,
               &user_order.order_list[user_order.order_count].quantity);
        user_order.order_count++;
    }

    float price_threshold = MAX_THRESHOLD;

    printf("Price threshold (default is %.2f): ", MAX_THRESHOLD);
    char input_buffer[256];
    fgets(input_buffer, sizeof(input_buffer), stdin);

    if (input_buffer[0] == '\n')
    {
        user_order.price_threshold = MAX_THRESHOLD;
        printf("No input provided. Setting price threshold to default value: %.2f\n", MAX_THRESHOLD);
    }
    else
    {

        float price_threshold;
        int threshold_status = sscanf(input_buffer, "%f", &price_threshold);

        if (threshold_status != 1 || price_threshold <= 0)
        {
            user_order.price_threshold = MAX_THRESHOLD;
        }
        else
        {
            user_order.price_threshold = price_threshold;
        }
    }

    pid_t user_pid = fork();
    if (user_pid < 0)
    {
        perror("Error forking user process");
        exit(EXIT_FAILURE);
    }

    if (user_pid == 0)
    {

        printf("User Process (PID: %d)\n", getpid());
        FILE *log_file = fopen(log_file_path, "w");
        if (!log_file)
        {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }

        pthread_t order_thread_id;
        pthread_t score_thread_id;
        pthread_t final_thread_id;

        pthread_create(&order_thread_id, NULL, process_orders, NULL);
        pthread_create(&score_thread_id, NULL, process_scores, NULL);
        pthread_create(&final_thread_id, NULL, process_final, NULL);

        pthread_join(order_thread_id, NULL);
        pthread_join(score_thread_id, NULL);
        pthread_join(final_thread_id, NULL);

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
                printf("Store Process (PID: %d) for Store: %s\n", getpid(), stores[i].store_name);

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
                        printf("Category Process (PID: %d) for Store: %s, Category: %s\n", getpid(), stores[i].store_name, stores[i].categories[j].category_name);

                        for (int k = 0; k < stores[i].categories[j].product_count; k++)
                        {
                            pthread_t product_thread;

                            for (int order_index = 0; order_index < user_order.order_count; order_index++)
                            {
                                if (strcmp(stores[i].categories[j].products[k].name, user_order.order_list[order_index].product_name) == 0)
                                {
                                    ProductContext *context = malloc(sizeof(ProductContext));
                                    context->product = &stores[i].categories[j].products[k];
                                    context->store_name = stores[i].store_name;
                                    context->category_name = stores[i].categories[j].category_name;
                                    context->log_file = log_file;
                                    context->price_threshold = user_order.price_threshold;
                                    context->quantity = user_order.order_list[order_index].quantity;
                                    context->process_id = getpid();
                                    context->thread_id = product_thread;

                                    pthread_create(&product_thread, NULL, process_product, (void *)context);
                                    pthread_join(product_thread, NULL);
                                    break;
                                }
                            }
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
        fclose(log_file);
        return 0;
    }

    wait(NULL);
    return 0;
}